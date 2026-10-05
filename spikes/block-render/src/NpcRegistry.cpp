#include "NpcRegistry.h"

#include <algorithm>
#include <format>

namespace cpmc
{
NpcRegistry::ThreadCalls& NpcRegistry::CallsOn(uint32_t aThread)
{
	const auto found = std::find_if(threads_.begin(), threads_.end(), [&](const ThreadCalls& aCalls) { return aCalls.thread == aThread; });
	if (found != threads_.end())
	{
		return *found;
	}
	if (threads_.size() + 1 >= kMaxThreads)
	{
		// The last entry gathers every further thread's calls, as thread 0.
		if (threads_.size() + 1 == kMaxThreads)
		{
			threads_.push_back({});
		}
		return threads_.back();
	}
	threads_.push_back({ aThread, 0, 0 });
	return threads_.back();
}

void NpcRegistry::Attached(uint64_t aId, uint32_t aThread)
{
	const std::lock_guard lock(mutex_);
	++CallsOn(aThread).attached;
	++counts_.attached;
	if (std::find(ids_.begin(), ids_.end(), aId) != ids_.end())
	{
		++counts_.repeats;
	}
	else if (ids_.size() >= kMaxIds)
	{
		++counts_.full;
	}
	else
	{
		ids_.push_back(aId);
	}
}

void NpcRegistry::Detached(uint64_t aId, uint32_t aThread)
{
	const std::lock_guard lock(mutex_);
	++CallsOn(aThread).detached;
	++counts_.detached;
	const auto found = std::find(ids_.begin(), ids_.end(), aId);
	if (found == ids_.end())
	{
		++counts_.strays;
		return;
	}
	ids_.erase(found);  // the others keep their order
}

int32_t NpcRegistry::Copy()
{
	const std::lock_guard lock(mutex_);
	copy_ = ids_;
	return int32_t(copy_.size());
}

uint64_t NpcRegistry::At(int32_t aIndex) const
{
	const std::lock_guard lock(mutex_);
	return aIndex >= 0 && size_t(aIndex) < copy_.size() ? copy_[size_t(aIndex)] : 0;
}

NpcRegistry::Counts NpcRegistry::Count() const
{
	const std::lock_guard lock(mutex_);
	Counts counts = counts_;
	counts.ids = ids_.size();
	return counts;
}

std::vector<NpcRegistry::ThreadCalls> NpcRegistry::Threads() const
{
	const std::lock_guard lock(mutex_);
	return threads_;
}

std::string DescribeThreads(const std::vector<NpcRegistry::ThreadCalls>& aThreads, uint32_t aMainThread)
{
	std::string text;
	for (const NpcRegistry::ThreadCalls& calls : aThreads)
	{
		const std::string thread = calls.thread == 0 ? std::string("other threads")
		                           : calls.thread == aMainThread ? std::format("{}, the main thread", calls.thread)
		                                                         : std::to_string(calls.thread);
		text += std::format("{}{}: {} attached, {} detached", text.empty() ? "" : "; ", thread, calls.attached, calls.detached);
	}
	return text.empty() ? "none" : text;
}
}
