#include "log/EventNames.h"

#include <cstring>

namespace cpmc
{
namespace
{
constexpr size_t   kMaxBytes = 512;
constexpr size_t   kMaxChars = 64;
constexpr uint32_t kUnicode = 0;
constexpr uint32_t kAnsi = 1;

bool Printable(uint32_t aChar)
{
	return aChar >= 0x20 && aChar < 0x7F;
}

void Append(std::string& aOut, uint32_t aChar)
{
	if (aOut.size() < kMaxChars)
	{
		aOut.push_back(Printable(aChar) ? char(aChar) : '?');
	}
}

// Runs of at least three printable characters, read with aStride bytes per character (1, or 2 for UTF-16 whose
// high bytes are zero), joined by spaces.
std::string Runs(const uint8_t* aBytes, size_t aSize, size_t aStride)
{
	std::string out;
	std::string run;
	const auto  flush = [&] {
		if (run.size() >= 3)
		{
			if (!out.empty())
			{
				out.push_back(' ');
			}
			out += run;
		}
		run.clear();
	};
	for (size_t i = 0; i + aStride <= aSize; i += aStride)
	{
		const bool wide = aStride == 2;
		if (Printable(aBytes[i]) && (!wide || aBytes[i + 1] == 0))
		{
			run.push_back(char(aBytes[i]));
		}
		else
		{
			flush();
		}
	}
	flush();
	return out.substr(0, kMaxChars);
}
}

std::string EventName(uint32_t aMetadata, const void* aData, uint32_t aSize)
{
	if (!aData || aSize == 0)
	{
		return "<event>";
	}
	const auto*  bytes = static_cast<const uint8_t*>(aData);
	const size_t size = aSize < kMaxBytes ? aSize : kMaxBytes;
	std::string  out;
	if (aMetadata == kUnicode)
	{
		for (size_t i = 0; i + 1 < size; i += 2)
		{
			const uint32_t ch = uint32_t(bytes[i]) | (uint32_t(bytes[i + 1]) << 8);
			if (ch == 0)
			{
				break;
			}
			Append(out, ch);
		}
	}
	else if (aMetadata == kAnsi)
	{
		for (size_t i = 0; i < size && bytes[i] != 0; ++i)
		{
			Append(out, bytes[i]);
		}
	}
	else
	{
		out = Runs(bytes, size, 1);
		if (out.empty())
		{
			out = Runs(bytes, size, 2);
		}
	}
	return out.empty() ? "<event>" : out;
}
}
