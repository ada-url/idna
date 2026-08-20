#ifndef ADA_IDNA_SIMD_HPP
#define ADA_IDNA_SIMD_HPP

// Portable SIMD helpers for IDNA hot paths. SSE2 is baseline on x86_64;
// NEON is baseline on aarch64. Other targets use SWAR.

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

// Byte-parallel ASCII A-Z -> a-z (Hacker's Delight / aqrit).
static constexpr uint64_t k80 = 0x8080808080808080ull;
static constexpr uint64_t kAp = 0x3F3F3F3F3F3F3F3Full;  // 128 - 'A'
static constexpr uint64_t kZp = 0x2525252525252525ull;  // 128 - 'Z' - 1

ADA_IDNA_REALLY_INLINE uint64_t lower8(uint64_t word) noexcept {
  return word ^ ((((word + kAp) ^ (word + kZp)) & k80) >> 2);
}

#if defined(ADA_IDNA_SSE2)
ADA_IDNA_REALLY_INLINE __m128i lower16(__m128i word) noexcept {
  const __m128i v80 = _mm_set1_epi8(static_cast<char>(0x80));
  const __m128i vAp = _mm_set1_epi8(static_cast<char>(128 - 'A'));
  const __m128i vZp = _mm_set1_epi8(static_cast<char>(128 - 'Z' - 1));
  const __m128i mask = _mm_and_si128(
      _mm_xor_si128(_mm_add_epi8(word, vAp), _mm_add_epi8(word, vZp)), v80);
  // mask is 0x00/0x80 per byte; >>2 -> 0x00/0x20 without crossing lanes.
  return _mm_xor_si128(word, _mm_srli_epi16(mask, 2));
}

ADA_IDNA_REALLY_INLINE void widen16(__m128i bytes, char32_t* dst) noexcept {
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
}

ADA_IDNA_REALLY_INLINE int popcount_u32(unsigned v) noexcept {
#if defined(_MSC_VER)
  return static_cast<int>(__popcnt(v));
#else
  return __builtin_popcount(v);
#endif
}
#endif

#if defined(ADA_IDNA_NEON)
ADA_IDNA_REALLY_INLINE uint8x16_t lower16(uint8x16_t word) noexcept {
  const uint8x16_t v80 = vdupq_n_u8(0x80);
  const uint8x16_t vAp = vdupq_n_u8(static_cast<uint8_t>(128 - 'A'));
  const uint8x16_t vZp = vdupq_n_u8(static_cast<uint8_t>(128 - 'Z' - 1));
  const uint8x16_t mask =
      vandq_u8(veorq_u8(vaddq_u8(word, vAp), vaddq_u8(word, vZp)), v80);
  return veorq_u8(word, vshrq_n_u8(mask, 2));
}

ADA_IDNA_REALLY_INLINE void widen16(uint8x16_t bytes, char32_t* dst) noexcept {
  const uint16x8_t lo16 = vmovl_u8(vget_low_u8(bytes));
  const uint16x8_t hi16 = vmovl_u8(vget_high_u8(bytes));
  vst1q_u32(reinterpret_cast<uint32_t*>(dst), vmovl_u16(vget_low_u16(lo16)));
  vst1q_u32(reinterpret_cast<uint32_t*>(dst + 4),
            vmovl_u16(vget_high_u16(lo16)));
  vst1q_u32(reinterpret_cast<uint32_t*>(dst + 8),
            vmovl_u16(vget_low_u16(hi16)));
  vst1q_u32(reinterpret_cast<uint32_t*>(dst + 12),
            vmovl_u16(vget_high_u16(hi16)));
}
#endif

// In-place A-Z -> a-z. Returns true iff every original byte was ASCII.
// One pass replaces a separate is_ascii scan plus ascii_map.
ADA_IDNA_REALLY_INLINE bool ascii_lowercase_is_ascii(char* input,
                                                     size_t length) noexcept {
  bool ascii = true;
  uint64_t high = 0;
  size_t i = 0;
#if defined(ADA_IDNA_SSE2)
  __m128i vacc = _mm_setzero_si128();
  for (; i + 32 <= length; i += 32) {
    const __m128i w0 =
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + i));
    const __m128i w1 =
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + i + 16));
    vacc = _mm_or_si128(vacc, _mm_or_si128(w0, w1));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(input + i), lower16(w0));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(input + i + 16), lower16(w1));
  }
  if (i + 16 <= length) {
    const __m128i w =
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + i));
    vacc = _mm_or_si128(vacc, w);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(input + i), lower16(w));
    i += 16;
  }
  ascii = _mm_movemask_epi8(vacc) == 0;
#elif defined(ADA_IDNA_NEON)
  uint8x16_t vacc = vdupq_n_u8(0);
  for (; i + 32 <= length; i += 32) {
    const uint8x16_t w0 = vld1q_u8(reinterpret_cast<const uint8_t*>(input + i));
    const uint8x16_t w1 =
        vld1q_u8(reinterpret_cast<const uint8_t*>(input + i + 16));
    vacc = vorrq_u8(vacc, vorrq_u8(w0, w1));
    vst1q_u8(reinterpret_cast<uint8_t*>(input + i), lower16(w0));
    vst1q_u8(reinterpret_cast<uint8_t*>(input + i + 16), lower16(w1));
  }
  if (i + 16 <= length) {
    const uint8x16_t w = vld1q_u8(reinterpret_cast<const uint8_t*>(input + i));
    vacc = vorrq_u8(vacc, w);
    vst1q_u8(reinterpret_cast<uint8_t*>(input + i), lower16(w));
    i += 16;
  }
  ascii = vmaxvq_u8(vacc) < 0x80;
#endif
  for (; i + 8 <= length; i += 8) {
    uint64_t word = 0;
    std::memcpy(&word, input + i, 8);
    high |= word;
    word = lower8(word);
    std::memcpy(input + i, &word, 8);
  }
  if (i < length) {
    uint64_t word = 0;
    std::memcpy(&word, input + i, length - i);
    high |= word;
    word = lower8(word);
    std::memcpy(input + i, &word, length - i);
  }
  return ascii && (high & k80) == 0;
}

ADA_IDNA_REALLY_INLINE bool is_ascii_8(const char* data, size_t len) noexcept {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
  uint64_t acc = 0;
  size_t i = 0;
#if defined(ADA_IDNA_SSE2)
  if (len >= 16) {
    __m128i vacc = _mm_setzero_si128();
    for (; i + 32 <= len; i += 32) {
      vacc = _mm_or_si128(
          vacc,
          _mm_or_si128(
              _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i)),
              _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i + 16))));
    }
    if (i + 16 <= len) {
      vacc = _mm_or_si128(
          vacc, _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i)));
      i += 16;
    }
    if (_mm_movemask_epi8(vacc) != 0) {
      return false;
    }
  }
#elif defined(ADA_IDNA_NEON)
  if (len >= 16) {
    uint8x16_t vacc = vdupq_n_u8(0);
    for (; i + 32 <= len; i += 32) {
      vacc = vorrq_u8(vacc, vorrq_u8(vld1q_u8(p + i), vld1q_u8(p + i + 16)));
    }
    if (i + 16 <= len) {
      vacc = vorrq_u8(vacc, vld1q_u8(p + i));
      i += 16;
    }
    if (vmaxvq_u8(vacc) >= 0x80) {
      return false;
    }
  }
#endif
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
  return (acc & k80) == 0;
}

// SWAR over uint64 pairs: one load / two code points, no vector setup.
ADA_IDNA_REALLY_INLINE bool is_ascii_32(const char32_t* data,
                                        size_t len) noexcept {
  const uint32_t* p = reinterpret_cast<const uint32_t*>(data);
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

// Load 16 bytes once: if ASCII, widen from the same register.
ADA_IDNA_REALLY_INLINE bool try_widen16_ascii(const uint8_t* src,
                                              char32_t* dst) noexcept {
#if defined(ADA_IDNA_SSE2)
  const __m128i bytes = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
  if (_mm_movemask_epi8(bytes) != 0) {
    return false;
  }
  widen16(bytes, dst);
  return true;
#elif defined(ADA_IDNA_NEON)
  const uint8x16_t bytes = vld1q_u8(src);
  if (vmaxvq_u8(bytes) >= 0x80) {
    return false;
  }
  widen16(bytes, dst);
  return true;
#else
  uint64_t v1 = 0;
  uint64_t v2 = 0;
  std::memcpy(&v1, src, 8);
  std::memcpy(&v2, src + 8, 8);
  if (((v1 | v2) & k80) != 0) {
    return false;
  }
  for (size_t k = 0; k < 16; ++k) {
    dst[k] = static_cast<char32_t>(src[k]);
  }
  return true;
#endif
}

// Load 4 UTF-32 values once: if ASCII, pack to 4 bytes.
ADA_IDNA_REALLY_INLINE bool try_pack4_ascii(const uint32_t* src,
                                            char* dst) noexcept {
#if defined(ADA_IDNA_SSE2)
  __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
  if (_mm_movemask_ps(
          _mm_castsi128_ps(_mm_cmpgt_epi32(v, _mm_set1_epi32(0x7F)))) != 0) {
    return false;
  }
  v = _mm_packs_epi32(v, v);
  v = _mm_packus_epi16(v, v);
  const uint32_t out = static_cast<uint32_t>(_mm_cvtsi128_si32(v));
  std::memcpy(dst, &out, 4);
  return true;
#elif defined(ADA_IDNA_NEON)
  const uint32x4_t v = vld1q_u32(src);
  if (vmaxvq_u32(v) >= 0x80u) {
    return false;
  }
  const uint8x8_t n8 = vmovn_u16(vcombine_u16(vmovn_u32(v), vmovn_u32(v)));
  vst1_lane_u32(reinterpret_cast<uint32_t*>(dst), vreinterpret_u32_u8(n8), 0);
  return true;
#else
  uint64_t w0 = 0;
  uint64_t w1 = 0;
  std::memcpy(&w0, src, 8);
  std::memcpy(&w1, src + 2, 8);
  if (((w0 | w1) & 0xFFFFFF80FFFFFF80ull) != 0) {
    return false;
  }
  dst[0] = static_cast<char>(src[0]);
  dst[1] = static_cast<char>(src[1]);
  dst[2] = static_cast<char>(src[2]);
  dst[3] = static_cast<char>(src[3]);
  return true;
#endif
}

ADA_IDNA_REALLY_INLINE size_t utf32_length_from_utf8(const char* buf,
                                                     size_t len) noexcept {
  const int8_t* p = reinterpret_cast<const int8_t*>(buf);
  size_t count = 0;
  size_t i = 0;
#if defined(ADA_IDNA_SSE2)
  const __m128i thresh = _mm_set1_epi8(static_cast<char>(-65));
  for (; i + 32 <= len; i += 32) {
    const unsigned m0 = static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpgt_epi8(
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i)), thresh)));
    const unsigned m1 = static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpgt_epi8(
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i + 16)),
        thresh)));
    count += static_cast<size_t>(popcount_u32(m0) + popcount_u32(m1));
  }
  for (; i + 16 <= len; i += 16) {
    const unsigned mask =
        static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpgt_epi8(
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i)), thresh)));
    count += static_cast<size_t>(popcount_u32(mask));
  }
#elif defined(ADA_IDNA_NEON)
  // vcgtq_s8 already returns uint8x16_t (ACLE). Do not vreinterpret.
  const int8x16_t thresh = vdupq_n_s8(static_cast<int8_t>(-65));
  const uint8x16_t one = vdupq_n_u8(1);
  for (; i + 16 <= len; i += 16) {
    const uint8x16_t gt = vcgtq_s8(vld1q_s8(p + i), thresh);
    count += static_cast<size_t>(vaddvq_u8(vandq_u8(gt, one)));
  }
#endif
  for (; i < len; ++i) {
    count += static_cast<size_t>(p[i] > static_cast<int8_t>(-65));
  }
  return count;
}

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
  __m128i acc = _mm_setzero_si128();
  for (; i + 4 <= len; i += 4) {
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
    __m128i c = ones;
    c = _mm_sub_epi32(c, _mm_cmpgt_epi32(v, lim7f));
    c = _mm_sub_epi32(c, _mm_cmpgt_epi32(v, lim7ff));
    c = _mm_sub_epi32(c, _mm_cmpgt_epi32(v, limffff));
    acc = _mm_add_epi32(acc, c);
  }
  acc = _mm_add_epi32(acc, _mm_shuffle_epi32(acc, 0x4E));
  acc = _mm_add_epi32(acc, _mm_shuffle_epi32(acc, 0xB1));
  count = static_cast<size_t>(static_cast<uint32_t>(_mm_cvtsi128_si32(acc)));
#elif defined(ADA_IDNA_NEON)
  uint32x4_t acc = vdupq_n_u32(0);
  const uint32x4_t one = vdupq_n_u32(1);
  for (; i + 4 <= len; i += 4) {
    const uint32x4_t v = vld1q_u32(p + i);
    uint32x4_t c = one;
    c = vaddq_u32(c, vandq_u32(vcgtq_u32(v, vdupq_n_u32(0x7F)), one));
    c = vaddq_u32(c, vandq_u32(vcgtq_u32(v, vdupq_n_u32(0x7FF)), one));
    c = vaddq_u32(c, vandq_u32(vcgtq_u32(v, vdupq_n_u32(0xFFFF)), one));
    acc = vaddq_u32(acc, c);
  }
  count = static_cast<size_t>(vaddvq_u32(acc));
#endif
  for (; i < len; ++i) {
    ++count;
    count += static_cast<size_t>(p[i] > 0x7Fu);
    count += static_cast<size_t>(p[i] > 0x7FFu);
    count += static_cast<size_t>(p[i] > 0xFFFFu);
  }
  return count;
}

}  // namespace ada::idna::simd

#endif  // ADA_IDNA_SIMD_HPP
