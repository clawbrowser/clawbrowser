#ifndef CLAWBROWSER_NOISE_PRNG_H_
#define CLAWBROWSER_NOISE_PRNG_H_

#include <cstdint>

namespace clawbrowser {

// Seeded PRNG for deterministic fingerprint noise.
// Uses xorshift64 — fast, lightweight, good enough for noise injection.
// NOT cryptographically secure.
class Prng {
 public:
  explicit Prng(uint64_t seed) : state_(seed ? seed : 1) {}

  // Raw random uint32.
  uint32_t NextUint32() {
    // xorshift64
    state_ ^= state_ << 13;
    state_ ^= state_ >> 7;
    state_ ^= state_ << 17;
    return static_cast<uint32_t>(state_ & 0xFFFFFFFF);
  }

  // Per-pixel noise: returns -1, 0, or +1 per color channel.
  int PixelNoise() {
    return static_cast<int>(NextUint32() % 3) - 1;
  }

  // Audio sample noise: returns float in [-1e-7, +1e-7].
  float AudioNoise() {
    const float normalized =
        static_cast<float>(NextUint32()) / static_cast<float>(UINT32_MAX);
    return (normalized * 2.0f - 1.0f) * 1e-7f;
  }

  // Sub-pixel offset: returns double in [-0.001, +0.001].
  double SubPixelNoise() {
    const double normalized =
        static_cast<double>(NextUint32()) / static_cast<double>(UINT32_MAX);
    return (normalized * 2.0 - 1.0) * 0.001;
  }

 private:
  uint64_t state_;
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_NOISE_PRNG_H_
