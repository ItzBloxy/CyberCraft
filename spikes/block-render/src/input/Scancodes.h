#pragma once

#include <cstdint>

// Keys as Raw Input reports them, to the SDL scancodes (USB HID usages) SkyCraft's input ring takes (the spec's
// "input/"). Raw Input gives a key's set-1 scan code without its E0 prefix (RAWKEYBOARD's MakeCode), and the prefix as
// a flag (RI_KEY_E0). The table is SkyCraft's (skse/src/Input.cpp at bfcaf17, MIT, as extern/skycraft/LICENSE), which
// is indexed by DirectInput's key codes: an extended key's code is its scan code + 0x80.
namespace cpmc::input
{
// The key's SDL scancode, or 0 for a key SkyCraft's table doesn't have.
uint16_t SdlScancode(uint16_t aScanCode, bool aExtended);
}
