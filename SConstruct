#!/usr/bin/env python
import os

os.makedirs("addons/ps1_audio_effect/bin", exist_ok=True)

local_env = Environment(tools=["default"], PLATFORM="")
env = SConscript("godot-cpp/SConstruct", {"api_version": "4.7", "env": local_env})
env.Append(CPPPATH=["src"])

# SCons requires the platform shared-library suffix in the target name.
# Godot's suffix supplies the OS, build target, and architecture.
suffix = env["suffix"].replace(".dev", "").replace(".universal", "")
library_name = "{}{}{}{}".format(
    env.subst("$SHLIBPREFIX"),
    "ps1_audio_effect",
    suffix,
    env.subst("$SHLIBSUFFIX"),
)

library = env.SharedLibrary(
    "addons/ps1_audio_effect/bin/" + library_name,
    source=Glob("src/*.cpp"),
)
Default(library)
