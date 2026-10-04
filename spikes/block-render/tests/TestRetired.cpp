#include "Harness.h"

#include "render/Retired.h"

#include <memory>

using namespace cpmc;

namespace
{
// A stand-in for a GPU buffer: the test watches when the last reference goes.
using Buffer = std::shared_ptr<int>;
}

TEST(RetiredFreesOnceTheGpuHasPassedTheLastSignal)
{
	Retired<Buffer> retired;
	Buffer          buffer = std::make_shared<int>(1);
	std::weak_ptr   watch = buffer;
	CHECK(!retired.Retire(std::move(buffer), 5));  // every list that drew it was signalled, the last with 5
	retired.Free(4);
	CHECK(!watch.expired());
	retired.Free(5);
	CHECK(watch.expired());
	CHECK_EQ(retired.Size(), size_t(0));
}

TEST(RetiredWaitsForTheListOnItsWayToTheGpu)
{
	// The crash at Task 52's step 14: a list of ours drew a section, and before its fence was signalled the section was
	// replaced. The old buffer must outlive that list, not just the fence signalled before it.
	Retired<Buffer> retired;
	Buffer          buffer = std::make_shared<int>(1);
	std::weak_ptr   watch = buffer;
	retired.ListRecorded();
	CHECK(retired.Retire(std::move(buffer), 5));  // 5 was signalled before the list
	retired.Free(5);
	CHECK(!watch.expired());
	retired.ListSubmitted(7);  // the upload's signal took 6
	retired.Free(6);
	CHECK(!watch.expired());
	retired.Free(7);
	CHECK(watch.expired());
	CHECK_EQ(retired.Waited(), uint64_t(1));
}

TEST(RetiredWaitsForEveryListOnItsWay)
{
	Retired<Buffer> retired;
	Buffer          buffer = std::make_shared<int>(1);
	std::weak_ptr   watch = buffer;
	retired.ListRecorded();
	retired.ListRecorded();
	retired.Retire(std::move(buffer), 3);
	retired.ListSubmitted(4);  // the second list still may draw it
	retired.Free(4);
	CHECK(!watch.expired());
	retired.ListSubmitted(5);
	retired.Free(5);
	CHECK(watch.expired());
}

TEST(RetiredKeepsWhatOnlySubmittedWorkUses)
{
	// Staging buffers, which only the upload list reads: their fence is the upload's, whatever is on its way.
	Retired<Buffer> retired;
	Buffer          buffer = std::make_shared<int>(1);
	std::weak_ptr   watch = buffer;
	retired.ListRecorded();
	retired.RetireAfter(std::move(buffer), 6);
	retired.Free(6);
	CHECK(watch.expired());
	CHECK_EQ(retired.Waited(), uint64_t(0));
	retired.ListSubmitted(7);
	retired.Clear();
	CHECK_EQ(retired.Size(), size_t(0));
}
