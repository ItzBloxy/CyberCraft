#include "Harness.h"

#include "log/EventNames.h"

#include <cstdint>
#include <cstring>
#include <string>

using cpmc::EventName;

TEST(EventNamesReadUnicodeAndAnsi)
{
	const wchar_t wide[] = L"GBuffer";
	CHECK_STR(EventName(0, wide, sizeof(wide)), "GBuffer");
	const char ansi[] = "Lighting";
	CHECK_STR(EventName(1, ansi, sizeof(ansi)), "Lighting");
	const wchar_t accented[] = L"Café";
	CHECK_STR(EventName(0, accented, sizeof(accented)), "Caf?");
}

TEST(EventNamesFindTextInBlobs)
{
	// A PIX 3 blob: 64-bit header and colour words, then the name packed into the next words.
	uint8_t blob[48]{};
	blob[0] = 0x01;
	blob[8] = 0xFF;
	std::memcpy(blob + 16, "DepthPrepass", 12);
	CHECK_STR(EventName(2, blob, sizeof(blob)), "DepthPrepass");

	uint8_t wide[32]{};
	const char name[] = "SSAO";
	for (size_t i = 0; i < 4; ++i)
	{
		wide[8 + 2 * i] = uint8_t(name[i]);
	}
	CHECK_STR(EventName(2, wide, sizeof(wide)), "SSAO");
}

TEST(EventNamesHandleNothingAndLongNames)
{
	CHECK_STR(EventName(0, nullptr, 10), "<event>");
	const uint8_t zeros[16]{};
	CHECK_STR(EventName(2, zeros, sizeof(zeros)), "<event>");
	const std::string longName(100, 'x');
	CHECK_EQ(EventName(1, longName.c_str(), uint32_t(longName.size() + 1)).size(), size_t(64));
}
