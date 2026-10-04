#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

// What the renderer no longer uses but the GPU may still read, kept until the GPU has passed a fence signalled after
// the last of our lists that drew it. A list of ours recorded on a render thread reaches the GPU before the fence after
// it is signalled, and the main thread's upload may retire what it draws in between: that waits for the fence after
// the list, not the one before it (Task 52's crash). Pure; the renderer locks around it.
namespace cpmc
{
template <typename T>
class Retired
{
public:
	// A list of ours that draws what's current is recorded, on its way to the GPU.
	void ListRecorded() { ++onTheirWay_; }

	// It was submitted, and aFence signalled after it: what waited for the lists on their way waits for aFence.
	void ListSubmitted(uint64_t aFence)
	{
		if (onTheirWay_ == 0 || --onTheirWay_ > 0)
		{
			return;
		}
		for (T& item : waiting_)
		{
			kept_.emplace_back(std::move(item), aFence);
		}
		waiting_.clear();
	}

	// No longer used. Every list of ours that drew it had a fence signalled after it, the last aLastSignalled, unless
	// one is on its way to the GPU: then it waits for that one's, and this says true.
	bool Retire(T aItem, uint64_t aLastSignalled)
	{
		if (onTheirWay_ > 0)
		{
			waiting_.push_back(std::move(aItem));
			++waited_;
			return true;
		}
		kept_.emplace_back(std::move(aItem), aLastSignalled);
		return false;
	}

	// Used only by work already submitted, with aFence signalled after it, such as an upload's staging buffers.
	void RetireAfter(T aItem, uint64_t aFence) { kept_.emplace_back(std::move(aItem), aFence); }

	// Drops what the GPU has finished with.
	void Free(uint64_t aCompleted)
	{
		std::erase_if(kept_, [aCompleted](const auto& aEntry) { return aEntry.second <= aCompleted; });
	}

	void Clear()
	{
		kept_.clear();
		waiting_.clear();
		onTheirWay_ = 0;
	}

	size_t   Size() const { return kept_.size() + waiting_.size(); }
	uint64_t Waited() const { return waited_; }  // retired while a list of ours was on its way, since the start

private:
	std::vector<std::pair<T, uint64_t>> kept_;  // and the fence the GPU must pass first
	std::vector<T>                      waiting_;
	uint32_t                            onTheirWay_ = 0;
	uint64_t                            waited_ = 0;
};
}
