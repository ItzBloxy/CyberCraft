#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace cpmc
{
// The command lists tracked even while tracking is off: drawing before DLSS needs the DLSS evaluate's lists' first
// transitions, and nothing else. The game alternates between two such lists, so the last few named are kept. One
// thread names them (the evaluate's); any thread may ask, with a few atomic reads and no lock.
class ListWatch
{
public:
	static constexpr size_t kSize = 4;

	void   Add(const void* aList);  // a list already watched stays where it is
	void   Clear();
	bool   Contains(const void* aList) const;
	size_t Count() const;

private:
	std::array<std::atomic<const void*>, kSize> lists_{};
	std::atomic<size_t>                         next_{ 0 };
};
}
