// Opt-in SSE2 microbenchmark; not a substitute for browser raster timing.
// Compile with -O2 -ffp-contract=off -msse2 -mno-avx -mno-fma and the actual
// extracted/patched Skia header directory on the include path.
#include "src/opts/SkClawbrowserFma.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

__attribute__((noinline)) __m128 pipeline(__m128 f, __m128 m, __m128 a) {
  auto r = skia_private::ClawbrowserFma4(f, m, a);
  auto g = skia_private::ClawbrowserFma4(m, a, f);
  auto b = skia_private::ClawbrowserFma4(a, f, m);
  return skia_private::ClawbrowserFma4(r, g, b);
}

int main(int argc, char** argv) {
  const int count = argc > 1 ? std::atoi(argv[1]) : 10000000;
  if (count < 1 || count > 100000000) return 1;
  __m128 sum = _mm_setzero_ps();
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < count; ++i) {
    const float v = (i % 1023) / 1024.0f;
    sum = _mm_add_ps(sum, pipeline(_mm_set_ps(v, .125f, .25f, .5f),
        _mm_set_ps(.25f, v, .75f, .625f), _mm_set_ps(.0625f, .25f, v, .125f)));
  }
  alignas(16) float values[4];
  _mm_store_ps(values, sum);
  const double ms = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start).count();
  std::printf("{\"milliseconds\":%.3f,\"iterations\":%d,\"checksum\":%.9g}\n",
      ms, count, double(values[0]) + values[1] + values[2] + values[3]);
}
