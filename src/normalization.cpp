#include "ada/idna/normalization.h"
#include "normalization_tables.cpp"

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

std::pair<bool, size_t> compute_decomposition_length(
    const std::u32string_view input) noexcept {
  bool decomposition_needed{false};
  size_t additional_elements{0};
  for (char32_t current_character : input) {
    size_t decomposition_length{0};

    if (current_character >= hangul_sbase &&
        current_character < hangul_sbase + hangul_scount) {
      decomposition_length = 2;
      if ((current_character - hangul_sbase) % hangul_tcount) {
        decomposition_length = 3;
      }
    } else if (current_character < 0x110000) {
      const uint8_t di = decomposition_index[current_character >> 8];
      const uint16_t* const decomposition =
          decomposition_block[di] + (current_character % 256);
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
      // Check decomposition_data.
      const uint16_t* decomposition =
          decomposition_block[decomposition_index[input[input_count] >> 8]] +
          (input[input_count] % 256);
      uint16_t decomposition_length =
          (decomposition[1] >> 2) - (decomposition[0] >> 2);
      if (decomposition_length > 0 && (decomposition[0] & 1)) {
        decomposition_length = 0;
      }
      if (decomposition_length > 0) {
        // Non-recursive decomposition.
        while (decomposition_length-- > 0) {
          input[--descending_idx] = decomposition_data[(decomposition[0] >> 2) +
                                                       decomposition_length];
        }
      } else {
        // No decomposition.
        input[--descending_idx] = input[input_count];
      }
    } else {
      // Non-Unicode character.
      input[--descending_idx] = input[input_count];
    }
  }
}

uint8_t get_ccc(char32_t c) noexcept {
  return c < 0x110000 ? canonical_combining_class_block
                            [canonical_combining_class_index[c >> 8]][c % 256]
                      : 0;
}

void sort_marks(std::u32string& input) {
  for (size_t idx = 1; idx < input.size(); idx++) {
    uint8_t ccc = get_ccc(input[idx]);
    if (ccc == 0) {
      continue;
    }  // Skip non-combining characters.
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
          &composition_block[composition_index[input[input_count] >> 8]]
                            [input[input_count] % 256];
      size_t initial_composition_count = composition_count;
      for (int32_t previous_ccc = -1; input_count + 1 < input.size();
           input_count++) {
        uint8_t ccc = get_ccc(input[input_count + 1]);

        if (composition[1] != composition[0] && previous_ccc < ccc) {
          // Try finding a composition.
          int left = composition[0];
          int right = composition[1];
          while (left + 2 < right) {
            // mean without overflow
            int middle = left + (((right - left) >> 1) & ~1);
            if (composition_data[middle] <= input[input_count + 1]) {
              left = middle;
            }
            if (composition_data[middle] >= input[input_count + 1]) {
              right = middle;
            }
          }
          if (composition_data[left] == input[input_count + 1]) {
            input[initial_composition_count] = composition_data[left + 1];
            composition =
                &composition_block
                    [composition_index[composition_data[left + 1] >> 8]]
                    [composition_data[left + 1] % 256];
            continue;
          }
        }

        if (ccc == 0) {
          break;
        }  // Not a combining character.
        previous_ccc = ccc;
        input[++composition_count] = input[input_count + 1];
      }
    }
  }

  if (composition_count < input_count) {
    input.resize(composition_count);
  }
}

void normalize(std::u32string& input) {
  /**
   * Normalize the domain_name string to Unicode Normalization Form C.
   * @see https://www.unicode.org/reports/tr46/#ProcessingStepNormalize
   */
  decompose_nfc(input);
  compose(input);
}

}  // namespace ada::idna