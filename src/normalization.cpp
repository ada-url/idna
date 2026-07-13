#include "ada/idna/normalization.h"
#include "table_store.hpp"
#include "normalization_tables.cpp"

#include <cstdint>

namespace ada::idna {

// See
// https://github.com/uni-algo/uni-algo/blob/c612968c5ed3ace39bde4c894c24286c5f2c7fe2/include/uni_algo/impl/impl_norm.h#L467
constexpr char32_t hangul_sbase = 0xAC00;
constexpr char32_t hangul_tbase = 0x11A7;
constexpr char32_t hangul_vbase = 0x1161;
constexpr char32_t hangul_lbase = 0x1100;
constexpr char32_t hangul_lcount = 19;
constexpr char32_t hangul_vcount = 21;
constexpr char32_t hangul_tcount = 28;
constexpr char32_t hangul_ncount = hangul_vcount * hangul_tcount;
constexpr char32_t hangul_scount =
    hangul_lcount * hangul_vcount * hangul_tcount;

// Canonical decomposition length (0 if none / compatibility-only / Hangul
// syllable). Length 1 means a singleton mapping (character is not NFC form).
// Length >= 2 means a primary composite that is already the NFC form of its
// decomposition (e.g. U+00E9 LATIN SMALL LETTER E WITH ACUTE).
static size_t canonical_decomp_length(char32_t current_character) noexcept {
  if (current_character >= hangul_sbase &&
      current_character < hangul_sbase + hangul_scount) {
    // Hangul precomposed syllables are NFC.
    return 0;
  }
  if (current_character >= 0x110000) {
    return 0;
  }
  const uint8_t di = decomposition_index[current_character >> 8];
  const uint16_t* const decomposition =
      decomposition_block_row(di) + (current_character % 256);
  size_t decomposition_length =
      (decomposition[1] >> 2) - (decomposition[0] >> 2);
  if ((decomposition_length > 0) && (decomposition[0] & 1)) {
    decomposition_length = 0;  // compatibility-only: ignored for NFC
  }
  return decomposition_length;
}

std::pair<bool, size_t> compute_decomposition_length(
    const std::u32string_view input) noexcept {
  bool decomposition_needed{false};
  size_t additional_elements{0};
  for (char32_t current_character : input) {
    size_t decomposition_length{0};

    if (current_character >= hangul_sbase &&
        current_character < hangul_sbase + hangul_scount) {
      // Full NFD-style Hangul decomp for the decompose() step.
      decomposition_length = 2;
      if ((current_character - hangul_sbase) % hangul_tcount) {
        decomposition_length = 3;
      }
    } else if (current_character < 0x110000) {
      const uint8_t di = decomposition_index[current_character >> 8];
      const uint16_t* const decomposition =
          decomposition_block_row(di) + (current_character % 256);
      decomposition_length = (decomposition[1] >> 2) - (decomposition[0] >> 2);
      if ((decomposition_length > 0) && (decomposition[0] & 1)) {
        decomposition_length = 0;
      }
    }
    if (decomposition_length != 0) {
      decomposition_needed = true;
      additional_elements += decomposition_length - 1;
    }
  }
  return {decomposition_needed, additional_elements};
}

void decompose(std::u32string& input, size_t additional_elements) {
  input.resize(input.size() + additional_elements);
  for (size_t descending_idx = input.size(),
              input_count = descending_idx - additional_elements;
       input_count--;) {
    if (input[input_count] >= hangul_sbase &&
        input[input_count] < hangul_sbase + hangul_scount) {
      // Hangul decomposition.
      char32_t s_index = input[input_count] - hangul_sbase;
      if (s_index % hangul_tcount != 0) {
        input[--descending_idx] = hangul_tbase + s_index % hangul_tcount;
      }
      input[--descending_idx] =
          hangul_vbase + (s_index % hangul_ncount) / hangul_tcount;
      input[--descending_idx] = hangul_lbase + s_index / hangul_ncount;
    } else if (input[input_count] < 0x110000) {
      const uint16_t* decomposition =
          decomposition_block_row(
              decomposition_index[input[input_count] >> 8]) +
          (input[input_count] % 256);
      uint16_t decomposition_length =
          (decomposition[1] >> 2) - (decomposition[0] >> 2);
      if (decomposition_length > 0 && (decomposition[0] & 1)) {
        decomposition_length = 0;
      }
      if (decomposition_length > 0) {
        const size_t base = static_cast<size_t>(decomposition[0] >> 2);
        if (base + decomposition_length > decomposition_data_size) {
          input[--descending_idx] = input[input_count];
        } else {
          while (decomposition_length-- > 0) {
            input[--descending_idx] =
                decomposition_data[base + decomposition_length];
          }
        }
      } else {
        input[--descending_idx] = input[input_count];
      }
    } else {
      input[--descending_idx] = input[input_count];
    }
  }
}

uint8_t get_ccc(char32_t c) noexcept {
  if (c >= 0x110000) {
    return 0;
  }
  return ccc_block_row(ccc_index[c >> 8])[c % 256];
}

void sort_marks(std::u32string& input) {
  for (size_t idx = 1; idx < input.size(); idx++) {
    uint8_t ccc = get_ccc(input[idx]);
    if (ccc == 0) {
      continue;
    }
    auto current_character = input[idx];
    size_t back_idx = idx;
    while (back_idx != 0 && get_ccc(input[back_idx - 1]) > ccc) {
      input[back_idx] = input[back_idx - 1];
      back_idx--;
    }
    input[back_idx] = current_character;
  }
}

void decompose_nfc(std::u32string& input) {
  /**
   * Decompose the domain_name string to Unicode Normalization Form C.
   * @see https://www.unicode.org/reports/tr46/#ProcessingStepDecompose
   */
  auto [decomposition_needed, additional_elements] =
      compute_decomposition_length(input);
  if (decomposition_needed) {
    decompose(input, additional_elements);
  }
  sort_marks(input);
}

// Returns true if a composition step would change the string.
static bool would_compose(std::u32string_view input) noexcept {
  for (size_t input_count = 0; input_count < input.size();) {
    const char32_t current = input[input_count];
    if (current >= hangul_lbase && current < hangul_lbase + hangul_lcount) {
      if (input_count + 1 < input.size() &&
          input[input_count + 1] >= hangul_vbase &&
          input[input_count + 1] < hangul_vbase + hangul_vcount) {
        return true;
      }
      ++input_count;
      continue;
    }
    if (current >= hangul_sbase && current < hangul_sbase + hangul_scount) {
      if ((current - hangul_sbase) % hangul_tcount &&
          input_count + 1 < input.size() &&
          input[input_count + 1] > hangul_tbase &&
          input[input_count + 1] < hangul_tbase + hangul_tcount) {
        return true;
      }
      ++input_count;
      continue;
    }
    if (current < 0x110000) {
      const uint16_t* composition =
          composition_block_row(composition_index[current >> 8]) +
          (current % 256);
      int32_t previous_ccc = -1;
      size_t j = input_count;
      for (; j + 1 < input.size(); ++j) {
        uint8_t ccc = get_ccc(input[j + 1]);
        if (composition[1] != composition[0] && previous_ccc < ccc) {
          int left = composition[0];
          int right = composition[1];
          if (left < 0 || right < left ||
              static_cast<size_t>(right) > composition_data_size) {
            break;
          }
          while (left + 2 < right) {
            int middle = left + (((right - left) >> 1) & ~1);
            if (composition_data[static_cast<size_t>(middle)] <= input[j + 1]) {
              left = middle;
            }
            if (composition_data[static_cast<size_t>(middle)] >= input[j + 1]) {
              right = middle;
            }
          }
          if (static_cast<size_t>(left + 1) < composition_data_size &&
              composition_data[static_cast<size_t>(left)] == input[j + 1]) {
            return true;
          }
        }
        if (ccc == 0) {
          break;
        }
        previous_ccc = ccc;
      }
      input_count = j + 1;
      continue;
    }
    ++input_count;
  }
  return false;
}

bool is_already_nfc(std::u32string_view input) noexcept {
  if (input.empty()) {
    return true;
  }
  if (!tables_are_ready() && !ensure_tables()) {
    return false;
  }
  // 1) Singleton decompositions are never NFC (e.g. U+212B ANGSTROM SIGN).
  //    Multi-code-point decomps are primary composites that are NFC as-is.
  for (char32_t c : input) {
    if (canonical_decomp_length(c) == 1) {
      return false;
    }
  }
  // 2) Combining marks already in canonical order.
  uint8_t prev_ccc = 0;
  for (char32_t c : input) {
    uint8_t ccc = get_ccc(c);
    if (ccc != 0 && prev_ccc > ccc) {
      return false;
    }
    prev_ccc = ccc;
  }
  // 3) No pairwise composition would apply (including Hangul L+V / LV+T).
  return !would_compose(input);
}

void compose(std::u32string& input) {
  /**
   * Compose the domain_name string to Unicode Normalization Form C.
   * @see https://www.unicode.org/reports/tr46/#ProcessingStepCompose
   */
  size_t input_count{0};
  size_t composition_count{0};
  for (; input_count < input.size(); input_count++, composition_count++) {
    input[composition_count] = input[input_count];
    if (input[input_count] >= hangul_lbase &&
        input[input_count] < hangul_lbase + hangul_lcount) {
      if (input_count + 1 < input.size() &&
          input[input_count + 1] >= hangul_vbase &&
          input[input_count + 1] < hangul_vbase + hangul_vcount) {
        input[composition_count] =
            hangul_sbase +
            ((input[input_count] - hangul_lbase) * hangul_vcount +
             input[input_count + 1] - hangul_vbase) *
                hangul_tcount;
        input_count++;
        if (input_count + 1 < input.size() &&
            input[input_count + 1] > hangul_tbase &&
            input[input_count + 1] < hangul_tbase + hangul_tcount) {
          input[composition_count] += input[++input_count] - hangul_tbase;
        }
      }
    } else if (input[input_count] >= hangul_sbase &&
               input[input_count] < hangul_sbase + hangul_scount) {
      if ((input[input_count] - hangul_sbase) % hangul_tcount &&
          input_count + 1 < input.size() &&
          input[input_count + 1] > hangul_tbase &&
          input[input_count + 1] < hangul_tbase + hangul_tcount) {
        input[composition_count] += input[++input_count] - hangul_tbase;
      }
    } else if (input[input_count] < 0x110000) {
      const uint16_t* composition =
          composition_block_row(composition_index[input[input_count] >> 8]) +
          (input[input_count] % 256);
      size_t initial_composition_count = composition_count;
      for (int32_t previous_ccc = -1; input_count + 1 < input.size();
           input_count++) {
        uint8_t ccc = get_ccc(input[input_count + 1]);

        if (composition[1] != composition[0] && previous_ccc < ccc) {
          int left = composition[0];
          int right = composition[1];
          if (left < 0 || right < left ||
              static_cast<size_t>(right) > composition_data_size) {
            break;
          }
          while (left + 2 < right) {
            int middle = left + (((right - left) >> 1) & ~1);
            if (composition_data[static_cast<size_t>(middle)] <=
                input[input_count + 1]) {
              left = middle;
            }
            if (composition_data[static_cast<size_t>(middle)] >=
                input[input_count + 1]) {
              right = middle;
            }
          }
          if (static_cast<size_t>(left + 1) < composition_data_size &&
              composition_data[static_cast<size_t>(left)] ==
                  input[input_count + 1]) {
            char32_t composed = composition_data[static_cast<size_t>(left + 1)];
            input[initial_composition_count] = composed;
            composition =
                composition_block_row(composition_index[composed >> 8]) +
                (composed % 256);
            continue;
          }
        }

        if (ccc == 0) {
          break;
        }
        previous_ccc = ccc;
        input[++composition_count] = input[input_count + 1];
      }
    }
  }

  if (composition_count < input_count) {
    input.resize(composition_count);
  }
}

bool normalize(std::u32string& input) {
  /**
   * Normalize the characters according to IDNA (Unicode Normalization Form C).
   * @see https://www.unicode.org/reports/tr46/#ProcessingStepNormalize
   */
  if (!ensure_tables() || decomposition_index == nullptr ||
      composition_index == nullptr) {
    return false;
  }
  if (is_already_nfc(input)) {
    return true;
  }
  decompose_nfc(input);
  compose(input);
  return true;
}

}  // namespace ada::idna
