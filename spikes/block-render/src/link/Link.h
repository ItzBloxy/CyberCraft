#pragma once

#include "Coords.h"
#include "link/Commands.h"
#include "link/Drive.h"
#include "link/Ground.h"
#include "link/GroundWorker.h"
#include "link/Look.h"
#include "link/Messages.h"
#include "link/Overlay.h"
#include "link/Rings.h"
#include "link/Solids.h"
#include "link/Ticks.h"

#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

// The game side of SkyCraft's protocol, once a frame (the minecraft-link spec's "The link" and "Blocks"): the heartbeat
// and SkyState, teleports and the floor, typed commands, Minecraft's events, the render ring, and HUD images. Spike 5
// adds build mode's drive (its spec's "Part 1"): one teleport, then Minecraft drives and the link says where V goes;
// Night City's ground (its "Part 2"): the city's collision instead of the floor, read around the centre and sent as it
// changes; and Minecraft's solid blocks by section (its "Part 3"), for V's bodies. Spike 6 writes Night City's NPCs into
// SkyCraft's actor table, and passes on Minecraft's hits on them and its player's death. It works on the mapping's
// memory, so the tests give it plain memory with a fake Minecraft.
namespace cpmc::link
{
// What Minecraft gets as the game's shape (set ground).
enum class Ground
{
	Floor,  // spike 4's flat floor at V's feet
	City,   // Night City's collision, from a GroundSource
};

const char* GroundName(Ground aGround);  // "floor" or "city"

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
	Ground   ground = Ground::Floor;    // switching sends a clear with a new epoch, then the other ground
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
	virtual void HitActor(const proto::McEvent&) {}    // spike 6: Minecraft hit an NPC's stand-in this tick
	virtual void PlayerDied(const proto::McEvent&) {}  // spike 6: Minecraft's player died, killed by formId, or 0
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
	// aBase: the mapping's memory, laid out as skycraft_protocol.h has it. aFirstTeleportSeq and aGroundThreads, the
	// ground worker's threads: tests give their own.
	explicit Link(uint8_t* aBase, uint32_t aFirstTeleportSeq = FirstTeleportSeq(), unsigned aGroundThreads = GroundWorker::DefaultThreads());

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

	// Night City's ground, once a frame after Update (spike 5's "The exporter"): reads batches from aSource, nearest
	// first, until aBudgetMs has gone (a batch is always read whole), and hands the regions whose triangles changed to
	// the worker; the next updates write its messages into the collision ring, in order. It reads only with the ground
	// the city's, linked, Minecraft there, and the last update's centre known: Minecraft's player while it drives, and
	// V otherwise.
	void ReadGround(double aNow, GroundSource& aSource, double aBudgetMs);
	// The city read again from the start at the next update, as when the ground switches to it: a clear with a new epoch,
	// then every region afresh, for a change in what the source keeps (spike 6's follow-up, set bushrule). On the floor
	// nothing is sent, the city starting over anyway when the ground switches to it.
	void RestartCity() { cityRestart_ = true; }

	// Push may be called from any thread. Everything else, from the thread that calls Update.
	bool Type(std::string_view aCommandOrName, std::string& aError);  // queues a command, or a named set
	bool Push(const proto::InputEvent& aEvent);                        // the input ring; false if dropped
	void RequestFloor();                                               // a fresh floor at the next update
	// Spike 6: the NPCs' records (Actors.h) into SkyCraft's actor table, every frame they're listed; ClearActors empties
	// the table when listing stops, once.
	void WriteActors(std::span<const proto::ActorRecord> aRecords);
	void ClearActors();

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
		// The ground: the floor, or the city's (spike 5's "Part 2")
		Ground                   ground = Ground::Floor;  // what Minecraft gets now
		uint32_t                 collisionEpoch = 0;      // the collision ring's: the floor's or the city's last clear
		bool                     floorSent = false;
		Vec3d                    floorCentre;           // Minecraft axes; y is the floor's height
		std::optional<RegionPos> cityCentre;            // the region the city's area is around
		size_t                   cityKnown = 0;         // regions read since the city last started over
		size_t                   cityWaiting = 0;       // the area's batches not read yet
		uint64_t                 cityBatches = 0;       // batches read, since the start
		uint64_t                 cityUnread = 0;        // batches the source couldn't read
		uint64_t                 cityRegions = 0;       // regions sent to the worker: new, or changed
		uint64_t                 cityTriangles = 0;     // their triangles
		double                   cityReadMs = 0.0;      // the last ReadGround's
		size_t                   workerJobs = 0;        // regions given to the worker whose messages aren't taken yet
		unsigned                 workerThreads = 0;     // its threads, once it's made
		uint64_t                 workerRegions = 0;     // regions it voxelized, since the start
		uint64_t                 workerTriangles = 0;   // their triangles
		uint64_t                 workerBlocks = 0;      // voxel blocks it made
		uint64_t                 workerReplaced = 0;    // regions it was given again before it started them
		double                   workerMs = 0.0;        // its time, all threads together
		uint64_t                 workerCycles = 0;      // its threads' CPU cycles meanwhile, and the processor clock's:
		uint64_t                 workerTicks = 0;       // the share of that time they were running
		size_t                   collisionWaiting = 0;  // the city's messages waiting for room in the collision ring
		uint64_t                 collisionBytes = 0;    // the city's, written into it since the start
		uint64_t                 collisionDropped = 0;  // messages too big for it
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
		size_t                       solidSections = 0;  // Minecraft's sections with solid blocks, kept (spike 5's "Part 3")
		uint64_t                     solidBlocks = 0;    // and their solid blocks
		// Events, input, commands and the HUD
		uint64_t              explosions = 0;
		uint64_t              hitEvents = 0;    // spike 6: kEvHitActor
		uint64_t              deathEvents = 0;  // spike 6: kEvPlayerDied
		uint64_t              otherEvents = 0;
		uint64_t              inputsDropped = 0;
		size_t                commandsWaiting = 0;
		std::optional<double> lastEnter;  // when a typed command's Enter last went, on Frame::now's clock
		uint64_t              hudImages = 0;
		uint64_t              hudRefused = 0;
		// Build mode's drive (spike 5)
		uint32_t              minecraftFlags = 0;  // McState's: on ground, sneaking, sprinting, flying and the rest
		float                 minecraftTickMs = 0.0f;  // McState's: Minecraft's milliseconds a tick (spike 6's sums)
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
		std::optional<double> lastHold;             // seconds Minecraft held its player when build mode last started
		// The NPCs (spike 6)
		size_t actors = 0;  // records in SkyCraft's actor table now
	};

	const Status& State() const { return status_; }
	const Solids& SolidSections() const { return solids_; }  // Minecraft's solid blocks, by section

private:
	// A floor around aCentre (Minecraft axes; its y is the height). A new epoch clears the last one first; without one,
	// the floor grows, which never takes the ground from under Minecraft's player.
	void SendFloor(const Vec3d& aCentre, bool aNewEpoch, LinkSink& aSink);
	// The city's messages into the collision ring, in order, as many as fit: a clear first if one is due, then the
	// worker's, those of an older epoch dropped. The rest wait for the next frame.
	void WriteCity(LinkSink& aSink);
	bool Take(const RingMessage& aMessage, const LinkSettings& aSettings, LinkSink& aSink);

	uint8_t*              base_;
	OverlayReader         overlay_;
	CommandTyper          typer_;
	std::mutex            inputMutex_;
	std::atomic<uint64_t> inputsDropped_{ 0 };
	Status                status_;
	Solids                solids_;
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
	// The city's ground (spike 5's "Part 2")
	bool                              wasLinked_ = false;
	bool                              cityClear_ = false;  // a clear is due before the city's next regions
	bool                              cityRestart_ = false;  // RestartCity: the city starts over at the next update
	bool                              centred_ = false;    // the last update knew the city's centre, linked
	GroundAreas                       areas_;
	unsigned                          groundThreads_;
	std::unique_ptr<GroundWorker>     worker_;  // made when the city is first read
	std::optional<RegionPos>          focus_;   // the worker's, as last given it: the city's centre
	std::deque<GroundWorker::Message> outbox_;  // the city's messages, waiting for room in the collision ring
	bool                              actorsWritten_ = false;  // the actor table holds records ClearActors would empty
};
}
