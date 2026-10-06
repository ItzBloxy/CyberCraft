#pragma once

#include "Coords.h"
#include "Measurements.h"
#include "link/Ground.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpmc
{
// What status.txt says (the spec's "Status and the overlay"). The plugin writes it once a second into the overlay's
// folder; tools/blockrender_hud/init.lua shows it.
// Spike 4's lines (the spec's "Measurements"): the link, build mode, the render ring, Minecraft's atlas, the HUD, latency,
// the last burst, and both games' processes. "In the last second" is from one status to the next.
struct LinkReport
{
	std::string          mapping = "not opened yet";  // created, reused, or refused and why
	bool                 linked = false;
	bool                 minecraft = false;  // its heartbeat is under 3 s old
	uint32_t             minecraftPid = 0;
	uint64_t             heartbeatAgeMs = 0;
	bool                 inWorld = false;
	bool                 screenOpen = false;
	double               minecraftFps = 0.0;  // its frame counter's, over the last second
	bool                 inGame = false;      // linked, with V found
	Vec3d                minecraftEye;        // Minecraft axes
	std::optional<Vec3d> camera;              // V's, Minecraft axes; none while V isn't found
	uint64_t             teleports = 0;       // in the last second
	Vec3d                target;              // the last teleport's, Minecraft axes
	uint32_t             floorEpoch = 0;
	bool                 floorSent = false;
	Vec3d                floorCentre;  // Minecraft axes; y is the floor's height
	// Build mode, and the GetRawInputData patch; counts since the start
	bool               inputHooked = false;
	bool               buildMode = false;
	std::string        buildWhy;             // why it's off
	std::optional<int> slot;                 // Minecraft's hotbar slot, once a number key has set it
	uint64_t           inputsRead = 0;       // keyboard and mouse events
	uint64_t           inputsForwarded = 0;  // to Minecraft
	uint64_t           inputsHidden = 0;     // from the game
	uint64_t           inputsDropped = 0;    // with the input ring full
	std::string        lastInput;            // the last forwarded
	// Build mode's drive (spike 5's "Status")
	std::string           drive = "off";             // off, waiting or driving
	double                driveFor = 0.0;            // seconds in that state
	double                eyeHeight = 1.60;          // V's, metres, measured outside build mode
	std::optional<double> eyeDistanceLast;           // V's camera from where the last frame put Minecraft's eye; none:
	double                eyeDistanceAverage = 0.0;  // not driving. Metres, in the last second
	double                eyeDistanceWorst = 0.0;
	double                renderDelayMs = 0.0;       // the tick interpolator's
	uint64_t              waitedFrames = 0;          // frames that used up the newest tick, in the last second
	uint64_t              skippedFrames = 0;         // bad targets, since the start
	double                teleportMsAverage = -1.0;  // Teleport's call, in the last second; below 0: not called
	double                teleportMsWorst = -1.0;
	std::optional<double> health;                    // V's, percent; none: not read
	bool                  turn = false;              // set turn 1: the mouse's turn is added to the teleport's yaw
	std::optional<double> turnRatio;                 // degrees a count, once measured or remembered
	bool                  turnRemembered = false;    // turnRatio is the last session's, from turn.txt
	uint64_t              turnTurns = 0;             // turns measured
	uint32_t              minecraftFlags = 0;        // McState's
	// V's meshes in build mode (spike 6's "Part 2")
	bool    hideV = true;     // set hidev 1: hidden while build mode is on
	bool    vHidden = false;  // they are now
	int32_t vMeshes = 0;      // as the module's HideV last said: the meshes it switched off; -1 no Codeware; -2 V isn't found
	int32_t vEffects = 0;     // and the effects it has stopped since V was hidden
	int32_t vCrosshair = 0;   // and the game's crosshair: 1 hidden; -1 not found among the HUD layer's game controllers
	// The ground (spike 5's "Part 2"): figures a second are over the last second
	std::string                    ground = "floor";          // what Minecraft gets as the game's shape
	std::string                    groundWhy;                 // why it fell back to the floor, if it did
	uint32_t                       collisionEpoch = 0;
	std::optional<link::RegionPos> cityCentre;                // the region the city's area is around
	size_t                         cityKnown = 0;             // regions read around it
	size_t                         cityWaiting = 0;           // the area's batches not read yet
	double                         cityReadMsAverage = 0.0;   // reading, a frame
	double                         cityReadMsWorst = 0.0;
	double                         cityBatches = 0.0;         // a second
	double                         cityTriangles = 0.0;       // a second, filed into regions sent
	double                         workerBlocks = 0.0;        // voxel blocks made, a second
	double                         workerMsPerRegion = -1.0;  // below 0: no region voxelized in the last second
	double                         workerTrianglesPerRegion = 0.0;  // of those regions
	double                         workerRunning = 0.0;       // the share of that time its threads were running
	unsigned                       workerThreads = 0;         // none: it hasn't started
	size_t                         workerJobs = 0;            // regions waiting for it
	uint64_t                       workerReplaced = 0;        // regions given again before it started them, since the start
	double                         collisionMb = 0.0;         // the city's, into the collision ring, a second
	size_t                         collisionWaiting = 0;      // messages waiting for room
	uint64_t                       collisionDropped = 0;      // too big for it, since the start
	std::optional<double>          lastHold;                  // seconds Minecraft held its player when build mode last started
	std::optional<Vec3d>           physxOrigin;               // the last PhysX origin read, world axes
	uint64_t                       controllerSearches = 0;    // times V's controller was looked for among all the capsules
	uint64_t                       shapesKept = 0;            // since the start
	uint64_t                       shapesLeft = 0;            // given by the overlap but not kept: simulation-only, triggers
	uint64_t                       shapesUnreadable = 0;
	std::string                    bushRule;                  // what else the exporter leaves out (set bushrule); empty: not said
	uint64_t                       shapesBush = 0;            // shapes it left out so, since the start
	std::string                    physxWhy;                  // why the exporter couldn't read, while it can't
	// Minecraft's solid blocks, and V's bodies for them (spike 5's "Part 3"): the counts are since the start
	size_t               solidSections = 0;
	uint64_t             solidBlocks = 0;
	bool                 bodiesOn = false;  // set blocks 1, with drawing on
	std::string          bodiesWhy;         // why they wait, while they do
	size_t               bodySections = 0;
	size_t               bodyBoxes = 0;
	uint64_t             bodiesMade = 0;
	uint64_t             bodiesRemoved = 0;
	uint64_t             bodiesFailed = 0;
	uint64_t             bodiesLost = 0;
	double               bodySlowestMs = 0.0;
	std::string          bodiesMadeOf;      // empty: not copied yet
	uint64_t             originChanges = 0;
	uint64_t             bodiesReposed = 0;
	std::optional<Vec3d> box;               // the test box's centre, world axes, while it stands
	// NPCs in Minecraft (spike 6's "Part 1"): the numbers given and the hits are since the start
	bool                  npcsOn = true;      // set npcs 1
	std::string           npcsWhy;            // why none are listed, while none can be
	size_t                npcsListed = 0;     // in SkyCraft's actor table
	int32_t               npcsInWorld = 0;    // the module's count of NPCs the game has attached
	std::optional<double> nearestNpc;         // metres; none: none listed
	std::string           nearestNpcName;
	size_t                capsulesFound = 0;  // of the NPCs known
	size_t                capsulesLookedFor = 0;
	uint32_t              npcIdsGiven = 0;
	double                moduleMsAverage = -1.0;  // the module's time a frame, in the last second; below 0: not called
	double                moduleMsWorst = -1.0;
	uint64_t              npcHits = 0;  // Minecraft's hits on them, logged
	// Combat both ways (spike 6's "Part 2"): the counts are since the start
	bool                  combatOn = false;  // set combat 1
	double                punch = 0.0;       // V's punch, as last read
	std::string           punchItem;         // what it's read from: what V's unarmed slot draws
	bool                  punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
	double                hitPower = 0.5;    // set hitpower (link::kHitPower): hits start at V's punch x damage^this
	uint64_t              hitsQueued = 0;    // Minecraft's hits made V's, and queued in the game
	uint64_t              hitsLanded = 0;    // and dealt by its damage pipeline
	std::string           hitsDropped;       // by reason, ours or the pipeline's: "2 not listed, 1 dodged"; empty: none
	std::string           lastHit;           // the NPC, Minecraft's damage, where it started, and how it went
	uint64_t              hurtsTaken = 0;    // the game's hits on V, taken for Minecraft
	uint64_t              hurtsSent = 0;     // kInHurts sent
	uint64_t              hurtsSummed = 0;   // hurts that went in a sum
	uint64_t              hurtsLeft = 0;     // offered while not taken: left to V
	uint64_t              hurtsNonLethal = 0;  // offered while taken, but the game won't let them kill V: left to V
	std::string           lastHurt;          // its amount and share, Minecraft's damage, kind, attack type and attacker
	std::string           lastHealth;        // V's health share last sent to Minecraft's player, and when; empty: none yet
	bool                  immortal = false;  // our Immortal on V
	uint64_t              deaths = 0;        // Minecraft's deaths that killed V
	std::optional<double> minecraftDeadFor;  // seconds; none: alive
	std::string           combatWhy;         // why combat can't do all it should, while it can't
	// Minecraft's blasts (spike 6's "Part 3"), with combat on: the count is since the start
	uint64_t    blasts = 0;          // taken
	double      blastPower = 0.5;    // set blastpower (link::kBlastPower): a blast's hits start at V's punch x damage^this
	bool        blastKnock = false;  // set blastknock 1: each NPC within reach gets the knockdown directly too
	std::string lastBlast;           // its centre, reach, NPCs within it and its own hits; after its push, knocked down and dead
	// The render ring
	double      ringMb = 0.0;            // in the last second
	double      ringMbWorstFrame = 0.0;  // the most in one frame of it
	double      drainMsAverage = 0.0;
	double      drainMsWorst = 0.0;
	std::string ringTypes;     // messages and MB by type, in the last second; empty: none
	uint64_t    rejected = 0;  // since the start
	uint64_t    resyncs = 0;
	// Minecraft's blocks
	std::string atlas = "none yet";
	uint64_t    translucentDropped = 0;  // since the start
	double      updateMsAverage = -1.0;  // the renderer's Update; below 0: not measured
	double      updateMsWorst = -1.0;
	// The HUD
	int         hudSetting = 1;  // 0 never, 1 in build mode, 2 always
	uint32_t    hudScale = 2;
	bool        hudWanted = false;
	uint64_t    hudReceived = 0;  // in the last second
	double      hudMb = 0.0;      // of the images received
	uint64_t    hudDropped = 0;   // with no upload buffer free
	uint64_t    hudDrawn = 0;
	std::string hudSize;                  // the newest image's; empty: none yet
	double      hudCopyMsAverage = -1.0;  // SetHudImage's copy; below 0: not measured
	double      hudGpuMsAverage = -1.0;   // the pass's
	uint64_t    hudSkipped = 0;           // in the last second
	std::string hudLastSkip;
	uint64_t    hudRefused = 0;  // by the link, since the start: a bad slot or size
	// Latency, the last burst, and the processes
	std::optional<LatencySample> latencyLast;
	std::optional<LatencySample> latencyAverage;
	size_t                       latencySamples = 0;
	std::optional<BurstReport>   burst;
	double                       cyberpunkCpu = 0.0;   // a share of one core
	uint64_t                     cyberpunkMemory = 0;  // its working set, bytes
	bool                         minecraftProcessKnown = false;
	double                       minecraftCpu = 0.0;
	uint64_t                     minecraftMemory = 0;
};

struct StatusReport
{
	struct Structure
	{
		std::string name;
		Vec3d       worldMin;  // the box it fills, in world coordinates
		Vec3d       worldMax;
	};

	std::string            mode = "off";
	uint64_t               framesDrawn = 0;    // in the last second
	uint64_t               framesPartial = 0;  // of those, gbuffer's drawn into the G-buffer but not the tagged depth
	uint64_t               framesSkipped = 0;  // in the last second
	std::string            lastSkip;           // why the latest skipped frame was skipped
	uint64_t               lateCamera = 0;     // frames drawn with the previous frame's camera, in the last second
	size_t                 sections = 0;
	size_t                 blocks = 0;
	bool                   minecraftBlocks = false;  // the sections are Minecraft's, whose blocks aren't counted
	size_t                 vertices = 0;
	double                 gpuMsAverage = -1.0;  // below 0: not measured
	double                 gpuMsWorst = -1.0;
	double                 frameMsAverage = -1.0;
	double                 frameMsWorst = -1.0;
	uint32_t               streamlineFrames = 0;  // frames Streamline started in the last second
	std::string            hooks;
	std::string               lastRequest;
	std::string               sceneWatch;  // spike 5's step 3: the watch's summary; empty until it's first on
	std::vector<Structure>    structures;
	std::optional<LinkReport> link;  // spike 4's lines, once the plugin has tried the mapping
};

// Lines of "key=value": mode, frames, blocks, gpu, frame, streamline, hooks, request; with a link, its lines (link, build,
// drive, v, positions, ground, solids, bodies, npcs, combat, blasts, ring, atlas, hud, latency, burst and processes); then
// one structure line per structure:
// "structure=<name> <min x> <min y> <min z> <max x> <max y> <max z>".
std::string FormatStatus(const StatusReport& aReport);
}
