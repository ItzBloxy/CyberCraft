#include "Harness.h"

#include "input/Text.h"

using namespace cpmc::input;

TEST(TextTakesWholeCodePoints)
{
	Utf16Text text;
	CHECK_EQ(text.Take(u'a').value_or(0), uint32_t('a'));
	CHECK_EQ(text.Take(0x00E6).value_or(0), uint32_t(0xE6));  // æ, typed on the user's Danish layout
	CHECK_EQ(text.Take(0x20AC).value_or(0), uint32_t(0x20AC));  // €, with AltGr
	CHECK(!text.Take(0xD83D).has_value());                     // 🙂's first half waits for its second
	CHECK_EQ(text.Take(0xDE42).value_or(0), uint32_t(0x1F642));
	CHECK_EQ(text.Refused(), 0ull);
}

TEST(TextRefusesLoneSurrogates)
{
	Utf16Text text;
	CHECK(!text.Take(0xDE42).has_value());  // a second half with no first
	CHECK_EQ(text.Refused(), 1ull);
	CHECK(!text.Take(0xD83D).has_value());
	CHECK_EQ(text.Take(u'b').value_or(0), uint32_t('b'));  // a first half with no second: it's refused, and b goes on
	CHECK_EQ(text.Refused(), 2ull);
	CHECK(!text.Take(0xD83D).has_value());
	CHECK(!text.Take(0xD83D).has_value());  // a first half after a first half: the earlier is refused
	CHECK_EQ(text.Refused(), 3ull);
	CHECK_EQ(text.Take(0xDE00).value_or(0), uint32_t(0x1F600));
}

TEST(TextKnowsWhatMinecraftTypes)
{
	CHECK(Printable(' '));
	CHECK(Printable('a'));
	CHECK(Printable(0xE6));
	CHECK(Printable(0x1F642));
	CHECK(!Printable(0x08));  // Backspace, a key to Minecraft
	CHECK(!Printable(0x0D));  // Enter
	CHECK(!Printable(0x16));  // Ctrl+V
	CHECK(!Printable(0x1B));  // Esc
	CHECK(!Printable(0x1F));
	CHECK(!Printable(0x7F));  // Ctrl+Backspace
}
