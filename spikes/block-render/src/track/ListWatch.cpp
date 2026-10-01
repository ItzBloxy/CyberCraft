#include "track/ListWatch.h"

namespace cpmc
{
void ListWatch::Add(const void* aList)
{
	if (!aList || Contains(aList))
	{
		return;
	}
	lists_[next_.fetch_add(1, std::memory_order_relaxed) % kSize].store(aList, std::memory_order_release);
}

void ListWatch::Clear()
{
	for (auto& list : lists_)
	{
		list.store(nullptr, std::memory_order_release);
	}
}

bool ListWatch::Contains(const void* aList) const
{
	for (const auto& list : lists_)
	{
		if (list.load(std::memory_order_acquire) == aList)
		{
			return aList != nullptr;
		}
	}
	return false;
}

size_t ListWatch::Count() const
{
	size_t count = 0;
	for (const auto& list : lists_)
	{
		count += list.load(std::memory_order_acquire) ? 1 : 0;
	}
	return count;
}
}
