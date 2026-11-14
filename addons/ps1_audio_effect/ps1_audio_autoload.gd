extends Node

# Optional: add this script as an Autoload named PS1Audio to install the effect
# on the Master bus automatically. Do not also add it manually in the Audio panel.

@export_range(4, 16, 1) var bit_depth: int = 10
@export_range(4000, 48000, 100) var sample_rate: int = 22050
@export_range(1000.0, 20000.0, 100.0) var low_pass_hz: float = 10000.0
@export_range(1.0, 4.0, 0.01) var drive: float = 1.15

func _ready() -> void:
    call_deferred("_install_on_master")

func _install_on_master() -> void:
    var master_index := AudioServer.get_bus_index("Master")
    if master_index < 0:
        push_error("PS1 Audio: could not find the Master audio bus.")
        return

    for i in range(AudioServer.get_bus_effect_count(master_index)):
        var existing := AudioServer.get_bus_effect(master_index, i)
        if existing != null and existing.get_class() == "PS1AudioEffect":
            _configure_effect(existing)
            print("PS1 Audio: updated existing Master-bus effect.")
            return

    var effect := ClassDB.instantiate("PS1AudioEffect") as AudioEffect
    if effect == null:
        push_error("PS1 Audio: PS1AudioEffect is not registered. Check the GDExtension DLL.")
        return

    _configure_effect(effect)
    AudioServer.add_bus_effect(master_index, effect)
    print("PS1 Audio: installed on the Master bus.")

func _configure_effect(effect: AudioEffect) -> void:
    effect.set("bit_depth", bit_depth)
    effect.set("sample_rate", sample_rate)
    effect.set("low_pass_hz", low_pass_hz)
    effect.set("drive", drive)
    effect.set("enabled", true)
