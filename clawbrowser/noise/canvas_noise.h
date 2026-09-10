#ifndef CLAWBROWSER_NOISE_CANVAS_NOISE_H_
#define CLAWBROWSER_NOISE_CANVAS_NOISE_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "clawbrowser/noise/prng.h"

namespace clawbrowser {

// Pixel layouts used by Canvas ImageData readback and image export.  The
// helper deliberately names logical channels instead of perturbing the first
// three storage components: BGRA buffers must consume the PRNG in R, G, B
// order just like RGBA buffers do.
enum class CanvasPixelFormat {
  kRgba8,
  kBgra8,
  kRgbaF16,
  kRgbaF32,
};

namespace canvas_noise_internal {

inline size_t BytesPerPixel(CanvasPixelFormat format) {
  switch (format) {
    case CanvasPixelFormat::kRgba8:
    case CanvasPixelFormat::kBgra8:
      return 4;
    case CanvasPixelFormat::kRgbaF16:
      return 8;
    case CanvasPixelFormat::kRgbaF32:
      return 16;
  }
  return 0;
}

inline void CanonicalizeUint8Lsb(uint8_t* channel, uint8_t canonical_lsb) {
  *channel = static_cast<uint8_t>((*channel & 0xfeu) |
                                  (canonical_lsb & 1u));
}

inline void CanonicalizeFloat16Lsb(uint8_t* channel,
                                   uint8_t canonical_lsb) {
  uint16_t bits;
  std::memcpy(&bits, channel, sizeof(bits));
  constexpr uint16_t kSignMask = 0x8000u;
  constexpr uint16_t kExponentMask = 0x7c00u;
  constexpr uint16_t kMagnitudeMask = 0x7fffu;
  const uint16_t magnitude = bits & kMagnitudeMask;

  // Preserve infinities, NaNs (including their payloads), and signed zero.
  // For every other finite value, forcing one mantissa bit is the smallest
  // representable deterministic perturbation and is exactly idempotent.
  if ((bits & kExponentMask) == kExponentMask || magnitude == 0)
    return;

  const uint16_t target = canonical_lsb & 1u;
  if ((bits & 1u) == target)
    return;

  uint16_t candidate =
      static_cast<uint16_t>((bits & 0xfffeu) | target);
  // Clearing the LSB of the smallest subnormal would produce signed zero.
  // The next subnormal is equally close in representable-value space and
  // already has the required canonical LSB.
  if ((candidate & kMagnitudeMask) == 0)
    candidate = static_cast<uint16_t>((bits & kSignMask) | 0x0002u);

  std::memcpy(channel, &candidate, sizeof(candidate));
}

inline void CanonicalizeFloat32Lsb(uint8_t* channel,
                                   uint8_t canonical_lsb) {
  uint32_t bits;
  std::memcpy(&bits, channel, sizeof(bits));
  constexpr uint32_t kSignMask = 0x80000000u;
  constexpr uint32_t kExponentMask = 0x7f800000u;
  constexpr uint32_t kMagnitudeMask = 0x7fffffffu;
  const uint32_t magnitude = bits & kMagnitudeMask;

  // Do not turn non-finite values into a different NaN payload, and retain
  // the distinction between positive and negative zero.
  if ((bits & kExponentMask) == kExponentMask || magnitude == 0)
    return;

  const uint32_t target = canonical_lsb & 1u;
  if ((bits & 1u) == target)
    return;

  uint32_t candidate = (bits & 0xfffffffeu) | target;
  if ((candidate & kMagnitudeMask) == 0)
    candidate = (bits & kSignMask) | 0x00000002u;

  std::memcpy(channel, &candidate, sizeof(candidate));
}

inline uint64_t PixelSeed(uint64_t base_seed, int64_t x, int64_t y) {
  // SplitMix64-style avalanching makes adjacent coordinates independent while
  // retaining a stable mapping for overlapping readback rectangles.
  const auto mix = [](uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  };
  uint64_t hash = mix(base_seed ? base_seed : 1);
  hash ^= mix(static_cast<uint64_t>(x));
  hash = mix(hash);
  hash ^= mix(static_cast<uint64_t>(y));
  hash = mix(hash);
  return hash ? hash : 1;
}

}  // namespace canvas_noise_internal

// Canonicalizes one low-order RGB bit per channel in logical R, G, B order.
// The canonical bit is stable for a fingerprint seed and source coordinate,
// making the transform deterministic and idempotent: applying it to an
// already-protected buffer cannot accumulate noise. `row_bytes` may include
// padding; padding and alpha bytes are never modified. Returns false for an
// invalid buffer description and leaves the buffer untouched.
inline bool ApplyDeterministicCanvasNoise(uint8_t* pixels,
                                          size_t byte_length,
                                          size_t row_bytes,
                                          size_t width,
                                          size_t height,
                                          CanvasPixelFormat format,
                                          uint64_t seed,
                                          int64_t origin_x = 0,
                                          int64_t origin_y = 0) {
  const size_t bytes_per_pixel =
      canvas_noise_internal::BytesPerPixel(format);
  if (width == 0 || height == 0)
    return true;
  if (!pixels || bytes_per_pixel == 0 ||
      width > std::numeric_limits<size_t>::max() / bytes_per_pixel) {
    return false;
  }

  const size_t active_row_bytes = width * bytes_per_pixel;
  if (row_bytes < active_row_bytes ||
      height - 1 >
          (std::numeric_limits<size_t>::max() - active_row_bytes) / row_bytes) {
    return false;
  }
  const size_t required_bytes = (height - 1) * row_bytes + active_row_bytes;
  if (byte_length < required_bytes)
    return false;

  for (size_t y = 0; y < height; ++y) {
    uint8_t* row = pixels + y * row_bytes;
    for (size_t x = 0; x < width; ++x) {
      Prng prng(canvas_noise_internal::PixelSeed(
          seed, origin_x + static_cast<int64_t>(x),
          origin_y + static_cast<int64_t>(y)));
      uint8_t* pixel = row + x * bytes_per_pixel;
      for (size_t logical_channel = 0; logical_channel < 3;
           ++logical_channel) {
        const uint8_t canonical_lsb =
            static_cast<uint8_t>(prng.NextUint32() & 1u);
        switch (format) {
          case CanvasPixelFormat::kRgba8:
            canvas_noise_internal::CanonicalizeUint8Lsb(
                pixel + logical_channel, canonical_lsb);
            break;
          case CanvasPixelFormat::kBgra8:
            canvas_noise_internal::CanonicalizeUint8Lsb(
                pixel + (2 - logical_channel), canonical_lsb);
            break;
          case CanvasPixelFormat::kRgbaF16:
            canvas_noise_internal::CanonicalizeFloat16Lsb(
                pixel + logical_channel * sizeof(uint16_t), canonical_lsb);
            break;
          case CanvasPixelFormat::kRgbaF32:
            canvas_noise_internal::CanonicalizeFloat32Lsb(
                pixel + logical_channel * sizeof(float), canonical_lsb);
            break;
        }
      }
    }
  }
  return true;
}

}  // namespace clawbrowser

#endif  // CLAWBROWSER_NOISE_CANVAS_NOISE_H_
