#include "Harness.h"

#include "physx/Session.h"

TEST(PhysxSessionSaysWhyWithoutTheGame)
{
	// The tests have no PhysX loaded: nothing is called, and the session says so.
	cpmc::px::Session session;
	std::string       why;
	CHECK(session.Scene0(why) == nullptr);
	CHECK_STR(why, "PhysX3_x64.dll isn't loaded");
}
