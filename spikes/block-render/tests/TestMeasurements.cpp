#include "Harness.h"

#include "Measurements.h"

using namespace cpmc;

TEST(LatencyTimesTheFirstSectionAfterAMark)
{
	LatencyTracker latency;
	CHECK(!latency.SectionArrived(1.0, 10));  // no mark: not timed
	latency.Mark(2.0);
	CHECK(latency.SectionArrived(2.12, 20));
	CHECK(!latency.SectionArrived(2.13, 20));  // only the first
	const auto sample = latency.Drawn(2.15, 22);
	CHECK(sample.has_value());
	CHECK_NEAR(sample->minecraftMs, 120.0, 1e-6);
	CHECK_NEAR(sample->oursMs, 30.0, 1e-6);
	CHECK_NEAR(sample->oursFrames, 2.0, 1e-9);
	CHECK(!latency.Drawn(2.2, 23).has_value());  // once
	CHECK_EQ(latency.Samples(), size_t(1));
}

TEST(LatencyDropsAMarkWithNoSectionWithinASecond)
{
	LatencyTracker latency;
	latency.Mark(1.0);
	CHECK(!latency.SectionArrived(2.5, 30));  // a click that changed nothing, or a number key
	CHECK(!latency.Drawn(2.6, 31).has_value());
	CHECK_EQ(latency.Samples(), size_t(0));
	CHECK(!latency.Last().has_value());
}

TEST(LatencyKeepsTheLastAndTheAverage)
{
	LatencyTracker latency;
	latency.Mark(1.0);
	latency.SectionArrived(1.1, 0);
	latency.Drawn(1.12, 1);
	latency.Mark(5.0);
	latency.SectionArrived(5.3, 10);
	latency.Drawn(5.34, 13);
	CHECK_NEAR(latency.Last()->minecraftMs, 300.0, 1e-6);
	CHECK_NEAR(latency.Average()->minecraftMs, 200.0, 1e-6);
	CHECK_NEAR(latency.Average()->oursMs, 30.0, 1e-6);
	CHECK_NEAR(latency.Average()->oursFrames, 2.0, 1e-9);
}

TEST(BurstsCoverTheSecondAfterTheirStart)
{
	BurstWindow     burst;
	const BurstFrame quiet{ 5, 1'000'000, 0.5, 2.0, 16.0 };
	const BurstFrame busy{ 20, 4'000'000, 1.5, 6.0, 30.0 };
	const BurstFrame after{ 1, 0, 0.1, 0.2, 15.0 };
	CHECK(!burst.Open());
	CHECK(!burst.Frame(9.0, busy).has_value());  // no burst: nothing counted
	burst.Start(10.0, "explosion");
	CHECK(burst.Open());
	burst.Start(10.2, "typed: /fill");  // one at a time
	CHECK(!burst.Frame(10.1, quiet).has_value());
	CHECK(!burst.Frame(10.5, busy).has_value());
	const auto report = burst.Frame(11.05, after);  // its second is up: this frame isn't counted
	CHECK(report.has_value());
	CHECK_STR(report->what, "explosion");
	CHECK_EQ(report->sections, 25ull);
	CHECK_EQ(report->bytes, 5'000'000ull);
	CHECK_EQ(report->frames, 2ull);
	CHECK_NEAR(report->drainMsWorst, 1.5, 1e-9);
	CHECK_NEAR(report->updateMsWorst, 6.0, 1e-9);
	CHECK_NEAR(report->frameMsWorst, 30.0, 1e-9);
	CHECK(!burst.Open());
}
