#include "clawbrowser/noise/prng.h"

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

}  // namespace
}  // namespace clawbrowser
