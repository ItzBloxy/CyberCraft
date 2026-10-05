"""Compiles BlockRender's redscript module against the game's own scripts, without starting the game.

    uv run --no-project python tools/check_reds.py <game folder> <scripts folder> [<more script folders>...]

It loads redscript's compiler library from the game (engine/tools/scc_lib.dll), with its error popup off, and compiles
the scripts folder, with any more folders given, against a copy of the game's untouched r6/cache/final.redscripts, so the
game's own files aren't touched. Codeware's Scripts folder, given too, compiles the module's parts that need Codeware
(@if(ModuleExists("Codeware"))), as the game does with Codeware installed; without it, the parts for its absence. It
prints "compiled" and exits 0, or prints redscript's errors and exits 1. Its work goes in build/reds-check. The game must
be installed; it needn't run, and may.
"""
import ctypes
import shutil
import sys
from pathlib import Path

if len(sys.argv) < 3:
    sys.exit(__doc__)
game, scripts = Path(sys.argv[1]), Path(sys.argv[2]).resolve()
more = [Path(folder).resolve() for folder in sys.argv[3:]]
work = Path("build").resolve() / "reds-check"
r6 = work / "r6"
for sub in ("scripts", "cache", "logs"):
    (r6 / sub).mkdir(parents=True, exist_ok=True)
blob = r6 / "cache" / "final.redscripts"
shutil.copyfile(game / "r6" / "cache" / "final.redscripts", blob)
out = work / "out.redscripts"
out.unlink(missing_ok=True)

lib = ctypes.CDLL(str(game / "engine" / "tools" / "scc_lib.dll"))
lib.scc_settings_new.restype = ctypes.c_void_p
lib.scc_settings_new.argtypes = [ctypes.c_char_p]
for name in ("scc_settings_set_custom_cache_file", "scc_settings_set_output_cache_file", "scc_settings_add_script_path"):
    getattr(lib, name).argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib.scc_settings_disable_error_popup.argtypes = [ctypes.c_void_p]
lib.scc_compile.restype = ctypes.c_void_p
lib.scc_compile.argtypes = [ctypes.c_void_p]
lib.scc_get_success.restype = ctypes.c_void_p
lib.scc_get_success.argtypes = [ctypes.c_void_p]
lib.scc_copy_error.restype = ctypes.c_size_t
lib.scc_copy_error.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
lib.scc_free_result.argtypes = [ctypes.c_void_p]

settings = lib.scc_settings_new(str(r6).encode())
lib.scc_settings_set_custom_cache_file(settings, str(blob).encode())
lib.scc_settings_set_output_cache_file(settings, str(out).encode())
lib.scc_settings_add_script_path(settings, str(scripts).encode())
for folder in more:
    lib.scc_settings_add_script_path(settings, str(folder).encode())
lib.scc_settings_disable_error_popup(settings)
result = lib.scc_compile(settings)
ok = bool(lib.scc_get_success(result)) and out.exists()
error = ctypes.create_string_buffer(65536)
size = lib.scc_copy_error(result, error, len(error))
lib.scc_free_result(result)
if ok:
    print("compiled")
else:
    print("failed")
    print(error.raw[:size].decode("utf-8", "replace"))
sys.exit(0 if ok else 1)
