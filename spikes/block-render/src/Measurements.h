#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

// What step 2 measures beyond the renderer's own figures (the spec's "Measurements"): latency, and bursts. The classes are
// pure, on times in seconds from any steady clock, and main thread only. The plugin and the renderer use SteadySeconds,
// which any thread may call.
namespace cpmc
{
// Seconds on the steady clock: the time base the plugin's measurements and the renderer's share.
inline double SteadySeconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// One typed command, or one click, timed.
struct LatencySample
{
	double minecraftMs = 0.0;  // the mark to the section's arrival: Minecraft's next tick, and for a command, its chat and
	                           // its server too
	double oursMs = 0.0;       // the section's arrival to the first list recorded that draws its upload
	double oursFrames = 0.0;   // the same, in the game's frames
};

// The spec's latency. A mark (a typed command's Enter, or a forwarded click) times the first section to arrive within a
// second after it, to the first list that draws its upload. A mark with no section within a second is dropped: a click
// that changed nothing, or a number key. Marks mean something only while nothing else changes sections, so they're
// taken while V stands still.
class LatencyTracker
{
public:
	void Mark(double aNow);  // replaces any mark still waiting for its section
	// A section arrived, aFrame being the game's frame count: true if it's the first within a second of a mark, and so
	// the one to time.
	bool SectionArrived(double aNow, uint64_t aFrame);
	// The section being timed was first drawn: its sample, kept as the last and in the average.
	std::optional<LatencySample> Drawn(double aNow, uint64_t aFrame);

	std::optional<LatencySample> Last() const { return last_; }
	std::optional<LatencySample> Average() const;
	size_t                       Samples() const { return samples_; }

private:
	std::optional<double>        mark_;
	std::optional<double>        timedMark_;  // the mark of the section on its way to being drawn
	double                       arrival_ = 0.0;
	uint64_t                     arrivalFrame_ = 0;
	std::optional<LatencySample> last_;
	LatencySample                sum_;
	size_t                       samples_ = 0;
};

// One frame's share of a burst.
struct BurstFrame
{
	uint64_t sections = 0;    // arrived this frame
	uint64_t bytes = 0;       // of the render ring, drained this frame
	double   drainMs = 0.0;
	double   updateMs = 0.0;  // the renderer's Update
	double   frameMs = 0.0;   // since the frame before
};

struct BurstReport
{
	std::string what;  // what started it
	uint64_t    frames = 0;
	uint64_t    sections = 0;
	uint64_t    bytes = 0;
	double      drainMsWorst = 0.0;
	double      updateMsWorst = 0.0;
	double      frameMsWorst = 0.0;
};

// The spec's bursts: an explosion, or a typed command's Enter, starts one, and the second after it is summed up. One at
// a time: a start while one is open is ignored.
class BurstWindow
{
public:
	void Start(double aNow, std::string aWhat);
	bool Open() const { return start_.has_value(); }
	// Each frame: counted while the burst's second lasts. The first frame after it closes the burst, and gets its report.
	std::optional<BurstReport> Frame(double aNow, const BurstFrame& aFrame);

private:
	std::optional<double> start_;
	BurstReport           report_;
};
}
