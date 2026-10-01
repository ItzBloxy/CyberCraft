#include "OriginWatch.h"

#include <cmath>

namespace cpmc
{
namespace
{
// The nearest grid step. Adding 0 turns -0 into 0.
double Snap(double aValue)
{
	return std::round(aValue / kOriginGrid) * kOriginGrid + 0.0;
}
}

OriginReading OriginFromController(const Vec3d& aFeet, const Vec3d& aControllerCentre)
{
	const Vec3d raw{ aFeet.x - aControllerCentre.x, aFeet.y - aControllerCentre.y, aFeet.z + 1.0 - aControllerCentre.z };
	const Vec3d origin{ Snap(raw.x), Snap(raw.y), Snap(raw.z) };
	return { origin, { raw.x - origin.x, raw.y - origin.y, raw.z - origin.z } };
}

bool FitsController(const OriginReading& aReading, float aRadius)
{
	return aRadius <= 0.6f && std::hypot(aReading.residual.x, aReading.residual.y) <= 0.5 &&
	       std::fabs(aReading.residual.z) <= 1.0;
}
}
