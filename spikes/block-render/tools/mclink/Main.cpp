// mclink: a console stand-in for Cyberpunk's side of SkyCraft's protocol, for spike 4's step 1. It runs BlockRender's
// link code against the real Minecraft, without the game (docs/superpowers/specs/2026-10-02-minecraft-link-design.md).
//
//     mclink.exe <script.txt>
//
// It runs the script (McLinkScript.h) at 60 Hz with Minecraft's player pinned where the script says, and reports
// what Minecraft sends. It refuses to start while another game side uses the mapping.
#include "McLinkScript.h"

#include "Coords.h"
#include "link/Link.h"
#include "link/Mapping.h"
#include "log/DdsWriter.h"

#include <Windows.h>
#include <timeapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <format>
#include <fstream>
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
		Say("explosion at {:.1f} {:.1f} {:.1f}, radius {:.1f}", aEvent.a, aEvent.b, aEvent.c, aEvent.d);
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
};

void PrintStatus(const Link::Status& aStatus, const Report& aReport)
{
	const auto& s = aStatus;
	Say("Minecraft: {}{}{}, feet {:.3f} {:.3f} {:.3f}, eye {:.3f} ({:.2f} above the feet), frame {}",
	    s.minecraft ? std::format("process {}", s.minecraftPid) : "absent", s.inWorld ? ", in its world" : "",
	    s.screenOpen ? ", a screen open" : "", s.minecraftFeet.x, s.minecraftFeet.y, s.minecraftFeet.z, s.minecraftEye.y,
	    s.minecraftEye.y - s.minecraftFeet.y, s.minecraftFrames);
	Say("game side: {}, {} teleports (sequence {}) to {:.3f} {:.3f} {:.3f}, look {:.1f} {:.1f}, floor {} {}", s.inGame ? "in game" : "not in game",
	    s.teleports, s.teleportSeq, s.target.x, s.target.y, s.target.z, s.look.yaw, s.look.pitch, s.floorEpoch,
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
	Say("events: {} explosions, {} others; {} inputs dropped; {} commands waiting; {} HUD images, {} refused", s.explosions, s.otherEvents,
	    s.inputsDropped, s.commandsWaiting, s.hudImages, s.hudRefused);
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
	Vec3d  feet{ -119.5, 6.14, 2007.5 };  // where V stood at spike 3's corner
	McLook look{ 180.0f, 0.0f };          // north
	bool   linked = false;
	bool   build = true;
	bool   hud = true;

	size_t   next = 0;
	bool     begun = false;
	double   begunAt = 0.0;
	double   stray = 0.0;
	uint64_t holdFrames = 0;
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
		Link::Frame frame;
		frame.now = now;
		frame.tickMs = GetTickCount64();
		frame.linked = linked;
		frame.buildMode = build;
		frame.takeHud = hud;
		frame.pose = GamePose{ WorldFromMc(feet), WorldFromMc({ feet.x, feet.y + kEyeHeight, feet.z }), ForwardFromLook(look) };
		link.Update(frame, settings, report);
		const auto& state = link.State();

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
			case Action::Kind::Build: build = action.on; break;
			case Action::Kind::Hud: hud = action.on; break;
			case Action::Kind::At: feet = { action.numbers[0], action.numbers[1], action.numbers[2] }; break;
			case Action::Kind::Look: look = { float(action.numbers[0]), float(action.numbers[1]) }; break;
			case Action::Kind::Floor: link.RequestFloor(); break;
			case Action::Kind::Status: PrintStatus(state, report); break;
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
	PrintStatus(link.State(), report);
	timeEndPeriod(1);
	return exitCode;
}
