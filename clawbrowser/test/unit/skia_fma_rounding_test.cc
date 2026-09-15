#include "src/opts/SkClawbrowserFma.h"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

uint64_t checked = 0;
uint64_t naive_mismatches = 0;
uint64_t simd_checked = 0;
bool SameFloat(float a, float b) {
  return (std::isnan(a) && std::isnan(b)) ||
         std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}
#if defined(SK_CLAWBROWSER_HAS_SSE2_FMA4)
alignas(16) float fs[4], ms[4], as[4], expected_lanes[4];
int lane = 0;
#endif
void check(float f, float m, float a) {
  const float expected = std::fma(f, m, a);
  const float actual = skia_private::ClawbrowserFma(f, m, a);
  const float naive = static_cast<float>(static_cast<double>(f) * m + a);
  if (std::isfinite(expected) &&
      std::bit_cast<uint32_t>(expected) != std::bit_cast<uint32_t>(naive)) ++naive_mismatches;
  ++checked;
  if (!SameFloat(expected, actual)) {
    std::printf("FAIL inputs=%a,%a,%a expected=%a actual=%a\n", f,m,a,expected,actual);
    std::exit(1);
  }
#if defined(SK_CLAWBROWSER_HAS_SSE2_FMA4)
  fs[lane]=f;ms[lane]=m;as[lane]=a;expected_lanes[lane]=expected;
  if (++lane == 4) {
    alignas(16) float values[4];
    _mm_store_ps(values, skia_private::ClawbrowserFma4(
        _mm_load_ps(fs), _mm_load_ps(ms), _mm_load_ps(as)));
    for (int i=0;i<4;++i) {
      if (!SameFloat(values[i], expected_lanes[i])) {
        std::printf("FAIL SIMD lane %d inputs=%a,%a,%a expected=%a actual=%a\n",
                    i,fs[i],ms[i],as[i],expected_lanes[i],values[i]);
        std::exit(1);
      }
    }
    simd_checked+=4;lane=0;
  }
#endif
}
int main() {
  const uint32_t edges[] = {0,1,2,0x007fffff,0x00800000,0x00800001,
    0x3effffff,0x3f000000,0x3f7fffff,0x3f800000,0x3f800001,
    0x4b800000,0x4b800001,0x7f7fffff,0x7f800000,0x7fc00000};
  for(auto f:edges) for(auto m:edges) for(auto a:edges)
    for(uint32_t signs=0;signs<8;++signs)
      check(std::bit_cast<float>(f|((signs&1)?0x80000000:0)),
            std::bit_cast<float>(m|((signs&2)?0x80000000:0)),
            std::bit_cast<float>(a|((signs&4)?0x80000000:0)));
  uint32_t state=123456789;
  const auto random=[&]() {state^=state<<13;state^=state>>17;state^=state<<5;return state;};
  for(int i=0;i<10000000;++i) {
    const auto f=random(),m=random(),a=random();
    check(std::bit_cast<float>(f),std::bit_cast<float>(m),std::bit_cast<float>(a));
  }
  // Adversarial normal midpoints whose discarded product tail changes a tie.
  for(int e=-100;e<=100;++e) for(int sign:{-1,1}) {
    const float f=std::ldexp(1.0f+0x1p-23f,e),m=1.0f-0x1p-23f;
    for(int offset=0;offset<8;++offset) {
      const float a=std::ldexp(16777216.0f+2*offset,e);
      check(sign*f,m,sign*a);check(sign*f,-m,sign*a);
    }
  }
  if (!naive_mismatches) {
    std::puts("FAIL: negative control did not detect naive double rounding");
    return 1;
  }
#if defined(SK_CLAWBROWSER_HAS_SSE2_FMA4)
  if (lane || simd_checked != checked) return 1;
#endif
  std::printf("PASS %llu scalar + %llu SIMD comparisons; naive-double negative control: %llu mismatches\n",
              static_cast<unsigned long long>(checked),
              static_cast<unsigned long long>(simd_checked),
              static_cast<unsigned long long>(naive_mismatches));
}
