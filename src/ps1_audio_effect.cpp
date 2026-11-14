#include "ps1_audio_effect.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace godot {

// The DSP treats our frames and Godot's AudioFrame as the same memory layout.
static_assert(sizeof(AudioFrame) == sizeof(ps1::Frame), "AudioFrame must be two packed floats");
static_assert(offsetof(AudioFrame, left) == offsetof(ps1::Frame, left), "AudioFrame::left offset mismatch");
static_assert(offsetof(AudioFrame, right) == offsetof(ps1::Frame, right), "AudioFrame::right offset mismatch");

void PS1AudioEffectInstance::_bind_methods() {
}

void PS1AudioEffectInstance::configure(const std::shared_ptr<PS1SharedParams> &p_params, float p_mix_rate) {
    params = p_params;
    processor.set_mix_rate(p_mix_rate);
}

// Runs on the audio thread: no allocation, no locks, no Godot API calls.
void PS1AudioEffectInstance::_process(const void *p_src_buffer,
        AudioFrame *r_dst_buffer, int32_t p_frame_count) {
    if (r_dst_buffer == nullptr || p_frame_count <= 0) {
        return;
    }

    ps1::Frame *dst = reinterpret_cast<ps1::Frame *>(r_dst_buffer);
    const ps1::Frame *src = reinterpret_cast<const ps1::Frame *>(p_src_buffer);

    ps1::Settings settings;
    if (params) {
        settings.bit_depth = params->bit_depth.load(std::memory_order_relaxed);
        settings.sample_rate = params->sample_rate.load(std::memory_order_relaxed);
        settings.low_pass_hz = params->low_pass_hz.load(std::memory_order_relaxed);
        settings.drive = params->drive.load(std::memory_order_relaxed);
        settings.enabled = params->enabled.load(std::memory_order_relaxed);
    } else {
        settings.enabled = false; // Never configured: pass audio through untouched.
    }

    processor.process(src, dst, p_frame_count, settings);
}

void PS1AudioEffect::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_bit_depth", "value"), &PS1AudioEffect::set_bit_depth);
    ClassDB::bind_method(D_METHOD("get_bit_depth"), &PS1AudioEffect::get_bit_depth);
    ClassDB::bind_method(D_METHOD("set_sample_rate", "value"), &PS1AudioEffect::set_sample_rate);
    ClassDB::bind_method(D_METHOD("get_sample_rate"), &PS1AudioEffect::get_sample_rate);
    ClassDB::bind_method(D_METHOD("set_low_pass_hz", "value"), &PS1AudioEffect::set_low_pass_hz);
    ClassDB::bind_method(D_METHOD("get_low_pass_hz"), &PS1AudioEffect::get_low_pass_hz);
    ClassDB::bind_method(D_METHOD("set_drive", "value"), &PS1AudioEffect::set_drive);
    ClassDB::bind_method(D_METHOD("get_drive"), &PS1AudioEffect::get_drive);
    ClassDB::bind_method(D_METHOD("set_enabled", "value"), &PS1AudioEffect::set_enabled);
    ClassDB::bind_method(D_METHOD("get_enabled"), &PS1AudioEffect::get_enabled);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "bit_depth", PROPERTY_HINT_RANGE, "4,16,1"),
            "set_bit_depth", "get_bit_depth");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "sample_rate", PROPERTY_HINT_RANGE, "4000,96000,100"),
            "set_sample_rate", "get_sample_rate");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "low_pass_hz", PROPERTY_HINT_RANGE, "100,20000,100,suffix:Hz"),
            "set_low_pass_hz", "get_low_pass_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drive", PROPERTY_HINT_RANGE, "1,4,0.01"),
            "set_drive", "get_drive");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "get_enabled");
}

void PS1AudioEffect::set_bit_depth(int p_value) {
    params->bit_depth.store(std::clamp(p_value, 4, 16), std::memory_order_relaxed);
}

int PS1AudioEffect::get_bit_depth() const {
    return params->bit_depth.load(std::memory_order_relaxed);
}

void PS1AudioEffect::set_sample_rate(int p_value) {
    params->sample_rate.store(std::clamp(p_value, 4000, 96000), std::memory_order_relaxed);
}

int PS1AudioEffect::get_sample_rate() const {
    return params->sample_rate.load(std::memory_order_relaxed);
}

void PS1AudioEffect::set_low_pass_hz(float p_value) {
    const float value = std::isfinite(p_value) ? p_value : 20000.0f;
    params->low_pass_hz.store(std::clamp(value, 100.0f, 48000.0f), std::memory_order_relaxed);
}

float PS1AudioEffect::get_low_pass_hz() const {
    return params->low_pass_hz.load(std::memory_order_relaxed);
}

void PS1AudioEffect::set_drive(float p_value) {
    const float value = std::isfinite(p_value) ? p_value : 1.0f;
    params->drive.store(std::clamp(value, 1.0f, 4.0f), std::memory_order_relaxed);
}

float PS1AudioEffect::get_drive() const {
    return params->drive.load(std::memory_order_relaxed);
}

void PS1AudioEffect::set_enabled(bool p_value) {
    params->enabled.store(p_value, std::memory_order_relaxed);
}

bool PS1AudioEffect::get_enabled() const {
    return params->enabled.load(std::memory_order_relaxed);
}

// Called by AudioServer (main thread, audio lock held) once per bus channel.
// Godot's mixer dereferences the returned instance without a null check, so
// this must always return a valid, fully configured instance.
Ref<AudioEffectInstance> PS1AudioEffect::_instantiate() {
    Ref<PS1AudioEffectInstance> instance;
    instance.instantiate();

    float mix_rate = 44100.0f;
    AudioServer *server = AudioServer::get_singleton();
    if (server != nullptr) {
        mix_rate = static_cast<float>(server->get_mix_rate());
    }

    if (instance.is_valid()) {
        instance->configure(params, mix_rate);
        UtilityFunctions::print("PS1 Audio: created effect instance (mix rate ",
                mix_rate, " Hz).");
    } else {
        UtilityFunctions::push_error("PS1 Audio: failed to create the effect instance.");
    }

    return instance;
}

} // namespace godot
