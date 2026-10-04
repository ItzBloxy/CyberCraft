#pragma once

#include "Coords.h"

// PhysX's origin, read from V's character controller. In the city the game keeps PhysX's origin on a 1,024 m grid
// near V, so PhysX coordinates = world - origin (see docs/spikes/2026-10-01-physx-probe-results.md).
namespace cpmc
{
constexpr double kOriginGrid = 1024.0;

struct OriginReading
{
	Vec3d origin;    // PhysX's origin in world coordinates: a whole number of grid steps on each axis
	Vec3d residual;  // what the grid leaves over: 0 when the controller sits exactly 1 m above V's feet
};

// V's controller centre (PhysX coordinates) sits 1 m above V's feet (world coordinates), so the origin is
// feet + (0, 0, 1) - centre, rounded to the grid on each axis.
OriginReading OriginFromController(const Vec3d& aFeet, const Vec3d& aControllerCentre);

// Whether a capsule could be V's controller: no wider than a person, and under V give or take the grid. It may be up
// to 0.5 m off across and 1 m up or down, which allows for crouching and a frame of lag.
bool FitsController(const OriginReading& aReading, float aRadius);
}
