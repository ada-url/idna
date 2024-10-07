#include "ada/idna/punycode.h"

#include <cstdint>

namespace ada::idna {

constexpr int32_t base = 36;
constexpr int32_t tmin = 1;
constexpr int32_t tmax = 26;
constexpr int32_t skew = 38;
constexpr int32_t damp = 700;
constexpr int32_t initial_bias = 72;
constexpr uint32_t initial_n = 128;

static constexpr int32_t char_to_digit_value(char value) {
  // Convert to unsigned for defined behavior on overflow
  auto uvalue = static_cast<unsigned char>(value);

  // Check if it's a lowercase letter (a-z)
  int32_t lowercase = (uvalue - 'a') * (uvalue >= 'a' && uvalue <= 'z');

  // Check if it's a digit (0-9)
  int32_t digit = (uvalue - '0' + 26) * (uvalue >= '0' && uvalue <= '9');

  // Combine results, will be 0 if neither condition is true
  int32_t result = lowercase | digit;

  // If result is 0 and input wasn't 'a' (which would give 0), return -1
  return result | (((result | (uvalue - 'a')) == 0) * -1);
}

static constexpr char digit_to_char(int32_t digit) {
  return digit < 26 ? char(digit + 97) : char(digit + 22);
}

static constexpr int32_t adapt(int32_t d, int32_t n, bool firsttime) {
  if (firsttime) {
    d = d / damp;
  } else {
    d = d / 2;
  }
  d += d / n;
  int32_t k = 0;
  while (d > ((base - tmin) * tmax) / 2) {
    d /= base - tmin;
    k += base;
  }
  return k + (((base - tmin + 1) * d) / (d + skew));
}

bool punycode_to_utf32(std::string_view input, std::u32string &out) {
  int32_t written_out{0};
  out.reserve(out.size() + input.size());
  uint32_t n = initial_n;
  int32_t i = 0;
  int32_t bias = initial_bias;
  // grab ascii content
  size_t end_of_ascii = input.find_last_of('-');
  if (end_of_ascii != std::string_view::npos) {
    for (uint8_t c : input.substr(0, end_of_ascii)) {
      if (c >= 0x80) {
        return false;
      }
      out.push_back(c);
      written_out++;
    }
    input.remove_prefix(end_of_ascii + 1);
  }
  while (!input.empty()) {
    int32_t oldi = i;
    int32_t w = 1;
    for (int32_t k = base;; k += base) {
      if (input.empty()) {
        return false;
      }
      uint8_t code_point = input.front();
      input.remove_prefix(1);
      int32_t digit = char_to_digit_value(code_point);
      if (digit < 0) {
        return false;
      }
      if (digit > (0x7fffffff - i) / w) {
        return false;
      }
      i = i + digit * w;
      int32_t t = k <= bias ? tmin : k >= bias + tmax ? tmax : k - bias;
      if (digit < t) {
        break;
      }
      if (w > 0x7fffffff / (base - t)) {
        return false;
      }
      w = w * (base - t);
    }
    bias = adapt(i - oldi, written_out + 1, oldi == 0);
    if (i / (written_out + 1) > int32_t(0x7fffffff - n)) {
      return false;
    }
    n = n + i / (written_out + 1);
    i = i % (written_out + 1);
    if (n < 0x80) {
      return false;
    }
    out.insert(out.begin() + i, n);
    written_out++;
    ++i;
  }

  return true;
}

bool verify_punycode(std::string_view input) {
  size_t written_out{0};
  uint32_t n = initial_n;
  int32_t i = 0;
  int32_t bias = initial_bias;
  // grab ascii content
  size_t end_of_ascii = input.find_last_of('-');
  if (end_of_ascii != std::string_view::npos) {
    for (uint8_t c : input.substr(0, end_of_ascii)) {
      if (c >= 0x80) {
        return false;
      }
      written_out++;
    }
    input.remove_prefix(end_of_ascii + 1);
  }
  while (!input.empty()) {
    int32_t oldi = i;
    int32_t w = 1;
    for (int32_t k = base;; k += base) {
      if (input.empty()) {
        return false;
      }
      uint8_t code_point = input.front();
      input.remove_prefix(1);
      int32_t digit = char_to_digit_value(code_point);
      if (digit < 0) {
        return false;
      }
      if (digit > (0x7fffffff - i) / w) {
        return false;
      }
      i = i + digit * w;
      int32_t t = k <= bias ? tmin : k >= bias + tmax ? tmax : k - bias;
      if (digit < t) {
        break;
      }
      if (w > 0x7fffffff / (base - t)) {
        return false;
      }
      w = w * (base - t);
    }
    bias = adapt(i - oldi, int32_t(written_out + 1), oldi == 0);
    if (i / (written_out + 1) > 0x7fffffff - n) {
      return false;
    }
    n = n + i / int32_t(written_out + 1);
    i = i % int32_t(written_out + 1);
    if (n < 0x80) {
      return false;
    }
    written_out++;
    ++i;
  }

  return true;
}

bool utf32_to_punycode(std::u32string_view input, std::string &out) {
  out.reserve(input.size() + out.size());
  uint32_t n = initial_n;
  int32_t d = 0;
  int32_t bias = initial_bias;
  size_t h = 0;
  // first push the ascii content
  for (uint32_t c : input) {
    if (c < 0x80) {
      ++h;
      out.push_back(char(c));
    }
    if (c > 0x10ffff || (c >= 0xd880 && c < 0xe000)) {
      return false;
    }
  }
  size_t b = h;
  if (b > 0) {
    out.push_back('-');
  }
  while (h < input.size()) {
    uint32_t m = 0x10FFFF;
    for (auto code_point : input) {
      if (code_point >= n && code_point < m) m = code_point;
    }

    if ((m - n) > (0x7fffffff - d) / (h + 1)) {
      return false;
    }
    d = d + int32_t((m - n) * (h + 1));
    n = m;
    for (auto c : input) {
      if (c < n) {
        if (d == 0x7fffffff) {
          return false;
        }
        ++d;
      }
      if (c == n) {
        int32_t q = d;
        for (int32_t k = base;; k += base) {
          int32_t t = k <= bias ? tmin : k >= bias + tmax ? tmax : k - bias;

          if (q < t) {
            break;
          }
          out.push_back(digit_to_char(t + ((q - t) % (base - t))));
          q = (q - t) / (base - t);
        }
        out.push_back(digit_to_char(q));
        bias = adapt(d, int32_t(h + 1), h == b);
        d = 0;
        ++h;
      }
    }
    ++d;
    ++n;
  }
  return true;
}

}  // namespace ada::idna
