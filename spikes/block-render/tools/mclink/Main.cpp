// mclink: a console stand-in for Cyberpunk's side of SkyCraft's protocol, for spike 4's step 1, spike 5's steps 1 and 3,
// and spike 6's steps 1, 3 and 5a. It runs BlockRender's link code against the real Minecraft, without the game
// (docs/superpowers/specs/2026-10-02-minecraft-link-design.md, 2026-10-03-collision-and-movement-design.md and
// 2026-10-05-npcs-and-combat-design.md).
//
//     mclink.exe <script.txt>
//
// It runs the script (McLinkScript.h) at 60 Hz with a stand-in V where the script says, or, in build mode, where the
// drive puts it, as the plugin teleports V. With the city's ground, Night City's shape comes from a dump spike 1 saved.
// Made-up NPCs go into SkyCraft's actor table, as the plugin writes the game's. It reports what Minecraft sends, its solid
// blocks and its hits on the NPCs among it, which of those hits are a blast's own, as the plugin tells them by the NPCs'
// feet, and when each blast's push would come; and refuses to start while another game side uses the mapping.
#include "DumpObj.h"
#include "McLinkScript.h"

#include "Coords.h"
#include "link/Actors.h"
#include "link/Combat.h"
#include "link/Link.h"
#include "link/Mapping.h"
#include "log/DdsWriter.h"

#include <Windows.h>
#include <timeapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <map>
#include <sstream>
#include <thread>

using namespace cpmc;
using namespace cpmc::link;
using cpmc::mclink::Action;

namespace
{
using Clock = std::chrono::steady_clock;
constexpr double kFrame = 1.0 / 60.0;
constexpr double kLongest = 600.0;  // seconds: no run goes longer

const Clock::time_point g_start = Clock::now();

double Now()
{
	return std::chrono::duration<double>(Clock::now() - g_start).count();
}

template<class... Args>
void Say(std::format_string<Args...> aFormat, Args&&... aArgs)
{
	std::printf("[%7.2f] %s\n", Now(), std::format(aFormat, std::forward<Args>(aArgs)...).c_str());
	std::fflush(stdout);
}

bool WriteDds(const std::string& aPath, uint32_t aWidth, uint32_t aHeight, const uint8_t* aRgba)
{
	constexpr uint32_t kRgba8 = 28;  // DXGI_FORMAT_R8G8B8A8_UNORM
	const auto         bytes = MakeDds(kRgba8, aWidth, aHeight, aRgba, size_t(aWidth) * 4);
	std::ofstream      out(aPath, std::ios::binary);
	out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
	return bool(out);
}

// What Minecraft sends, reported as it comes.
struct Report : LinkSink
{
	void Atlas(const AtlasImage& aAtlas) override
	{
		atlasWidth = aAtlas.width;
		atlasHeight = aAtlas.height;
		atlas.assign(aAtlas.rgba.begin(), aAtlas.rgba.end());
		Say("atlas: {} x {}, {:.1f} MB", aAtlas.width, aAtlas.height, double(aAtlas.rgba.size()) / 1048576.0);
	}

	void Section(SectionMesh&& aMesh) override
	{
		++sections;
		if (sections <= 20 || sections % 100 == 0)
		{
			Describe(aMesh);
		}
	}

	void ClearAll() override { Say("clear all"); }

	void Explosion(const proto::McEvent& aEvent) override
	{
		const std::optional<Blast> blast = AcceptBlast(aEvent, Now());
		Say("explosion at {:.2f} {:.2f} {:.2f}, radius {:.1f}{}", aEvent.a, aEvent.b, aEvent.c, aEvent.d,
		    blast ? std::format(": a blast reaching {:.1f} m", blast->reach) : std::string(": not a blast the plugin takes"));
		if (blast)
		{
			blasts.Add(*blast);
		}
	}

	void HitActor(const proto::McEvent& aEvent) override
	{
		// A blast's own hit, as the plugin tells it: by the NPC's feet, which are the made-up NPC's record.
		std::string blastWords;
		const auto  actor = actors ? actors->find(aEvent.formId) : std::map<uint32_t, proto::ActorRecord>::const_iterator();
		if (actors && actor != actors->end())
		{
			const Vec3d feet = WorldFromMc({ actor->second.x, actor->second.y, actor->second.z });
			if (const std::optional<Blast> blast = blasts.HitOn(feet, Now()))
			{
				blastWords = std::format("; a blast's own, {:.2f} m from its centre, {:.3f} s after it",
				                         std::hypot(feet.x - blast->centre.x, feet.y - blast->centre.y, feet.z - blast->centre.z),
				                         Now() - blast->at);
			}
		}
		Say("hit: actor {} for {:.2f}, knockback {:.2f} towards x {:.2f} z {:.2f}; {}{}", aEvent.formId, aEvent.a, aEvent.d, aEvent.b, aEvent.c,
		    HitWords(aEvent), blastWords);
	}

	void PlayerDied(const proto::McEvent& aEvent) override
	{
		if (aEvent.formId != 0)
		{
			Say("Minecraft's player died, killed by actor {}", aEvent.formId);
		}
		else
		{
			Say("Minecraft's player died, killed by none of the actors");
		}
	}

	void Hud(const HudFrame& aFrame) override
	{
		++huds;
		if (aFrame.width != hudWidth || aFrame.height != hudHeight || aFrame.bottomUp != hudBottomUp)
		{
			Say("HUD images: {} x {}, rows {}", aFrame.width, aFrame.height, aFrame.bottomUp ? "bottom-up" : "top-down");
		}
		hudWidth = aFrame.width;
		hudHeight = aFrame.height;
		hudBottomUp = aFrame.bottomUp;
		if (wantHud)
		{
			hud.assign(aFrame.pixels, aFrame.pixels + size_t(aFrame.width) * aFrame.height * 4);
			wantHud = false;
			hudReady = true;
		}
	}

	void Log(const std::string& aLine) override { Say("{}", aLine); }

	// The section's blocks, as the extent of its vertices, and the atlas texel at the centre of its first triangle's UVs:
	// inside that face's sprite, where a corner's UV can land on the next sprite over.
	void Describe(const SectionMesh& aMesh) const
	{
		if (aMesh.vertices.empty())
		{
			Say("section {} {} {}: removed", aMesh.x, aMesh.y, aMesh.z);
			return;
		}
		float low[3] = { 1e9f, 1e9f, 1e9f };
		float high[3] = { -1e9f, -1e9f, -1e9f };
		for (const Vertex& v : aMesh.vertices)
		{
			const float at[3] = { v.x, v.y, v.z };
			for (int i = 0; i < 3; ++i)
			{
				low[i] = std::min(low[i], at[i]);
				high[i] = std::max(high[i], at[i]);
			}
		}
		std::string sample;
		if (!atlas.empty())
		{
			const float    u = (aMesh.vertices[0].u + aMesh.vertices[1].u + aMesh.vertices[2].u) / 3.0f;
			const float    v = (aMesh.vertices[0].v + aMesh.vertices[1].v + aMesh.vertices[2].v) / 3.0f;
			const uint32_t x = std::min(uint32_t(std::clamp(u, 0.0f, 1.0f) * atlasWidth), atlasWidth - 1);
			const uint32_t y = std::min(uint32_t(std::clamp(v, 0.0f, 1.0f) * atlasHeight), atlasHeight - 1);
			const uint8_t* texel = &atlas[(size_t(y) * atlasWidth + x) * 4];
			sample = std::format("; its first triangle's texel ({}, {}) is rgba({}, {}, {}, {})", x, y, texel[0], texel[1], texel[2], texel[3]);
		}
		const std::string dropped = aMesh.translucentDropped ? std::format(" ({} translucent triangles dropped)", aMesh.translucentDropped) : "";
		Say("section {} {} {}: {} vertices{}, blocks x {:.2f} to {:.2f}, y {:.2f} to {:.2f}, z {:.2f} to {:.2f}{}", aMesh.x, aMesh.y, aMesh.z,
		    aMesh.vertices.size(), dropped, aMesh.x * 16.0 + low[0], aMesh.x * 16.0 + high[0], aMesh.y * 16.0 + low[1],
		    aMesh.y * 16.0 + high[1], aMesh.z * 16.0 + low[2], aMesh.z * 16.0 + high[2], sample);
	}

	std::vector<uint8_t> atlas;
	uint32_t             atlasWidth = 0;
	uint32_t             atlasHeight = 0;
	uint64_t             sections = 0;
	uint64_t             huds = 0;
	uint32_t             hudWidth = 0;
	uint32_t             hudHeight = 0;
	bool                 hudBottomUp = false;
	bool                 wantHud = false;
	bool                 hudReady = false;
	std::vector<uint8_t> hud;
	Blasts                                        blasts;            // Minecraft's, as the plugin takes them
	const std::map<uint32_t, proto::ActorRecord>* actors = nullptr;  // the made-up NPCs, by number
};

// Minecraft's solid blocks: all of them, and in the 3 x 3 x 3 sections around the stand-in V's, where V's bodies would go.
void PrintSolids(const Solids& aSolids, const Vec3d& aFeet)
{
	const SectionPos at = SectionOf({ int32_t(std::floor(aFeet.x)), int32_t(std::floor(aFeet.y)), int32_t(std::floor(aFeet.z)) });
	size_t           sections = 0;
	uint64_t         blocks = 0;
	for (int32_t dy = -1; dy <= 1; ++dy)
	{
		for (int32_t dz = -1; dz <= 1; ++dz)
		{
			for (int32_t dx = -1; dx <= 1; ++dx)
			{
				if (const Solids::Section* section = aSolids.Find({ at.x + dx, at.y + dy, at.z + dz }))
				{
					++sections;
					blocks += section->count;
				}
			}
		}
	}
	Say("solids: {} sections with {} solid blocks; around the stand-in V's section {} {} {}: {} sections with {} blocks", aSolids.Sections(),
	    aSolids.Blocks(), at.x, at.y, at.z, sections, blocks);
}

void PrintStatus(const Link& aLink, const Report& aReport, const Vec3d& aFeet)
{
	const auto& s = aLink.State();
	Say("Minecraft: {}{}{}, feet {:.3f} {:.3f} {:.3f}, eye {:.3f} ({:.2f} above the feet), frame {}",
	    s.minecraft ? std::format("process {}", s.minecraftPid) : "absent", s.inWorld ? ", in its world" : "",
	    s.screenOpen ? ", a screen open" : "", s.minecraftFeet.x, s.minecraftFeet.y, s.minecraftFeet.z, s.minecraftEye.y,
	    s.minecraftEye.y - s.minecraftFeet.y, s.minecraftFrames);
	Say("game side: {}, {} teleports (sequence {}) to {:.3f} {:.3f} {:.3f}, look {:.1f} {:.1f}, floor {} {}", s.inGame ? "in game" : "not in game",
	    s.teleports, s.teleportSeq, s.target.x, s.target.y, s.target.z, s.look.yaw, s.look.pitch, s.collisionEpoch,
	    s.floorSent ? std::format("at height {:.2f}", s.floorCentre.y) : "not sent");
	std::string types;
	for (size_t type = 0; type < Link::kTypes; ++type)
	{
		if (s.messages[type])
		{
			types += std::format("{}{} {} ({:.2f} MB)", types.empty() ? "" : ", ", s.messages[type], RenderTypeName(type),
			                     double(s.bytes[type]) / 1048576.0);
		}
	}
	Say("render ring: {:.2f} MB; {}", double(s.drainedBytes) / 1048576.0, types.empty() ? "no messages" : types);
	Say("render ring: {} resyncs{}, {} rejected{}, {} sections before the atlas, {} translucent triangles dropped, {} sections reported",
	    s.resyncs, s.resyncs ? " (" + s.lastResync + ")" : "", s.rejected, s.rejected ? " (" + s.lastRejected + ")" : "", s.sectionsBeforeAtlas,
	    s.translucentDropped, aReport.sections);
	Say("events: {} explosions, {} hits on actors, {} deaths, {} others; {} actors in the table; {} inputs dropped; {} commands waiting; "
	    "{} HUD images, {} refused",
	    s.explosions, s.hitEvents, s.deathEvents, s.otherEvents, s.actors, s.inputsDropped, s.commandsWaiting, s.hudImages, s.hudRefused);
	Say("drive: {}, teleport {} applied of {}; V's eye height {:.2f}; render delay {:.1f} ms, {} frames waited for a tick, {} targets skipped",
	    DriveStateName(s.drive), s.teleportAck, s.teleportSeq, s.eyeHeight, s.renderDelayMs, s.waitedFrames, s.driveSkipped);
	Say("ground: the {}, epoch {}; {}; {} batches read ({} unread), {} regions sent ({} triangles); the worker: {} threads, {} regions "
	    "({} triangles), {} voxel blocks, {:.1f} ms, its threads running {:.0f}% of it, {} waiting, {} replaced; ring: {:.2f} MB, {} "
	    "messages waiting, {} dropped",
	    GroundName(s.ground), s.collisionEpoch,
	    s.cityCentre ? std::format("around region {} {} {}, {} regions known, {} batches to read", s.cityCentre->x, s.cityCentre->y,
	                               s.cityCentre->z, s.cityKnown, s.cityWaiting)
	                 : "no centre yet",
	    s.cityBatches, s.cityUnread, s.cityRegions, s.cityTriangles, s.workerThreads, s.workerRegions, s.workerTriangles, s.workerBlocks,
	    s.workerMs, s.workerTicks ? 100.0 * double(s.workerCycles) / double(s.workerTicks) : 0.0, s.workerJobs, s.workerReplaced,
	    double(s.collisionBytes) / 1048576.0, s.collisionWaiting, s.collisionDropped);
	PrintSolids(aLink.SolidSections(), aFeet);
}

// Spike 1's saved collision as Night City's ground (step 3's offline half): the triangles touching each box read.
struct DumpGround : GroundSource
{
	bool Read(const McBox& aBox, std::vector<McTriangle>& aOut) override
	{
		const double low[3] = { aBox.low.x, aBox.low.y, aBox.low.z };
		const double high[3] = { aBox.high.x, aBox.high.y, aBox.high.z };
		for (const McTriangle& triangle : triangles)
		{
			bool touches = true;
			for (int axis = 0; axis < 3 && touches; ++axis)
			{
				touches = std::max({ triangle.v[axis], triangle.v[3 + axis], triangle.v[6 + axis] }) >= low[axis] &&
				          std::min({ triangle.v[axis], triangle.v[3 + axis], triangle.v[6 + axis] }) <= high[axis];
			}
			if (touches)
			{
				aOut.push_back(triangle);
			}
		}
		return true;
	}

	std::vector<McTriangle> triangles;  // Minecraft axes
};

// Minecraft's player's flags, as words.
std::string Flags(uint32_t aFlags)
{
	std::string words;
	for (const auto& [flag, word] : { std::pair{ proto::kMcOnGround, "on ground" }, std::pair{ proto::kMcSneaking, "sneaking" },
	                                  std::pair{ proto::kMcSprinting, "sprinting" }, std::pair{ proto::kMcFlying, "flying" } })
	{
		if (aFlags & flag)
		{
			words += std::string(words.empty() ? "" : ", ") + word;
		}
	}
	return words.empty() ? "in the air" : words;
}
}

int main(int aArgc, char** aArgv)
{
	if (aArgc != 2)
	{
		std::puts("usage: mclink <script.txt>");
		return 2;
	}
	std::ifstream in(aArgv[1], std::ios::binary);
	if (!in)
	{
		std::printf("can't read %s\n", aArgv[1]);
		return 2;
	}
	std::stringstream text;
	text << in.rdbuf();
	std::string error;
	const auto  script = mclink::ParseScript(text.str(), error);
	if (!script)
	{
		std::printf("%s: %s\n", aArgv[1], error.c_str());
		return 2;
	}

	Mapping    mapping;
	const auto opened = mapping.Open(proto::kMappingName, GetCurrentProcessId(), GetTickCount64());
	if (opened.outcome != Mapping::Outcome::Created && opened.outcome != Mapping::Outcome::Reused)
	{
		Say("the mapping: {}", opened.why);
		return 1;
	}
	Say("the mapping: {} (process {}); waiting for Minecraft", opened.why, GetCurrentProcessId());

	timeBeginPeriod(1);  // 1 ms sleeps, for an even 60 Hz
	Link         link(mapping.Base());
	Report       report;
	LinkSettings settings;
	settings.viewportWidth = 1280;
	settings.viewportHeight = 720;
	Vec3d  feet{ -119.5, 6.14, 2007.5 };  // the stand-in V's, where V stood at spike 3's corner; its eye 1.62 up
	McLook look{ 180.0f, 0.0f };          // north
	bool   linked = false;
	bool   build = false;
	bool   hud = true;
	DumpGround dump;
	std::map<uint32_t, proto::ActorRecord> actors;  // the made-up NPCs, by number
	HurtSums                               sums;    // V's hurts, as the plugin sums them
	const auto send = [&](const Hurt& aHurt, const char* aHow) {
		const proto::InputEvent input = HurtInput(aHurt);
		const bool              sent = link.Push(input);
		Say("hurt {}: {} {:.4f} of V's health, a = {}, attacker {}{}{}", aHow, HurtKindName(aHurt.kind), aHurt.share, input.a, aHurt.attacker,
		    aHurt.power ? ", power attack" : "", sent ? "" : "; DROPPED, the input ring is full");
	};
	report.actors = &actors;  // for telling a blast's own hits

	LARGE_INTEGER qpcRate;
	QueryPerformanceFrequency(&qpcRate);
	size_t   next = 0;
	bool     begun = false;
	double   begunAt = 0.0;
	double   stray = 0.0;
	uint64_t holdFrames = 0;
	double   trackAt = 0.0;
	int      exitCode = 0;
	double   lastReport = 0.0;
	uint64_t reportedFrames = 0;
	uint64_t reportedBytes = 0;
	uint64_t reportedHuds = 0;
	for (double tick = Now();; tick += kFrame)
	{
		const double now = Now();
		if (now > kLongest)
		{
			Say("stopping: no run goes over {} s", kLongest);
			exitCode = 1;
			break;
		}
		LARGE_INTEGER qpc;
		QueryPerformanceCounter(&qpc);
		Link::Frame frame;
		frame.now = now;
		frame.tickMs = GetTickCount64();
		frame.qpc = qpc.QuadPart;
		frame.qpcPerSecond = qpcRate.QuadPart;
		frame.linked = linked;
		frame.buildMode = build;
		frame.takeHud = hud;
		frame.pose = GamePose{ WorldFromMc(feet), WorldFromMc({ feet.x, feet.y + kEyeHeight, feet.z }), ForwardFromLook(look) };
		link.Update(frame, settings, report);
		link.ReadGround(now, dump, 1.5);  // the plugin's budget a frame
		if (actors.empty())
		{
			link.ClearActors();
		}
		else
		{
			std::vector<proto::ActorRecord> records;
			for (const auto& [id, record] : actors)
			{
				records.push_back(record);
			}
			link.WriteActors(records);  // every frame, as the plugin writes the game's NPCs
		}
		const auto& state = link.State();
		if (const std::optional<Hurt> due = sums.Due(now, state.minecraftTickMs))
		{
			send(*due, "sent, summed");
		}
		for (const Blast& blast : report.blasts.Pushes(now))
		{
			Say("the blast at world {:.2f} {:.2f} {:.2f} would push now, {:.2f} s after it, with {} hits of its own", blast.centre.x,
			    blast.centre.y, blast.centre.z, now - blast.at, blast.hits);
		}
		if (state.placeV)
		{
			feet = McFromWorld(*state.placeV);  // the stand-in V goes where the drive puts it, as the plugin teleports V
		}
		if (!state.endBuild.empty())
		{
			Say("build mode ends: {}", state.endBuild);
			build = false;
			link.Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
		}

		if (now - lastReport >= 5.0)
		{
			const double seconds = lastReport == 0.0 ? 1.0 : now - lastReport;
			Say("Minecraft {}: {:.0f} frames/s; the render ring {:.2f} MB/s; {} sections so far; {:.0f} HUD images/s",
			    state.minecraft ? "there" : "absent", double(state.minecraftFrames - reportedFrames) / seconds,
			    double(state.drainedBytes - reportedBytes) / 1048576.0 / seconds, report.sections, double(report.huds - reportedHuds) / seconds);
			lastReport = now;
			reportedFrames = state.minecraftFrames;
			reportedBytes = state.drainedBytes;
			reportedHuds = report.huds;
		}

		if (next >= script->size())
		{
			break;
		}
		const Action& action = (*script)[next];
		const double  elapsed = begun ? now - begunAt : 0.0;
		bool          done = false;
		if (!begun)
		{
			begun = true;
			begunAt = now;
			switch (action.kind)
			{
			case Action::Kind::Link: linked = action.on; break;
			case Action::Kind::Build:
				if (build && !action.on)
				{
					link.Push({ proto::kInReleaseAll, 0, 0, 0, 0 });  // as the plugin does when build mode ends
				}
				build = action.on;
				break;
			case Action::Kind::Hud: hud = action.on; break;
			case Action::Kind::At: feet = { action.numbers[0], action.numbers[1], action.numbers[2] }; break;
			case Action::Kind::Look: look = { float(action.numbers[0]), float(action.numbers[1]) }; break;
			case Action::Kind::Floor: link.RequestFloor(); break;
			case Action::Kind::Ground: settings.ground = action.text == "city" ? Ground::City : Ground::Floor; break;
			case Action::Kind::Dump:
			{
				// Its triangles are relative to the feet of V when it was dumped: they go around the stand-in V's.
				std::ifstream     file(action.text, std::ios::binary);
				std::stringstream obj;
				obj << file.rdbuf();
				const auto triangles = file ? mclink::ParseDumpObj(obj.str(), "s0_static_", error) : std::nullopt;
				if (!triangles)
				{
					Say("line {}: can't load {}: {}", action.line, action.text, file ? error : "it can't be read");
					exitCode = 1;
					next = script->size();
					break;
				}
				dump.triangles = *triangles;
				for (McTriangle& triangle : dump.triangles)
				{
					for (int k = 0; k < 3; ++k)
					{
						triangle.v[k * 3] = float(triangle.v[k * 3] + feet.x);
						triangle.v[k * 3 + 1] = float(triangle.v[k * 3 + 1] + feet.y);
						triangle.v[k * 3 + 2] = float(triangle.v[k * 3 + 2] + feet.z);
					}
				}
				Say("dump: {} static triangles from {}, around Minecraft {:.2f} {:.2f} {:.2f}", dump.triangles.size(), action.text, feet.x,
				    feet.y, feet.z);
				break;
			}
			case Action::Kind::Actor:
			{
				proto::ActorRecord record{};
				record.formId = uint32_t(action.numbers[0]);
				record.x = float(action.numbers[1]);
				record.y = float(action.numbers[2]);
				record.z = float(action.numbers[3]);
				record.yaw = float(action.numbers[4]);
				record.width = float(action.numbers[5]);
				record.height = float(action.numbers[6]);
				record.healthFrac = 1.0f;
				const std::string name = CutName(action.text, sizeof record.name - 1);
				std::memcpy(record.name, name.data(), name.size());
				actors[record.formId] = record;
				Say("actor {} at {:.2f} {:.2f} {:.2f}, {:.2f} by {:.2f}{}", record.formId, record.x, record.y, record.z, record.width,
				    record.height, name.empty() ? "" : ": " + name);
				break;
			}
			case Action::Kind::ActorRemove:
				if (action.numbers[0] == 0.0)
				{
					actors.clear();
				}
				else
				{
					actors.erase(uint32_t(action.numbers[0]));
				}
				Say("{} actors left in the table", actors.size());
				break;
			case Action::Kind::Hurt:
			{
				Hurt hurt;
				hurt.kind = proto::HurtKind(action.numbers[0]);
				hurt.share = action.numbers[1];
				hurt.attacker = uint32_t(action.numbers[2]);
				hurt.power = action.on;
				if (const std::optional<Hurt> now_ = sums.Add(hurt, now, link.State().minecraftTickMs))
				{
					send(*now_, "sent at once");
				}
				else
				{
					Say("hurt joins a sum: {} {:.4f} of V's health, attacker {}{}", HurtKindName(hurt.kind), hurt.share, hurt.attacker,
					    hurt.power ? ", power attack" : "");
				}
				break;
			}
			case Action::Kind::Health:
			{
				const proto::InputEvent input = HealthInput(action.numbers[0]);
				Say("health: Minecraft's player to {:.4f} of its max, b = {}{}", action.numbers[0], input.b,
				    link.Push(input) ? "" : "; DROPPED, the input ring is full");
				break;
			}
			case Action::Kind::Status: PrintStatus(link, report, feet); break;
			case Action::Kind::Type:
				if (!link.Type(action.text, error))
				{
					Say("line {}: {}", action.line, error);
					exitCode = 1;
				}
				break;
			case Action::Kind::Key:
				link.Push({ proto::kInKey, uint16_t(action.numbers[0]), 1, 0, 0 });
				link.Push({ proto::kInKey, uint16_t(action.numbers[0]), 0, 0, 0 });
				break;
			case Action::Kind::Down:
			case Action::Kind::Up:
				link.Push({ proto::kInKey, uint16_t(action.numbers[0]), action.kind == Action::Kind::Down ? 1 : 0, 0, 0 });
				break;
			case Action::Kind::Track: trackAt = now; break;
			case Action::Kind::Click:
				link.Push({ proto::kInMouseButton, uint16_t(action.numbers[0]), 1, 0, 0 });
				link.Push({ proto::kInMouseButton, uint16_t(action.numbers[0]), 0, 0, 0 });
				break;
			case Action::Kind::SaveHud:
				report.wantHud = true;
				report.hudReady = false;
				break;
			case Action::Kind::HoldCheck:
				stray = 0.0;
				holdFrames = 0;
				break;
			default: break;
			}
		}
		switch (action.kind)
		{
		case Action::Kind::Wait: done = elapsed >= action.numbers[0]; break;
		case Action::Kind::WaitWorld:
			done = state.inWorld;
			if (!done && elapsed > action.numbers[0])
			{
				Say("line {}: Minecraft wasn't in its world within {} s", action.line, action.numbers[0]);
				exitCode = 1;
				next = script->size();
			}
			break;
		case Action::Kind::WaitGround:
			done = state.ground == Ground::City && state.cityKnown > 0 && state.cityWaiting == 0 && state.workerJobs == 0 &&
			       state.collisionWaiting == 0;
			if (done)
			{
				Say("the city around region {} {} {} is all in the ring: {} regions", state.cityCentre->x, state.cityCentre->y,
				    state.cityCentre->z, state.cityKnown);
			}
			else if (elapsed > action.numbers[0])
			{
				Say("line {}: the city wasn't all sent within {} s", action.line, action.numbers[0]);
				exitCode = 1;
				done = true;
			}
			break;
		case Action::Kind::Type:
			done = elapsed > 0.0 && state.commandsWaiting == 0;
			if (!done && elapsed > 10.0)
			{
				Say("line {}: still typing after 10 s", action.line);
				exitCode = 1;
				done = true;
			}
			break;
		case Action::Kind::SaveAtlas:
			done = !report.atlas.empty();
			if (done)
			{
				Say("atlas saved to {}: {}", action.text,
				    WriteDds(action.text, report.atlasWidth, report.atlasHeight, report.atlas.data()) ? "ok" : "FAILED");
			}
			else if (elapsed > 30.0)
			{
				Say("line {}: no atlas within 30 s", action.line);
				exitCode = 1;
				done = true;
			}
			break;
		case Action::Kind::SaveHud:
			done = report.hudReady;
			if (done)
			{
				const uint32_t       w = report.hudWidth;
				const uint32_t       h = report.hudHeight;
				std::vector<uint8_t> rows(report.hud.size());
				for (uint32_t y = 0; y < h; ++y)
				{
					const uint32_t from = report.hudBottomUp ? h - 1 - y : y;
					std::copy_n(report.hud.data() + size_t(from) * w * 4, size_t(w) * 4, rows.data() + size_t(y) * w * 4);
				}
				uint64_t above = 0;
				uint64_t covered = 0;
				for (size_t i = 0; i < rows.size(); i += 4)
				{
					above += std::max({ rows[i], rows[i + 1], rows[i + 2] }) > rows[i + 3];
					covered += rows[i + 3] != 0;
				}
				Say("HUD image saved to {} ({} x {}, top row first): {}; {} pixels not transparent, {} with a colour above their alpha", action.text,
				    w, h, WriteDds(action.text, w, h, rows.data()) ? "ok" : "FAILED", covered, above);
			}
			else if (elapsed > 5.0)
			{
				Say("line {}: no HUD image within 5 s", action.line);
				exitCode = 1;
				done = true;
			}
			break;
		case Action::Kind::HoldCheck:
			if (state.inGame && state.inWorld)
			{
				const Vec3d& a = state.minecraftFeet;
				const Vec3d& b = state.target;
				stray = std::max(stray, std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)));
				++holdFrames;
			}
			done = elapsed >= action.numbers[0];
			if (done)
			{
				Say("over {} s ({} frames in game), Minecraft's feet strayed at most {:.4f} blocks from the teleport target", action.numbers[0],
				    holdFrames, stray);
			}
			break;
		case Action::Kind::Track:
			if (now >= trackAt)
			{
				trackAt += 0.25;
				const double since = state.drive == DriveState::Off ? 0.0 : now - state.driveSince;
				Say("track: Minecraft's feet {:.3f} {:.3f} {:.3f}, eye {:.2f} up, {}; teleport {} applied of {}; drive {}{}; V's feet "
				    "{:.3f} {:.3f} {:.3f}; delay {:.1f} ms, {} frames waited, {} skipped",
				    state.minecraftFeet.x, state.minecraftFeet.y, state.minecraftFeet.z, state.minecraftEye.y - state.minecraftFeet.y,
				    Flags(state.minecraftFlags), state.teleportAck, state.teleportSeq, DriveStateName(state.drive),
				    state.drive == DriveState::Off ? "" : std::format(" for {:.2f} s", since), feet.x, feet.y, feet.z, state.renderDelayMs,
				    state.waitedFrames, state.driveSkipped);
			}
			done = elapsed >= action.numbers[0];
			break;
		case Action::Kind::Quit: next = script->size(); break;
		default: done = true; break;
		}
		if (done)
		{
			++next;
			begun = false;
		}
		std::this_thread::sleep_until(g_start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(tick + kFrame)));
	}
	PrintStatus(link, report, feet);
	timeEndPeriod(1);
	return exitCode;
}
