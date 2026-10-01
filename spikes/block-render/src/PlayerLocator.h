#pragma once

#include "Coords.h"

#include <optional>

namespace cpmc
{
struct CameraPose
{
	Vec3d position;
	Vec3f forward;
	float fov = 0.0f;  // vertical, degrees
};

struct PlayerPose
{
	Vec3d                     feet;
	Vec3f                     forward;
	std::optional<CameraPose> camera;
};

// V's feet and facing, plus the active camera when it can be read, through the game's own script functions:
// PlayerSystem.GetLocalPlayerMainGameObject, then GetWorldPosition and GetWorldForward on V, and
// CameraSystem.GetActiveCameraWorldTransform, GetActiveCameraForward and GetActiveCameraFOV. Copied from spike 1.
// Nothing when there is no player (main menu, loading). Main thread only.
std::optional<PlayerPose> LocatePlayer();
}
