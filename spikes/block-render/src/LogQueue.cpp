#include "LogQueue.h"

#include <format>

namespace cpmc
{
LogQueue::LogQueue(size_t aCapacity)
	: capacity_(aCapacity)
{
}

void LogQueue::Push(std::string aLine)
{
	std::lock_guard lock(mutex_);
	if (lines_.size() >= capacity_)
	{
		++dropped_;
		return;
	}
	lines_.push_back(std::move(aLine));
}

std::vector<std::string> LogQueue::Drain()
{
	std::lock_guard          lock(mutex_);
	std::vector<std::string> lines;
	lines.swap(lines_);
	if (dropped_ > 0)
	{
		lines.push_back(std::format("({} more log lines were dropped)", dropped_));
		dropped_ = 0;
	}
	return lines;
}
}
