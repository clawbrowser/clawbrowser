#include "clawbrowser/noise/prng.h"

#include <set>

#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(PrngTest, SameSeedProducesSameSequence) {
  Prng a(12345);
  Prng b(12345);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(a.NextUint32(), b.NextUint32());
  }
}

TEST(PrngTest, DifferentSeedsProduceDifferentSequence) {
  Prng a(12345);
  Prng b(54321);
  bool any_different = false;
  for (int i = 0; i < 100; ++i) {
    if (a.NextUint32() != b.NextUint32()) {
      any_different = true;
      break;
    }
  }
  EXPECT_TRUE(any_different);
}

TEST(PrngTest, PixelNoiseWithinBounds) {
  Prng prng(99999);
  for (int i = 0; i < 1000; ++i) {
    int offset = prng.PixelNoise();
    EXPECT_GE(offset, -1);
    EXPECT_LE(offset, 1);
  }
}

TEST(PrngTest, AudioNoiseWithinBounds) {
  Prng prng(99999);
  for (int i = 0; i < 1000; ++i) {
    float offset = prng.AudioNoise();
    EXPECT_GE(offset, -1e-7f);
    EXPECT_LE(offset, 1e-7f);
  }
}

TEST(PrngTest, SubPixelNoiseWithinBounds) {
  Prng prng(99999);
  for (int i = 0; i < 1000; ++i) {
    double offset = prng.SubPixelNoise();
    EXPECT_GE(offset, -0.001);
    EXPECT_LE(offset, 0.001);
  }
}

TEST(PrngTest, DeterministicPixelNoise) {
  Prng a(42);
  Prng b(42);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(a.PixelNoise(), b.PixelNoise());
  }
}

// ---------------------------------------------------------------------------
// RectNoiseSeed -- guards the client-rects fix.
// ---------------------------------------------------------------------------

TEST(RectNoiseSeedTest, SameGeometryProducesSameSeed) {
  // Repeated reads of an unchanged element must not drift; values that change
  // between two getBoundingClientRect() calls are themselves a signal.
  EXPECT_EQ(RectNoiseSeed(1234, 10.0, 20.0, 30.0, 40.0),
            RectNoiseSeed(1234, 10.0, 20.0, 30.0, 40.0));
}

TEST(RectNoiseSeedTest, DifferentGeometryProducesDifferentSeed) {
  const uint64_t base = RectNoiseSeed(1234, 10.0, 20.0, 30.0, 40.0);
  EXPECT_NE(base, RectNoiseSeed(1234, 11.0, 20.0, 30.0, 40.0));
  EXPECT_NE(base, RectNoiseSeed(1234, 10.0, 21.0, 30.0, 40.0));
  EXPECT_NE(base, RectNoiseSeed(1234, 10.0, 20.0, 31.0, 40.0));
  EXPECT_NE(base, RectNoiseSeed(1234, 10.0, 20.0, 30.0, 41.0));
}

TEST(RectNoiseSeedTest, DifferentFingerprintSeedProducesDifferentSeed) {
  EXPECT_NE(RectNoiseSeed(1234, 10.0, 20.0, 30.0, 40.0),
            RectNoiseSeed(5678, 10.0, 20.0, 30.0, 40.0));
}

TEST(RectNoiseSeedTest, DistinctRectsDoNotShareFirstOffset) {
  // The regression this protects against: seeding one Prng from the
  // fingerprint seed restarts the sequence per call, so every rect on the page
  // receives the identical first draw and shifts by the same amount.
  std::set<int64_t> offsets;
  for (int i = 0; i < 50; ++i) {
    Prng prng(RectNoiseSeed(99, i * 13.0, i * 7.0, 100.0 + i, 50.0 + i));
    offsets.insert(static_cast<int64_t>(prng.SubPixelNoise() * 1e9));
  }
  // Allow a couple of collisions, but nothing close to "all identical".
  EXPECT_GT(offsets.size(), 45u);
}

TEST(RectNoiseSeedTest, NeverReturnsZero) {
  // Prng treats 0 as "reseed to 1", which would collapse distinct rects.
  EXPECT_NE(RectNoiseSeed(0, 0.0, 0.0, 0.0, 0.0), 0u);
}

TEST(RectNoiseSeedTest, QuantizesInsignificantFloatDifferences) {
  // Sub-1/64px jitter should not reseed what is visually the same rect.
  EXPECT_EQ(RectNoiseSeed(7, 10.0, 20.0, 30.0, 40.0),
            RectNoiseSeed(7, 10.0001, 20.0001, 30.0001, 40.0001));
}

}  // namespace
}  // namespace clawbrowser
