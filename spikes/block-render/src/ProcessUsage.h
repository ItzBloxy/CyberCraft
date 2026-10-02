#pragma once

#include <Windows.h>

#include <cstdint>
#include <optional>

// A process's CPU and memory, for the status (the spec's "Measurements": Cyberpunk's, and Minecraft's from mcPid).
namespace cpmc
{
struct ProcessSample
{
	double   at = 0.0;        // seconds, on the caller's steady clock
	uint64_t cpu100ns = 0;    // kernel and user time so far, in 100 ns units
	uint64_t workingSet = 0;  // bytes
};

// Nothing if it can't be read. aProcess needs PROCESS_QUERY_LIMITED_INFORMATION and PROCESS_VM_READ.
std::optional<ProcessSample> SampleProcess(HANDLE aProcess, double aNow);

// The CPU used between two samples, as a share of one core: 1 is a whole core busy all the time.
double CpuShare(const ProcessSample& aBefore, const ProcessSample& aAfter);

// One process, followed by its ID: sampled once a second, and opened again whenever the ID changes.
class ProcessWatch
{
public:
	ProcessWatch() = default;
	ProcessWatch(const ProcessWatch&) = delete;
	ProcessWatch& operator=(const ProcessWatch&) = delete;
	~ProcessWatch();

	void     Sample(uint32_t aPid, double aNow);  // aPid 0: none
	bool     Known() const { return last_.has_value(); }  // the last sample read the process
	double   CpuShare() const { return cpuShare_; }       // since the sample before; 0 until there are two
	uint64_t WorkingSet() const { return last_ ? last_->workingSet : 0; }

private:
	void Close();

	HANDLE                       handle_ = nullptr;
	uint32_t                     pid_ = 0;
	std::optional<ProcessSample> last_;
	double                       cpuShare_ = 0.0;
};
}
