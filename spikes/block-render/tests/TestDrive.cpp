#include "Harness.h"

#include "link/Drive.h"

#include <limits>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// Minecraft's player at spike 3's corner, aEast blocks east: feet at Minecraft -119.5 6.14 2007.5, the eye aEyeHeight up.
TickSample Standing(double aEast = 0.0, double aEyeHeight = 1.62)
{
	TickSample sample;
	sample.feet = { -119.5 + aEast, 6.14, 2007.5 };
	sample.eyeHeight = aEyeHeight;
	return sample;
}

const Vec3d kVFeet{ -119.5, -2007.5, 6.14 };  // world axes: the same spot
}

TEST(DrivePutsVsCameraOnMinecraftsEye)
{
	// V's eye height 1.60, Minecraft's 1.62: V's feet go 2 cm above Minecraft's, in world axes.
	const Vec3d feet = FeetUnderEye(Standing(), 1.60);
	CHECK_NEAR(feet.x, -119.5, 1e-9);
	CHECK_NEAR(feet.y, -2007.5, 1e-9);
	CHECK_NEAR(feet.z, 6.16, 1e-9);
	// Sneaking: Minecraft's eye drops to 1.27, and V's feet a third of a metre into the floor.
	CHECK_NEAR(FeetUnderEye(Standing(0.0, 1.27), 1.60).z, 5.81, 1e-9);
}

TEST(DriveDoesNothingWhileOff)
{
	Drive      drive;
	const auto step = drive.Update(1.0, true, Standing(), kVFeet, 1.60);
	CHECK(!step.place.has_value());
	CHECK(step.end.empty());
	CHECK(drive.State() == DriveState::Off);
	CHECK_STR(DriveStateName(drive.State()), "off");
}

TEST(DriveHoldsVWhileMinecraftWaits)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	CHECK(drive.State() == DriveState::Waiting);
	// Minecraft hasn't let go yet: V stays where build mode started, though a key held from before walked it off.
	const auto step = drive.Update(1.1, false, Standing(3.0), { -118.0, -2007.5, 6.14 }, 1.60);
	CHECK(step.place.has_value());
	CHECK_NEAR(step.place->x, -119.5, 1e-9);
	CHECK(drive.State() == DriveState::Waiting);
	CHECK_NEAR(drive.Since(), 1.0, 1e-9);
	CHECK_STR(DriveStateName(drive.State()), "waiting");
}

TEST(DriveFollowsMinecraftOnceItLetsGo)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	const auto step = drive.Update(1.2, true, Standing(0.5), kVFeet, 1.60);
	CHECK(drive.State() == DriveState::Driving);
	CHECK_STR(DriveStateName(drive.State()), "driving");
	CHECK_NEAR(drive.Since(), 1.2, 1e-9);
	CHECK(step.place.has_value());
	CHECK_NEAR(step.place->x, -119.0, 1e-9);
	CHECK_NEAR(step.place->z, 6.16, 1e-9);
	CHECK(drive.Eye().has_value());
	CHECK_NEAR(drive.Eye()->z, 7.76, 1e-9);
}

TEST(DriveWaitsAgainWhenMinecraftTeleportsByItself)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	drive.Update(1.2, true, Standing(0.5), kVFeet, 1.60);
	// The acknowledgement falls behind: Minecraft holds its player again, wherever it teleported it. V stays put.
	const Vec3d now{ -119.0, -2007.5, 6.16 };
	const auto  step = drive.Update(1.3, false, Standing(9.0), now, 1.60);
	CHECK(drive.State() == DriveState::Waiting);
	CHECK_NEAR(step.place->x, -119.0, 1e-9);
	CHECK_NEAR(step.place->z, 6.16, 1e-9);
}

TEST(DriveSkipsTargetsTooFarAndEndsAfterASecond)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	drive.Update(1.1, true, Standing(), kVFeet, 1.60);
	auto step = drive.Update(1.2, true, Standing(25.0), kVFeet, 1.60);  // 25 m from V
	CHECK(!step.place.has_value());
	CHECK(step.end.empty());
	CHECK_EQ(drive.Skipped(), 1ull);
	step = drive.Update(2.1, true, Standing(25.0), kVFeet, 1.60);
	CHECK(step.end.empty());  // 0.9 s of bad targets
	step = drive.Update(2.2, true, Standing(25.0), kVFeet, 1.60);
	CHECK_STR(step.end, "Minecraft's player stayed over 20 m from V for a second");
	CHECK_EQ(drive.Skipped(), 3ull);
	// A good target in between starts the second over.
	Drive other;
	other.Enter(kVFeet, 1.0);
	other.Update(1.1, true, Standing(), kVFeet, 1.60);
	other.Update(1.2, true, Standing(25.0), kVFeet, 1.60);
	other.Update(1.9, true, Standing(), kVFeet, 1.60);
	CHECK(other.Update(2.5, true, Standing(25.0), kVFeet, 1.60).end.empty());
}

TEST(DriveSkipsTargetsThatArentThere)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	drive.Update(1.1, true, Standing(), kVFeet, 1.60);
	TickSample nowhere = Standing();
	nowhere.feet.x = std::numeric_limits<double>::quiet_NaN();
	CHECK(!drive.Update(1.2, true, nowhere, kVFeet, 1.60).place.has_value());
	CHECK(!drive.Update(1.3, true, std::nullopt, kVFeet, 1.60).place.has_value());
	CHECK_EQ(drive.Skipped(), 2ull);
	CHECK_STR(drive.Update(2.3, true, std::nullopt, kVFeet, 1.60).end, "Minecraft's player had no usable position for a second");
}

TEST(DriveLeaves)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	drive.Update(1.1, true, Standing(), kVFeet, 1.60);
	drive.Leave();
	CHECK(drive.State() == DriveState::Off);
	CHECK(!drive.Update(1.2, true, Standing(), kVFeet, 1.60).place.has_value());
}

TEST(EyeHeightStartsAtSpike4sMeasure)
{
	EyeHeight eye;
	CHECK_NEAR(eye.Value(), 1.60, 1e-9);
}

TEST(EyeHeightAveragesOnlyWhileTheFeetStayLevel)
{
	EyeHeight eye;
	eye.Note({ 0.0, 0.0, 6.0 }, { 0.0, 0.0, 7.62 });  // the first frame: nothing to compare the feet with
	CHECK_NEAR(eye.Value(), 1.60, 1e-9);
	eye.Note({ 0.1, 0.0, 6.0 }, { 0.1, 0.0, 7.62 });  // level: the first sample is taken as it is
	CHECK_NEAR(eye.Value(), 1.62, 1e-9);
	eye.Note({ 0.2, 0.0, 6.3 }, { 0.2, 0.0, 7.5 });  // the feet went up 0.3 m: a jump, or a read across a teleport
	CHECK_NEAR(eye.Value(), 1.62, 1e-9);
	eye.Note({ 0.2, 0.0, 6.3 }, { 0.2, 0.0, 7.82 });  // level again, at 1.52: a tenth of the way there
	CHECK_NEAR(eye.Value(), 1.61, 1e-9);
	eye.Note({ 0.2, 0.0, 6.3 }, { 0.2, 0.0, 16.3 });  // a camera 10 m up isn't V's eye
	CHECK_NEAR(eye.Value(), 1.61, 1e-9);
}
