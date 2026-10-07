#pragma once

#include <cstdint>
#include <optional>

// Typed text (spike 8's "Typed text"): WM_CHAR's UTF-16 code units made into code points, and which of those Minecraft
// gets as text. Pure, and not thread-safe: the router holds its lock around every call.
namespace cpmc::input
{
// Whether Minecraft gets aPoint as text: nothing below 0x20, and not 0x7F. Backspace, Enter, Tab, Esc and Ctrl's letters
// reach Minecraft as keys, as SDL gives them.
bool Printable(uint32_t aPoint);

class Utf16Text
{
public:
	// One code unit: the code point it completes, or nothing while a surrogate pair's first half waits for its second.
	// A lone half is refused: a second half with no first, or a first half followed by anything but a second.
	std::optional<uint32_t> Take(uint16_t aUnit);
	uint64_t                Refused() const { return refused_; }

private:
	std::optional<uint16_t> high_;  // a first half, waiting
	uint64_t                refused_ = 0;
};
}
