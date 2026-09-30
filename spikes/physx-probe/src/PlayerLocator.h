#pragma once

#include "Report.h"

#include <optional>

namespace cpmc
{
// V's feet and facing, plus the active camera when it can be read, through the game's own script functions:
// PlayerSystem.GetLocalPlayerMainGameObject, then GetWorldPosition and GetWorldForward on V, and
// CameraSystem.GetActiveCameraWorldTransform / GetActiveCameraForward / GetActiveCameraFOV.
// Nothing when there is no player (main menu, loading). Main thread only.
std::optional<PlayerPose> LocatePlayer();
}
