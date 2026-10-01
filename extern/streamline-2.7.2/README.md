# NVIDIA Streamline 2.7.2 public headers

Copied unmodified from https://github.com/NVIDIA-RTX/Streamline at tag v2.7.2: the eight headers in `include/` that
`sl_core_api.h` needs, and `license.txt` (MIT). Cyberpunk 2077 2.31 ships Streamline 2.7.1 (`sl.interposer.dll`,
FileVersion 2.7.1.0), and 2.7.2 is the nearest public tag.

BlockRender uses only their types and links nothing from Streamline. `spikes/block-render/tests/TestSl.cpp` pins the
offset of every field the plugin reads, and the plugin checks each struct's type GUID and version before reading it.
