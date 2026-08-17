#ifndef ADA_IDNA_SIMD_HPP
#define ADA_IDNA_SIMD_HPP

// Portable SIMD helpers for IDNA hot paths (ASCII scan, lowercase, UTF
// widen/narrow). SSE2 is baseline on x86_64; NEON is baseline on aarch64.
// Other targets (s390x, RV64 without V, ...) use SWAR / scalar fallbacks.

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__SSE2__) || defined(__x86_64__) || defined(__x86_64) || \
    defined(_M_AMD64) || defined(_M_X64) ||                          \
    (defined(_M_IX86_FP) && _M_IX86_FP == 2)
#define ADA_IDNA_SSE2 1
#include <emmintrin.h>
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define ADA_IDNA_NEON 1
#include <arm_neon.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define ADA_IDNA_REALLY_INLINE __forceinline
#else
#define ADA_IDNA_REALLY_INLINE inline __attribute__((always_inline))
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif

namespace ada::idna::simd {

#if defined(ADA_IDNA_SSE2)
ADA_IDNA_REALLY_INLINE int popcount_u32(unsigned v) noexcept {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
  return static_cast<int>(__popcnt(v));
#elif defined(__GNUC__) || defined(__clang__)
  return __builtin_popcount(v);
#else
  v = v - ((v >> 1) & 0x55555555u);
  v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
  return static_cast<int>((((v + (v >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
#endif
}
#endif

// True if every byte has the high bit clear (UTF-8 ASCII).
ADA_IDNA_REALLY_INLINE bool is_ascii_8(const char* data, size_t len) noexcept {
  if (len == 0) {
    return true;
  }
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
#if defined(ADA_IDNA_SSE2)
  if (len >= 16) {
    __m128i acc = _mm_setzero_si128();
    size_t i = 0;
    for (; i + 16 <= len; i += 16) {
      acc = _mm_or_si128(
          acc, _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i)));
    }
    if (i < len) {
      acc = _mm_or_si128(
          acc, _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + len - 16)));
    }
    return _mm_movemask_epi8(acc) == 0;
  }
#elif defined(ADA_IDNA_NEON)
  if (len >= 16) {
    uint8x16_t acc = vdupq_n_u8(0);
    size_t i = 0;
    for (; i + 16 <= len; i += 16) {
      acc = vorrq_u8(acc, vld1q_u8(p + i));
    }
    if (i < len) {
      acc = vorrq_u8(acc, vld1q_u8(p + len - 16));
    }
    return vmaxvq_u8(acc) < 0x80;
  }
#endif
  uint64_t acc = 0;
  size_t i = 0;
  for (; i + 8 <= len; i += 8) {
    uint64_t word = 0;
    std::memcpy(&word, p + i, 8);
    acc |= word;
  }
  if (i < len) {
    uint64_t word = 0;
    std::memcpy(&word, p + i, len - i);
    acc |= word;
  }
  return (acc & 0x8080808080808080ull) == 0;
}

// True if every UTF-32 code point is < 0x80.
ADA_IDNA_REALLY_INLINE bool is_ascii_32(const char32_t* data,
                                        size_t len) noexcept {
  if (len == 0) {
    return true;
  }
  const uint32_t* p = reinterpret_cast<const uint32_t*>(data);
#if defined(ADA_IDNA_SSE2)
  if (len >= 4) {
    const __m128i high = _mm_set1_epi32(static_cast<int>(0xFFFFFF80u));
    __m128i acc = _mm_setzero_si128();
    size_t i = 0;
    for (; i + 4 <= len; i += 4) {
      acc = _mm_or_si128(
          acc, _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i)));
    }
    if (i < len) {
      acc = _mm_or_si128(
          acc, _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + len - 4)));
    }
    acc = _mm_and_si128(acc, high);
    return _mm_movemask_epi8(_mm_cmpeq_epi32(acc, _mm_setzero_si128())) ==
           0xFFFF;
  }
#elif defined(ADA_IDNA_NEON)
  if (len >= 4) {
    uint32x4_t acc = vdupq_n_u32(0);
    size_t i = 0;
    for (; i + 4 <= len; i += 4) {
      acc = vorrq_u32(acc, vld1q_u32(p + i));
    }
    if (i < len) {
      acc = vorrq_u32(acc, vld1q_u32(p + len - 4));
    }
    return vmaxvq_u32(acc) < 0x80u;
  }
#endif
  uint64_t acc = 0;
  size_t i = 0;
  for (; i + 2 <= len; i += 2) {
    uint64_t word = 0;
    std::memcpy(&word, p + i, 8);
    acc |= word;
  }
  if (i < len) {
    acc |= p[i];
  }
  return (acc & 0xFFFFFF80FFFFFF80ull) == 0;
}

// In-place ASCII A-Z -> a-z. Caller guarantees bytes are ASCII (or accepts
// the same SWAR transform the scalar path already applied to any tail).
ADA_IDNA_REALLY_INLINE void ascii_lowercase(char* input,
                                            size_t length) noexcept {
  auto broadcast = [](uint8_t v) -> uint64_t {
    return 0x101010101010101ull * v;
  };
  const uint64_t broadcast_80 = broadcast(0x80);
  const uint64_t broadcast_Ap = broadcast(static_cast<uint8_t>(128 - 'A'));
  const uint64_t broadcast_Zp = broadcast(static_cast<uint8_t>(128 - 'Z' - 1));
  size_t i = 0;

#if defined(ADA_IDNA_SSE2)
  const __m128i v80 = _mm_set1_epi8(static_cast<char>(0x80));
  const __m128i vAp = _mm_set1_epi8(static_cast<char>(128 - 'A'));
  const __m128i vZp = _mm_set1_epi8(static_cast<char>(128 - 'Z' - 1));
  for (; i + 16 <= length; i += 16) {
    __m128i word = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + i));
    __m128i mask = _mm_and_si128(
        _mm_xor_si128(_mm_add_epi8(word, vAp), _mm_add_epi8(word, vZp)), v80);
    // mask is 0x00 or 0x80 per byte; >>2 yields 0x00 or 0x20 without
    // crossing byte lanes (low 2 bits of each byte are zero).
    word = _mm_xor_si128(word, _mm_srli_epi16(mask, 2));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(input + i), word);
  }
#elif defined(ADA_IDNA_NEON)
  const uint8x16_t v80 = vdupq_n_u8(0x80);
  const uint8x16_t vAp = vdupq_n_u8(static_cast<uint8_t>(128 - 'A'));
  const uint8x16_t vZp = vdupq_n_u8(static_cast<uint8_t>(128 - 'Z' - 1));
  for (; i + 16 <= length; i += 16) {
    uint8x16_t word = vld1q_u8(reinterpret_cast<const uint8_t*>(input + i));
    uint8x16_t mask =
        vandq_u8(veorq_u8(vaddq_u8(word, vAp), vaddq_u8(word, vZp)), v80);
    word = veorq_u8(word, vshrq_n_u8(mask, 2));
    vst1q_u8(reinterpret_cast<uint8_t*>(input + i), word);
  }
#endif

  for (; i + 8 <= length; i += 8) {
    uint64_t word = 0;
    std::memcpy(&word, input + i, sizeof(word));
    word ^=
        (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80) >> 2;
    std::memcpy(input + i, &word, sizeof(word));
  }
  if (i < length) {
    uint64_t word = 0;
    std::memcpy(&word, input + i, length - i);
    word ^=
        (((word + broadcast_Ap) ^ (word + broadcast_Zp)) & broadcast_80) >> 2;
    std::memcpy(input + i, &word, length - i);
  }
}

// Widen 16 ASCII bytes to 16 UTF-32 code points.
ADA_IDNA_REALLY_INLINE void widen16_ascii_to_utf32(const uint8_t* src,
                                                   char32_t* dst) noexcept {
#if defined(ADA_IDNA_SSE2)
  const __m128i bytes = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
  const __m128i zero = _mm_setzero_si128();
  const __m128i lo16 = _mm_unpacklo_epi8(bytes, zero);
  const __m128i hi16 = _mm_unpackhi_epi8(bytes, zero);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst),
                   _mm_unpacklo_epi16(lo16, zero));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 4),
                   _mm_unpackhi_epi16(lo16, zero));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 8),
                   _mm_unpacklo_epi16(hi16, zero));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 12),
                   _mm_unpackhi_epi16(hi16, zero));
#elif defined(ADA_IDNA_NEON)
  const uint8x16_t bytes = vld1q_u8(src);
  const uint16x8_t lo16 = vmovl_u8(vget_low_u8(bytes));
  const uint16x8_t hi16 = vmovl_u8(vget_high_u8(bytes));
  vst1q_u32(reinterpret_cast<uint32_t*>(dst), vmovl_u16(vget_low_u16(lo16)));
  vst1q_u32(reinterpret_cast<uint32_t*>(dst + 4),
            vmovl_u16(vget_high_u16(lo16)));
  vst1q_u32(reinterpret_cast<uint32_t*>(dst + 8),
            vmovl_u16(vget_low_u16(hi16)));
  vst1q_u32(reinterpret_cast<uint32_t*>(dst + 12),
            vmovl_u16(vget_high_u16(hi16)));
#else
  for (size_t k = 0; k < 16; ++k) {
    dst[k] = static_cast<char32_t>(src[k]);
  }
#endif
}

// Pack 4 ASCII UTF-32 code points to 4 bytes. Caller guarantees values < 0x80.
ADA_IDNA_REALLY_INLINE void pack4_ascii_utf32(const uint32_t* src,
                                              char* dst) noexcept {
#if defined(ADA_IDNA_SSE2)
  __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
  v = _mm_and_si128(v, _mm_set1_epi32(0xFF));
  v = _mm_packs_epi32(v, v);
  v = _mm_packus_epi16(v, v);
  const uint32_t out = static_cast<uint32_t>(_mm_cvtsi128_si32(v));
  std::memcpy(dst, &out, 4);
#elif defined(ADA_IDNA_NEON)
  const uint32x4_t v = vld1q_u32(src);
  const uint16x4_t n16 = vmovn_u32(v);
  const uint8x8_t n8 = vmovn_u16(vcombine_u16(n16, n16));
  vst1_lane_u32(reinterpret_cast<uint32_t*>(dst), vreinterpret_u32_u8(n8), 0);
#else
  dst[0] = static_cast<char>(src[0]);
  dst[1] = static_cast<char>(src[1]);
  dst[2] = static_cast<char>(src[2]);
  dst[3] = static_cast<char>(src[3]);
#endif
}

// True if 4 UTF-32 code points are all ASCII.
ADA_IDNA_REALLY_INLINE bool is_ascii_32x4(const uint32_t* src) noexcept {
#if defined(ADA_IDNA_SSE2)
  const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
  const __m128i masked =
      _mm_and_si128(v, _mm_set1_epi32(static_cast<int>(0xFFFFFF80u)));
  return _mm_movemask_epi8(_mm_cmpeq_epi32(masked, _mm_setzero_si128())) ==
         0xFFFF;
#elif defined(ADA_IDNA_NEON)
  return vmaxvq_u32(vld1q_u32(src)) < 0x80u;
#else
  uint64_t w0 = 0;
  uint64_t w1 = 0;
  std::memcpy(&w0, src, 8);
  std::memcpy(&w1, src + 2, 8);
  return ((w0 | w1) & 0xFFFFFF80FFFFFF80ull) == 0;
#endif
}

// Count UTF-8 leading bytes (code points): bytes whose signed value is > -65.
ADA_IDNA_REALLY_INLINE size_t utf32_length_from_utf8(const char* buf,
                                                     size_t len) noexcept {
  const int8_t* p = reinterpret_cast<const int8_t*>(buf);
  size_t count = 0;
  size_t i = 0;
#if defined(ADA_IDNA_SSE2)
  const __m128i thresh = _mm_set1_epi8(static_cast<char>(-65));
  for (; i + 16 <= len; i += 16) {
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
    const unsigned mask =
        static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpgt_epi8(v, thresh)));
    count += static_cast<size_t>(popcount_u32(mask));
  }
#elif defined(ADA_IDNA_NEON)
  const int8x16_t thresh = vdupq_n_s8(static_cast<int8_t>(-65));
  for (; i + 16 <= len; i += 16) {
    const int8x16_t v = vld1q_s8(p + i);
    const uint8x16_t gt = vreinterpretq_u8_s8(vcgtq_s8(v, thresh));
    count += static_cast<size_t>(vaddvq_u8(vandq_u8(gt, vdupq_n_u8(1))));
  }
#endif
  for (; i < len; ++i) {
    count += static_cast<size_t>(p[i] > static_cast<int8_t>(-65));
  }
  return count;
}

// UTF-8 length of a UTF-32 sequence (1-4 bytes per code point).
ADA_IDNA_REALLY_INLINE size_t utf8_length_from_utf32(const char32_t* buf,
                                                     size_t len) noexcept {
  const uint32_t* p = reinterpret_cast<const uint32_t*>(buf);
  size_t count = 0;
  size_t i = 0;
#if defined(ADA_IDNA_SSE2)
  const __m128i ones = _mm_set1_epi32(1);
  const __m128i lim7f = _mm_set1_epi32(0x7F);
  const __m128i lim7ff = _mm_set1_epi32(0x7FF);
  const __m128i limffff = _mm_set1_epi32(0xFFFF);
  for (; i + 4 <= len; i += 4) {
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
    // Signed compares are valid for Unicode code points (< 0x110000).
    __m128i c = ones;
    c = _mm_sub_epi32(c, _mm_cmpgt_epi32(v, lim7f));
    c = _mm_sub_epi32(c, _mm_cmpgt_epi32(v, lim7ff));
    c = _mm_sub_epi32(c, _mm_cmpgt_epi32(v, limffff));
    // Horizontal sum of 4 lanes.
    __m128i shuf = _mm_shuffle_epi32(c, 0x4E);  // swap 64-bit halves
    c = _mm_add_epi32(c, shuf);
    shuf = _mm_shuffle_epi32(c, 0xB1);  // swap 32-bit pairs
    c = _mm_add_epi32(c, shuf);
    count += static_cast<size_t>(static_cast<uint32_t>(_mm_cvtsi128_si32(c)));
  }
#elif defined(ADA_IDNA_NEON)
  for (; i + 4 <= len; i += 4) {
    const uint32x4_t v = vld1q_u32(p + i);
    uint32x4_t c = vdupq_n_u32(1);
    c = vaddq_u32(c,
                  vandq_u32(vcgtq_u32(v, vdupq_n_u32(0x7F)), vdupq_n_u32(1)));
    c = vaddq_u32(c,
                  vandq_u32(vcgtq_u32(v, vdupq_n_u32(0x7FF)), vdupq_n_u32(1)));
    c = vaddq_u32(c,
                  vandq_u32(vcgtq_u32(v, vdupq_n_u32(0xFFFF)), vdupq_n_u32(1)));
    count += static_cast<size_t>(vaddvq_u32(c));
  }
#endif
  for (; i < len; ++i) {
    ++count;
    count += static_cast<size_t>(p[i] > 0x7Fu);
    count += static_cast<size_t>(p[i] > 0x7FFu);
    count += static_cast<size_t>(p[i] > 0xFFFFu);
  }
  return count;
}

// First index of `needle` in `data[0, len)`, or `len` if not found.
ADA_IDNA_REALLY_INLINE size_t find_char32(const char32_t* data, size_t len,
                                          char32_t needle) noexcept {
  const uint32_t* p = reinterpret_cast<const uint32_t*>(data);
  const uint32_t n = static_cast<uint32_t>(needle);
  size_t i = 0;
#if defined(ADA_IDNA_SSE2)
  const __m128i nd = _mm_set1_epi32(static_cast<int>(n));
  for (; i + 4 <= len; i += 4) {
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
    const int mask = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(v, nd)));
    if (mask != 0) {
#if defined(_MSC_VER)
      unsigned long idx = 0;
      _BitScanForward(&idx, static_cast<unsigned long>(mask));
      return i + static_cast<size_t>(idx);
#else
      return i +
             static_cast<size_t>(__builtin_ctz(static_cast<unsigned>(mask)));
#endif
    }
  }
#elif defined(ADA_IDNA_NEON)
  const uint32x4_t nd = vdupq_n_u32(n);
  for (; i + 4 <= len; i += 4) {
    const uint32x4_t eq = vceqq_u32(vld1q_u32(p + i), nd);
    const uint64x2_t pair = vreinterpretq_u64_u32(eq);
    const uint64_t lo = vgetq_lane_u64(pair, 0);
    const uint64_t hi = vgetq_lane_u64(pair, 1);
    if (lo != 0) {
      return i + (static_cast<uint32_t>(lo) == 0 ? 1 : 0);
    }
    if (hi != 0) {
      return i + 2 + (static_cast<uint32_t>(hi) == 0 ? 1 : 0);
    }
  }
#endif
  for (; i < len; ++i) {
    if (p[i] == n) {
      return i;
    }
  }
  return len;
}

}  // namespace ada::idna::simd

#endif  // ADA_IDNA_SIMD_HPP
