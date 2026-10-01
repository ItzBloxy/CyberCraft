#include "Harness.h"

#include "track/ViewRegistry.h"

using cpmc::CopyRuns;
using cpmc::ViewInfo;
using cpmc::ViewRegistry;

namespace
{
ViewInfo Rtv(uintptr_t aResource, uint32_t aFormat)
{
	ViewInfo view;
	view.kind = ViewInfo::Kind::Rtv;
	view.resource = reinterpret_cast<void*>(aResource);
	view.viewFormat = aFormat;
	return view;
}
}

TEST(ViewRegistryRemembersViews)
{
	ViewRegistry registry;
	registry.SetIncrements(32, 8);
	registry.Record(1000, Rtv(0xA, 24));
	ViewInfo depth;
	depth.kind = ViewInfo::Kind::Dsv;
	depth.resource = reinterpret_cast<void*>(0xB);
	registry.Record(5000, depth);
	CHECK(registry.Find(1000)->resource == reinterpret_cast<void*>(0xA));
	CHECK_EQ(registry.Find(1000)->viewFormat, 24u);
	CHECK(registry.Find(5000)->kind == ViewInfo::Kind::Dsv);
	CHECK(!registry.Find(1032));
	CHECK_EQ(registry.Count(ViewInfo::Kind::Rtv), size_t(1));
	CHECK_EQ(registry.Count(ViewInfo::Kind::Dsv), size_t(1));
	CHECK_EQ(registry.Increment(ViewInfo::Kind::Dsv), 8u);
}

TEST(ViewRegistryFollowsCopiedDescriptors)
{
	ViewRegistry registry;
	registry.SetIncrements(32, 8);
	registry.Record(1000, Rtv(0xA, 24));
	registry.Record(1032, Rtv(0xB, 10));
	registry.Record(2064, Rtv(0xC, 2));  // the source at 1064 has no known view, so this one goes
	registry.Copy(ViewInfo::Kind::Rtv, 2000, 1000, 3);
	CHECK(registry.Find(2000)->resource == reinterpret_cast<void*>(0xA));
	CHECK(registry.Find(2032)->resource == reinterpret_cast<void*>(0xB));
	CHECK(!registry.Find(2064));
	CHECK(registry.Find(1000).has_value());  // the source keeps its view
}

TEST(ViewRegistryCopiesOverlappingRanges)
{
	ViewRegistry registry;
	registry.SetIncrements(32, 8);
	registry.Record(1000, Rtv(0xA, 1));
	registry.Record(1032, Rtv(0xB, 1));
	registry.Copy(ViewInfo::Kind::Rtv, 1032, 1000, 2);
	CHECK(registry.Find(1032)->resource == reinterpret_cast<void*>(0xA));
	CHECK(registry.Find(1064)->resource == reinterpret_cast<void*>(0xB));
}

TEST(CopyRunsLinesUpUnevenRanges)
{
	// Destinations: 3 at 100, 1 at 500. Sources: 1 at 900, 3 at 1300. Step 10.
	const uint64_t destStarts[] = { 100, 500 };
	const uint32_t destSizes[] = { 3, 1 };
	const uint64_t sourceStarts[] = { 900, 1300 };
	const uint32_t sourceSizes[] = { 1, 3 };
	const auto     runs = CopyRuns(2, destStarts, destSizes, 2, sourceStarts, sourceSizes, 10);
	CHECK_EQ(runs.size(), size_t(3));
	CHECK_EQ(runs[0].dest, uint64_t(100));
	CHECK_EQ(runs[0].source, uint64_t(900));
	CHECK_EQ(runs[0].count, 1u);
	CHECK_EQ(runs[1].dest, uint64_t(110));
	CHECK_EQ(runs[1].source, uint64_t(1300));
	CHECK_EQ(runs[1].count, 2u);
	CHECK_EQ(runs[2].dest, uint64_t(500));
	CHECK_EQ(runs[2].source, uint64_t(1320));
	CHECK_EQ(runs[2].count, 1u);
}

TEST(CopyRunsTakesNullSizesAsOnes)
{
	const uint64_t destStarts[] = { 100, 200 };
	const uint64_t sourceStarts[] = { 300, 400 };
	const auto     runs = CopyRuns(2, destStarts, nullptr, 2, sourceStarts, nullptr, 10);
	CHECK_EQ(runs.size(), size_t(2));
	CHECK_EQ(runs[1].dest, uint64_t(200));
	CHECK_EQ(runs[1].source, uint64_t(400));
	CHECK(CopyRuns(1, nullptr, nullptr, 1, sourceStarts, nullptr, 10).empty());
}
