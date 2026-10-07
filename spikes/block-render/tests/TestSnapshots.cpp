#include "Harness.h"

#include "render/Snapshots.h"

using cpmc::Snapshots;

TEST(SnapshotsNothingToTakeAtFirst)
{
	Snapshots snapshots(3);
	CHECK(!snapshots.Newest().has_value());
	CHECK(!snapshots.TakeNewest().has_value());
	CHECK_EQ(snapshots.Writable(0).value_or(9), 0u);
}

TEST(SnapshotsTheNewestIsNeverWrittenOver)
{
	Snapshots snapshots(3);
	snapshots.Written(0);
	CHECK_EQ(snapshots.Newest().value_or(9), 0u);
	CHECK_EQ(snapshots.Writable(0).value_or(9), 1u);  // not 0: a list may take it any moment
	CHECK_EQ(snapshots.TakeNewest().value_or(9), 0u);
}

TEST(SnapshotsHeldUntilTheFenceAfterTheList)
{
	Snapshots snapshots(2);
	snapshots.Written(0);
	CHECK_EQ(snapshots.TakeNewest().value_or(9), 0u);  // a G-buffer list reads it
	snapshots.Written(1);                               // the next frame
	CHECK(!snapshots.Writable(100).has_value());        // 0 is on its way to the GPU, and 1 is the newest
	snapshots.Submitted(0, 7);                          // the fence after the list
	CHECK(!snapshots.Writable(6).has_value());          // the GPU may still be reading it
	CHECK_EQ(snapshots.Writable(7).value_or(9), 0u);
}

TEST(SnapshotsTakenTwiceWaitForBoth)
{
	Snapshots snapshots(2);
	snapshots.Written(0);
	snapshots.TakeNewest();  // two frames' lists, with no newer frame between them
	snapshots.TakeNewest();
	snapshots.Written(1);
	snapshots.Submitted(0, 5);
	CHECK(!snapshots.Writable(9).has_value());  // the second list is still on its way
	snapshots.Submitted(0, 8);
	CHECK(!snapshots.Writable(7).has_value());
	CHECK_EQ(snapshots.Writable(8).value_or(9), 0u);
}

TEST(SnapshotsHeldForTheFramesForwardList)
{
	Snapshots snapshots(2);
	snapshots.Written(0);
	CHECK_EQ(snapshots.TakeNewest().value_or(9), 0u);  // the frame's G-buffer list
	CHECK(snapshots.Take(0));                          // and, for the same frame, its forward list
	CHECK(!snapshots.Take(2));                         // no such snapshot
	snapshots.Written(1);                              // the next frame
	snapshots.Submitted(0, 5);                         // the G-buffer list went
	CHECK(!snapshots.Writable(5).has_value());         // the forward list hasn't: it would read the next frame's
	snapshots.Submitted(0, 8);                         // it went, the fence after it 8
	CHECK(!snapshots.Writable(7).has_value());
	CHECK_EQ(snapshots.Writable(8).value_or(9), 0u);
	// A forward list that doesn't come holds it no longer than its G-buffer list.
	snapshots.Written(0);
	snapshots.TakeNewest();
	snapshots.Take(0);
	snapshots.Written(1);
	snapshots.Submitted(0, 12);
	snapshots.Abandon(0);
	CHECK_EQ(snapshots.Writable(12).value_or(9), 0u);
}

TEST(SnapshotsAbandonedListHoldsNothing)
{
	Snapshots snapshots(2);
	snapshots.Written(0);
	snapshots.TakeNewest();
	snapshots.Written(1);
	CHECK(!snapshots.Writable(0).has_value());
	snapshots.Abandon(0);  // the list wasn't recorded after all
	CHECK_EQ(snapshots.Writable(0).value_or(9), 0u);
}
