#ifndef PS1_DSP_H
#define PS1_DSP_H

// Pure DSP core for the PS1 effect. This header deliberately has NO Godot
// dependencies: it never allocates, locks, logs or calls into the engine, so
// it is safe to run on the audio thread and can be unit-tested on its own
// (see tests/dsp_test.cpp).

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ps1 {

// Layout-compatible with Godot's AudioFrame (two floats, left then right).
struct Frame {
    float left;
    float right;
};

struct Settings {
    int bit_depth = 10;
    int sample_rate = 22050;
    float low_pass_hz = 10000.0f;
    float drive = 1.15f;
    bool enabled = true;
};

class Processor {
public:
    void set_mix_rate(float p_mix_rate) {
        mix_rate = (std::isfinite(p_mix_rate) && p_mix_rate >= 8000.0f && p_mix_rate <= 384000.0f)
                ? p_mix_rate
                : 44100.0f;
        reset();
    }

    float get_mix_rate() const { return mix_rate; }

    void reset() {
        filter_left = filter_right = 0.0f;
        held_left = held_right = 0.0f;
        // Start "full" so the very first input frame is captured immediately.
        phase = static_cast<double>(mix_rate);
    }

    // Processes p_frame_count frames. Always writes every destination frame
    // (when both buffers are non-null) and never reads/writes out of range.
    void process(const Frame *p_src, Frame *p_dst, int32_t p_frame_count, const Settings &p_settings) {
        if (p_dst == nullptr || p_frame_count <= 0) {
            return;
        }
        if (p_src == nullptr) {
            for (int32_t i = 0; i < p_frame_count; ++i) {
                p_dst[i].left = 0.0f;
                p_dst[i].right = 0.0f;
            }
            return;
        }

        if (!p_settings.enabled) {
            for (int32_t i = 0; i < p_frame_count; ++i) {
                p_dst[i] = p_src[i];
            }
            return;
        }

        // Derive everything from the (re-validated) settings once per block.
        const int bits = std::clamp(p_settings.bit_depth, 4, 16);
        const float levels = static_cast<float>((1 << (bits - 1)) - 1); // 7 .. 32767, never 0.
        const float drive = std::isfinite(p_settings.drive) ? std::clamp(p_settings.drive, 1.0f, 4.0f) : 1.0f;
        const float drive_norm = std::tanh(drive); // tanh(1) ~ 0.76, so never 0.
        const double target_rate = std::clamp(static_cast<double>(p_settings.sample_rate), 1000.0, static_cast<double>(mix_rate));
        const bool reduce_rate = target_rate < static_cast<double>(mix_rate) - 0.5;

        const float nyquist = mix_rate * 0.5f;
        float cutoff = std::isfinite(p_settings.low_pass_hz) ? p_settings.low_pass_hz : nyquist;
        cutoff = std::clamp(cutoff, 100.0f, nyquist);
        const float alpha = (cutoff >= nyquist * 0.998f)
                ? 1.0f
                : 1.0f - std::exp(-6.28318530718f * cutoff / mix_rate);

        for (int32_t i = 0; i < p_frame_count; ++i) {
            const float in_left = finite_or_zero(p_src[i].left);
            const float in_right = finite_or_zero(p_src[i].right);

            // Low-pass before holding to tame aliasing.
            filter_left += alpha * (in_left - filter_left);
            filter_right += alpha * (in_right - filter_right);
            filter_left = flush_denormal(filter_left);
            filter_right = flush_denormal(filter_right);

            if (!reduce_rate) {
                held_left = filter_left;
                held_right = filter_right;
            } else {
                phase += target_rate;
                if (phase >= static_cast<double>(mix_rate)) {
                    phase -= static_cast<double>(mix_rate);
                    if (phase >= static_cast<double>(mix_rate)) {
                        phase = 0.0; // Defensive: only reachable if settings changed mid-stream.
                    }
                    held_left = filter_left;
                    held_right = filter_right;
                }
            }

            p_dst[i].left = shape(held_left, drive, drive_norm, levels);
            p_dst[i].right = shape(held_right, drive, drive_norm, levels);
        }
    }

private:
    static float finite_or_zero(float p_value) {
        return std::isfinite(p_value) ? p_value : 0.0f;
    }

    static float flush_denormal(float p_value) {
        return (std::fabs(p_value) < 1e-30f) ? 0.0f : p_value;
    }

    static float shape(float p_sample, float p_drive, float p_drive_norm, float p_levels) {
        float x = p_sample;
        if (p_drive > 1.001f) {
            x = std::tanh(x * p_drive) / p_drive_norm;
        }
        x = std::clamp(x, -1.0f, 1.0f);
        return std::round(x * p_levels) / p_levels;
    }

    float mix_rate = 44100.0f;
    float filter_left = 0.0f;
    float filter_right = 0.0f;
    float held_left = 0.0f;
    float held_right = 0.0f;
    double phase = 44100.0;
};

} // namespace ps1

#endif // PS1_DSP_H
