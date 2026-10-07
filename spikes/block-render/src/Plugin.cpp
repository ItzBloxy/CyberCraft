// BlockRender: spike 3's RED4ext plugin, linked to Minecraft by spike 4. It hooks the game's Streamline calls and D3D12
// to map Cyberpunk's frame (spike 3's step 1: the frame log and the dumps), and draws Minecraft blocks into it (step 2:
// on the finished frame; step 3: before DLSS; step 4: into the G-buffer). Spike 4 links it to SkyCraft's Minecraft
// through SkyCraft's shared memory: Minecraft's player follows V, Minecraft's blocks and HUD are drawn in the game, and
// in build mode (F8) the mouse buttons, the wheel and the keys 1 to 9 are Minecraft's. Spike 5 has Minecraft drive in
// build mode: the movement keys are Minecraft's too, and V is teleported to its player's eye every frame; it reads Night
// City's shape for Minecraft from the game's PhysX; and it makes Minecraft's solid blocks around V into PhysX bodies,
// for V to stand on. Spike 6 lists Night City's NPCs into SkyCraft's actor table, through its redscript module
// (scripts/BlockRender.reds), so that Minecraft's weapons hit them and they press its pressure plates; with combat on,
// Minecraft's hits land on them as V's, through the game's whole damage pipeline, and in build mode the game's hits on V
// go to Minecraft's player, whose death kills V. In build mode V's meshes and effects and the game's crosshair are
// hidden, and with combat on V's unarmed weapon is drawn, so that hits carry it, and Minecraft's player takes V's health
// share as build mode starts. Minecraft's blasts run the game's knockdown grenade's attack over what they reach, knock its
// living NPCs down with the game's forced reaction and tell the crowd, their own hits land as explosions, and their push
// throws the bodies, cars and loose things.
// Commands come through blockrender.request, in its folder or the overlay's, and status.txt in the overlay's folder says
// what it's doing. See docs/superpowers/specs/2026-10-01-block-render-design.md, 2026-10-02-minecraft-link-design.md,
// 2026-10-03-collision-and-movement-design.md and 2026-10-05-npcs-and-combat-design.md.
#include <RED4ext/RED4ext.hpp>

#include "GameScript.h"
#include "LogQueue.h"
#include "Measurements.h"
#include "Natives.h"
#include "PlayerLocator.h"
#include "ProcessUsage.h"
#include "Puppet.h"
#include "Requests.h"
#include "Status.h"
#include "hooks/D3D12Hooks.h"
#include "hooks/HookGroups.h"
#include "hooks/StreamlineHooks.h"
#include "input/RawInputHook.h"
#include "input/Router.h"
#include "link/Actors.h"
#include "link/Combat.h"
#include "link/Link.h"
#include "link/Mapping.h"
#include "link/Turn.h"
#include "log/DriveLog.h"
#include "log/Dumps.h"
#include "log/FrameLog.h"
#include "log/FrameLogData.h"
#include "physx/Bodies.h"
#include "physx/Characters.h"
#include "physx/Export.h"
#include "physx/SceneHooks.h"
#include "physx/Session.h"
#include "render/Hud.h"
#include "render/Renderer.h"
#include "track/GpuApi.h"
#include "world/BlockWorld.h"
#include "world/Mesher.h"
#include "world/Structures.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr const char* kVersion = "0.20.0";

// A number of a version "major.minor.patch", aIndex 0 being the major, so that Query reports kVersion, as the log does.
constexpr uint32_t VersionNumber(std::string_view aVersion, size_t aIndex)
{
	uint32_t number = 0;
	size_t   index = 0;
	for (const char ch : aVersion)
	{
		if (ch == '.')
		{
			if (index == aIndex)
			{
				return number;
			}
			++index;
			number = 0;
		}
		else
		{
			number = number * 10 + uint32_t(ch - '0');
		}
	}
	return index == aIndex ? number : 0;
}
static_assert(VersionNumber("1.23.456", 0) == 1 && VersionNumber("1.23.456", 1) == 23 && VersionNumber("1.23.456", 2) == 456);

RED4ext::v1::PluginHandle g_handle = nullptr;
const RED4ext::v1::Sdk*   g_sdk = nullptr;
std::filesystem::path     g_pluginDir;
std::filesystem::path     g_hudDir;  // the CET overlay's folder, where status.txt goes
cpmc::LogQueue            g_log;
bool                      g_hooked = false;       // Streamline's imports are patched
bool                      g_inputHooked = false;  // GetRawInputData's import is patched

// Made at load and never destroyed: the hooks may call them until the process ends.
cpmc::FrameLog*      g_frameLog = nullptr;
cpmc::Dumps*         g_dumps = nullptr;
cpmc::Renderer*      g_renderer = nullptr;
cpmc::input::Router* g_router = nullptr;  // where keys and clicks go (the spec's "Input")

cpmc::BlockWorld                           g_world;
cpmc::MeshOptions                          g_meshOptions;
std::vector<cpmc::StatusReport::Structure> g_structures;
std::string                                g_lastRequest = "none yet";
Clock::time_point                          g_lastSecond{};
uint64_t                                   g_lastFrameTokens = 0;
bool                                       g_removalReported = false;
bool                                       g_hudMissingReported = false;
bool                                       g_trackingHeld = false;  // set tracking 1: to measure what tracking costs

// Spike 4: the link to SkyCraft's Minecraft (the spec's "The link"). Main thread only.
cpmc::link::Mapping              g_mapping;
cpmc::link::Link*                g_link = nullptr;  // once the mapping is open
std::string                      g_mappingState = "not opened yet";
cpmc::link::LinkSettings         g_linkSettings;     // set teleportstep, teleportrate, ringmb, translucent; and the viewport
bool                             g_linked = false;   // link on|off
int                              g_hudSetting = 1;   // set hud: 0 never, 1 in build mode, 2 always
uint32_t                         g_hudScale = 2;     // set hudscale: 1, 2 or 4
bool                             g_hudWanted = false;
bool                             g_minecraftAtlas = false;  // Minecraft's atlas has replaced atlas.bin's
std::optional<cpmc::Vec3d>       g_camera;                  // V's camera, Minecraft axes, as the link last had it
std::string                      g_lastTyped;               // the last mc request, which names its burst
std::optional<double>            g_lastEnter;               // the link's last Enter, once marked
bool                             g_buildWanted = false;     // F8's link, or build on: build mode, after the link's next update
cpmc::LatencyTracker             g_latency;
cpmc::BurstWindow                g_burst;
std::optional<cpmc::BurstReport> g_lastBurst;
cpmc::ProcessWatch               g_cyberpunkProcess;
cpmc::ProcessWatch               g_minecraftProcess;
double                           g_lastFrameAt = 0.0;  // SteadySeconds() at the last OnUpdate

// Spike 5: build mode's drive (its spec's "Part 1"). Main thread only.
cpmc::Puppet          g_puppet{ g_log };  // the game's script calls that place V
cpmc::link::TurnRatio g_turnRatio;        // the mouse's counts to V's degrees, measured outside build mode
bool                  g_turn = true;      // set turn 1, the default since step 2: the teleport's yaw gains the mouse's turn
std::optional<double> g_savedTurn;        // the ratio turn.txt holds: read at load, or saved since
bool                  g_turnSaveLogged = false;
cpmc::DriveLog        g_driveLog;

// Spike 5's Night City in Minecraft (its spec's "Part 2"). Main thread only.
cpmc::px::Session     g_physx;            // the game's PhysX, checked against the audit
cpmc::px::PhysxGround g_ground{ g_physx };  // Night City's shape, read for the link
double                g_exportBudget = 1.5;  // set exportbudget: ms a frame the link may spend reading the city
std::string           g_groundWhy;        // why the ground fell back to the floor, if it did
std::optional<double> g_watchSince;       // scenewatch on: since when (SteadySeconds)
std::string           g_lastWatch;        // the last watch's summary, once it's off
DWORD                 g_mainThread = 0;   // OnUpdate's

// Spike 5's V against blocks (its spec's "Part 3"). Main thread only.
cpmc::px::Bodies g_bodies{ g_physx, g_ground };  // PhysX bodies for Minecraft's blocks around V, and step 4's test box
bool             g_blocks = true;                // set blocks 0|1: the bodies, while drawing is on; on since step 4 passed

// Spike 6's NPCs in Minecraft (its spec's "Part 1"). Main thread only.
cpmc::GameScript                          g_script{ g_log };      // the redscript module's calls
bool                                      g_npcs = true;          // set npcs 0|1: on, since listing NPCs changes nothing in the game
cpmc::link::ActorIds                      g_actorIds;             // the NPCs' numbers in SkyCraft's actor table
cpmc::px::NpcSizes                        g_npcSizes;             // their boxes, from their capsules
std::optional<cpmc::NpcList>              g_npcList;              // the module's last list
std::optional<cpmc::Vec3d>                g_npcsFeet;             // V's feet when it was made
std::vector<skycraft::proto::ActorRecord> g_actorRecords;         // the table as last written, nearest first
std::vector<uint64_t>                     g_listedEntities;       // the NPC behind each of those records
std::string                               g_npcsWhy;              // why none are listed, while none can be
uint64_t                                  g_npcHits = 0;          // Minecraft's hits on them, logged
double                                    g_npcsForgotten = 0.0;  // when NPCs unlisted for a minute were last forgotten
size_t                                    g_wrapThreads = 0;      // the wraps' threads, as last logged

// Spike 6's combat both ways (its spec's "Part 2"). Main thread only: the natives' inbox takes what the damage
// pipeline's wraps report, on whichever thread runs it.
bool                                  g_combat = false;      // set combat 0|1: off until step 4 passes
std::vector<skycraft::proto::McEvent> g_events;              // Minecraft's blasts, hits and deaths this frame, in the ring's order
double                                g_punch = 0.0;         // V's punch and max health, as the module last read them
double                                g_vHealthMax = 0.0;
std::string                           g_punchItem;           // what V's unarmed slot draws, which the punch is read from
bool                                  g_punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
double                                g_hitPower = cpmc::link::kHitPower;  // set hitpower: hits start at the punch x damage^this
cpmc::link::VSwitch                   g_armed{ 1.0 };        // V's unarmed weapon, drawn while combat and build mode are on
cpmc::link::VSwitch                   g_hidden{ 0.25 };      // V's meshes, hidden while build mode is on and hidev is 1
bool                                  g_hideV = true;        // set hidev 0|1
std::optional<int32_t>                g_lastArm;             // what Arm and HideV last said, so that a refresh logs a change
std::optional<cpmc::VLook>            g_lastHide;
cpmc::VLook                           g_vLook;               // what HideV has done while V is hidden: meshes, effects, crosshair
cpmc::link::HurtSums                  g_hurtSums;            // V's hurts, summed for Minecraft
std::vector<cpmc::link::Hurt>         g_hurtsOut;            // waiting for room in the input ring
cpmc::link::GodMode                   g_godMode;             // V's Immortal, under our own source name
bool                                  g_immortal = false;    // as the game last said
bool                                  g_wasBuild = false;
std::optional<double>                 g_buildEndedAt;        // when build mode last ended, for Minecraft's death
std::optional<double>                 g_minecraftDiedAt;     // since when McState has said Minecraft's player is dead
// One of our hits, queued and not yet dealt by the game.
struct PendingHit
{
	uint64_t entity = 0;
	uint32_t formId = 0;
	double   damage = 0.0;
	double   amount = 0.0;  // where the pipeline starts it
	double   at = 0.0;
	bool     blast = false;  // a blast's own hit
};
std::vector<PendingHit>         g_pendingHits;
std::map<std::string, uint64_t> g_hitDrops;  // hits not applied, by reason
uint64_t                        g_hitsQueued = 0;
uint64_t                        g_hitsLanded = 0;
uint64_t                        g_hurtsSent = 0;
uint64_t                        g_deaths = 0;
std::string                     g_lastHit;
std::string                     g_lastHurt;

// Spike 7: Minecraft's entities (its spec's "The frame"): the newest scene and world-entity table, made into each frame's
// geometry for the renderer, and the textures' sizes, for the entities request. Main thread only.
cpmc::entities::Entities                          g_entities;
std::map<uint32_t, std::pair<uint32_t, uint32_t>> g_entityTextureSizes;  // by id: width and height

// Spike 6's Minecraft blasts (its spec's "Part 3"), and V's health share for Minecraft's player (its follow-up). Main
// thread only.
cpmc::link::Blasts g_blasts;                                 // the last moments' blasts: their own hits, their pushes
double             g_blastPower = cpmc::link::kBlastPower;  // set blastpower: a blast's hits start at the punch x damage^this
bool               g_blastKnock = true;  // set blastknock: each living NPC within a blast's reach knocked down by force
std::string        g_lastBlast;
double             g_vHealth = 0.0;  // V's health share, as the module last read it
std::string        g_lastHealth;

// This frame's figures, for bursts.
struct FrameFigures
{
	uint64_t sections = 0;
	uint64_t bytes = 0;
	double   drainMs = 0.0;
};
FrameFigures g_frame;

// This second's, for the status.
struct SecondFigures
{
	double                                         start = 0.0;  // SteadySeconds()
	uint64_t                                       frames = 0;
	uint64_t                                       bytes = 0;
	uint64_t                                       bytesWorstFrame = 0;
	double                                         drainMsSum = 0.0;
	double                                         drainMsWorst = 0.0;
	uint64_t                                       teleportsAtStart = 0;
	uint64_t                                       minecraftFramesAtStart = 0;
	std::array<uint64_t, cpmc::link::Link::kTypes> messagesAtStart{};
	std::array<uint64_t, cpmc::link::Link::kTypes> bytesAtStart{};
	// The drive's
	uint64_t waitedAtStart = 0;
	double   eyeDistanceSum = 0.0;
	double   eyeDistanceWorst = 0.0;
	uint64_t eyeDistanceFrames = 0;
	double   teleportMsSum = 0.0;
	double   teleportMsWorst = 0.0;
	uint64_t teleportCalls = 0;
	// The city's
	double   cityReadMsSum = 0.0;
	double   cityReadMsWorst = 0.0;
	uint64_t cityBatchesAtStart = 0;
	uint64_t cityTrianglesAtStart = 0;
	uint64_t workerRegionsAtStart = 0;
	uint64_t workerTrianglesAtStart = 0;
	uint64_t workerBlocksAtStart = 0;
	double   workerMsAtStart = 0.0;
	uint64_t workerCyclesAtStart = 0;
	uint64_t workerTicksAtStart = 0;
	uint64_t collisionBytesAtStart = 0;
	// The module's
	double   moduleMsSum = 0.0;
	double   moduleMsWorst = 0.0;
	uint64_t moduleCalls = 0;
	// Spike 7's entities
	uint64_t scenesAtStart = 0;
	uint64_t sceneBytesAtStart = 0;
};
SecondFigures g_second;

template<class... Args>
void Log(std::format_string<Args...> aFormat, Args&&... aArgs)
{
	if (g_sdk)
	{
		g_sdk->logger->Info(g_handle, std::format(aFormat, std::forward<Args>(aArgs)...).c_str());
	}
}

void DrainLog()
{
	for (const std::string& line : g_log.Drain())
	{
		Log("{}", line);
	}
}

std::filesystem::path PluginFolder()
{
	HMODULE module = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                   reinterpret_cast<LPCWSTR>(&PluginFolder), &module);
	wchar_t path[MAX_PATH]{};
	GetModuleFileNameW(module, path, MAX_PATH);
	return std::filesystem::path(path).parent_path();
}

// "2026-10-01T12:34:56" for the logs, "2026-10-01_12-34-56" for folder names.
std::pair<std::string, std::string> Timestamps()
{
	SYSTEMTIME now{};
	GetLocalTime(&now);
	return { std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond),
	         std::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond) };
}

// At most the first 4 KB of a small text file; empty if it can't be read.
std::string ReadSmallFile(const std::filesystem::path& aPath)
{
	std::ifstream in(aPath, std::ios::binary);
	std::string   text(4096, '\0');
	in.read(text.data(), std::streamsize(text.size()));
	text.resize(size_t(in.gcount()));
	return text;
}

// aText on one line for the log: control characters and anything past ASCII become '?', at most 200 characters.
std::string Printable(const std::string& aText)
{
	std::string out;
	for (const char ch : aText.substr(0, 200))
	{
		out += ch >= 32 && ch < 127 ? ch : '?';
	}
	return out;
}

void Report(std::string aResult)
{
	Log("request: {}", aResult);
	g_lastRequest = std::move(aResult);
}

// What Minecraft sends, handed to the renderer (the spec's "Blocks" and "The HUD").
class MinecraftSink : public cpmc::link::LinkSink
{
public:
	void Atlas(const cpmc::link::AtlasImage& aAtlas) override
	{
		const double               start = cpmc::SteadySeconds();
		std::string                error;
		std::optional<cpmc::Atlas> atlas = cpmc::AtlasFromRgba(aAtlas.width, aAtlas.height, aAtlas.rgba, error);
		if (!atlas)
		{
			return ::Log("Minecraft's atlas can't be built ({}), so the atlas stays as it was", error);
		}
		// Spike 3's own structures go, with every section: their UVs belong to the atlas before. Spike 3's world isn't
		// meshed again while Minecraft's atlas is loaded, so its removals can't undo Minecraft's sections.
		g_world.Clear();
		g_world.TakeDirty();
		g_structures.clear();
		g_renderer->ClearSections();
		g_renderer->SetAtlas(std::move(*atlas));
		g_minecraftAtlas = true;
		::Log("Minecraft's atlas: {} x {}, {} levels, built in {:.1f} ms", aAtlas.width, aAtlas.height, cpmc::kMinecraftAtlasLevels,
		      (cpmc::SteadySeconds() - start) * 1000.0);
	}

	void Section(cpmc::link::SectionMesh&& aMesh) override
	{
		const cpmc::SectionPos section{ aMesh.x, aMesh.y, aMesh.z };
		++g_frame.sections;
		if (g_latency.SectionArrived(cpmc::SteadySeconds(), g_renderer->Presents()))
		{
			g_renderer->TimeUpload(section);  // the first section after a mark: timed to the list that first draws it
		}
		g_renderer->UploadSection(section, std::move(aMesh.vertices));
	}

	void ClearAll() override
	{
		g_renderer->ClearSections();
		g_renderer->ClearEntityTextures();
		g_entities.Clear();
		g_entityTextureSizes.clear();
		::Log("Minecraft cleared its sections and textures: it sends everything again");
	}

	// Spike 7: Minecraft's entity textures, its scenes of entities and particles, and its world-entity table.
	void Texture(const cpmc::link::TextureImage& aTexture) override
	{
		g_entityTextureSizes[aTexture.id] = { aTexture.width, aTexture.height };
		g_renderer->AddEntityTexture(aTexture.id, aTexture.width, aTexture.height, aTexture.rgba);
	}

	void Scene(const cpmc::link::SceneMessage& aScene) override { g_entities.Scene(aScene); }
	void WorldEntities(const cpmc::link::WorldTable& aTable) override { g_entities.World(aTable); }

	void Explosion(const skycraft::proto::McEvent& aEvent) override
	{
		g_burst.Start(cpmc::SteadySeconds(), "explosion");
		g_events.push_back(aEvent);  // Fight logs it
	}

	// Spike 6: Minecraft's blasts, hits on NPCs and its player's death, kept in the ring's order for Fight, after the link's
	// update: a blast before its own hits.
	void HitActor(const skycraft::proto::McEvent& aEvent) override { g_events.push_back(aEvent); }
	void PlayerDied(const skycraft::proto::McEvent& aEvent) override { g_events.push_back(aEvent); }

	void Hud(const cpmc::link::HudFrame& aFrame) override
	{
		g_renderer->SetHudImage(aFrame.width, aFrame.height, aFrame.bottomUp, aFrame.pixels);
	}

	void Log(const std::string& aLine) override
	{
		::Log("link: {}", aLine);
	}
};
MinecraftSink g_sink;

// The mapping, made as SkyCraft's Skyrim side makes it (the spec's "link/"), at the first update rather than at load:
// the heartbeat comes once a frame from Link::Update, and the game doesn't update until it has loaded, so Minecraft
// would link to a game side that then falls silent. While another game side uses it, it's refused, and tried again
// once a second.
void OpenMapping()
{
	using Outcome = cpmc::link::Mapping::Outcome;
	const cpmc::link::Mapping::Opened opened = g_mapping.Open(skycraft::proto::kMappingName, GetCurrentProcessId(), GetTickCount64());
	const bool                        open = opened.outcome == Outcome::Created || opened.outcome == Outcome::Reused;
	const std::string state = open ? opened.why : (opened.outcome == Outcome::Refused ? "refused: " : "failed: ") + opened.why;
	if (state != g_mappingState)
	{
		Log("the mapping: {}", state);
	}
	g_mappingState = state;
	if (open)
	{
		g_link = new cpmc::link::Link(g_mapping.Base());
		g_router->SetLink(g_link);
		g_second = {};
		g_second.start = cpmc::SteadySeconds();
	}
}

// QueryPerformanceCounter: the clock Minecraft stamps its ticks with.
int64_t QpcNow()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

int64_t QpcPerSecond()
{
	static const int64_t perSecond = [] {
		LARGE_INTEGER frequency{};
		QueryPerformanceFrequency(&frequency);
		return frequency.QuadPart;
	}();
	return perSecond;
}

// What placing V did this frame, for the status and the drive log.
struct Placed
{
	double                teleportMs = 0.0;
	double                turn = 0.0;  // degrees set turn 1 added
	std::optional<double> yaw;         // the teleport's, if V was teleported
};

// Spike 5's drive, after the link's update: build mode ends if the link says so; otherwise, while it's on, V goes where
// the link puts it, facing its own yaw, and the frame's turn from the mouse with set turn 1. A teleport that fails ends
// build mode, so Minecraft never walks away from a V that can't follow.
Placed PlaceV(const cpmc::link::Link::Status& aState, const std::optional<double>& aYaw, int64_t aMouseX)
{
	Placed placed;
	if (!aState.endBuild.empty())
	{
		g_router->TurnOff(aState.endBuild);
		return placed;
	}
	if (!aState.placeV || !g_router->On())
	{
		return placed;
	}
	if (!aYaw)
	{
		g_router->TurnOff("V's yaw can't be read");
		return placed;
	}
	placed.turn = g_turn ? g_turnRatio.Turn(aMouseX) : 0.0;
	std::string  error;
	const double start = cpmc::SteadySeconds();
	const bool   moved = g_puppet.Teleport(*aState.placeV, *aYaw + placed.turn, error);
	placed.teleportMs = (cpmc::SteadySeconds() - start) * 1000.0;
	if (!moved)
	{
		g_router->TurnOff("V couldn't be placed: " + error);
		return placed;
	}
	placed.yaw = *aYaw + placed.turn;
	++g_second.teleportCalls;
	g_second.teleportMsSum += placed.teleportMs;
	g_second.teleportMsWorst = std::max(g_second.teleportMsWorst, placed.teleportMs);
	return placed;
}

// The link, once a frame (the spec's "The link"): V's pose and the HUD's size, then Link::Update, which hands what
// Minecraft sends to the sink. In build mode, V then goes where the link puts it (spike 5). V's feet, if V is found.
std::optional<cpmc::Vec3d> UpdateLink(double aNow, double aFrameMs)
{
	g_frame = {};
	const int64_t mouseX = g_router->TakeMouseX();  // every frame, so a frame's turn is only its own
	if (!g_link)
	{
		return std::nullopt;
	}
	const cpmc::link::Link::Status&     state = g_link->State();
	if (std::string why; g_linkSettings.ground == cpmc::link::Ground::City && !g_physx.Audited(why))
	{
		// PhysX isn't the build the audit checked: nothing of it is read, and Minecraft gets the floor.
		g_linkSettings.ground = cpmc::link::Ground::Floor;
		g_groundWhy = why;
		Log("the ground falls back to the floor: {}", why);
	}
	const std::optional<cpmc::PlayerPose> player = cpmc::LocatePlayer();
	std::optional<cpmc::link::GamePose>   pose;
	if (player && player->camera)
	{
		pose = cpmc::link::GamePose{ player->feet, player->camera->position, player->camera->forward };
	}
	g_camera = pose ? std::optional<cpmc::Vec3d>(cpmc::McFromWorld(pose->camera)) : std::nullopt;
	const bool buildMode = g_router->On();
	// While linked: V's yaw, and what keeps V from being placed. Puppet finds V itself, so the reason is known without
	// the camera too: in a car the camera isn't V's eye, and the link finds no V. The turn ratio is measured outside
	// build mode, while V can be placed, so its yaw is V's own doing.
	const std::optional<double> yaw = g_linked && pose ? g_puppet.Yaw() : std::nullopt;
	const std::string           blocked = g_linked ? g_puppet.Blocked() : std::string();
	if (!buildMode && yaw && blocked.empty())
	{
		g_turnRatio.Note(mouseX, *yaw);
	}
	else
	{
		g_turnRatio.Break();
	}
	g_hudWanted = cpmc::HudWanted(g_hudSetting, buildMode, state.inWorld);
	g_renderer->SetHud(g_hudWanted);
	const cpmc::HudSize back = g_renderer->BackBufferSize();
	const cpmc::HudSize viewport = cpmc::HudViewport(back.width, back.height, g_hudScale);
	g_linkSettings.viewportWidth = viewport.width;
	g_linkSettings.viewportHeight = viewport.height;

	cpmc::link::Link::Frame frame;
	frame.now = aNow;
	frame.tickMs = GetTickCount64();
	frame.qpc = QpcNow();
	frame.qpcPerSecond = QpcPerSecond();
	frame.pose = pose;
	frame.linked = g_linked;
	frame.buildMode = buildMode;
	frame.placeable = blocked.empty();
	frame.takeHud = g_hudWanted;
	const uint64_t drainedBefore = state.drainedBytes;
	const uint64_t waitedBefore = state.waitedFrames;
	g_link->Update(frame, g_linkSettings, g_sink);
	g_router->Update(g_linked, state, blocked);  // build mode ends if Minecraft or V has gone, or the link, or V can't be placed
	const Placed placed = PlaceV(state, yaw, mouseX);
	if (state.eyeDistance)
	{
		g_second.eyeDistanceSum += *state.eyeDistance;
		g_second.eyeDistanceWorst = std::max(g_second.eyeDistanceWorst, *state.eyeDistance);
		++g_second.eyeDistanceFrames;
	}
	if (g_driveLog.Recording())
	{
		cpmc::DriveRow row;
		row.time = aNow;
		row.frameMs = aFrameMs;
		row.state = cpmc::link::DriveStateName(state.drive);
		row.camera = pose ? std::optional<cpmc::Vec3d>(pose->camera) : std::nullopt;
		row.target = state.placeV ? std::optional<cpmc::Vec3d>(cpmc::Vec3d{ state.placeV->x, state.placeV->y, state.placeV->z + state.eyeHeight })
		                          : std::nullopt;
		row.lastDistance = state.eyeDistance;
		row.delayMs = state.renderDelayMs;
		row.waited = state.waitedFrames != waitedBefore;
		row.teleportMs = placed.teleportMs;
		row.mouseX = mouseX;
		row.yaw = placed.yaw;
		row.turn = placed.turn;
		g_driveLog.Add(row);
	}
	// Night City's shape, and V's bodies, given V's feet from before this frame's teleport, in a car too: V's controller is
	// within a frame's move of them, whether the teleport has moved it yet or not. The city is read only with V's camera.
	const std::optional<cpmc::Vec3d> feet = player ? std::optional<cpmc::Vec3d>(player->feet) : std::nullopt;
	g_ground.Frame(feet);
	g_link->ReadGround(aNow, g_ground, g_exportBudget);
	for (const std::string& line : g_ground.TakeSlowBatches())
	{
		Log("{}", line);
	}
	g_bodies.Frame(feet, g_blocks && g_renderer->ModeName() != "off", g_link->SolidSections());
	g_second.cityReadMsSum += state.cityReadMs;
	g_second.cityReadMsWorst = std::max(g_second.cityReadMsWorst, state.cityReadMs);
	if (std::exchange(g_buildWanted, false))
	{
		const std::optional<std::string> why = g_router->TurnOn();
		Report(why ? "build on: it stays off: " + *why
		           : "build on: Minecraft's player drives V; the movement keys, the mouse buttons, the wheel and 1 to 9 are Minecraft's");
	}
	g_frame.bytes = state.drainedBytes - drainedBefore;
	g_frame.drainMs = state.drainMs;
	++g_second.frames;
	g_second.bytes += g_frame.bytes;
	g_second.bytesWorstFrame = std::max(g_second.bytesWorstFrame, g_frame.bytes);
	g_second.drainMsSum += g_frame.drainMs;
	g_second.drainMsWorst = std::max(g_second.drainMsWorst, g_frame.drainMs);
	if (state.lastEnter && state.lastEnter != g_lastEnter)
	{
		// A typed command's Enter: a latency mark, and a burst's start.
		g_lastEnter = state.lastEnter;
		g_latency.Mark(*state.lastEnter);
		g_burst.Start(*state.lastEnter, "mc " + g_lastTyped);
	}
	if (const std::optional<double> press = g_router->TakePress())
	{
		g_latency.Mark(*press);  // a click that went to Minecraft
	}
	if (const std::optional<cpmc::Renderer::TimedUpload> timed = g_renderer->TakeTimedUpload())
	{
		if (const std::optional<cpmc::LatencySample> sample = g_latency.Drawn(timed->drawn, timed->drawnFrame))
		{
			Log("latency: Minecraft {:.0f} ms, ours {:.1f} ms ({:.0f} frames)", sample->minecraftMs, sample->oursMs, sample->oursFrames);
		}
	}
	return feet;
}

// Spike 6's NPCs in Minecraft (its spec's "The frame", "The table" and "Sizes"), after the link's update: while linked
// with V found and npcs on, the module lists the NPCs within 80 m, the nearest 256 go into SkyCraft's actor table, and
// up to eight NPCs listed for the first time have their capsules looked for. When any of that stops, the table is
// emptied, once.
void ListNpcs(double aNow, const std::optional<cpmc::Vec3d>& aFeet)
{
	g_npcList.reset();
	g_npcsFeet = aFeet;
	if (!g_npcs || !g_link || !g_linked || !aFeet)
	{
		g_npcsWhy = !g_npcs ? "" : !g_link ? "the mapping isn't open" : !g_linked ? "not linked" : "V isn't found";
	}
	else
	{
		g_script.Check();
		DrainLog();  // the check's line, before the module's first call
		std::string  why;
		const double start = cpmc::SteadySeconds();
		g_npcList = g_script.List(cpmc::link::kActorRange, why);
		const double ms = (cpmc::SteadySeconds() - start) * 1000.0;
		g_npcsWhy = why;
		g_second.moduleMsSum += ms;
		g_second.moduleMsWorst = std::max(g_second.moduleMsWorst, ms);
		++g_second.moduleCalls;
	}
	if (!g_npcList)
	{
		g_actorRecords.clear();
		g_listedEntities.clear();
		if (g_link)
		{
			g_link->ClearActors();  // once: an empty table isn't written again
		}
		return;
	}
	g_punch = g_npcList->punch;
	g_punchItem = g_npcList->punchItem;
	g_punchDrawn = g_npcList->punchDrawn;
	g_vHealthMax = g_npcList->vHealthMax;
	g_vHealth = g_npcList->vHealth;
	g_actorRecords = cpmc::link::ActorRecords(g_npcList->npcs, *aFeet, g_npcSizes.Boxes(), g_actorIds, aNow);
	g_link->WriteActors(g_actorRecords);
	g_listedEntities.clear();
	for (const skycraft::proto::ActorRecord& record : g_actorRecords)
	{
		g_listedEntities.push_back(g_actorIds.Entity(record.formId, aNow).value_or(0));
	}
	// Capsules only while scene 0 and PhysX's origin can be read: an NPC looked for waits 2 s before it's looked for again.
	cpmc::Vec3d origin;
	if (g_ground.Prepared(origin))
	{
		for (const uint64_t entity : g_npcSizes.ToLookFor(g_listedEntities, aNow))
		{
			const auto npc = std::find_if(g_npcList->npcs.begin(), g_npcList->npcs.end(),
			                              [&](const cpmc::link::NpcSample& aNpc) { return aNpc.entity == entity; });
			if (npc == g_npcList->npcs.end())
			{
				continue;
			}
			const cpmc::Vec3d&                       feet = npc->position;
			const std::vector<cpmc::px::CapsuleSeen> seen =
			    g_ground.Capsules({ feet.x, feet.y, feet.z + cpmc::px::kCapsuleSearchHeight }, cpmc::px::kCapsuleSearchRadius);
			if (const std::optional<cpmc::px::CapsuleSeen> capsule = cpmc::px::PickCapsule(seen, feet))
			{
				g_npcSizes.Found(entity, *capsule, cpmc::px::TallestCapsule(seen, *capsule, feet), feet);
			}
		}
	}
	if (aNow - g_npcsForgotten >= 1.0)
	{
		g_npcsForgotten = aNow;
		g_actorIds.Forget(aNow);
		g_npcSizes.Forget(g_actorIds);
	}
}

// The name SkyCraft's actor table last had for stand-in aFormId.
std::string NpcName(uint32_t aFormId)
{
	const auto record = std::find_if(g_actorRecords.begin(), g_actorRecords.end(),
	                                 [&](const skycraft::proto::ActorRecord& aRecord) { return aRecord.formId == aFormId; });
	return record != g_actorRecords.end() ? std::string(record->name) : std::string("not listed now");
}

void DropHit(const std::string& aHit, const std::string& aWhy)
{
	++g_hitDrops[aWhy];
	Log("{}; not applied: {}", aHit, aWhy);
}

// The NPC aEntity as the module last listed it; null if it isn't listed.
const cpmc::link::NpcSample* ListedByEntity(uint64_t aEntity)
{
	if (!g_npcList)
	{
		return nullptr;
	}
	const auto npc = std::find_if(g_npcList->npcs.begin(), g_npcList->npcs.end(),
	                              [&](const cpmc::link::NpcSample& aNpc) { return aNpc.entity == aEntity; });
	return npc != g_npcList->npcs.end() ? &*npc : nullptr;
}

// One of Minecraft's hits on an NPC (spike 6's "Minecraft's hits on NPCs"): logged, and with combat on, made V's through
// the module, starting the game's damage pipeline at V's punch x Minecraft's damage to the hit power. One within a blast's
// window, on an NPC whose last listed feet are within the blast's reach, is the blast's own (its "Blast hits"): of type
// Explosion, from the blast's centre, starting at V's punch x Minecraft's damage to the blast power.
void HitOnNpc(const skycraft::proto::McEvent& aEvent, double aNow, bool aVFound)
{
	++g_npcHits;
	std::string hit = std::format("hit on NPC {} ({}) for {:.2f} of Minecraft's damage, knockback {:.2f}; {}", aEvent.formId,
	                              NpcName(aEvent.formId), aEvent.a, aEvent.d, cpmc::link::HitWords(aEvent));
	if (!g_combat)
	{
		return Log("{}; not applied: combat is off", hit);
	}
	std::optional<cpmc::link::Blast> blast;
	if (const std::optional<uint64_t> entity = g_actorIds.Entity(aEvent.formId, aNow))
	{
		if (const cpmc::link::NpcSample* npc = ListedByEntity(*entity))
		{
			blast = g_blasts.HitOn(npc->position, aNow);
		}
	}
	if (blast)
	{
		hit += "; a blast's own";
	}
	const double                            power = blast ? g_blastPower : g_hitPower;
	cpmc::link::HitDrop                     drop = cpmc::link::HitDrop::Count;
	const std::optional<cpmc::link::NpcHit> accepted = cpmc::link::AcceptHit(aEvent, g_actorIds, aNow, g_punch, power, drop);
	if (!accepted)
	{
		return DropHit(hit, cpmc::link::HitDropName(drop));
	}
	if (!aVFound)
	{
		return DropHit(hit, "V isn't found");
	}
	std::string                  why;
	const std::optional<int32_t> result = blast ? g_script.BlastHit(accepted->entity, accepted->amount, blast->centre, why)
	                                            : g_script.Hit(accepted->entity, accepted->amount, accepted->strong, why);
	if (!result || *result != 1)
	{
		return DropHit(hit, result ? cpmc::link::HitResultName(*result) : "the module can't be called: " + why);
	}
	++g_hitsQueued;
	g_pendingHits.push_back({ accepted->entity, accepted->formId, accepted->damage, accepted->amount, aNow, blast.has_value() });
	const bool strong = accepted->strong && !blast;
	g_lastHit = std::format("NPC {} ({}): {:.2f} of Minecraft's damage{}, starting at {:.2f}{}; queued", accepted->formId,
	                        NpcName(accepted->formId), accepted->damage, blast ? ", a blast's" : "", accepted->amount,
	                        strong ? ", strong" : "");
	Log("{}: starting at {:.2f} = V's punch {:.2f} x {:.2f}^{:.2f}{}{}; queued", hit, accepted->amount, g_punch, accepted->damage, power,
	    strong ? ", strong" : "", blast || g_punchDrawn ? "" : ", without V's weapon drawn");
}

// One of Minecraft's blasts (spike 6's "The blast in the game"): with combat on, the game's knockdown grenade's attack and
// the explosion's sound at its centre, in the game's axes, through the module; its push follows a quarter second after
// (PushBlast).
void BlastIn(const skycraft::proto::McEvent& aEvent, double aNow, bool aVFound)
{
	const std::string explosion =
	    std::format("explosion at Minecraft {:.1f} {:.1f} {:.1f}, radius {:.1f}", aEvent.a, aEvent.b, aEvent.c, aEvent.d);
	if (!g_combat)
	{
		return Log("{}; not applied: combat is off", explosion);
	}
	const std::optional<cpmc::link::Blast> blast = cpmc::link::AcceptBlast(aEvent, aNow);
	if (!blast)
	{
		return Log("{}; not applied: its centre or radius isn't usable", explosion);
	}
	if (!aVFound)
	{
		return Log("{}; not applied: V isn't found", explosion);
	}
	g_blasts.Add(*blast);
	size_t within = 0;
	if (g_npcList)
	{
		for (const cpmc::link::NpcSample& npc : g_npcList->npcs)
		{
			within += std::hypot(npc.position.x - blast->centre.x, npc.position.y - blast->centre.y, npc.position.z - blast->centre.z) <=
			          blast->reach;
		}
	}
	const std::string where = std::format("at world ({:.2f}, {:.2f}, {:.2f}), reaching {:.1f} m", blast->centre.x, blast->centre.y,
	                                      blast->centre.z, blast->reach);
	std::string                           why;
	const std::optional<cpmc::BlastStart> start = g_script.Explode(blast->centre, blast->reach, g_blastKnock, why);
	std::string                           outcome = "there's no attack record or effect for its knockdowns";
	std::string                           knocked;
	if (!start)
	{
		outcome = "the module can't be called: " + why;
	}
	else if (start->result == 1)
	{
		knocked = g_blastKnock ? std::format("{} knocked down by the game's forced reaction, {} immune to it", start->knocked, start->immune)
		                       : std::string("the forced knockdowns off");
		outcome = std::format("its knockdown attack and sound started; {}; {}", knocked,
		                      start->panic ? "the crowd told, from the NPC nearest it" : "no NPC near enough to tell the crowd");
	}
	else if (start->result == 0)
	{
		outcome = "V isn't found";
	}
	g_lastBlast = std::format("{}: {} NPCs within it{}; its push to come", where, within, knocked.empty() ? "" : ", " + knocked);
	Log("{}: a blast {}, {} listed NPCs within it; {}", explosion, where, within, outcome);
}

// A blast's push, a quarter second after it (spike 6's "The blast in the game"), through the module, which then says what
// is within the blast's reach: knocked down, and dead.
void PushBlast(const cpmc::link::Blast& aBlast, double aNow, bool aVFound)
{
	const std::string where = std::format("at world ({:.2f}, {:.2f}, {:.2f}), reaching {:.1f} m", aBlast.centre.x, aBlast.centre.y,
	                                      aBlast.centre.z, aBlast.reach);
	std::string                    why = "V isn't found";
	const std::optional<cpmc::BlastPush> push = aVFound ? g_script.Push(aBlast.centre, aBlast.reach, why) : std::nullopt;
	if (!push || push->npcs < 0)
	{
		g_lastBlast = std::format("{}: {} hits of its own; not pushed: {}", where, aBlast.hits, push ? "V isn't found" : why);
		return Log("blast {} not pushed: {}", where, push ? "V isn't found" : why);
	}
	g_lastBlast = std::format("{}: {} NPCs within it, {} hits of its own; after its push {} knocked down, {} dead", where, push->npcs,
	                          aBlast.hits, push->down, push->dead);
	Log("blast {} pushed, {:.2f} s after it: {} NPCs within it, {} knocked down, {} dead; {} hits of its own", where, aNow - aBlast.at,
	    push->npcs, push->down, push->dead, aBlast.hits);
}

// V's health share to Minecraft's player as V's Immortal goes on, with build mode and combat (spike 6's follow-up), through
// CyberCraft Tweaks 1.3.0: a SkyCraft without it leaves the input alone.
void CarryHealth()
{
	if (!g_link || g_vHealth <= 0.0)
	{
		return Log("combat: Minecraft's health not set: {}", !g_link ? "the mapping isn't open" : "V's health isn't read yet");
	}
	const bool sent = g_link->Push(cpmc::link::HealthInput(g_vHealth));
	g_lastHealth = std::format("set to V's share {:.4f} when build mode last started{}", g_vHealth, sent ? "" : ", but the input ring was full");
	Log("combat: Minecraft's health {}", g_lastHealth);
}

// Minecraft's player's death (spike 6's "Deaths"): with combat on, in build mode or just after it, V's death too.
void MinecraftDied(const skycraft::proto::McEvent& aEvent, double aNow, bool aVFound)
{
	const std::string died =
	    std::format("Minecraft's player died{}", aEvent.formId != 0 ? std::format(", killed by NPC {}", aEvent.formId) : std::string());
	if (!cpmc::link::DeathKillsV(g_combat, g_router->On(), g_buildEndedAt, aNow))
	{
		return Log("{}; V lives: {}", died, g_combat ? "build mode wasn't on" : "combat is off");
	}
	if (!aVFound)
	{
		return Log("{}; V lives: V isn't found", died);
	}
	const uint64_t            killer = aEvent.formId != 0 ? g_actorIds.Entity(aEvent.formId, aNow).value_or(0) : 0;
	std::string               why;
	const std::optional<bool> killed = g_script.KillV(killer, why);
	++g_deaths;
	g_immortal = false;
	Log("{}; so V dies: {}", died, killed ? (*killed ? "V's health set to its least" : "but another god mode keeps V alive") : "not done: " + why);
}

// What the module's Arm said, in words.
std::string ArmWords(int32_t aResult, bool aOn)
{
	switch (aResult)
	{
	case 1:
		return aOn ? "asked the game to draw V's unarmed weapon" : "asked the game for V's own weapon back";
	case 2:
		return aOn ? "V has it drawn" : "nothing to put back";
	case 3:
		return "put away, and drawn again in a second, so that the game starts the effects it starts as it's drawn";
	case 0:
		return "V isn't found";
	case -1:
		return "V has no unarmed weapon";
	case -2:
		return "there's no equipment system";
	default:
		return std::format("result {}", aResult);
	}
}

// What the module's HideV did, in words.
std::string HideWords(const cpmc::VLook& aLook, bool aOn)
{
	switch (aLook.meshes)
	{
	case -1:
		return "not done: hiding V's meshes needs Codeware, which isn't installed";
	case -2:
		return "not done: V isn't found";
	default:
		break;
	}
	if (aOn)
	{
		return std::format("{} meshes switched off, {} effects stopped, the crosshair {}", aLook.meshes, aLook.effects,
		                   aLook.crosshair > 0 ? "hidden" : "not found among the HUD layer's game controllers");
	}
	return std::format("{} meshes switched on again, {} effects started again{}", aLook.meshes, aLook.effects,
	                   aLook.crosshair > 0 ? ", the crosshair shown again" : "");
}

// What the game says of V's Immortal after the module's SetImmortal, in words.
std::string ImmortalWords(bool aOn, bool aImmortal)
{
	if (aOn)
	{
		return aImmortal ? "the game says V is Immortal" : "but the game says V isn't Immortal";
	}
	return aImmortal ? "but the game says V is still Immortal, from another source such as Second Heart" : "the game says V isn't Immortal";
}

// V's unarmed weapon, drawn while combat and build mode are on (spike 6's "V's weapon in build mode"), and V's meshes,
// hidden while build mode is on (its "V unseen in build mode"): each through the module, logged when what it says changes.
void SwitchV(bool aBuild, bool aVFound, double aNow)
{
	if (const std::optional<bool> arm = g_armed.Frame(g_combat && aBuild, aVFound, aNow))
	{
		std::string                  why;
		const std::optional<int32_t> result = g_script.Arm(*arm, why);
		if (!*arm || result != g_lastArm)
		{
			Log("combat: V's unarmed weapon {}: {}", *arm ? "drawn" : "put away", result ? ArmWords(*result, *arm) : "not done: " + why);
		}
		g_lastArm = *arm ? result : std::nullopt;
	}
	if (const std::optional<bool> hide = g_hidden.Frame(g_hideV && aBuild, aVFound, aNow))
	{
		std::string                      why;
		const double                     start = cpmc::SteadySeconds();
		const std::optional<cpmc::VLook> look = g_script.HideV(*hide, why);
		const double                     ms = (cpmc::SteadySeconds() - start) * 1000.0;
		if (!*hide || look != g_lastHide)
		{
			Log("build: V's meshes {}: {} ({:.2f} ms)", *hide ? "hidden" : "shown", look ? HideWords(*look, *hide) : "not done: " + why, ms);
		}
		g_lastHide = *hide ? look : std::nullopt;
		g_vLook = *hide ? look.value_or(cpmc::VLook{}) : cpmc::VLook{};
	}
	else if (!g_hidden.On())
	{
		g_vLook = {};
	}
}

// Spike 6's combat, after the link's update and before the NPCs are listed (its spec's "The frame"): Minecraft's blasts,
// hits and deaths in the ring's order, a blast before its own hits, and the blasts' pushes as they come due; our hits the
// game has dealt, those it dealt nothing and why, and those it hasn't said within kHitTrail; V's god mode, with V's health
// share for Minecraft as it goes on, V's unarmed weapon and V's meshes; and whether V's hurts are taken.
void Fight(double aNow, const std::optional<cpmc::Vec3d>& aFeet)
{
	const bool build = g_router->On();
	if (g_wasBuild && !build)
	{
		g_buildEndedAt = aNow;
	}
	g_wasBuild = build;
	const bool vFound = aFeet.has_value();
	for (const skycraft::proto::McEvent& event : std::exchange(g_events, {}))
	{
		if (event.type == skycraft::proto::kEvExplosion)
		{
			BlastIn(event, aNow, vFound && g_linked);
		}
		else if (event.type == skycraft::proto::kEvHitActor)
		{
			HitOnNpc(event, aNow, vFound && g_linked);
		}
		else if (event.type == skycraft::proto::kEvPlayerDied)
		{
			MinecraftDied(event, aNow, vFound);
		}
	}
	for (const cpmc::link::Blast& blast : g_blasts.Pushes(aNow))
	{
		PushBlast(blast, aNow, vFound && g_linked);
	}
	for (const cpmc::CombatInbox::Landed& landed : cpmc::CombatReported().TakeLanded())
	{
		++g_hitsLanded;
		const auto pending = std::find_if(g_pendingHits.begin(), g_pendingHits.end(),
		                                  [&](const PendingHit& aHit) { return aHit.entity == landed.target; });
		const uint32_t formId = pending != g_pendingHits.end() ? pending->formId : g_actorIds.IdOf(landed.target).value_or(0);
		const double   damage = pending != g_pendingHits.end() ? pending->damage : 0.0;
		const double   start = pending != g_pendingHits.end() ? pending->amount : 0.0;
		const bool     blast = pending != g_pendingHits.end() && pending->blast;
		if (pending != g_pendingHits.end())
		{
			g_pendingHits.erase(pending);
		}
		const std::string how = cpmc::link::LandedWords(landed.flags);
		g_lastHit = std::format("NPC {} ({}): {:.2f} of Minecraft's damage{}, started at {:.2f}, {:.2f} dealt{}; {:.0f} -> {:.0f}", formId,
		                        NpcName(formId), damage, blast ? ", a blast's" : "", start, landed.amount, how.empty() ? "" : " (" + how + ")",
		                        landed.before, landed.after);
		Log("{}hit on NPC {} landed: started at {:.2f}, {:.2f} dealt{}; its health {:.0f} -> {:.0f}", blast ? "blast " : "", formId, start,
		    landed.amount, how.empty() ? "" : " (" + how + ")", landed.before, landed.after);
	}
	for (const cpmc::CombatInbox::Ended& ended : cpmc::CombatReported().TakeEnded())
	{
		const auto pending = std::find_if(g_pendingHits.begin(), g_pendingHits.end(),
		                                  [&](const PendingHit& aHit) { return aHit.entity == ended.target; });
		const uint32_t formId = pending != g_pendingHits.end() ? pending->formId : g_actorIds.IdOf(ended.target).value_or(0);
		const bool     blast = pending != g_pendingHits.end() && pending->blast;
		if (pending != g_pendingHits.end())
		{
			g_pendingHits.erase(pending);
		}
		const std::string why = cpmc::link::HitEndedName(ended.why);
		++g_hitDrops[why];
		Log("{}hit on NPC {} dealt nothing: {}", blast ? "blast " : "", formId, why);
	}
	std::erase_if(g_pendingHits, [&](const PendingHit& aHit) {
		if (aNow - aHit.at <= cpmc::link::kHitTrail)
		{
			return false;
		}
		++g_hitDrops["not dealt by the game"];
		Log("{}hit on NPC {} (starting at {:.2f}) wasn't dealt by the game within {:.0f} s, nor said why", aHit.blast ? "blast " : "",
		    aHit.formId, aHit.amount, cpmc::link::kHitTrail);
		return true;
	});
	if (const std::optional<bool> change = g_godMode.Frame(g_combat, build, vFound))
	{
		std::string               why;
		const std::optional<bool> immortal = g_script.SetImmortal(*change, why);
		if (immortal)
		{
			g_immortal = *immortal;
		}
		Log("combat: V's Immortal {}: {}", *change ? "on" : "off", immortal ? ImmortalWords(*change, *immortal) : "not done: " + why);
		if (*change)
		{
			CarryHealth();
		}
	}
	SwitchV(build, vFound, aNow);
	cpmc::CombatReported().SetTaking(g_combat && build && vFound && g_vHealthMax > 0.0);
	const bool dead = g_link && (g_link->State().minecraftFlags & skycraft::proto::kMcDead) != 0;
	if (!dead)
	{
		g_minecraftDiedAt.reset();
	}
	else if (!g_minecraftDiedAt)
	{
		g_minecraftDiedAt = aNow;
	}
}

// V's hurts the pipeline's wrap took since the last frame (spike 6's "The game's hits on V"), to Minecraft as kInHurt,
// through the sums. What the input ring has no room for waits for the next frame.
void SendHurts(double aNow)
{
	if (!g_link)
	{
		return;
	}
	const double tickMs = g_link->State().minecraftTickMs;
	for (const cpmc::link::GameHurt& taken : cpmc::CombatReported().TakeHurts())
	{
		const cpmc::link::Hurt hurt = cpmc::link::HurtFrom(taken, g_vHealthMax, g_actorIds);
		g_lastHurt = std::format("{:.1f} of V's health ({:.4f}), {:.2f} of Minecraft's 20, {} ({}){}{}", taken.amount, hurt.share,
		                         hurt.share * 20.0, cpmc::link::HurtKindName(hurt.kind), cpmc::link::AttackTypeName(taken.attackType),
		                         hurt.attacker != 0 ? std::format(", from NPC {}", hurt.attacker) : std::string(),
		                         hurt.power ? ", a power attack" : "");
		Log("combat: V hurt, taken for Minecraft: {}", g_lastHurt);
		if (const std::optional<cpmc::link::Hurt> now = g_hurtSums.Add(hurt, aNow, tickMs))
		{
			g_hurtsOut.push_back(*now);
		}
	}
	if (const std::optional<cpmc::link::Hurt> due = g_hurtSums.Due(aNow, tickMs))
	{
		g_hurtsOut.push_back(*due);
	}
	size_t sent = 0;
	while (sent < g_hurtsOut.size() && g_link->Push(cpmc::link::HurtInput(g_hurtsOut[sent])))
	{
		++sent;
	}
	g_hurtsSent += sent;
	g_hurtsOut.erase(g_hurtsOut.begin(), g_hurtsOut.begin() + ptrdiff_t(sent));
}

// The hits not applied, by reason: "2 not listed, 1 the NPC is dead".
std::string HitDrops()
{
	std::string text;
	for (const auto& [why, count] : g_hitDrops)
	{
		text += std::format("{}{} {}", text.empty() ? "" : ", ", count, why);
	}
	return text;
}

// Why combat can't do all it should, while combat is on.
std::string CombatWhy()
{
	if (!g_combat)
	{
		return {};
	}
	if (g_punch <= 0.0)
	{
		return "V's punch isn't read yet, so hits aren't applied";
	}
	if (g_vHealthMax <= 0.0)
	{
		return "V's max health isn't read yet, so V's hurts aren't taken";
	}
	return {};
}

// The threads the game has called the module's wraps on (NPCPuppet's OnGameAttached and OnDetach), logged whenever a new
// one shows: step 2 crashed while the wraps kept the list in the module's own arrays, on several threads at once.
void LogWrapThreads()
{
	const std::vector<cpmc::NpcRegistry::ThreadCalls> threads = cpmc::NpcsReported().Threads();
	if (threads.size() == g_wrapThreads)
	{
		return;
	}
	g_wrapThreads = threads.size();
	Log("npcs: the wraps have run on {} threads: {}", threads.size(), cpmc::DescribeThreads(threads, g_mainThread));
}

// The NPC behind the table's record aIndex, as the module last listed it; null if there's none.
const cpmc::link::NpcSample* ListedNpc(size_t aIndex)
{
	if (!g_npcList || aIndex >= g_listedEntities.size())
	{
		return nullptr;
	}
	const auto npc = std::find_if(g_npcList->npcs.begin(), g_npcList->npcs.end(),
	                              [&](const cpmc::link::NpcSample& aNpc) { return aNpc.entity == g_listedEntities[aIndex]; });
	return npc != g_npcList->npcs.end() ? &*npc : nullptr;
}

// An NPC's distance from V's feet, straight-line, when it was listed.
double DistanceFromV(const cpmc::link::NpcSample& aNpc)
{
	const cpmc::Vec3d feet = g_npcsFeet.value_or(aNpc.position);
	return std::hypot(aNpc.position.x - feet.x, aNpc.position.y - feet.y, aNpc.position.z - feet.z);
}

void Place(const cpmc::Request& aRequest)
{
	if (g_minecraftAtlas)
	{
		return Report("place " + aRequest.what + ": refused while Minecraft's atlas is loaded");
	}
	std::string error;
	if (!g_renderer->AtlasData() && !g_renderer->LoadAtlas(error))
	{
		return Report("place " + aRequest.what + ": " + error);
	}
	const std::optional<cpmc::PlayerPose> pose = cpmc::LocatePlayer();
	if (!pose)
	{
		return Report("place " + aRequest.what + ": there's no player");
	}
	const cpmc::BlockPos              origin = cpmc::PlacementOrigin(pose->feet, pose->forward);
	const cpmc::Atlas&                atlas = *g_renderer->AtlasData();
	const std::optional<cpmc::Placed> placed = aRequest.what == "field"
	                                               ? cpmc::PlaceField(g_world, atlas, origin, size_t(aRequest.number), error)
	                                               : cpmc::PlaceStructure(g_world, atlas, aRequest.what, origin, error);
	if (!placed)
	{
		return Report("place " + aRequest.what + ": " + error);
	}
	const auto [low, high] = cpmc::WorldBox(*placed);
	g_structures.push_back({ aRequest.what, low, high });
	Report(std::format("placed {}: {} blocks, Minecraft {} {} {} to {} {} {}", aRequest.what, placed->blocks, placed->min.x, placed->min.y,
	                   placed->min.z, placed->max.x, placed->max.y, placed->max.z));
}

// Spike 4's settings (the spec's "Requests"). True if aRequest names one of them.
bool SetLinkSetting(const cpmc::Request& aRequest)
{
	const std::string& name = aRequest.what;
	const double       value = aRequest.number;
	if (name == "translucent")
	{
		if (value != 0.0 && value != 1.0)
		{
			Report("set translucent: 0 drops translucent triangles, and 1 draws them as cut-outs");
			return true;
		}
		g_linkSettings.keepTranslucent = value == 1.0;
	}
	else if (name == "teleportstep" || name == "teleportrate")
	{
		if (value <= 0.0)
		{
			Report(std::format("set {}: it's above 0", name));
			return true;
		}
		(name == "teleportstep" ? g_linkSettings.teleportStep : g_linkSettings.teleportRate) = value;
	}
	else if (name == "ringmb")
	{
		if (value < 1.0 || value > 64.0)
		{
			Report("set ringmb: from 1 to 64");
			return true;
		}
		g_linkSettings.ringBudget = uint64_t(value * 1048576.0);
	}
	else if (name == "hud")
	{
		if (value != 0.0 && value != 1.0 && value != 2.0)
		{
			Report("set hud: 0 never, 1 in build mode, 2 always");
			return true;
		}
		g_hudSetting = int(value);
	}
	else if (name == "hudscale")
	{
		if (value != 1.0 && value != 2.0 && value != 4.0)
		{
			Report("set hudscale: 1, 2 or 4");
			return true;
		}
		g_hudScale = uint32_t(value);
	}
	else if (name == "ground")
	{
		std::string why;
		if (value == 1.0 && !g_physx.Audited(why))
		{
			Report("set ground city: " + why + ", so the ground stays the floor");
			return true;
		}
		g_linkSettings.ground = value == 1.0 ? cpmc::link::Ground::City : cpmc::link::Ground::Floor;
		g_groundWhy.clear();
		Report(std::format("set ground {}", cpmc::link::GroundName(g_linkSettings.ground)));
		return true;
	}
	else if (name == "exportbudget")
	{
		if (value < 0.1 || value > 20.0)
		{
			Report("set exportbudget: from 0.1 to 20 ms a frame");
			return true;
		}
		g_exportBudget = value;
	}
	else if (name == "turn")
	{
		if (value != 0.0 && value != 1.0)
		{
			Report("set turn: 0 teleports V with its own yaw, and 1 adds the frame's turn from the mouse");
			return true;
		}
		g_turn = value == 1.0;
	}
	else
	{
		return false;
	}
	Report(std::format("set {} {}", name, value));
	return true;
}

void Set(const cpmc::Request& aRequest)
{
	if (SetLinkSetting(aRequest))
	{
		return;
	}
	if (aRequest.what == "ao")
	{
		g_meshOptions.ambientOcclusion = aRequest.number != 0.0;
		g_world.MarkAllDirty();
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
	}
	if (aRequest.what == "combat")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0)
		{
			return Report("set combat: 1 lands Minecraft's hits on NPCs as V's, and in build mode gives the game's hits on V to Minecraft; "
			              "0 only logs hits");
		}
		g_combat = aRequest.number == 1.0;
		return Report(g_combat ? "set combat 1: Minecraft's hits and blasts land on NPCs as V's; in build mode V is Immortal with its "
		                         "unarmed weapon drawn, Minecraft's player takes V's health share as build mode starts, the game's hits on V "
		                         "go to Minecraft's player, and its death kills V"
		                       : "set combat 0: Minecraft's hits and blasts are only logged, and V's health is V's");
	}
	if (aRequest.what == "hidev")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0)
		{
			return Report("set hidev: 1 hides V's meshes while build mode is on, so only Minecraft's view shows; 0 leaves V seen");
		}
		g_hideV = aRequest.number == 1.0;
		return Report(g_hideV ? "set hidev 1: V's meshes are hidden while build mode is on" : "set hidev 0: V stays seen in build mode");
	}
	if (aRequest.what == "hitpower")
	{
		if (aRequest.number < 0.0 || aRequest.number > 1.0)
		{
			return Report("set hitpower: from 0 to 1, the power of Minecraft's damage that V's punch is multiplied by: 0.5, the default, is "
			              "the square root; 1 makes each point of damage a whole punch, 0 every hit one punch");
		}
		g_hitPower = aRequest.number;
		return Report(std::format("set hitpower {:.2f}: a hit starts at V's punch x Minecraft's damage^{:.2f}: a fist (1) one punch, a "
		                          "diamond sword (7) {:.2f} punches",
		                          g_hitPower, g_hitPower, std::pow(7.0, g_hitPower)));
	}
	if (aRequest.what == "blastpower")
	{
		if (aRequest.number < 0.0 || aRequest.number > 1.0)
		{
			return Report("set blastpower: from 0 to 1, the power of Minecraft's damage that V's punch is multiplied by for a blast's own "
			              "hits: 0.5, the default, is the square root");
		}
		g_blastPower = aRequest.number;
		return Report(std::format("set blastpower {:.2f}: a blast's hits start at V's punch x Minecraft's damage^{:.2f}: TNT's 57 at its "
		                          "centre {:.2f} punches, its 1 at the edge one punch",
		                          g_blastPower, g_blastPower, std::pow(57.0, g_blastPower)));
	}
	if (aRequest.what == "blastknock")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0)
		{
			return Report("set blastknock: 1, the default, knocks down each living NPC within a blast's reach with the game's forced "
			              "reaction, unless the game keeps it from being knocked down; 0 knocks none down");
		}
		g_blastKnock = aRequest.number == 1.0;
		return Report(g_blastKnock ? "set blastknock 1: each living NPC within a blast's reach is knocked down by the game's forced reaction"
		                           : "set blastknock 0: a blast knocks no NPC down; its attack, sound, own hits and push stay");
	}
	if (aRequest.what == "bushrule")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0 && aRequest.number != 2.0)
		{
			return Report("set bushrule: what else the city's shape leaves out for Minecraft: 0 nothing, 1 sight-only shapes (a bush's "
			              "foliage, the default), 2 unsimulated shapes");
		}
		g_ground.SetRule(cpmc::px::LeaveOut(int(aRequest.number)));
		if (g_link)
		{
			g_link->RestartCity();
		}
		return Report(std::format("set bushrule {:.0f}: the city's shape leaves out {}, and Minecraft gets it again", aRequest.number,
		                          cpmc::px::LeaveOutName(g_ground.Rule())));
	}
	if (aRequest.what == "npcs")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0)
		{
			return Report("set npcs: 0 empties SkyCraft's actor table, and 1 lists the NPCs around V into it");
		}
		g_npcs = aRequest.number == 1.0;
		return Report(g_npcs ? "set npcs 1: the NPCs around V go into SkyCraft's actor table while linked"
		                     : "set npcs 0: SkyCraft's actor table is emptied, and Minecraft's stand-ins go");
	}
	if (aRequest.what == "blocks")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0)
		{
			return Report("set blocks: 0 leaves Minecraft's blocks without bodies, and 1 gives those around V bodies V stands on");
		}
		g_blocks = aRequest.number == 1.0;
		return Report(!g_blocks                            ? "set blocks 0: the bodies go"
		              : g_renderer->ModeName() == "off" ? "set blocks 1: the bodies come once drawing is on"
		                                                  : "set blocks 1: Minecraft's blocks around V get bodies");
	}
	if (aRequest.what.starts_with("hooks."))
	{
		const std::string_view               name = std::string_view(aRequest.what).substr(6);
		const std::optional<cpmc::HookGroup> group = cpmc::HookGroupNamed(name);
		if (!group)
		{
			return Report(std::format("set: there's no hook group called \"{}\": they're lists, views, submit, present and sl", name));
		}
		cpmc::SetHookGroup(*group, aRequest.number != 0.0);
		return Report(std::format("set {} {}: {}", aRequest.what, aRequest.number != 0.0 ? 1 : 0, cpmc::HookGroupsSummary()));
	}
	if (aRequest.what == "tracking")
	{
		const bool hold = aRequest.number != 0.0;
		if (hold != g_trackingHeld)
		{
			hold ? cpmc::d3d::RequestTracking() : cpmc::d3d::ReleaseTracking();
			g_trackingHeld = hold;
		}
		return Report(std::format("set tracking {}: command lists are {}tracked", hold ? 1 : 0, cpmc::d3d::Tracking() ? "" : "not "));
	}
	std::string error;
	Report(g_renderer->Set(aRequest.what, aRequest.number, error) ? std::format("set {} {}", aRequest.what, aRequest.number)
	                                                              : "set: " + error);
}

// link on|off (the spec's "Linking"): linking also switches drawing to gbuffer if it's off. aBy says what asked, when it
// wasn't a link request.
void LinkRequest(bool aOn, std::string_view aBy = {})
{
	g_linked = aOn;
	if (!aOn)
	{
		return Report("link off: Minecraft holds its player where it is, and the blocks stay");
	}
	std::string why;
	if (g_renderer->ModeName() == "off" && !g_renderer->SetMode("gbuffer", why))
	{
		return Report(std::format("{}link on, but drawing stays off: {}", aBy, why));
	}
	const std::string link = g_link ? "Minecraft's player follows V" : "it starts once the mapping opens, which is " + g_mappingState;
	Report(std::format("{}link on: {}; drawing is {}", aBy, link, g_renderer->ModeName()));
}

// build on|off (the spec's "Requests"): as F8. On links first, if it isn't linked, and goes on after the link's next
// update, once it knows whether V and Minecraft are there.
void BuildRequest(bool aOn)
{
	if (!aOn)
	{
		return Report(g_router->TurnOff("a build off request") ? "build off: Minecraft lets go of everything"
		                                                         : "build off: it's off already");
	}
	if (g_router->On())
	{
		return Report("build on: it's on already");
	}
	if (!g_linked)
	{
		LinkRequest(true, "build on: ");
	}
	g_buildWanted = true;
}

void TypeRequest(const std::string& aWhat)
{
	if (!g_link)
	{
		return Report("mc: there's no link: the mapping is " + g_mappingState);
	}
	std::string error;
	if (!g_link->Type(aWhat, error))
	{
		return Report("mc: " + error);
	}
	g_lastTyped = aWhat;
	Report("mc " + aWhat + ": typing it into Minecraft's chat");
}

// The scene watch's summary (spike 5's "The scene watch"): each thread's calls on scene 0.
std::string WatchSummary()
{
	const cpmc::px::SceneRecorder& recorder = cpmc::px::Recorder();
	return cpmc::px::WatchSummary(recorder.Threads(), g_mainThread, recorder.Shifts(), recorder.ShiftCount());
}

// scenewatch on|off (spike 5's "The scene watch"): hooks on scene 0's calls, which the status sums by thread.
void SceneWatch(bool aOn)
{
	if (!aOn)
	{
		if (!cpmc::px::SceneHooksInstalled())
		{
			return Report("scenewatch off: it's off already");
		}
		cpmc::px::RemoveSceneHooks();
		g_lastWatch = std::format("{:.0f} s: {}", cpmc::SteadySeconds() - *g_watchSince, WatchSummary());
		g_watchSince.reset();
		Log("scene watch, {}", g_lastWatch);
		return Report("scenewatch off: the hooks are out, and the status keeps what the watch saw");
	}
	if (cpmc::px::SceneHooksInstalled())
	{
		return Report("scenewatch on: it's on already");
	}
	std::string           why;
	physx::PxScene* const scene = g_physx.Scene0(why);
	if (!scene || !cpmc::px::InstallSceneHooks(scene, g_physx.Bases().physx3, why))
	{
		return Report("scenewatch: " + why);
	}
	g_watchSince = cpmc::SteadySeconds();
	Report(std::format("scenewatch on: {} of scene 0's calls hooked; from a second on, the status sums them by thread",
	                   cpmc::px::kSceneCalls));
}

// shapes (spike 5's "Requests"): every shape within 2 m of V's middle, into the log with its filters. Where Minecraft's
// player and V disagree about the ground, these say why.
void Shapes()
{
	const std::optional<cpmc::PlayerPose> pose = cpmc::LocatePlayer();
	if (!pose)
	{
		return Report("shapes: there's no player");
	}
	g_ground.Frame(pose->feet);
	const std::vector<std::string> lines = g_ground.Describe({ pose->feet.x, pose->feet.y, pose->feet.z + 1.0 }, 2.0);
	for (const std::string& line : lines)
	{
		Log("shapes: {}", line);
	}
	Report(std::format("shapes: {} lines in the log, of the shapes within 2 m of V's middle", lines.size()));
}

// npcs (spike 6's "Requests and status"): every NPC listed, into the log, with its number, name, distance, box and where
// the box came from, health, level and flags; V's punch and max health as the module reads them; and how many NPCs the
// game's targeting finds within 80 m, with those the list lacks.
void NpcsRequest()
{
	if (!g_npcList)
	{
		return Report("npcs: none listed: " + (g_npcsWhy.empty() ? std::string("set npcs is 0") : g_npcsWhy));
	}
	Log("npcs: {} listed of {} in the world; V's punch {:.2f}, read from {}, {}; V's max health {:.0f}", g_actorRecords.size(),
	    g_npcList->attached, g_npcList->punch, g_npcList->punchItem, g_npcList->punchDrawn ? "drawn" : "not drawn", g_npcList->vHealthMax);
	for (size_t i = 0; i < g_actorRecords.size(); ++i)
	{
		if (const cpmc::link::NpcSample* npc = ListedNpc(i))
		{
			Log("npcs: {}", cpmc::px::DescribeNpc(*npc, g_actorRecords[i].formId, DistanceFromV(*npc), g_npcSizes));
		}
	}
	Log("npcs: {}", g_script.Compare(cpmc::link::kActorRange));
	const cpmc::NpcRegistry::Counts counts = cpmc::NpcsReported().Count();
	Log("npcs: the wraps' list holds {}, of which the module found {} by their EntityIDs; {} attached and {} detached since the game "
	    "started, {} already there, {} not there, {} past the list's {}",
	    counts.ids, g_npcList->found, counts.attached, counts.detached, counts.repeats, counts.strays, counts.full,
	    cpmc::NpcRegistry::kMaxIds);
	Log("npcs: the wraps ran on {}", cpmc::DescribeThreads(cpmc::NpcsReported().Threads(), g_mainThread));
	Report(std::format("npcs: {} NPCs in the log, and what the game's targeting finds", g_actorRecords.size()));
}

// hit <id> <damage> [strong] (spike 6's step 4): as if Minecraft hit NPC <id> for <damage>, through all its hits go
// through; strong as a crit is.
void HitRequest(const cpmc::Request& aRequest)
{
	skycraft::proto::McEvent event{};
	event.type = skycraft::proto::kEvHitActor;
	event.formId = aRequest.id;
	event.a = float(aRequest.number);
	event.d = 0.4f;  // Minecraft's own knockback
	event.flags = aRequest.what == "strong" ? skycraft::proto::kHitCritical : 0u;
	event.weapon = skycraft::proto::kWeaponUnarmed;
	g_events.push_back(event);
	Report(std::format("hit: as if Minecraft hit NPC {} for {:.2f}{}; the log says how it went", aRequest.id, aRequest.number,
	                   aRequest.what == "strong" ? ", strong" : ""));
}

// explode <metres> <radius> (spike 6's step 5b): a blast that far ahead of V, at the height of V's feet, as if it were
// Minecraft's, through all that Minecraft's blasts go through. With no stand-in hit by it, it knocks down, pushes and
// sounds, and deals nothing.
void ExplodeRequest(const cpmc::Request& aRequest)
{
	const std::optional<cpmc::PlayerPose> pose = cpmc::LocatePlayer();
	if (!pose)
	{
		return Report("explode: there's no player");
	}
	const double      flat = std::hypot(double(pose->forward.x), double(pose->forward.y));
	const double      east = flat > 1e-6 ? pose->forward.x / flat : 0.0;
	const double      north = flat > 1e-6 ? pose->forward.y / flat : 1.0;
	const cpmc::Vec3d centre{ pose->feet.x + east * aRequest.number, pose->feet.y + north * aRequest.number, pose->feet.z };
	const cpmc::Vec3d mc = cpmc::McFromWorld(centre);
	skycraft::proto::McEvent event{};
	event.type = skycraft::proto::kEvExplosion;
	event.a = float(mc.x);
	event.b = float(mc.y);
	event.c = float(mc.z);
	event.d = float(aRequest.radius);
	g_events.push_back(event);
	Report(std::format("explode: as if Minecraft blew up {:.1f} m ahead of V, at world ({:.2f}, {:.2f}, {:.2f}), radius {:.1f}; the log "
	                   "says how it went",
	                   aRequest.number, centre.x, centre.y, centre.z, aRequest.radius));
}

// box on|off (spike 5's step 4): a test box 3 m ahead of V, made as the bodies for blocks are, before any block is.
void BoxRequest(bool aOn)
{
	const std::optional<cpmc::PlayerPose> player = cpmc::LocatePlayer();
	if (!player)
	{
		return Report(std::format("box {}: there's no player", aOn ? "on" : "off"));
	}
	g_ground.Frame(player->feet);  // scene 0 and PhysX's origin this frame, from V's controller
	Report(g_bodies.Box(aOn, player->feet, player->forward));
}

// entities (spike 7): the newest scene batch by batch, the textures and the world entities, into the log.
void EntitiesRequest()
{
	for (const std::string& line : g_entities.Lines(g_renderer->GlowLight(), g_entityTextureSizes))
	{
		Log("entities: {}", line);
	}
	std::string textures;
	for (const auto& [id, size] : g_entityTextureSizes)
	{
		textures += std::format("{}{} ({} x {})", textures.empty() ? "" : ", ", id, size.first, size.second);
	}
	Log("entities: {} textures: {}", g_entityTextureSizes.size(), textures.empty() ? "none" : textures);
	Report("entities: the newest scene, the textures and the world entities, in the log");
}

// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
// the overlay's, where its buttons do. The overlay can't rename a finished file into place, so there a command counts
// only once its line ends (aWholeLine).
void RunRequest(const std::filesystem::path& aFolder, bool aWholeLine)
{
	const std::filesystem::path path = aFolder / "blockrender.request";
	std::error_code             error;
	if (!std::filesystem::exists(path, error))
	{
		return;
	}
	const std::string text = ReadSmallFile(path);
	if (aWholeLine && (text.empty() || text.back() != '\n'))
	{
		return;  // still being written: read it next second
	}
	std::filesystem::remove(path, error);
	std::string                        why;
	const std::optional<cpmc::Request> request = cpmc::ParseRequest(text, why);
	if (!request)
	{
		return Report(std::format("\"{}\" ignored: {}", Printable(text), why));
	}
	switch (request->kind)
	{
	case cpmc::Request::Kind::Log: Report(g_frameLog->Arm() ? "log: armed for the next two frames" : "log: one is already running"); break;
	case cpmc::Request::Kind::Dump:
		Report(g_dumps->Arm(request->what, why) ? "dump " + request->what + ": armed for the next frame" : "dump: " + why);
		break;
	case cpmc::Request::Kind::Mode:
		Report(g_renderer->SetMode(request->what, why) ? "mode " + request->what : "mode " + request->what + ": " + why);
		break;
	case cpmc::Request::Kind::Place: Place(*request); break;
	case cpmc::Request::Kind::Clear:
		g_world.Clear();
		g_structures.clear();
		g_renderer->ClearSections();
		Report(g_minecraftAtlas ? "cleared: Minecraft's sections come back as they change, or when it next sends everything" : "cleared");
		break;
	case cpmc::Request::Kind::Reload: Report(g_renderer->ReloadShaders(why) ? "reload: shaders compiled" : "reload: " + why); break;
	case cpmc::Request::Kind::Set: Set(*request); break;
	case cpmc::Request::Kind::Link: LinkRequest(request->what == "on"); break;
	case cpmc::Request::Kind::Floor:
		if (!g_link)
		{
			Report("floor: there's no link: the mapping is " + g_mappingState);
			break;
		}
		g_link->RequestFloor();
		Report("floor: a fresh one at the next update, once Minecraft is there");
		break;
	case cpmc::Request::Kind::Mc: TypeRequest(request->what); break;
	case cpmc::Request::Kind::Build: BuildRequest(request->what == "on"); break;
	case cpmc::Request::Kind::DriveLog:
		Report(g_driveLog.Start(request->number) ? std::format("drivelog: recording the drive for {:g} s", request->number)
		                                         : "drivelog: one is already recording");
		break;
	case cpmc::Request::Kind::SceneWatch: SceneWatch(request->what == "on"); break;
	case cpmc::Request::Kind::Shapes: Shapes(); break;
	case cpmc::Request::Kind::Box: BoxRequest(request->what == "on"); break;
	case cpmc::Request::Kind::Npcs: NpcsRequest(); break;
	case cpmc::Request::Kind::Hit: HitRequest(*request); break;
	case cpmc::Request::Kind::Punch:
		Log("punch: {}", g_script.Punch());
		Report("punch: V's punch, what it's read from, and what hits are made with, in the log");
		break;
	case cpmc::Request::Kind::Explode: ExplodeRequest(*request); break;
	case cpmc::Request::Kind::Entities: EntitiesRequest(); break;
	}
}

// If the device or the swap chain was made before BlockRender loaded, the detours never saw it: patch it through
// GpuApi instead (risk 1), once its vtable checks out.
void PatchLate()
{
	if (cpmc::d3d::Device() && cpmc::d3d::Original().present)
	{
		return;
	}
	const cpmc::gpuapi::Report report = cpmc::gpuapi::Check(cpmc::d3d::Device());
	if (!cpmc::d3d::Device() && report.nativeDevice)
	{
		cpmc::d3d::PatchLateDevice(report.nativeDevice);
		Log("the device was made before BlockRender loaded: patched it through GpuApi ({})", report.device);
	}
	if (!cpmc::d3d::Original().present && report.swapChainObject)
	{
		void* native = cpmc::slhooks::NativeInterface(report.swapChainObject);  // null unless it's a Streamline proxy
		cpmc::d3d::PatchLateSwapChain(native ? static_cast<IDXGISwapChain*>(native) : report.swapChainObject);
		if (native)
		{
			static_cast<IUnknown*>(native)->Release();
		}
		Log("the swap chain was made before BlockRender loaded: patched it through GpuApi ({})", report.swapChain);
	}
}

void CheckDevice()
{
	ID3D12Device* const device = cpmc::d3d::Device();
	if (!device || g_removalReported)
	{
		return;
	}
	const HRESULT reason = device->GetDeviceRemovedReason();
	if (reason == S_OK)
	{
		return;
	}
	g_removalReported = true;
	std::string error;
	g_renderer->SetMode("off", error);
	Log("the D3D12 device was removed (0x{:08x}), so drawing is off", uint32_t(reason));
	for (const std::string& line : cpmc::d3d::DredReport())
	{
		Log("{}", line);
	}
}

void WriteFrameLog(const cpmc::FrameLogData& aLog, const std::string& aFolderName)
{
	const std::filesystem::path folder = g_pluginDir / "logs" / aFolderName;
	std::error_code             error;
	std::filesystem::create_directories(folder, error);
	std::ofstream out(folder / "frame.json", std::ios::binary);
	out << cpmc::FrameLogJson(aLog);
	if (!out)
	{
		return Report("log: couldn't write " + (folder / "frame.json").string());
	}
	Report(std::format("log: written to logs\\{}\\frame.json ({} evaluates, {} submissions, {} lists, {} G-buffer candidates)",
	                   aFolderName, aLog.evaluates.size(), aLog.submissions.size(), aLog.lists.size(), aLog.gbufferCandidates.size()));
}

void WriteDriveLog(const cpmc::DriveLog::Finished& aLog, const std::string& aFolderName)
{
	const std::filesystem::path folder = g_pluginDir / "logs" / aFolderName;
	std::error_code             error;
	std::filesystem::create_directories(folder, error);
	std::ofstream out(folder / "drive.csv", std::ios::binary);
	out << aLog.csv;
	if (!out)
	{
		return Report("drivelog: couldn't write " + (folder / "drive.csv").string());
	}
	Report(std::format("drivelog: written to logs\\{}\\drive.csv ({} frames)", aFolderName, aLog.rows));
}

// turn.txt keeps the turn ratio for the next session, so build mode turns at once after a game start: saved once this
// session has measured its own, and again whenever that changes, written whole and then renamed into place.
void SaveTurnRatio()
{
	const std::optional<double> measured = g_turnRatio.Measured();
	if (!measured || (g_savedTurn && std::abs(*measured - *g_savedTurn) < 1e-6))
	{
		return;
	}
	const std::filesystem::path temporary = g_pluginDir / "turn.tmp";
	{
		std::ofstream out(temporary, std::ios::binary);
		out << cpmc::link::TurnFileText(*measured);
	}
	std::error_code error;
	std::filesystem::rename(temporary, g_pluginDir / "turn.txt", error);
	if (error)
	{
		return;  // tried again next second
	}
	g_savedTurn = measured;
	if (!std::exchange(g_turnSaveLogged, true))
	{
		Log("the turn: {:.4f} degrees a count, measured, and saved in turn.txt for the next session", *measured);
	}
}

// The link's lines of the status (the spec's "Measurements"), from the second since the last status.
cpmc::LinkReport LinkLines(const cpmc::Renderer::Stats& aStats, double aNow)
{
	cpmc::LinkReport report;
	report.mapping = g_mappingState;
	report.linked = g_linked;
	report.camera = g_camera;
	const cpmc::input::InputReport input = g_router->Report();
	report.inputHooked = g_inputHooked;
	report.buildMode = input.on;
	report.buildWhy = input.why;
	report.slot = input.slot;
	report.inputsRead = input.read;
	report.inputsForwarded = input.forwarded;
	report.inputsHidden = input.hidden;
	report.lastInput = input.last;
	if (g_link)
	{
		const cpmc::link::Link::Status& state = g_link->State();
		const double                    seconds = std::max(aNow - g_second.start, 0.001);
		const uint64_t                  frames = state.minecraftFrames >= g_second.minecraftFramesAtStart
		                                             ? state.minecraftFrames - g_second.minecraftFramesAtStart
		                                             : state.minecraftFrames;  // a new Minecraft counts from 0
		report.minecraft = state.minecraft;
		report.minecraftPid = state.minecraftPid;
		report.heartbeatAgeMs = state.heartbeatAgeMs;
		report.inWorld = state.inWorld;
		report.screenOpen = state.screenOpen;
		report.minecraftFps = double(frames) / seconds;
		report.inGame = state.inGame;
		report.minecraftEye = state.minecraftEye;
		report.teleports = state.teleports - g_second.teleportsAtStart;
		report.target = state.target;
		report.floorEpoch = state.collisionEpoch;
		report.floorSent = state.floorSent;
		report.floorCentre = state.floorCentre;
		report.inputsDropped = state.inputsDropped;
		report.ringMb = double(g_second.bytes) / 1048576.0;
		report.ringMbWorstFrame = double(g_second.bytesWorstFrame) / 1048576.0;
		report.drainMsAverage = g_second.frames ? g_second.drainMsSum / double(g_second.frames) : 0.0;
		report.drainMsWorst = g_second.drainMsWorst;
		for (size_t type = 0; type < cpmc::link::Link::kTypes; ++type)
		{
			const uint64_t messages = state.messages[type] - g_second.messagesAtStart[type];
			if (messages != 0)
			{
				report.ringTypes += std::format("{}{} {} ({:.2f} MB)", report.ringTypes.empty() ? "" : ", ", messages,
				                                cpmc::link::RenderTypeName(type),
				                                double(state.bytes[type] - g_second.bytesAtStart[type]) / 1048576.0);
			}
		}
		report.rejected = state.rejected;
		report.resyncs = state.resyncs;
		report.translucentDropped = state.translucentDropped;
		report.hudRefused = state.hudRefused;
		report.drive = cpmc::link::DriveStateName(state.drive);
		report.driveFor = state.drive == cpmc::link::DriveState::Off ? 0.0 : aNow - state.driveSince;
		report.eyeHeight = state.eyeHeight;
		report.eyeDistanceLast = state.eyeDistance;
		report.eyeDistanceAverage = g_second.eyeDistanceFrames ? g_second.eyeDistanceSum / double(g_second.eyeDistanceFrames) : 0.0;
		report.eyeDistanceWorst = g_second.eyeDistanceWorst;
		report.renderDelayMs = state.renderDelayMs;
		report.waitedFrames = state.waitedFrames - g_second.waitedAtStart;
		report.skippedFrames = state.driveSkipped;
		report.teleportMsAverage = g_second.teleportCalls ? g_second.teleportMsSum / double(g_second.teleportCalls) : -1.0;
		report.teleportMsWorst = g_second.teleportCalls ? g_second.teleportMsWorst : -1.0;
		report.minecraftFlags = state.minecraftFlags;
		report.ground = cpmc::link::GroundName(state.ground);
		report.collisionEpoch = state.collisionEpoch;
		report.cityCentre = state.cityCentre;
		report.cityKnown = state.cityKnown;
		report.cityWaiting = state.cityWaiting;
		report.cityReadMsAverage = g_second.frames ? g_second.cityReadMsSum / double(g_second.frames) : 0.0;
		report.cityReadMsWorst = g_second.cityReadMsWorst;
		report.cityBatches = double(state.cityBatches - g_second.cityBatchesAtStart) / seconds;
		report.cityTriangles = double(state.cityTriangles - g_second.cityTrianglesAtStart) / seconds;
		report.workerBlocks = double(state.workerBlocks - g_second.workerBlocksAtStart) / seconds;
		const uint64_t regions = state.workerRegions - g_second.workerRegionsAtStart;
		const uint64_t ticks = state.workerTicks - g_second.workerTicksAtStart;
		report.workerMsPerRegion = regions ? (state.workerMs - g_second.workerMsAtStart) / double(regions) : -1.0;
		report.workerTrianglesPerRegion = regions ? double(state.workerTriangles - g_second.workerTrianglesAtStart) / double(regions) : 0.0;
		report.workerRunning = ticks ? double(state.workerCycles - g_second.workerCyclesAtStart) / double(ticks) : 0.0;
		report.workerThreads = state.workerThreads;
		report.workerJobs = state.workerJobs;
		report.workerReplaced = state.workerReplaced;
		report.collisionMb = double(state.collisionBytes - g_second.collisionBytesAtStart) / 1048576.0 / seconds;
		report.collisionWaiting = state.collisionWaiting;
		report.collisionDropped = state.collisionDropped;
		report.lastHold = state.lastHold;
		report.solidSections = state.solidSections;
		report.solidBlocks = state.solidBlocks;
	}
	report.groundWhy = g_groundWhy;
	const cpmc::px::PhysxGround::Report& physx = g_ground.State();
	report.physxOrigin = physx.origin;
	report.controllerSearches = physx.searches;
	report.shapesKept = physx.kept;
	report.shapesLeft = physx.left;
	report.shapesUnreadable = physx.unreadable;
	report.bushRule = cpmc::px::LeaveOutName(g_ground.Rule());
	report.shapesBush = physx.leftByRule;
	report.physxWhy = physx.why;
	const cpmc::px::Bodies::Report& bodies = g_bodies.State();
	report.bodiesOn = g_blocks && g_renderer->ModeName() != "off";
	report.bodiesWhy = bodies.why;
	report.bodySections = bodies.sections;
	report.bodyBoxes = bodies.boxes;
	report.bodiesMade = bodies.made;
	report.bodiesRemoved = bodies.removed;
	report.bodiesFailed = bodies.failed;
	report.bodiesLost = bodies.lost;
	report.bodySlowestMs = bodies.slowestMs;
	report.bodiesMadeOf = bodies.madeOf;
	report.originChanges = bodies.originChanges;
	report.bodiesReposed = bodies.reposed;
	report.box = bodies.box;
	report.npcsOn = g_npcs;
	report.npcsWhy = g_npcsWhy;
	report.npcsListed = g_actorRecords.size();
	report.npcsInWorld = g_npcList ? g_npcList->attached : 0;
	if (const cpmc::link::NpcSample* nearest = ListedNpc(0))
	{
		report.nearestNpc = DistanceFromV(*nearest);
		report.nearestNpcName = nearest->name;
	}
	report.capsulesFound = g_npcSizes.FoundCount();
	report.capsulesLookedFor = g_npcSizes.LookedForCount();
	report.npcIdsGiven = g_actorIds.Given();
	report.moduleMsAverage = g_second.moduleCalls ? g_second.moduleMsSum / double(g_second.moduleCalls) : -1.0;
	report.moduleMsWorst = g_second.moduleCalls ? g_second.moduleMsWorst : -1.0;
	report.npcHits = g_npcHits;
	report.combatOn = g_combat;
	report.punch = g_punch;
	report.punchItem = g_punchItem;
	report.punchDrawn = g_punchDrawn;
	report.hitPower = g_hitPower;
	report.hitsQueued = g_hitsQueued;
	report.hitsLanded = g_hitsLanded;
	report.hitsDropped = HitDrops();
	report.lastHit = g_lastHit;
	const cpmc::CombatInbox::Counts combat = cpmc::CombatReported().Count();
	report.hurtsTaken = combat.taken;
	report.hurtsSent = g_hurtsSent;
	report.hurtsSummed = g_hurtSums.Summed();
	report.hurtsLeft = combat.refused;
	report.hurtsNonLethal = combat.nonLethal;
	report.lastHurt = g_lastHurt;
	report.lastHealth = g_lastHealth;
	report.immortal = g_immortal;
	report.deaths = g_deaths;
	report.minecraftDeadFor = g_minecraftDiedAt ? std::optional<double>(aNow - *g_minecraftDiedAt) : std::nullopt;
	report.combatWhy = CombatWhy();
	report.blasts = g_blasts.Count();
	report.blastPower = g_blastPower;
	report.blastKnock = g_blastKnock;
	report.lastBlast = g_lastBlast;
	// Spike 7's entities, Minecraft's player's feet standing for V's
	const uint32_t                              glowLight = g_renderer->GlowLight();
	const cpmc::entities::Entities::SceneCounts counts = g_entities.Counts(glowLight);
	const double                                entitySeconds = std::max(aNow - g_second.start, 0.001);
	report.entitiesOn = aStats.entitiesOn;
	report.scenesPerSecond = double(g_entities.Scenes() - g_second.scenesAtStart) / entitySeconds;
	report.sceneMbPerSecond = double(g_entities.SceneBytes() - g_second.sceneBytesAtStart) / 1048576.0 / entitySeconds;
	report.sceneBatches = counts.batches;
	report.sceneVertices = counts.vertices;
	report.sceneGlowing = counts.glowing;
	report.sceneTranslucent = counts.translucent;
	report.glowLight = glowLight;
	report.entityTextures = aStats.entityTextures;
	report.entityTextureMb = aStats.entityTextureMb;
	report.entityTexturesLeftOut = aStats.entityTexturesLeftOut;
	report.entityFrames = aStats.entityFrames;
	report.entityDrawsSkipped = aStats.entityDrawsSkipped;
	report.snapshotWaits = aStats.snapshotWaits;
	report.snapshotCut = aStats.snapshotCut;
	report.entityGpuMsAverage = aStats.entityGpuMsAverage;
	report.worldEntities = cpmc::link::WorldWords(g_entities.Table());
	report.nearestMob = g_link ? g_entities.NearestMob(g_link->State().minecraftFeet) : std::nullopt;
	report.health = g_linked ? g_puppet.Health() : std::nullopt;  // once a second, so fall damage shows
	report.hideV = g_hideV;
	report.vHidden = g_hidden.On();
	report.vMeshes = g_vLook.meshes;
	report.vEffects = g_vLook.effects;
	report.vCrosshair = g_vLook.crosshair;
	report.turn = g_turn;
	report.turnRatio = g_turnRatio.Ratio();
	report.turnRemembered = g_turnRatio.Remembered();
	report.turnTurns = g_turnRatio.Turns();
	report.atlas = aStats.atlasWidth == 0 ? "none yet"
	                                      : std::format("{} x {}, {} levels, {}", aStats.atlasWidth, aStats.atlasHeight, aStats.atlasLevels,
	                                                    g_minecraftAtlas ? "Minecraft's" : "atlas.bin's");
	report.updateMsAverage = aStats.updateMsAverage;
	report.updateMsWorst = aStats.updateMsWorst;
	report.hudSetting = g_hudSetting;
	report.hudScale = g_hudScale;
	report.hudWanted = g_hudWanted;
	report.hudReceived = aStats.hudReceived;
	report.hudMb = double(aStats.hudBytes) / 1048576.0;
	report.hudDropped = aStats.hudDropped;
	report.hudDrawn = aStats.hudDrawn;
	report.hudSize = aStats.hudWidth == 0 ? ""
	                                      : std::format("{} x {}, rows {}", aStats.hudWidth, aStats.hudHeight,
	                                                    aStats.hudBottomUp ? "bottom-up" : "top-down");
	report.hudCopyMsAverage = aStats.hudCopyMsAverage;
	report.hudGpuMsAverage = aStats.hudGpuMsAverage;
	report.hudSkipped = aStats.hudSkipped;
	report.hudLastSkip = aStats.hudLastSkip;
	report.latencyLast = g_latency.Last();
	report.latencyAverage = g_latency.Average();
	report.latencySamples = g_latency.Samples();
	report.burst = g_lastBurst;
	report.cyberpunkCpu = g_cyberpunkProcess.CpuShare();
	report.cyberpunkMemory = g_cyberpunkProcess.WorkingSet();
	report.minecraftProcessKnown = g_minecraftProcess.Known();
	report.minecraftCpu = g_minecraftProcess.CpuShare();
	report.minecraftMemory = g_minecraftProcess.WorkingSet();
	return report;
}

// The next second's figures start from where the link is now.
void StartSecond(double aNow)
{
	g_second = {};
	g_second.start = aNow;
	g_second.scenesAtStart = g_entities.Scenes();
	g_second.sceneBytesAtStart = g_entities.SceneBytes();
	if (g_link)
	{
		const cpmc::link::Link::Status& state = g_link->State();
		g_second.teleportsAtStart = state.teleports;
		g_second.minecraftFramesAtStart = state.minecraftFrames;
		g_second.messagesAtStart = state.messages;
		g_second.bytesAtStart = state.bytes;
		g_second.waitedAtStart = state.waitedFrames;
		g_second.cityBatchesAtStart = state.cityBatches;
		g_second.cityTrianglesAtStart = state.cityTriangles;
		g_second.workerRegionsAtStart = state.workerRegions;
		g_second.workerTrianglesAtStart = state.workerTriangles;
		g_second.workerBlocksAtStart = state.workerBlocks;
		g_second.workerMsAtStart = state.workerMs;
		g_second.workerCyclesAtStart = state.workerCycles;
		g_second.workerTicksAtStart = state.workerTicks;
		g_second.collisionBytesAtStart = state.collisionBytes;
	}
}

void WriteStatus(double aNow)
{
	g_cyberpunkProcess.Sample(GetCurrentProcessId(), aNow);
	g_minecraftProcess.Sample(g_link && g_link->State().minecraft ? g_link->State().minecraftPid : 0, aNow);  // not one that's gone
	const cpmc::Renderer::Stats stats = g_renderer->TakeStats();
	cpmc::StatusReport          report;
	report.mode = g_renderer->ModeName();
	report.framesDrawn = stats.drawn;
	report.framesPartial = stats.partial;
	report.framesSkipped = stats.skipped;
	report.lastSkip = stats.lastSkip;
	report.lateCamera = stats.lateCamera;
	report.sections = stats.sections;
	report.blocks = g_world.BlockCount();
	report.minecraftBlocks = g_minecraftAtlas;
	report.vertices = stats.vertices;
	report.gpuMsAverage = stats.gpuMsAverage;
	report.gpuMsWorst = stats.gpuMsWorst;
	report.frameMsAverage = stats.frameMsAverage;
	report.frameMsWorst = stats.frameMsWorst;
	const uint64_t tokens = cpmc::slhooks::FrameTokens();
	report.streamlineFrames = uint32_t(tokens - g_lastFrameTokens);
	g_lastFrameTokens = tokens;
	report.hooks = g_hooked ? std::format("{}; {}; {}; log {}; dump {}", cpmc::slhooks::Summary(), cpmc::d3d::Summary(),
	                                      cpmc::HookGroupsSummary(), g_frameLog->State(), g_dumps->State())
	                        : "not installed (see red4ext\\logs)";
	report.lastRequest = g_lastRequest;
	report.sceneWatch = g_watchSince ? std::format("on for {:.0f} s: {}", aNow - *g_watchSince, WatchSummary())
	                    : g_lastWatch.empty() ? std::string()
	                                          : "off; the last watch, " + g_lastWatch;
	report.structures = g_structures;
	if (const std::optional<cpmc::Vec3d> box = g_bodies.State().box)
	{
		// Step 4's test box can't be seen: as a structure, the overlay shows how far V is from it.
		report.structures.push_back({ "testbox", { box->x - 1.0, box->y - 1.0, box->z - 0.5 }, { box->x + 1.0, box->y + 1.0, box->z + 0.5 } });
	}
	report.link = LinkLines(stats, aNow);
	StartSecond(aNow);
	std::error_code error;
	if (!std::filesystem::is_directory(g_hudDir, error))
	{
		if (!g_hudMissingReported)
		{
			g_hudMissingReported = true;
			Log("the overlay's folder {} is missing, so there's no status file", g_hudDir.string());
		}
		return;
	}
	const std::filesystem::path temporary = g_hudDir / "status.tmp";
	{
		std::ofstream out(temporary, std::ios::binary);
		out << cpmc::FormatStatus(report);
	}
	// While the overlay is reading status.txt this fails; the next second's file replaces it.
	std::filesystem::rename(temporary, g_hudDir / "status.txt", error);
}

bool OnEnter(RED4ext::CGameApplication*)
{
	return true;
}

bool OnExit(RED4ext::CGameApplication*)
{
	return true;
}

// Every frame on the main thread while the game runs (RED4ext ignores the return value in this state), in the spec's
// order: requests once a second, the link, the frame log and the dumps, meshing and the renderer's Update, the log,
// and the status once a second.
bool OnUpdate(RED4ext::CGameApplication*)
{
	cpmc::px::SetInUpdate(true);  // the scene watch's hooks note calls the game makes meanwhile
	g_mainThread = GetCurrentThreadId();
	const auto   now = Clock::now();
	const double seconds = cpmc::SteadySeconds();
	const double frameMs = g_lastFrameAt > 0.0 ? (seconds - g_lastFrameAt) * 1000.0 : 0.0;
	g_lastFrameAt = seconds;
	const bool newSecond = now - g_lastSecond >= 1s;
	if (newSecond)
	{
		g_lastSecond = now;
		if (g_hooked)
		{
			PatchLate();
			CheckDevice();
		}
		if (!g_link)
		{
			OpenMapping();
		}
		RunRequest(g_pluginDir, false);
		RunRequest(g_hudDir, true);
	}
	if (g_link && g_router->TakeLinkRequest())
	{
		LinkRequest(true, "F8: ");  // F8 while unlinked: the link, then build mode once the link has seen V
		g_buildWanted = true;
	}
	const std::optional<cpmc::Vec3d> feet = UpdateLink(seconds, frameMs);
	Fight(seconds, feet);
	ListNpcs(seconds, feet);
	SendHurts(seconds);
	if (g_link)
	{
		// Spike 7: this frame's entities, which the renderer's Update copies into a snapshot, drawn only into the G-buffer.
		g_entities.InWorld(g_link->State().inWorld);
		if (g_renderer->ModeName() == "gbuffer")
		{
			g_renderer->SetEntities(g_entities.Frame(g_link->State().minecraftEye));
		}
	}
	const auto [time, folderName] = Timestamps();
	if (g_frameLog->Capturing())
	{
		if (const std::optional<cpmc::PlayerPose> pose = cpmc::LocatePlayer(); pose && pose->camera)
		{
			g_frameLog->NoteScriptCamera({ pose->camera->position, pose->camera->forward, pose->camera->fov });
		}
	}
	if (const std::optional<cpmc::FrameLogData> finished = g_frameLog->TakeFinished(kVersion, time))
	{
		WriteFrameLog(*finished, folderName);
	}
	if (const std::optional<cpmc::DriveLog::Finished> finished = g_driveLog.TakeFinished())
	{
		WriteDriveLog(*finished, folderName);
	}
	g_dumps->Update(folderName);
	if (g_minecraftAtlas)
	{
		g_world.TakeDirty();  // spike 3's world stays empty, and isn't meshed with Minecraft's atlas
	}
	else if (const cpmc::Atlas* atlas = g_renderer->AtlasData())
	{
		for (const cpmc::SectionPos& section : g_world.TakeDirty())
		{
			g_renderer->UploadSection(section, cpmc::MeshSection(g_world, section, *atlas, g_meshOptions));
		}
	}
	const double updateStart = cpmc::SteadySeconds();
	g_renderer->Update();
	const double updateMs = (cpmc::SteadySeconds() - updateStart) * 1000.0;
	const cpmc::BurstFrame burstFrame{ g_frame.sections, g_frame.bytes, g_frame.drainMs, updateMs, frameMs };
	if (const std::optional<cpmc::BurstReport> burst = g_burst.Frame(seconds, burstFrame))
	{
		g_lastBurst = burst;
		Log("burst after {}: {} sections, {:.2f} MB over {} frames; worst drain {:.3f} ms, update {:.3f} ms, frame {:.1f} ms", burst->what,
		    burst->sections, double(burst->bytes) / 1048576.0, burst->frames, burst->drainMsWorst, burst->updateMsWorst,
		    burst->frameMsWorst);
	}
	DrainLog();
	if (newSecond)
	{
		LogWrapThreads();
		for (const std::string& line : cpmc::d3d::DebugMessages())
		{
			Log("{}", line);
		}
		SaveTurnRatio();
		WriteStatus(seconds);
	}
	cpmc::px::SetInUpdate(false);
	return false;
}
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle, RED4ext::v1::EMainReason aReason,
                                        const RED4ext::v1::Sdk* aSdk)
{
	switch (aReason)
	{
	case RED4ext::v1::EMainReason::Load:
	{
		g_handle = aHandle;
		g_sdk = aSdk;
		// The module's natives, added once the game has registered its types, before it loads its scripts.
		const bool natives = cpmc::RegisterNatives(g_log);
		// Pinned: the hooks point into this DLL, and threads may be inside them when the game unloads plugins.
		HMODULE self = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&PluginFolder),
		                   &self);
		g_pluginDir = PluginFolder();
		g_hudDir = g_pluginDir.parent_path().parent_path().parent_path() / "bin" / "x64" / "plugins" / "cyber_engine_tweaks" / "mods" /
		           "blockrender_hud";
		const cpmc::PluginSettings settings = cpmc::ParseSettings(ReadSmallFile(g_pluginDir / "settings.txt"));
		g_linkSettings.ground = cpmc::link::Ground::City;  // Night City's shape from step 3 on; the floor is the fallback
		g_savedTurn = cpmc::link::ParseTurnFile(ReadSmallFile(g_pluginDir / "turn.txt"));
		if (g_savedTurn)
		{
			g_turnRatio.Remember(*g_savedTurn);  // build mode turns at once, until this session has measured its own
		}
		g_frameLog = new cpmc::FrameLog(g_log);
		g_dumps = new cpmc::Dumps(g_log, g_pluginDir);
		g_renderer = new cpmc::Renderer(g_log, g_pluginDir);
		// Input first: it passes everything until the user links or presses F8.
		g_router = new cpmc::input::Router(g_log);
		g_inputHooked = cpmc::input::InstallRawInputHook(*g_router, g_log);
		for (cpmc::HookEvents* listener : std::initializer_list<cpmc::HookEvents*>{ g_frameLog, g_dumps, g_renderer })
		{
			cpmc::d3d::AddListener(listener);
			cpmc::slhooks::AddListener(listener);
		}
		g_hooked = cpmc::slhooks::Install(g_log);
		if (g_hooked && !cpmc::d3d::InstallDetours(aSdk, aHandle, g_log, { settings.dred, settings.debugLayer }))
		{
			g_log.Push("some D3D12 or DXGI detours are missing (see above)");
		}
		static RED4ext::v1::GameState running{ &OnEnter, &OnUpdate, &OnExit };
		aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &running);
		// Spike 6's redscript module, which redscript compiles with the game's scripts: the install puts it in Scripts.
		const std::filesystem::path module = g_pluginDir / "Scripts" / "BlockRender.reds";
		std::error_code             error;
		const bool                  present = std::filesystem::exists(module, error);
		const bool                  added = present && aSdk->scripts && aSdk->scripts->Add(aHandle, module.parent_path().c_str());
		DrainLog();
		Log("BlockRender {} loaded (game {}.{}.{}), {}; dred {}, debug layer {}; requests go in {}", kVersion,
		    unsigned(aSdk->runtime->major), aSdk->runtime->minor, aSdk->runtime->patch, g_hooked ? "hooked" : "NOT hooked",
		    settings.dred ? "on" : "off", settings.debugLayer ? "on" : "off", (g_pluginDir / "blockrender.request").string());
		Log("the turn: {}", g_savedTurn ? std::format("{:.4f} degrees a count, remembered from the last session, until this one has "
		                                              "measured its own", *g_savedTurn)
		                                : std::string("nothing remembered, so build mode turns once V has turned 5 times"));
		Log("the redscript module {}: {}; its natives {}", module.string(),
		    added ? "added to redscript's compile" : present ? "NOT added to redscript's compile" : "missing, so NPCs can't be listed",
		    natives ? "are added once the game has registered its types" : "CAN'T be added: the game has no type system yet");
		break;
	}
	case RED4ext::v1::EMainReason::Unload:
		if (g_router)
		{
			g_router->TurnOff("the plugin unloads");  // Minecraft lets go of everything
			cpmc::input::RemoveRawInputHook();
			g_router->SetLink(nullptr);  // from here on, nothing pushes into the mapping
		}
		if (g_renderer)
		{
			g_renderer->Shutdown();
		}
		cpmc::d3d::RemovePatches();
		cpmc::slhooks::Remove();
		cpmc::px::RemoveSceneHooks();
		delete g_link;  // main thread only, as everything of the link's
		g_link = nullptr;
		g_mapping.Close();  // Minecraft sees the heartbeat stop
		DrainLog();
		break;
	}
	return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
	aInfo->name = L"BlockRender";
	aInfo->author = L"Cyberpunk-Minecraft";
	aInfo->version = RED4EXT_V1_SEMVER(uint8_t(VersionNumber(kVersion, 0)), uint16_t(VersionNumber(kVersion, 1)), VersionNumber(kVersion, 2));
	aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
	aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
	return RED4EXT_API_VERSION_1;
}
