#include "ProcessUsage.h"

#include <psapi.h>

namespace cpmc
{
namespace
{
uint64_t Ticks(const FILETIME& aTime)
{
	return (uint64_t(aTime.dwHighDateTime) << 32) | aTime.dwLowDateTime;
}
}

std::optional<ProcessSample> SampleProcess(HANDLE aProcess, double aNow)
{
	FILETIME                   created{}, exited{}, kernel{}, user{};
	PROCESS_MEMORY_COUNTERS_EX memory{};
	if (!aProcess || !GetProcessTimes(aProcess, &created, &exited, &kernel, &user) ||
	    !GetProcessMemoryInfo(aProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof memory))
	{
		return std::nullopt;
	}
	return ProcessSample{ aNow, Ticks(kernel) + Ticks(user), memory.WorkingSetSize };
}

double CpuShare(const ProcessSample& aBefore, const ProcessSample& aAfter)
{
	const double seconds = aAfter.at - aBefore.at;
	if (seconds <= 0.0 || aAfter.cpu100ns < aBefore.cpu100ns)
	{
		return 0.0;
	}
	return double(aAfter.cpu100ns - aBefore.cpu100ns) / (seconds * 1e7);
}

ProcessWatch::~ProcessWatch()
{
	Close();
}

void ProcessWatch::Close()
{
	if (handle_)
	{
		CloseHandle(handle_);
		handle_ = nullptr;
	}
	last_.reset();
	cpuShare_ = 0.0;
}

void ProcessWatch::Sample(uint32_t aPid, double aNow)
{
	if (aPid != pid_)
	{
		Close();
		pid_ = aPid;
	}
	if (!handle_ && pid_ != 0)
	{
		handle_ = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid_);  // again each second until it opens
	}
	const std::optional<ProcessSample> sample = SampleProcess(handle_, aNow);
	cpuShare_ = sample && last_ ? cpmc::CpuShare(*last_, *sample) : 0.0;
	last_ = sample;
}
}
