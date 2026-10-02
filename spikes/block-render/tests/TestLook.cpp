#include "Harness.h"

#include "link/Look.h"

using namespace cpmc;
using namespace cpmc::link;

TEST(LookCompassDirections)
{
	// World axes: x east, y north, z up. Minecraft's yaw: 0 south, 90 west, 180 north, -90 east.
	CHECK_NEAR(LookFromForward({ 0, 1, 0 }, 0)->yaw, 180.0, 1e-4);
	CHECK_NEAR(LookFromForward({ 1, 0, 0 }, 0)->yaw, -90.0, 1e-4);
	CHECK_NEAR(LookFromForward({ 0, -1, 0 }, 0)->yaw, 0.0, 1e-4);
	CHECK_NEAR(LookFromForward({ -1, 0, 0 }, 0)->yaw, 90.0, 1e-4);
	CHECK_NEAR(LookFromForward({ 0, 1, 0 }, 0)->pitch, 0.0, 1e-4);
}

TEST(LookPitch)
{
	CHECK_NEAR(LookFromForward({ 0, 0.7071068f, -0.7071068f }, 0)->pitch, 45.0, 1e-3);  // down, toward the north
	CHECK_NEAR(LookFromForward({ 0, 0.7071068f, -0.7071068f }, 0)->yaw, 180.0, 1e-3);
	CHECK_NEAR(LookFromForward({ 0, 0, 2 }, 0)->pitch, -90.0, 1e-4);  // any length
}

TEST(LookStraightUpOrDownKeepsTheYaw)
{
	CHECK_NEAR(LookFromForward({ 0, 0, 1 }, 37.0f)->yaw, 37.0, 1e-6);
	CHECK_NEAR(LookFromForward({ 0, 0, -1 }, -12.0f)->yaw, -12.0, 1e-6);
	CHECK_NEAR(LookFromForward({ 0, 0, -1 }, -12.0f)->pitch, 90.0, 1e-4);
	CHECK(!LookFromForward({ 0, 0, 0 }, 0).has_value());
}

TEST(LookTeleportTargetPutsTheEyeAtTheCamera)
{
	// V's camera at spike 3's corner, 1.62 m above the road at z 6.14.
	const Vec3d feet = TeleportTarget({ -119.5, -2007.5, 7.76 });
	CHECK_NEAR(feet.x, -119.5, 1e-9);
	CHECK_NEAR(feet.y, 6.14, 1e-9);
	CHECK_NEAR(feet.z, 2007.5, 1e-9);
}

TEST(LookForwardRoundTrip)
{
	const auto look = LookFromForward(ForwardFromLook({ 30.0f, -20.0f }), 0);
	CHECK_NEAR(look->yaw, 30.0, 1e-3);
	CHECK_NEAR(look->pitch, -20.0, 1e-3);
}
