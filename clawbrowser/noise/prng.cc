#include "clawbrowser/noise/prng.h"

namespace clawbrowser {

Prng::Prng(uint64_t seed) : state_(seed ? seed : 1) {}

uint32_t Prng::NextUint32() {
  // xorshift64
  state_ ^= state_ << 13;
  state_ ^= state_ >> 7;
  state_ ^= state_ << 17;
  return static_cast<uint32_t>(state_ & 0xFFFFFFFF);
}

int Prng::PixelNoise() {
  uint32_t val = NextUint32();
  return static_cast<int>(val % 3) - 1;  // -1, 0, or +1
}

float Prng::AudioNoise() {
  uint32_t val = NextUint32();
  // Map to [-1e-7, +1e-7]
  float normalized = static_cast<float>(val) / static_cast<float>(UINT32_MAX);
  return (normalized * 2.0f - 1.0f) * 1e-7f;
}

double Prng::SubPixelNoise() {
  uint32_t val = NextUint32();
  // Map to [-0.001, +0.001]
  double normalized = static_cast<double>(val) / static_cast<double>(UINT32_MAX);
  return (normalized * 2.0 - 1.0) * 0.001;
}

}  // namespace clawbrowser
