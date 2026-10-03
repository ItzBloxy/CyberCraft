#pragma once

#include "Coords.h"
#include "link/Ticks.h"

#include <cstdint>
#include <optional>
#include <string>

// Build mode's drive (the spec's "Part 1"): V placed where Minecraft's player is. Build mode starts with one teleport
// of Minecraft's player to V's feet; Minecraft holds its player until the ground around it has arrived, and meanwhile V
// is held where it is. Once Minecraft lets go, each frame V's camera goes to Minecraft's eye: V's feet under it, by V's
// own eye height. Pure: the plugin teleports V, and mclink moves its stand-in V, wherever it says.
namespace cpmc::link
{
enum class DriveState
{
	Off,
	Waiting,  // Minecraft holds its player: V is held where it is
	Driving,  // V follows Minecraft's player
};

const char* DriveStateName(DriveState aState);  // "off", "waiting", "driving"

// V's eye height: the camera's height above V's feet, averaged over frames where V's feet don't move up or down, so a
// jump, or a camera and feet read either side of a teleport, doesn't count. It's measured outside build mode only, and
// V can't crouch in build mode, so it holds while Minecraft drives. Spike 4's measure, 1.60 m, until a frame is taken.
class EyeHeight
{
public:
	void   Note(const Vec3d& aFeet, const Vec3d& aCamera);  // world axes
	double Value() const { return value_; }

private:
	std::optional<double> lastFeetZ_;
	bool                  measured_ = false;
	double                value_ = 1.60;
};

// Where V's feet go (world axes) so that V's camera, aEyeHeight above them, is at Minecraft's eye. Horizontally, they
// go straight under it.
Vec3d FeetUnderEye(const TickSample& aMinecraft, double aEyeHeight);

class Drive
{
public:
	static constexpr double kFarthest = 20.0;  // metres from V: a target farther away isn't used
	static constexpr double kBadFor = 1.0;     // seconds of targets not used that end build mode

	// Build mode started with V's feet at aFeet (world axes): V is held there until Minecraft lets go.
	void Enter(const Vec3d& aFeet, double aNow);
	void Leave();

	struct Step
	{
		std::optional<Vec3d> place;  // where V's feet go this frame (world axes); nothing: V is left as it is
		std::string          end;    // why build mode must end; empty: it goes on
	};

	// Once a frame while build mode is on. aAcked: Minecraft's teleportAck equals our teleportSeq, so it has let its
	// player go. aMinecraft: where its player is, interpolated, if it has ticked. aFeet: V's feet now (world axes).
	Step Update(double aNow, bool aAcked, const std::optional<TickSample>& aMinecraft, const Vec3d& aFeet, double aEyeHeight);

	DriveState           State() const { return state_; }
	double               Since() const { return since_; }      // when it entered its state
	uint64_t             Skipped() const { return skipped_; }  // frames whose target wasn't used, since the start
	std::optional<Vec3d> Eye() const { return eye_; }          // Minecraft's eye (world axes), the last frame it drove

private:
	DriveState            state_ = DriveState::Off;
	double                since_ = 0.0;
	Vec3d                 hold_;
	std::optional<double> badSince_;
	uint64_t              skipped_ = 0;
	std::optional<Vec3d>  eye_;
};
}
