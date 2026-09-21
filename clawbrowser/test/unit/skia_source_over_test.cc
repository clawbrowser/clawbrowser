#if defined(CLAWBROWSER_TEST_PRAGMA_AVX2)
// Skia selects optimized TUs through target pragmas, not compiler -mavx2.
#define SK_CPU_X64_LEVEL 8
#define SK_CPU_X64_LEVEL_AVX2 8
#pragma clang attribute push(__attribute__((target("avx2"))), apply_to = function)
#endif
#include "src/opts/SkClawbrowserSrcOver.h"
#include <cstdio>
#include <cstdlib>

uint64_t checked = 0, sse_checked = 0, avx_checked = 0, negative = 0;
alignas(32) uint32_t sources[8], destinations[8], expected[8];
int lane = 0;

uint32_t reference(uint32_t s, uint32_t d) {
    uint32_t result = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        const unsigned product = ((d >> shift) & 255) * (255 - (s >> 24));
        result |= std::min(255u, ((s >> shift) & 255) + (product + 127) / 255) << shift;
    }
    return result;
}

void check(uint32_t s, uint32_t d) {
    const uint32_t r = reference(s, d);
    if (skia_private::ClawbrowserSrcOver(s, d) != r) std::abort();
    sources[lane] = s; destinations[lane] = d; expected[lane] = r;
    ++checked;
    if (++lane != 8) return;
#if defined(SK_CLAWBROWSER_HAS_SRCOVER4)
    alignas(32) uint32_t actual[8];
    for (int offset : {0, 4}) {
        _mm_store_si128(reinterpret_cast<__m128i*>(actual + offset),
            skia_private::ClawbrowserSrcOver4(
                _mm_load_si128(reinterpret_cast<const __m128i*>(sources + offset)),
                _mm_load_si128(reinterpret_cast<const __m128i*>(destinations + offset))));
    }
    for (int i = 0; i < 8; ++i) if (actual[i] != expected[i]) std::abort();
    sse_checked += 8;
#endif
#if defined(SK_CLAWBROWSER_HAS_SRCOVER8)
    alignas(32) uint32_t avx[8];
    _mm256_store_si256(reinterpret_cast<__m256i*>(avx),
        skia_private::ClawbrowserSrcOver8(
            _mm256_load_si256(reinterpret_cast<const __m256i*>(sources)),
            _mm256_load_si256(reinterpret_cast<const __m256i*>(destinations))));
    for (int i = 0; i < 8; ++i) if (avx[i] != expected[i]) std::abort();
    avx_checked += 8;
#endif
    lane = 0;
}
#if defined(CLAWBROWSER_TEST_PRAGMA_AVX2)
#pragma clang attribute pop
#endif

int main() {
    // Exhaust every byte channel / source-alpha combination, including
    // saturation for non-premultiplied RGB input, not just random colors.
    for (unsigned a = 0; a < 256; ++a)
        for (unsigned d = 0; d < 256; ++d)
            for (unsigned s = 0; s < 256; ++s) {
                check((a << 24) | s * 0x010101u, d * 0x01010101u);
                const unsigned exact = std::min(255u, s + (d * (255-a) + 127) / 255);
                const unsigned old = std::min(255u, s + ((d * (256-a)) >> 8));
                negative += exact != old;
            }
    // Different channels and alpha values in each packed lane.
    uint32_t state = 654321;
    auto next = [&]() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; };
    for (int i = 0; i < 1000000; ++i) { const auto s = next(), d = next(); check(s, d); }
    if (lane || !negative) return 1;
#if defined(CLAWBROWSER_TEST_REQUIRE_SSE2)
    if (sse_checked != checked) return 1;
#endif
    std::printf("PASS %llu scalar, %llu SSE2, %llu AVX2; approximate negative control %llu mismatches\n",
        (unsigned long long)checked, (unsigned long long)sse_checked,
        (unsigned long long)avx_checked, (unsigned long long)negative);
}
