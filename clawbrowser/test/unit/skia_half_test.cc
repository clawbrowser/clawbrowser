#include "src/opts/SkClawbrowserHalf.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static double referenceHalf(unsigned h) {
    unsigned e = (h >> 10) & 31, m = h & 1023;
    double value = e ? std::ldexp(1024.0 + m, int(e) - 25) : std::ldexp(double(m), -24);
    return (h & 0x8000) ? -value : value;
}
int main() {
    using skia_private::ClawbrowserFloatToHalf;
    using skia_private::ClawbrowserHalfToFloat;
    std::vector<double> positive;
    for (unsigned h = 0; h <= 0x7bff; ++h) positive.push_back(referenceHalf(h));
    size_t checked = 0, old_mismatches = 0;
    auto check = [&](float f) {
        unsigned bits = std::bit_cast<unsigned>(f), sign = (bits >> 16) & 0x8000;
        unsigned expected;
        double value = std::abs(double(f));
        if (std::isnan(f)) {
            expected = sign | 0x7c00 | ((bits & 0x7fffff) >> 13) | 0x200;
        } else if (value >= 65520) {
            expected = sign | 0x7c00;
        } else {
            auto it = std::lower_bound(positive.begin(), positive.end(), value);
            unsigned hi = unsigned(it - positive.begin());
            unsigned nearest = hi;
            if (hi == positive.size()) nearest = hi - 1;
            else if (hi > 0) {
                double below = value - positive[hi - 1], above = positive[hi] - value;
                if (below < above || (below == above && ((hi - 1) & 1) == 0)) nearest = hi - 1;
            }
            expected = sign | nearest;
        }
        if (ClawbrowserFloatToHalf(f) != expected) {
            std::fprintf(stderr, "float=%08x got=%04x expected=%04x\n", bits, ClawbrowserFloatToHalf(f), expected);
            std::exit(1);
        }
#if defined(__SSE2__) || defined(_M_X64)
        alignas(16) uint16_t half_lanes[8];
        _mm_store_si128(reinterpret_cast<__m128i*>(half_lanes),
            skia_private::ClawbrowserFloatToHalf4(_mm_setr_ps(f, -f, .25f, -.75f)));
        if (half_lanes[0] != expected || half_lanes[1] != (expected ^ 0x8000) ||
            half_lanes[2] != ClawbrowserFloatToHalf(.25f) ||
            half_lanes[3] != ClawbrowserFloatToHalf(-.75f)) return std::exit(6);
#endif
        unsigned mag = bits & 0x7fffffff;
        if (std::isfinite(f) && value <= 65504) {
            unsigned old = mag < 0x38800000 ? 0 : sign + (mag >> 13) - (112 << 10);
            old_mismatches += old != expected;
        }
        ++checked;
    };
    for (unsigned h = 0; h < 65536; ++h) {
        float got = ClawbrowserHalfToFloat(uint16_t(h));
        if ((h & 0x7c00) == 0x7c00) {
            if ((h & 1023) ? !std::isnan(got) : !std::isinf(got)) return 2;
            if (std::signbit(got) != bool(h & 0x8000)) return 3;
        } else {
            float expected = float(referenceHalf(h));
            if (std::bit_cast<unsigned>(got) != std::bit_cast<unsigned>(expected)) return 4;
        }
        check(got);
#if defined(__SSE2__) || defined(_M_X64)
        alignas(16) float lanes[4];
        _mm_store_ps(lanes, skia_private::ClawbrowserHalfToFloat4(
            _mm_setr_epi16(h, h ^ 0x8000, 0x3c00, -32768, 0, 0, 0, 0)));
        const uint16_t inputs[] = {uint16_t(h), uint16_t(h ^ 0x8000), 0x3c00, 0x8000};
        for (int i = 0; i < 4; ++i) {
            if (std::bit_cast<unsigned>(lanes[i]) !=
                std::bit_cast<unsigned>(ClawbrowserHalfToFloat(inputs[i]))) return 7;
        }
#endif
    }
    for (unsigned h = 0; h < 0x7bff; ++h) {
        float midpoint = float((positive[h] + positive[h + 1]) / 2);
        for (float f : {std::nextafter(midpoint, -INFINITY), midpoint, std::nextafter(midpoint, INFINITY)}) {
            check(f); check(-f);
        }
    }
    for (float f : {65504.f,65519.f,65520.f,65521.f,65536.f}) { check(f); check(-f); }
    unsigned rng = 0x12345678;
    for (unsigned i = 0; i < 1000000; ++i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        check(std::bit_cast<float>(rng));
    }
    if (!old_mismatches) return 5;
    std::printf("PASS half decode=65536 encode=%zu old-formula-mismatches=%zu\n", checked, old_mismatches);
#if defined(__SSE2__) || defined(_M_X64)
    std::printf("PASS SSE2 mixed lanes decode=262144 encode=%zu\n", checked * 4);
#endif
}
