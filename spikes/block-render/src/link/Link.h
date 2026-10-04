#pragma once

#include "Coords.h"
#include "link/Commands.h"
#include "link/Drive.h"
#include "link/Look.h"
#include "link/Messages.h"
#include "link/Overlay.h"
#include "link/Rings.h"
#include "link/Ticks.h"

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

// The game side of SkyCraft's protocol, once a frame (the minecraft-link spec's "The link" and "Blocks"): the heartbeat
// and SkyState, teleports and the floor, typed commands, Minecraft's events, the render ring, and HUD images. Spike 5
// adds build mode's drive (its spec's "Part 1"): one teleport, then Minecraft drives and the link says where V goes.
// It works on the mapping's memory, so the tests give it plain memory with a fake Minecraft.
namespace cpmc::link
{
// V's pose in the game's world axes (metres: x east, y north, z up), as PlayerLocator finds it.
struct GamePose
{
	Vec3d feet;
	Vec3d camera;
	Vec3f forward;  // the camera's
};

struct LinkSettings
{
	double   teleportStep = 0.1;        // metres the camera moves before Minecraft's player follows
	double   teleportRate = 10.0;       // teleports a second, at most
	uint64_t ringBudget = 16ull << 20;  // ring bytes of atlases and sections taken each frame, checked between messages
	bool     keepTranslucent = false;   // draw translucent triangles as cut-outs instead of dropping them
	uint32_t viewportWidth = 0;         // the HUD image's size to ask for; 0 leaves Minecraft's window alone
	uint32_t viewportHeight = 0;
};

// Where the link hands on what Minecraft sends: the renderer in the game, a report in mclink. Called inside Update,
// with payloads that point into the mapping and stay valid only during the call.
class LinkSink
{
public:
	virtual ~LinkSink() = default;
	virtual void Atlas(const AtlasImage& aAtlas) = 0;
	virtual void Section(SectionMesh&& aMesh) = 0;
	virtual void ClearAll() = 0;
	virtual void Explosion(const proto::McEvent&) {}
	virtual void Hud(const HudFrame&) {}
	virtual void Log(const std::string&) {}
};

// A render message type's name, for reports: "section", "atlas patch" and so on, and "other" from Link::kTypes - 1 on.
const char* RenderTypeName(size_t aType);

// Where a run's teleportSeq starts: from the clock, and odd, as SkyCraft's Skyrim side starts its own. A Minecraft that
// kept running keeps the last value it applied and teleports only on a change, so a run must not repeat it.
uint32_t FirstTeleportSeq();

class Link
{
public:
	// aBase: the mapping's memory, laid out as skycraft_protocol.h has it. aFirstTeleportSeq: tests give their own.
	explicit Link(uint8_t* aBase, uint32_t aFirstTeleportSeq = FirstTeleportSeq());

	struct Frame
	{
		double                  now = 0.0;          // seconds, on any steady clock
		uint64_t                tickMs = 0;         // GetTickCount64(), the clock both heartbeats use
		int64_t                 qpc = 0;            // QueryPerformanceCounter now: the clock Minecraft stamps its ticks with
		int64_t                 qpcPerSecond = 0;   // its rate; 0: no ticks are interpolated
		std::optional<GamePose> pose;               // nothing: V isn't there
		bool                    linked = false;     // the user's Link or Unlink
		bool                    buildMode = false;  // Minecraft drives (spike 5); off sets SkyState's menu flag
		bool                    placeable = true;   // nothing keeps V from being placed (a vehicle, a workspot, death),
		                                            // so V's eye height can be measured
		bool                    takeHud = false;    // hand on HUD images this frame
	};

	void Update(const Frame& aFrame, const LinkSettings& aSettings, LinkSink& aSink);

	// Push may be called from any thread. Everything else, from the thread that calls Update.
	bool Type(std::string_view aCommandOrName, std::string& aError);  // queues a command, or a named set
	bool Push(const proto::InputEvent& aEvent);                        // the input ring; false if dropped
	void RequestFloor();                                               // a fresh floor at the next update

	static constexpr size_t kTypes = 13;  // the render ring's message types, 0 to 11, and 12 for any other

	struct Status
	{
		// Minecraft, as its heartbeat and McState say
		bool     minecraft = false;  // its heartbeat is under 3 s old
		uint32_t minecraftPid = 0;
		uint64_t heartbeatAgeMs = 0;
		bool     inWorld = false;
		bool     screenOpen = false;
		Vec3d    minecraftFeet;        // Minecraft axes
		Vec3d    minecraftEye;         // Minecraft axes
		uint64_t minecraftFrames = 0;  // its frame counter
		// The game side
		bool     inGame = false;
		uint32_t teleportSeq = 0;
		uint64_t teleports = 0;
		Vec3d    target;  // the last teleport's, Minecraft axes
		McLook   look;
		uint32_t floorEpoch = 0;
		bool     floorSent = false;
		Vec3d    floorCentre;  // Minecraft axes; y is the floor's height
		// The render ring, since the start
		std::array<uint64_t, kTypes> messages{};
		std::array<uint64_t, kTypes> bytes{};
		uint64_t                     drainedBytes = 0;
		double                       drainMs = 0.0;  // the last update's drain
		uint64_t                     resyncs = 0;
		std::string                  lastResync;
		uint64_t                     rejected = 0;  // messages whose payload failed its layout's checks
		std::string                  lastRejected;
		bool                         atlasSeen = false;
		uint64_t                     sectionsBeforeAtlas = 0;
		uint64_t                     translucentDropped = 0;
		// Events, input, commands and the HUD
		uint64_t              explosions = 0;
		uint64_t              otherEvents = 0;
		uint64_t              inputsDropped = 0;
		size_t                commandsWaiting = 0;
		std::optional<double> lastEnter;  // when a typed command's Enter last went, on Frame::now's clock
		uint64_t              hudImages = 0;
		uint64_t              hudRefused = 0;
		// Build mode's drive (spike 5)
		uint32_t              minecraftFlags = 0;  // McState's: on ground, sneaking, sprinting, flying and the rest
		uint32_t              teleportAck = 0;     // the last teleportSeq Minecraft applied, less 1 while it holds its player
		DriveState            drive = DriveState::Off;
		double                driveSince = 0.0;     // when the drive entered its state, on Frame::now's clock
		uint64_t              driveSkipped = 0;     // frames whose target wasn't used, since the start
		double                eyeHeight = 1.60;     // V's, measured outside build mode
		double                renderDelayMs = 0.0;  // how far in the past Minecraft's ticks are interpolated
		uint64_t              waitedFrames = 0;     // frames, while driving, that used up the newest tick, since the start
		std::optional<double> eyeDistance;          // V's camera from where the last frame put Minecraft's eye, metres
		std::optional<Vec3d>  placeV;               // this frame: where V's feet go (world axes); nothing: V is left be
		std::string           endBuild;             // this frame: why build mode must end; empty: it goes on
	};

	const Status& State() const { return status_; }

private:
	// A floor around aCentre (Minecraft axes; its y is the height). A new epoch clears the last one first; without one,
	// the floor grows, which never takes the ground from under Minecraft's player.
	void SendFloor(const Vec3d& aCentre, bool aNewEpoch, LinkSink& aSink);
	bool Take(const RingMessage& aMessage, const LinkSettings& aSettings, LinkSink& aSink);

	uint8_t*              base_;
	OverlayReader         overlay_;
	CommandTyper          typer_;
	std::mutex            inputMutex_;
	std::atomic<uint64_t> inputsDropped_{ 0 };
	Status                status_;
	bool                  wasThere_ = false;
	uint32_t              lastPid_ = 0;
	bool                  wasInGame_ = false;
	bool                  teleportDue_ = true;
	double                lastTeleportAt_ = -1e9;
	bool                  floorRequested_ = false;
	uint64_t              problemsLogged_ = 0;  // resyncs and rejected messages already logged
	double                problemLoggedAt_ = -1e9;
	std::string           lastProblem_;
	TickInterpolator      ticks_;
	Drive                 drive_;
	EyeHeight             eyeHeight_;
	bool                  wasBuild_ = false;
	std::optional<Vec3d>  lastEye_;  // where the last frame put Minecraft's eye, world axes
};
}
