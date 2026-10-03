#pragma once

#include "Coords.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// `drivelog <seconds>` (spike 5's step 2): build mode's drive, a row a frame for a few seconds, as CSV, so judder can be
// measured: where V's camera was, where the frame put it, the interpolator's delay and late ticks, and the teleport's
// time and turn. The frame log (`log`) records two frames of the renderer's work; this one records the drive. Pure: the
// plugin adds a row a frame, and writes the text once it's finished.
namespace cpmc
{
struct DriveRow
{
	double                time = 0.0;      // seconds, on any steady clock
	double                frameMs = 0.0;   // since the last frame
	const char*           state = "off";   // the drive's
	std::optional<Vec3d>  camera;          // V's camera this frame (world axes)
	std::optional<Vec3d>  target;          // where this frame puts V's camera (world axes)
	std::optional<double> lastDistance;    // V's camera from where the last frame put it, metres
	double                delayMs = 0.0;   // the interpolator's render delay
	bool                  waited = false;  // the frame used up the newest tick
	double                teleportMs = 0.0;
	int64_t               mouseX = 0;      // the mouse's counts along x this frame
	std::optional<double> yaw;             // the teleport's, degrees, turn included
	double                turn = 0.0;      // the degrees `set turn 1` added
};

class DriveLog
{
public:
	static constexpr size_t kMostRows = 30000;  // a minute at 500 frames a second

	struct Finished
	{
		std::string csv;
		size_t      rows = 0;
	};

	// Records aSeconds from the next row. False if one is already recording.
	bool Start(double aSeconds);
	bool Recording() const { return seconds_ > 0.0; }
	// Once a frame while recording. A row aSeconds or more after the first finishes it, and is left out.
	void Add(const DriveRow& aRow);
	// The recording, once, when it has finished: its time is up, or it's full.
	std::optional<Finished> TakeFinished();

private:
	void Finish();

	double                  seconds_ = 0.0;  // 0: not recording
	std::optional<double>   start_;
	std::string             csv_;
	size_t                  rows_ = 0;
	std::optional<Finished> finished_;
};
}
