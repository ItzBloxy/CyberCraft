#include "Harness.h"

#include "track/ListWatch.h"

namespace
{
const void* List(uintptr_t aAddress)
{
	return reinterpret_cast<const void*>(aAddress);
}
}

TEST(ListWatchKeepsTheLastFewListsNamed)
{
	cpmc::ListWatch watch;
	CHECK_EQ(watch.Count(), size_t(0));
	CHECK(!watch.Contains(List(0x10)));
	CHECK(!watch.Contains(nullptr));
	watch.Add(List(0x10));
	watch.Add(List(0x20));
	watch.Add(List(0x10));  // already watched: nothing moves
	CHECK_EQ(watch.Count(), size_t(2));
	CHECK(watch.Contains(List(0x10)) && watch.Contains(List(0x20)));
	CHECK(!watch.Contains(nullptr));  // empty slots aren't lists
	watch.Add(List(0x30));
	watch.Add(List(0x40));
	watch.Add(List(0x50));  // a fifth: the oldest goes
	CHECK_EQ(watch.Count(), cpmc::ListWatch::kSize);
	CHECK(!watch.Contains(List(0x10)));
	CHECK(watch.Contains(List(0x50)));
	watch.Add(nullptr);  // ignored
	CHECK(watch.Contains(List(0x20)));
	watch.Clear();
	CHECK_EQ(watch.Count(), size_t(0));
	CHECK(!watch.Contains(List(0x50)));
}
