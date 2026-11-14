// Standalone test for src/ps1_dsp.h (no Godot needed).
//   g++ -std=c++17 -O1 -fsanitize=address,undefined -I src tests/dsp_test.cpp -o dsp_test && ./dsp_test

#include "ps1_dsp.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>
#include <vector>

static int failures = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            std::printf("FAIL: %s (line %d)\n", msg, __LINE__);      \
            ++failures;                                               \
        }                                                             \
    } while (0)

static bool all_valid(const std::vector<ps1::Frame> &p_frames) {
    for (const ps1::Frame &f : p_frames) {
        if (!std::isfinite(f.left) || !std::isfinite(f.right) ||
                std::fabs(f.left) > 1.0f || std::fabs(f.right) > 1.0f) {
            return false;
        }
    }
    return true;
}

static std::vector<ps1::Frame> sine(int p_count, float p_amp, float p_hz, float p_rate) {
    std::vector<ps1::Frame> v(p_count);
    for (int i = 0; i < p_count; ++i) {
        const float s = p_amp * std::sin(6.2831853f * p_hz * i / p_rate);
        v[i] = { s, -s };
    }
    return v;
}

int main() {
    const int N = 4096;

    // 1. Null / empty buffers must not crash and must zero the output.
    {
        ps1::Processor p;
        ps1::Settings s;
        p.process(nullptr, nullptr, 128, s);
        std::vector<ps1::Frame> dst(16, { 5.0f, 5.0f });
        p.process(nullptr, dst.data(), 16, s);
        CHECK(dst[0].left == 0.0f && dst[15].right == 0.0f, "null src zeroes dst");
        p.process(dst.data(), dst.data(), 0, s);
        p.process(dst.data(), dst.data(), -5, s);
    }

    // 2. Hostile input: NaN, +/-inf, huge values, denormals.
    {
        ps1::Processor p;
        ps1::Settings s;
        std::vector<ps1::Frame> src(N), dst(N);
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        for (int i = 0; i < N; ++i) {
            switch (i % 6) {
                case 0: src[i] = { nan, nan }; break;
                case 1: src[i] = { inf, -inf }; break;
                case 2: src[i] = { 1e30f, -1e30f }; break;
                case 3: src[i] = { 1e-40f, -1e-40f }; break;
                case 4: src[i] = { 0.5f, -0.5f }; break;
                default: src[i] = { 0.0f, 0.0f }; break;
            }
        }
        p.process(src.data(), dst.data(), N, s);
        CHECK(all_valid(dst), "hostile input stays finite and within [-1, 1]");
    }

    // 3. Hostile settings and mix rates.
    {
        const float rates[] = { 0.0f, -1.0f, 1.0f, 8000.0f, 44100.0f, 48000.0f, 192000.0f, 1e9f,
                std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() };
        const int bits[] = { -100, 0, 1, 4, 10, 16, 17, 1000 };
        const int srates[] = { -5, 0, 1, 4000, 22050, 96000, 2000000000 };
        const float lps[] = { -1.0f, 0.0f, 100.0f, 10000.0f, 1e9f,
                std::numeric_limits<float>::quiet_NaN() };
        const float drives[] = { -3.0f, 0.0f, 1.0f, 4.0f, 1e9f,
                std::numeric_limits<float>::quiet_NaN() };
        const std::vector<ps1::Frame> src = sine(512, 0.9f, 440.0f, 44100.0f);
        std::vector<ps1::Frame> dst(512);
        long combos = 0;
        for (float r : rates)
            for (int b : bits)
                for (int sr : srates)
                    for (float lp : lps)
                        for (float d : drives) {
                            ps1::Processor p;
                            p.set_mix_rate(r);
                            ps1::Settings s;
                            s.bit_depth = b;
                            s.sample_rate = sr;
                            s.low_pass_hz = lp;
                            s.drive = d;
                            p.process(src.data(), dst.data(), 512, s);
                            ++combos;
                            if (!all_valid(dst)) {
                                std::printf("FAIL: bad output rate=%g bits=%d sr=%d lp=%g drive=%g\n",
                                        r, b, sr, lp, d);
                                ++failures;
                                goto done_sweep;
                            }
                        }
    done_sweep:
        std::printf("settings sweep: %ld combinations checked\n", combos);
    }

    // 4. Disabled = bit-exact passthrough.
    {
        ps1::Processor p;
        ps1::Settings s;
        s.enabled = false;
        std::vector<ps1::Frame> src = sine(256, 0.7f, 300.0f, 44100.0f), dst(256);
        p.process(src.data(), dst.data(), 256, s);
        bool same = true;
        for (int i = 0; i < 256; ++i) {
            same = same && src[i].left == dst[i].left && src[i].right == dst[i].right;
        }
        CHECK(same, "disabled passes audio through unchanged");
    }

    // 5. Bit depth limits the number of distinct output levels.
    {
        ps1::Processor p;
        ps1::Settings s;
        s.bit_depth = 4;
        s.sample_rate = 96000;
        s.low_pass_hz = 20000.0f;
        s.drive = 1.0f;
        std::vector<ps1::Frame> src = sine(N, 0.95f, 220.0f, 44100.0f), dst(N);
        p.process(src.data(), dst.data(), N, s);
        std::set<float> levels;
        for (const ps1::Frame &f : dst) {
            levels.insert(f.left);
        }
        CHECK(levels.size() <= 15, "4-bit output has at most 15 distinct levels");
        CHECK(levels.size() >= 5, "4-bit output still carries signal");
    }

    // 6. Sample-rate reduction holds samples ~mix/target frames each.
    {
        ps1::Processor p;
        ps1::Settings s;
        s.bit_depth = 16;
        s.sample_rate = 11025; // 44100 / 11025 = 4
        s.low_pass_hz = 20000.0f;
        s.drive = 1.0f;
        std::vector<ps1::Frame> src = sine(N, 0.8f, 1000.0f, 44100.0f), dst(N);
        p.process(src.data(), dst.data(), N, s);
        int changes = 0;
        for (int i = 1; i < N; ++i) {
            changes += (dst[i].left != dst[i - 1].left) ? 1 : 0;
        }
        CHECK(changes > N / 5 && changes < N / 3, "11.025 kHz hold changes about every 4th frame");
    }

    // 7. Block-size independence: chunked processing == one big block.
    {
        ps1::Settings s;
        std::vector<ps1::Frame> src = sine(N, 0.6f, 523.0f, 44100.0f), whole(N), chunked(N);
        ps1::Processor a, b;
        a.process(src.data(), whole.data(), N, s);
        for (int off = 0; off < N;) {
            const int n = std::min(37 + (off % 11), N - off);
            b.process(src.data() + off, chunked.data() + off, n, s);
            off += n;
        }
        bool same = true;
        for (int i = 0; i < N; ++i) {
            same = same && whole[i].left == chunked[i].left && whole[i].right == chunked[i].right;
        }
        CHECK(same, "result does not depend on audio block size");
    }

    if (failures == 0) {
        std::printf("ALL DSP TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", failures);
    return 1;
}
