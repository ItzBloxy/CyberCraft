#include "Harness.h"

#include "physx/OriginWatch.h"

#include <cmath>

TEST(ControllerUnderVGivesTheOrigin)
{
	// Spot B in the results: V's feet in world coordinates, V's controller centre in PhysX coordinates.
	const cpmc::OriginReading reading = cpmc::OriginFromController({ -189.898, 616.754, 74.432 }, { -189.898, -407.246, 75.432 });
	CHECK_NEAR(reading.origin.x, 0.0, 1e-9);
	CHECK_NEAR(reading.origin.y, 1024.0, 1e-9);
	CHECK_NEAR(reading.origin.z, 0.0, 1e-9);
	CHECK_NEAR(reading.residual.x, 0.0, 1e-9);
	CHECK_NEAR(reading.residual.y, 0.0, 1e-9);
	CHECK_NEAR(reading.residual.z, 0.0, 1e-9);
}

TEST(OriginSnapsToTheGridAndKeepsTheRest)
{
	// A controller 5 cm off V and 20 cm higher than usual, with PhysX's origin a grid step west of zero.
	const cpmc::OriginReading reading = cpmc::OriginFromController({ -600.0, 10.0, 5.0 }, { 424.05, 9.97, 6.2 });
	CHECK_NEAR(reading.origin.x, -1024.0, 1e-9);
	CHECK_NEAR(reading.origin.y, 0.0, 1e-9);
	CHECK_NEAR(reading.origin.z, 0.0, 1e-9);
	CHECK(!std::signbit(reading.origin.z));  // 0, not -0, so logs read "0"
	CHECK_NEAR(reading.residual.x, -0.05, 1e-9);
	CHECK_NEAR(reading.residual.y, 0.03, 1e-9);
	CHECK_NEAR(reading.residual.z, -0.2, 1e-9);
}

TEST(OnlyAPersonSizedCapsuleUnderVFits)
{
	const cpmc::OriginReading under = cpmc::OriginFromController({ 0.0, 0.0, 0.0 }, { 0.1, -0.2, 0.6 });
	CHECK(cpmc::FitsController(under, 0.32f));
	CHECK(!cpmc::FitsController(under, 1.0f));  // the wide capsule that also surrounds V
	const cpmc::OriginReading beside = cpmc::OriginFromController({ 0.0, 0.0, 0.0 }, { 0.6, 0.0, 1.0 });
	CHECK(!cpmc::FitsController(beside, 0.32f));
	const cpmc::OriginReading below = cpmc::OriginFromController({ 0.0, 0.0, 0.0 }, { 0.0, 0.0, -0.5 });
	CHECK(!cpmc::FitsController(below, 0.32f));
}
