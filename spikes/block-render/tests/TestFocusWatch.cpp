#include "Harness.h"

#include "input/FocusWatch.h"

using cpmc::input::FocusWatch;

TEST(FocusWatchLetsGoOnceOnLosingTheFront)
{
	FocusWatch watch;
	CHECK(!watch.LostFront(true));
	CHECK(watch.LostFront(false));
	CHECK(!watch.LostFront(false));  // still away: once is enough
	CHECK(!watch.LostFront(true));   // back
	CHECK(!watch.LostFront(true));
	CHECK(watch.LostFront(false));  // away again
}

TEST(FocusWatchLetsGoIfTheGameStartsBehind)
{
	FocusWatch watch;  // the plugin loaded while the user was in another window
	CHECK(watch.LostFront(false));
	CHECK(!watch.LostFront(false));
}
