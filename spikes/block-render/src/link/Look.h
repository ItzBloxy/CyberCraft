#pragma once

#include "Coords.h"

#include <optional>

// Where Minecraft's player stands and looks, from V's camera (the spec's "The link"): its eye at the camera, looking
// the camera's way.
namespace cpmc::link
{
constexpr double kEyeHeight = 1.62;  // Minecraft's standing eye height, in blocks

// Minecraft's degrees: yaw 0 faces south (+z), 90 west, 180 north, -90 east; pitch is positive looking down.
struct McLook
{
	float yaw = 0.0f;
	float pitch = 0.0f;
};

// From a forward vector in the game's world axes, any length. Looking straight up or down (a horizontal part under
// 0.001 of its length), the yaw is aLastYaw. Nothing for a zero vector. The yaw comes out in (-180, 180].
std::optional<McLook> LookFromForward(const Vec3f& aForward, float aLastYaw);

// Where Minecraft's player's feet go (Minecraft axes) so that its eye is at aCamera (world axes).
Vec3d TeleportTarget(const Vec3d& aCamera);

// The forward vector, in the game's world axes, of a Minecraft look.
Vec3f ForwardFromLook(const McLook& aLook);
}
