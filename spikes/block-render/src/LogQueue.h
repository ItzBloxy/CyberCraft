#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace cpmc
{
// Log lines from any thread, written later by the main thread (only the main thread writes to RED4ext's log). Holds
// at most aCapacity lines; further lines are counted and dropped until the queue is drained.
class LogQueue
{
public:
	explicit LogQueue(size_t aCapacity = 2000);

	void Push(std::string aLine);

	// Every queued line, oldest first, then a line saying how many were dropped, if any were.
	std::vector<std::string> Drain();

private:
	std::mutex               mutex_;
	std::vector<std::string> lines_;
	size_t                   capacity_;
	size_t                   dropped_ = 0;
};
}
