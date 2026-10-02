#include "link/Look.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cpmc::link
{
namespace
{
constexpr double kDegrees = 180.0 / std::numbers::pi;
}

std::optional<McLook> LookFromForward(const Vec3f& aForward, float aLastYaw)
{
	const Vec3d  mc = McFromWorld({ aForward.x, aForward.y, aForward.z });
	const double length = std::sqrt(mc.x * mc.x + mc.y * mc.y + mc.z * mc.z);
	if (length < 1e-9)
	{
		return std::nullopt;
	}
	const double x = mc.x / length;
	const double y = mc.y / length;
	const double z = mc.z / length;
	float        yaw = aLastYaw;
	if (std::sqrt(x * x + z * z) >= 0.001)
	{
		// The turn from the last yaw, within 180 degrees either way (a half turn goes up).
		double turn = std::fmod(std::atan2(-x, z) * kDegrees - aLastYaw, 360.0);
		if (turn > 180.0)
		{
			turn -= 360.0;
		}
		else if (turn <= -180.0)
		{
			turn += 360.0;
		}
		yaw = float(aLastYaw + turn);
	}
	return McLook{ yaw, float(-std::asin(std::clamp(y, -1.0, 1.0)) * kDegrees) };
}

Vec3d TeleportTarget(const Vec3d& aCamera)
{
	const Vec3d eye = McFromWorld(aCamera);
	return { eye.x, eye.y - kEyeHeight, eye.z };
}

Vec3f ForwardFromLook(const McLook& aLook)
{
	const double yaw = aLook.yaw / kDegrees;
	const double pitch = aLook.pitch / kDegrees;
	const Vec3d  world = WorldFromMc({ -std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch) });
	return { float(world.x), float(world.y), float(world.z) };
}
}
