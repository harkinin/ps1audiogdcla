#ifndef PS1_AUDIO_EFFECT_H
#define PS1_AUDIO_EFFECT_H

#include <godot_cpp/classes/audio_effect.hpp>
#include <godot_cpp/classes/audio_effect_instance.hpp>
#include <godot_cpp/classes/audio_frame.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <atomic>
#include <cstdint>
#include <memory>

#include "ps1_dsp.h"

namespace godot {

// Parameters shared between the effect resource (edited on the main thread)
// and its processing instance(s) (read on the audio thread). Plain atomics:
// no Godot objects, no reference counting and no locks on the audio thread.
struct PS1SharedParams {
    std::atomic<int> bit_depth{ 10 };
    std::atomic<int> sample_rate{ 22050 };
    std::atomic<float> low_pass_hz{ 10000.0f };
    std::atomic<float> drive{ 1.15f };
    std::atomic<bool> enabled{ true };
};

class PS1AudioEffectInstance : public AudioEffectInstance {
    GDCLASS(PS1AudioEffectInstance, AudioEffectInstance)

    std::shared_ptr<PS1SharedParams> params;
    ps1::Processor processor;

protected:
    static void _bind_methods();

public:
    // Called once from PS1AudioEffect::_instantiate() on the main thread,
    // before the instance is handed to the audio server.
    void configure(const std::shared_ptr<PS1SharedParams> &p_params, float p_mix_rate);

    void _process(const void *p_src_buffer, AudioFrame *r_dst_buffer,
            int32_t p_frame_count) override;
};

class PS1AudioEffect : public AudioEffect {
    GDCLASS(PS1AudioEffect, AudioEffect)

    std::shared_ptr<PS1SharedParams> params = std::make_shared<PS1SharedParams>();

protected:
    static void _bind_methods();

public:
    void set_bit_depth(int p_value);
    int get_bit_depth() const;

    void set_sample_rate(int p_value);
    int get_sample_rate() const;

    void set_low_pass_hz(float p_value);
    float get_low_pass_hz() const;

    void set_drive(float p_value);
    float get_drive() const;

    void set_enabled(bool p_value);
    bool get_enabled() const;

    Ref<AudioEffectInstance> _instantiate() override;
};

} // namespace godot

#endif // PS1_AUDIO_EFFECT_H
