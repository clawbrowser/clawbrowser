#include "clawbrowser/noise/canvas_noise.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(CanvasNoiseTest, RgbaAndBgraUseTheSameLogicalChannelSequence) {
  std::array<uint8_t, 4> rgba = {50, 100, 150, 77};
  std::array<uint8_t, 4> bgra = {150, 100, 50, 77};

  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      rgba.data(), rgba.size(), rgba.size(), 1, 1,
      CanvasPixelFormat::kRgba8, 1234567890));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      bgra.data(), bgra.size(), bgra.size(), 1, 1,
      CanvasPixelFormat::kBgra8, 1234567890));

  EXPECT_EQ(rgba[0], bgra[2]);
  EXPECT_EQ(rgba[1], bgra[1]);
  EXPECT_EQ(rgba[2], bgra[0]);
  EXPECT_EQ(rgba[3], 77);
  EXPECT_EQ(bgra[3], 77);
}

TEST(CanvasNoiseTest, FloatFormatsUseTheSameLogicalChannelSequence) {
  constexpr uint64_t kSeed = 1234567890;
  std::array<float, 4> rgba_f32 = {0.25f, 0.5f, 0.75f, 0.375f};
  std::array<uint16_t, 4> rgba_f16 = {0x3400, 0x3800, 0x3a00, 0x3600};
  const auto original_f32 = rgba_f32;
  const auto original_f16 = rgba_f16;
  uint32_t original_f32_bits[3];
  for (size_t channel = 0; channel < 3; ++channel) {
    std::memcpy(&original_f32_bits[channel], &original_f32[channel],
                sizeof(uint32_t));
  }
  const uint16_t original_f16_alpha = rgba_f16[3];

  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      reinterpret_cast<uint8_t*>(rgba_f32.data()), sizeof(rgba_f32),
      sizeof(rgba_f32), 1, 1, CanvasPixelFormat::kRgbaF32, kSeed));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      reinterpret_cast<uint8_t*>(rgba_f16.data()), sizeof(rgba_f16),
      sizeof(rgba_f16), 1, 1, CanvasPixelFormat::kRgbaF16, kSeed));

  Prng expected_bits(canvas_noise_internal::PixelSeed(kSeed, 0, 0));
  for (size_t channel = 0; channel < 3; ++channel) {
    const uint32_t canonical_lsb = expected_bits.NextUint32() & 1u;
    uint32_t actual_f32_bits;
    std::memcpy(&actual_f32_bits, &rgba_f32[channel], sizeof(uint32_t));
    EXPECT_EQ(actual_f32_bits,
              (original_f32_bits[channel] & 0xfffffffeu) | canonical_lsb);
    EXPECT_EQ(rgba_f16[channel],
              (original_f16[channel] & 0xfffeu) | canonical_lsb);
  }
  EXPECT_EQ(rgba_f32[3], 0.375f);
  EXPECT_EQ(rgba_f16[3], original_f16_alpha);
}

TEST(CanvasNoiseTest, FloatCanonicalizationPreservesSpecialsAndSignedZero) {
  std::array<uint16_t, 8> f16 = {
      0x0000,  // +0
      0x8000,  // -0
      0x7c00,  // +infinity
      0xfc00,  // -infinity
      0x7e55,  // NaN with payload
      0xfe55,  // negative NaN with payload
      0x0001,  // smallest positive subnormal
      0x8001,  // smallest negative subnormal
  };
  for (uint16_t& value : f16) {
    canvas_noise_internal::CanonicalizeFloat16Lsb(
        reinterpret_cast<uint8_t*>(&value), 0);
  }
  EXPECT_EQ(f16[0], 0x0000);
  EXPECT_EQ(f16[1], 0x8000);
  EXPECT_EQ(f16[2], 0x7c00);
  EXPECT_EQ(f16[3], 0xfc00);
  EXPECT_EQ(f16[4], 0x7e55);
  EXPECT_EQ(f16[5], 0xfe55);
  EXPECT_EQ(f16[6], 0x0002);
  EXPECT_EQ(f16[7], 0x8002);

  std::array<uint32_t, 8> f32 = {
      0x00000000u,  // +0
      0x80000000u,  // -0
      0x7f800000u,  // +infinity
      0xff800000u,  // -infinity
      0x7fc12345u,  // NaN with payload
      0xffc12345u,  // negative NaN with payload
      0x00000001u,  // smallest positive subnormal
      0x80000001u,  // smallest negative subnormal
  };
  for (uint32_t& value : f32) {
    canvas_noise_internal::CanonicalizeFloat32Lsb(
        reinterpret_cast<uint8_t*>(&value), 0);
  }
  EXPECT_EQ(f32[0], 0x00000000u);
  EXPECT_EQ(f32[1], 0x80000000u);
  EXPECT_EQ(f32[2], 0x7f800000u);
  EXPECT_EQ(f32[3], 0xff800000u);
  EXPECT_EQ(f32[4], 0x7fc12345u);
  EXPECT_EQ(f32[5], 0xffc12345u);
  EXPECT_EQ(f32[6], 0x00000002u);
  EXPECT_EQ(f32[7], 0x80000002u);
}

TEST(CanvasNoiseTest, FloatCanonicalizationMovesAtMostOneRepresentableValue) {
  constexpr std::array<uint16_t, 10> kF16Samples = {
      0x0001, 0x0002, 0x03ff, 0x0400, 0x3c00,
      0x7bff, 0x8001, 0x83ff, 0xbc00, 0xfbff,
  };
  for (const uint16_t original : kF16Samples) {
    for (uint8_t target = 0; target <= 1; ++target) {
      uint16_t actual = original;
      canvas_noise_internal::CanonicalizeFloat16Lsb(
          reinterpret_cast<uint8_t*>(&actual), target);
      const uint16_t original_magnitude = original & 0x7fffu;
      const uint16_t actual_magnitude = actual & 0x7fffu;
      const uint16_t distance = original_magnitude > actual_magnitude
                                    ? original_magnitude - actual_magnitude
                                    : actual_magnitude - original_magnitude;
      EXPECT_LE(distance, 1u);
      EXPECT_NE(actual_magnitude, 0u);
      EXPECT_NE(actual & 0x7c00u, 0x7c00u);
      EXPECT_EQ(actual & 0x8000u, original & 0x8000u);
      EXPECT_EQ(actual & 1u, target);
    }
  }

  constexpr std::array<uint32_t, 10> kF32Samples = {
      0x00000001u, 0x00000002u, 0x007fffffu, 0x00800000u,
      0x3f800000u, 0x7f7fffffu, 0x80000001u, 0x807fffffu,
      0xbf800000u, 0xff7fffffu,
  };
  for (const uint32_t original : kF32Samples) {
    for (uint8_t target = 0; target <= 1; ++target) {
      uint32_t actual = original;
      canvas_noise_internal::CanonicalizeFloat32Lsb(
          reinterpret_cast<uint8_t*>(&actual), target);
      const uint32_t original_magnitude = original & 0x7fffffffu;
      const uint32_t actual_magnitude = actual & 0x7fffffffu;
      const uint32_t distance = original_magnitude > actual_magnitude
                                    ? original_magnitude - actual_magnitude
                                    : actual_magnitude - original_magnitude;
      EXPECT_LE(distance, 1u);
      EXPECT_NE(actual_magnitude, 0u);
      EXPECT_NE(actual & 0x7f800000u, 0x7f800000u);
      EXPECT_EQ(actual & 0x80000000u, original & 0x80000000u);
      EXPECT_EQ(actual & 1u, target);
    }
  }
}

TEST(CanvasNoiseTest, IsIdempotentForEveryPixelFormat) {
  constexpr uint64_t kSeed = 0xf00dcafe12345678ULL;
  constexpr int64_t kOriginX = -17;
  constexpr int64_t kOriginY = 29;

  std::array<uint8_t, 8> rgba8 = {0, 1, 254, 19, 255, 42, 99, 201};
  std::array<uint8_t, 8> bgra8 = {254, 1, 0, 19, 99, 42, 255, 201};
  std::array<uint16_t, 8> rgba_f16 = {
      0x0001, 0x3c00, 0x7e55, 0x3555,
      0x8001, 0xbc01, 0x7c00, 0xb555,
  };
  std::array<uint32_t, 8> rgba_f32 = {
      0x00000001u, 0x3f800000u, 0x7fc12345u, 0x3eaaaaabu,
      0x80000001u, 0xbf800001u, 0x7f800000u, 0xbeaaaaabu,
  };

  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      rgba8.data(), sizeof(rgba8), 8, 2, 1, CanvasPixelFormat::kRgba8,
      kSeed, kOriginX, kOriginY));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      bgra8.data(), sizeof(bgra8), 8, 2, 1, CanvasPixelFormat::kBgra8,
      kSeed, kOriginX, kOriginY));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      reinterpret_cast<uint8_t*>(rgba_f16.data()), sizeof(rgba_f16),
      sizeof(rgba_f16), 2, 1, CanvasPixelFormat::kRgbaF16, kSeed,
      kOriginX, kOriginY));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      reinterpret_cast<uint8_t*>(rgba_f32.data()), sizeof(rgba_f32),
      sizeof(rgba_f32), 2, 1, CanvasPixelFormat::kRgbaF32, kSeed,
      kOriginX, kOriginY));

  const auto once_rgba8 = rgba8;
  const auto once_bgra8 = bgra8;
  const auto once_f16 = rgba_f16;
  const auto once_f32 = rgba_f32;

  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      rgba8.data(), sizeof(rgba8), 8, 2, 1, CanvasPixelFormat::kRgba8,
      kSeed, kOriginX, kOriginY));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      bgra8.data(), sizeof(bgra8), 8, 2, 1, CanvasPixelFormat::kBgra8,
      kSeed, kOriginX, kOriginY));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      reinterpret_cast<uint8_t*>(rgba_f16.data()), sizeof(rgba_f16),
      sizeof(rgba_f16), 2, 1, CanvasPixelFormat::kRgbaF16, kSeed,
      kOriginX, kOriginY));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      reinterpret_cast<uint8_t*>(rgba_f32.data()), sizeof(rgba_f32),
      sizeof(rgba_f32), 2, 1, CanvasPixelFormat::kRgbaF32, kSeed,
      kOriginX, kOriginY));

  EXPECT_EQ(rgba8, once_rgba8);
  EXPECT_EQ(bgra8, once_bgra8);
  EXPECT_EQ(rgba_f16, once_f16);
  EXPECT_EQ(rgba_f32, once_f32);
  EXPECT_EQ(rgba8[3], 19);
  EXPECT_EQ(rgba8[7], 201);
  EXPECT_EQ(bgra8[3], 19);
  EXPECT_EQ(bgra8[7], 201);
  EXPECT_EQ(rgba_f16[3], 0x3555);
  EXPECT_EQ(rgba_f16[7], 0xb555);
  EXPECT_EQ(rgba_f32[3], 0x3eaaaaabu);
  EXPECT_EQ(rgba_f32[7], 0xbeaaaaabu);
}

TEST(CanvasNoiseTest, OverlappingRectanglesUseSourcePixelCoordinates) {
  constexpr uint64_t kSeed = 8675309;
  constexpr int64_t kOriginX = -3;
  constexpr int64_t kOriginY = 7;
  std::array<uint8_t, 4 * 3 * 2> full = {
      10, 20, 30, 40, 11, 21, 31, 41, 12, 22, 32, 42,
      13, 23, 33, 43, 14, 24, 34, 44, 15, 25, 35, 45,
  };
  std::array<uint8_t, 4 * 2> subrect = {
      11, 21, 31, 41, 12, 22, 32, 42,
  };

  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      full.data(), full.size(), 12, 3, 2, CanvasPixelFormat::kRgba8,
      kSeed, kOriginX, kOriginY));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      subrect.data(), subrect.size(), 8, 2, 1, CanvasPixelFormat::kRgba8,
      kSeed, kOriginX + 1, kOriginY));

  EXPECT_TRUE(std::equal(subrect.begin(), subrect.end(), full.begin() + 4));
}

TEST(CanvasNoiseTest, IsDeterministicAndPreservesRowPadding) {
  constexpr size_t kRowBytes = 12;
  std::array<uint8_t, kRowBytes * 2> first = {
      10, 20, 30, 40, 50, 60, 70, 80, 0xaa, 0xaa, 0xaa, 0xaa,
      90, 91, 92, 93, 94, 95, 96, 97, 0xbb, 0xbb, 0xbb, 0xbb,
  };
  auto second = first;

  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      first.data(), first.size(), kRowBytes, 2, 2,
      CanvasPixelFormat::kRgba8, 42));
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      second.data(), second.size(), kRowBytes, 2, 2,
      CanvasPixelFormat::kRgba8, 42));

  EXPECT_EQ(first, second);
  EXPECT_EQ(first[3], 40);
  EXPECT_EQ(first[7], 80);
  EXPECT_EQ(first[15], 93);
  EXPECT_EQ(first[19], 97);
  EXPECT_EQ(first[8], 0xaa);
  EXPECT_EQ(first[11], 0xaa);
  EXPECT_EQ(first[20], 0xbb);
  EXPECT_EQ(first[23], 0xbb);
}

TEST(CanvasNoiseTest, CanonicalizesEightBitColorWithMinimalPerturbation) {
  std::array<uint8_t, 4> rgba = {255, 0, 255, 19};
  const auto original = rgba;
  ASSERT_TRUE(ApplyDeterministicCanvasNoise(
      rgba.data(), rgba.size(), rgba.size(), 1, 1,
      CanvasPixelFormat::kRgba8, 1234567890));
  Prng expected_bits(
      canvas_noise_internal::PixelSeed(1234567890, 0, 0));
  for (size_t channel = 0; channel < 3; ++channel) {
    const uint8_t expected = static_cast<uint8_t>(
        (original[channel] & 0xfeu) | (expected_bits.NextUint32() & 1u));
    const int distance = rgba[channel] > original[channel]
                             ? rgba[channel] - original[channel]
                             : original[channel] - rgba[channel];
    EXPECT_EQ(rgba[channel], expected);
    EXPECT_LE(distance, 1);
  }
  EXPECT_EQ(rgba[3], 19);
}

TEST(CanvasNoiseTest, RejectsInvalidLayoutWithoutMutation) {
  std::array<uint8_t, 8> pixels = {1, 2, 3, 4, 5, 6, 7, 8};
  const auto original = pixels;
  EXPECT_FALSE(ApplyDeterministicCanvasNoise(
      pixels.data(), pixels.size(), 3, 1, 1,
      CanvasPixelFormat::kRgba8, 7));
  EXPECT_EQ(pixels, original);

  EXPECT_FALSE(ApplyDeterministicCanvasNoise(
      pixels.data(), pixels.size(), 8, 3, 1,
      CanvasPixelFormat::kRgba8, 7));
  EXPECT_EQ(pixels, original);
}

}  // namespace
}  // namespace clawbrowser
