#include "ps1_audio_effect.h"

#include <godot_cpp/godot.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initialize_ps1_audio_effect_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(PS1AudioEffectInstance);
    GDREGISTER_CLASS(PS1AudioEffect);
}

void uninitialize_ps1_audio_effect_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {
GDExtensionBool GDE_EXPORT ps1_audio_effect_library_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization) {
    GDExtensionBinding::InitObject init_object(p_get_proc_address, p_library, r_initialization);
    init_object.register_initializer(initialize_ps1_audio_effect_module);
    init_object.register_terminator(uninitialize_ps1_audio_effect_module);
    init_object.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_object.init();
}
}
