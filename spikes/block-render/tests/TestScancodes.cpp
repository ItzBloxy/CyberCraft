#include "Harness.h"

#include "input/Scancodes.h"

using cpmc::input::SdlScancode;

TEST(ScancodesMapTheKeysBuildModeForwards)
{
	for (uint16_t key = 1; key <= 9; ++key)
	{
		CHECK_EQ(SdlScancode(uint16_t(0x01 + key), false), uint16_t(29 + key));  // 1 to 9: SDL 30 to 38
	}
	CHECK_EQ(SdlScancode(0x0B, false), uint16_t(39));  // 0
	CHECK_EQ(SdlScancode(0x14, false), uint16_t(23));  // T, as Commands types it
	CHECK_EQ(SdlScancode(0x1C, false), uint16_t(40));  // Enter
	CHECK_EQ(SdlScancode(0x1E, false), uint16_t(4));   // A
	CHECK_EQ(SdlScancode(0x01, false), uint16_t(41));  // Esc
	CHECK_EQ(SdlScancode(0x42, false), uint16_t(65));  // F8
	CHECK_EQ(SdlScancode(0x57, false), uint16_t(68));  // F11
}

TEST(ScancodesTellExtendedKeysApart)
{
	CHECK_EQ(SdlScancode(0x1D, false), uint16_t(224));  // left Ctrl
	CHECK_EQ(SdlScancode(0x1D, true), uint16_t(228));   // right Ctrl
	CHECK_EQ(SdlScancode(0x1C, true), uint16_t(88));    // the keypad's Enter
	CHECK_EQ(SdlScancode(0x48, false), uint16_t(96));   // the keypad's 8
	CHECK_EQ(SdlScancode(0x48, true), uint16_t(82));    // Up
	CHECK_EQ(SdlScancode(0x2A, true), uint16_t(0));     // the fake Shift Windows sends around some extended keys
	CHECK_EQ(SdlScancode(0x59, false), uint16_t(0));    // not in the table
	CHECK_EQ(SdlScancode(0x9D, false), uint16_t(0));    // past set 1's keys: not the right Ctrl
	CHECK_EQ(SdlScancode(0xFF, true), uint16_t(0));     // a keyboard overrun
}
