#ifndef CLAWBROWSER_NOISE_PRNG_H_
#define CLAWBROWSER_NOISE_PRNG_H_

#include <cmath>
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

// Derives a stable, per-rect PRNG seed by mixing a fingerprint's client-rects
// seed with the rect's own geometry.
//
// Seeding Prng directly from the fingerprint seed restarts the sequence on
// every call, so every rect on the page receives the identical first draws and
// shifts by the same offset. That is trivially detectable -- and trivially
// removable -- by comparing any two elements. Mixing the geometry in keeps the
// noise stable for a given rect across repeated reads (real layout is stable,
// so values that drift between reads are themselves a signal) while making the
// offsets differ from one element to the next.
inline uint64_t RectNoiseSeed(uint64_t base_seed,
                              double x,
                              double y,
                              double width,
                              double height) {
  uint64_t hash = base_seed ? base_seed : 1;
  const auto mix = [&hash](double value) {
    // Quantize to 1/64 px so insignificant float noise in the incoming
    // geometry does not reseed what is visually the same rect.
    const int64_t quantized = static_cast<int64_t>(std::llround(value * 64.0));
    hash ^= static_cast<uint64_t>(quantized) + 0x9e3779b97f4a7c15ULL +
            (hash << 6) + (hash >> 2);
  };
  mix(x);
  mix(y);
  mix(width);
  mix(height);
  return hash ? hash : 1;
}

}  // namespace clawbrowser

#endif  // CLAWBROWSER_NOISE_PRNG_H_
