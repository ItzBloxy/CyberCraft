#include "Harness.h"

#include "ProcessUsage.h"

TEST(ProcessUsageSamplesThisProcess)
{
	const auto sample = cpmc::SampleProcess(GetCurrentProcess(), 1.0);
	CHECK(sample.has_value());
	CHECK(sample->workingSet > 1'000'000);  // the test program takes more than a megabyte
	CHECK_EQ(sample->at, 1.0);
}

TEST(ProcessUsageCpuIsAShareOfOneCore)
{
	const cpmc::ProcessSample before{ 10.0, 0, 0 };
	const cpmc::ProcessSample after{ 12.0, 10'000'000, 0 };  // 1 s of CPU in 2 s
	CHECK_NEAR(cpmc::CpuShare(before, after), 0.5, 1e-9);
	const cpmc::ProcessSample busy{ 12.0, 40'000'000, 0 };  // 4 s of CPU in 2 s: two cores
	CHECK_NEAR(cpmc::CpuShare(before, busy), 2.0, 1e-9);
	CHECK_NEAR(cpmc::CpuShare(after, after), 0.0, 1e-9);  // no time between them
}

TEST(ProcessWatchFollowsAProcessById)
{
	cpmc::ProcessWatch watch;
	CHECK(!watch.Known());
	watch.Sample(GetCurrentProcessId(), 1.0);
	CHECK(watch.Known());
	CHECK(watch.WorkingSet() > 1'000'000);
	CHECK_NEAR(watch.CpuShare(), 0.0, 1e-9);  // one sample: no share yet
	watch.Sample(GetCurrentProcessId(), 2.0);
	CHECK(watch.CpuShare() >= 0.0);
	watch.Sample(0, 3.0);  // none
	CHECK(!watch.Known());
	watch.Sample(0xFFFFFFF0, 4.0);  // no process has it
	CHECK(!watch.Known());
}
