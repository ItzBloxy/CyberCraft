#pragma once

#include <cstdint>
#include <deque>
#include <optional>

// The turn `set turn 1` adds to each teleport's yaw in build mode (spike 5's "Each frame while Minecraft drives"): if a
// teleport with V's own yaw loses the frame's turn from the mouse, this adds it back. The mouse's raw counts become
// degrees by a ratio measured outside build mode while V turns: V's turn over a turn of the mouse, against the counts the
// input patch saw. A turn ends once the mouse has been still for a few frames, since the game may take a frame's counts
// in the next one. Each turn gives a ratio, and the median of the last few is used, so a menu's cursor or a turn the
// game made by itself doesn't move it. Pure.
namespace cpmc::link
{
class TurnRatio
{
public:
	static constexpr int    kStill = 5;        // frames without movement that end a turn
	static constexpr int    kLongest = 120;    // frames that end a turn anyway
	static constexpr double kFewest = 100.0;   // counts a turn needs to count
	static constexpr double kSmallest = 1.0;   // degrees V must turn in it
	static constexpr size_t kKnown = 5;        // turns before the ratio is used
	static constexpr size_t kKept = 21;        // the last turns the median is taken from

	// Once a frame outside build mode while V can be placed: the mouse's counts along x since the last frame, and V's
	// yaw now, in degrees.
	void Note(int64_t aCounts, double aYaw);
	// Any other frame: V's yaw may change by itself (a vehicle, a scene, a load, build mode's teleports), so the turn
	// under way is dropped, and the yaw starts afresh.
	void Break();

	std::optional<double> Ratio() const;                 // degrees a count, once kKnown turns have counted
	double                Turn(int64_t aCounts) const;   // aCounts in degrees; 0 while the ratio isn't known
	uint64_t              Turns() const { return turns_; }  // turns that counted, since the start

private:
	void EndTurn();

	std::optional<double> lastYaw_;
	double                counts_ = 0.0;  // the turn under way
	double                degrees_ = 0.0;
	int                   frames_ = 0;
	int                   still_ = 0;
	std::deque<double>    ratios_;  // the last kKept turns'
	uint64_t              turns_ = 0;
};
}
