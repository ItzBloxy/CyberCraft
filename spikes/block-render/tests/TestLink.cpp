#include "Harness.h"

#include "link/Floor.h"
#include "link/Link.h"

#include <Windows.h>

#include <cstring>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// The mapping's layout in plain memory, with Minecraft's side played the way SkyCraft's SkyLink plays it.
struct FakeMinecraft
{
	FakeMinecraft()
	{
		base = static_cast<uint8_t*>(VirtualAlloc(nullptr, proto::kMappingBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	}
	~FakeMinecraft() { VirtualFree(base, 0, MEM_RELEASE); }

	proto::Header* Header() { return reinterpret_cast<proto::Header*>(base); }

	void Beat(uint32_t aPid, uint64_t aTickMs)
	{
		Header()->mcPid = aPid;
		Header()->mcHeartbeatMs = aTickMs;
	}

	void State(uint32_t aFlags)
	{
		proto::McState state{};
		std::memcpy(&state, base + proto::kOffMcState, sizeof state);
		state.seq += 2;
		state.flags = aFlags;
		state.x = -119.5;
		state.eyeX = -119.5;
		state.eyeY = 7.76;
		state.eyeZ = 2007.5;
		state.frameCounter += 1;
		std::memcpy(base + proto::kOffMcState, &state, sizeof state);
	}

	// McState with a tick stamped at aStamp (microseconds, the tests' clock): Minecraft's player standing at aFeet
	// (Minecraft axes), its eye 1.62 up, having applied teleport aAck.
	void Ticked(uint32_t aAck, const Vec3d& aFeet, double aStamp)
	{
		proto::McState state{};
		std::memcpy(&state, base + proto::kOffMcState, sizeof state);
		state.seq += 2;
		state.flags = proto::kMcInWorld | proto::kMcOnGround;
		state.teleportAck = aAck;
		state.x = state.prevX = state.curX = aFeet.x;
		state.y = state.prevY = state.curY = aFeet.y;
		state.z = state.prevZ = state.curZ = aFeet.z;
		state.tickEyeO = state.tickEye = 1.62f;
		state.tickQpc = int64_t(aStamp * 1'000'000.0);
		state.tickMs = 50.0f;
		state.frameCounter += 1;
		std::memcpy(base + proto::kOffMcState, &state, sizeof state);
	}

	void Send(uint32_t aType, const std::vector<uint8_t>& aPayload)
	{
		CHECK(WriteByteRing(base + proto::kOffRenderRing, proto::kRenRingDataBytes, aType, aPayload));
	}

	proto::SkyState Sky()
	{
		proto::SkyState state;
		std::memcpy(&state, base + proto::kOffSkyState, sizeof state);
		return state;
	}

	std::vector<RingMessage> Collision(std::vector<std::vector<uint8_t>>& aPayloads)
	{
		std::vector<RingMessage> messages;
		DrainByteRing(base + proto::kOffCollisionRing, proto::kColRingDataBytes, ~0ull, [&](const RingMessage& aMessage) {
			aPayloads.emplace_back(aMessage.payload.begin(), aMessage.payload.end());
			messages.push_back(aMessage);
			return true;
		});
		return messages;
	}

	std::vector<proto::InputEvent> Inputs()
	{
		auto*                          ring = base + proto::kOffInputRing;
		uint64_t&                      head = *reinterpret_cast<uint64_t*>(ring);
		uint64_t&                      tail = *reinterpret_cast<uint64_t*>(ring + 0x40);
		std::vector<proto::InputEvent> events;
		for (; tail < head; ++tail)
		{
			proto::InputEvent event;
			std::memcpy(&event, ring + 0x80 + (tail % proto::kInputRingEntries) * sizeof event, sizeof event);
			events.push_back(event);
		}
		return events;
	}

	void Explode(float aX, float aY, float aZ, float aRadius)
	{
		auto*          ring = base + proto::kOffEventRing;
		uint64_t&      head = *reinterpret_cast<uint64_t*>(ring);
		proto::McEvent event{ proto::kEvExplosion, 0, aX, aY, aZ, aRadius, 0, 0 };
		std::memcpy(ring + 0x80 + (head % proto::kEventRingEntries) * sizeof event, &event, sizeof event);
		++head;
	}

	void PublishHud(uint32_t aWidth, uint32_t aHeight)
	{
		proto::OverlaySlotHdr slot{};
		slot.width = aWidth;
		slot.height = aHeight;
		slot.flags = 1;
		std::memcpy(base + proto::kOffOverlaySlotHdr + hudBack * sizeof slot, &slot, sizeof slot);
		uint32_t&      state = *reinterpret_cast<uint32_t*>(base + proto::kOffOverlayCtl);
		const uint32_t old = state;
		state = hudBack | proto::kOverlayDirty;
		hudBack = old & 3;
	}

	// The overlay's middle slot, and the slot a HUD image's pixels are in.
	uint32_t Middle() { return *reinterpret_cast<uint32_t*>(base + proto::kOffOverlayCtl) & 3; }
	uint32_t SlotOf(const uint8_t* aPixels) { return uint32_t((aPixels - (base + proto::kOffOverlayPixels)) / proto::kOverlaySlotBytes); }

	uint8_t* base = nullptr;
	uint32_t hudBack = 1;
};

struct Sink : LinkSink
{
	void Atlas(const AtlasImage& aAtlas) override { atlases.push_back({ aAtlas.width, aAtlas.height }); }
	void Section(SectionMesh&& aMesh) override { sections.push_back(std::move(aMesh)); }
	void ClearAll() override { ++clears; }
	void Explosion(const proto::McEvent& aEvent) override { explosions.push_back(aEvent); }
	void Hud(const HudFrame& aFrame) override
	{
		huds.push_back({ aFrame.width, aFrame.height });
		hudPixels = aFrame.pixels;
	}
	void Log(const std::string& aLine) override { log.push_back(aLine); }

	int Count(const std::string& aStart) const  // log lines starting with aStart
	{
		int count = 0;
		for (const auto& line : log)
		{
			if (line.rfind(aStart, 0) == 0)
			{
				++count;
			}
		}
		return count;
	}

	bool Logged(const std::string& aStart) const { return Count(aStart) > 0; }

	std::vector<std::pair<uint32_t, uint32_t>> atlases;
	std::vector<SectionMesh>                   sections;
	int                                        clears = 0;
	std::vector<proto::McEvent>                explosions;
	std::vector<std::pair<uint32_t, uint32_t>> huds;
	const uint8_t*                             hudPixels = nullptr;  // the last HUD image's
	std::vector<std::string>                   log;
};

// The overlay's three slots stay apart: Minecraft's back slot, the middle, and the one the link took last.
bool SlotsApart(FakeMinecraft& aMinecraft, const Sink& aSink)
{
	const uint32_t taken = aMinecraft.SlotOf(aSink.hudPixels);
	const uint32_t middle = aMinecraft.Middle();
	return taken != middle && taken != aMinecraft.hudBack && middle != aMinecraft.hudBack;
}

// V at spike 3's corner, looking north: feet on the road at z 6.14, the camera 1.60 m above, as spike 4 measured.
GamePose Corner(double aEast = 0.0, double aUp = 0.0)
{
	return { { -119.5 + aEast, -2007.5, 6.14 + aUp }, { -119.5 + aEast, -2007.5, 7.74 + aUp }, { 0.0f, 1.0f, 0.0f } };
}

// A frame outside build mode, its clock in seconds and in microseconds for the ticks.
Link::Frame At(double aNow, std::optional<GamePose> aPose = Corner(), bool aLinked = true)
{
	Link::Frame frame;
	frame.now = aNow;
	frame.tickMs = uint64_t(aNow * 1000.0);
	frame.qpc = int64_t(aNow * 1'000'000.0);
	frame.qpcPerSecond = 1'000'000;
	frame.pose = aPose;
	frame.linked = aLinked;
	return frame;
}

Link::Frame InBuild(double aNow, std::optional<GamePose> aPose = Corner())
{
	Link::Frame frame = At(aNow, aPose);
	frame.buildMode = true;
	return frame;
}

Link::Frame HudAt(double aNow)  // a frame that takes HUD images
{
	Link::Frame frame = At(aNow);
	frame.takeHud = true;
	return frame;
}

std::vector<uint8_t> Atlas2x2()
{
	std::vector<uint8_t> payload(8 + 16, 0x7F);
	const uint32_t       side = 2;
	std::memcpy(payload.data(), &side, 4);
	std::memcpy(payload.data() + 4, &side, 4);
	return payload;
}

std::vector<uint8_t> SectionPayload(int32_t aX, uint32_t aVertices, uint32_t aClaimed)
{
	std::vector<uint8_t> payload(16 + aVertices * sizeof(Vertex));
	const int32_t        zero = 0;
	std::memcpy(payload.data(), &aX, 4);
	std::memcpy(payload.data() + 4, &zero, 4);
	std::memcpy(payload.data() + 8, &zero, 4);
	std::memcpy(payload.data() + 12, &aClaimed, 4);
	return payload;
}

const LinkSettings kSettings;
}

TEST(LinkWritesTheHeartbeatAndSkyState)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	LinkSettings  settings;
	settings.viewportWidth = 1280;
	settings.viewportHeight = 720;
	auto frame = At(1.0, std::nullopt, false);
	frame.buildMode = false;
	link.Update(frame, settings, sink);
	CHECK_EQ(minecraft.Header()->skyrimHeartbeatMs, 1000ull);
	const auto sky = minecraft.Sky();
	CHECK_EQ(sky.seq, 2u);
	CHECK_EQ(sky.flags, uint32_t(proto::kSkyMenuOpen));  // not in game, and build mode is off
	CHECK_EQ(sky.viewportW, 1280u);
	CHECK_EQ(sky.viewportH, 720u);
	CHECK_EQ(sky.worldId, 1u);
	frame.linked = true;  // linked, but V isn't there: not in game, and no teleport
	link.Update(frame, settings, sink);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyMenuOpen));
	CHECK_EQ(minecraft.Sky().teleportSeq, 0u);
}

TEST(LinkSeesMinecraftComeAndGo)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	CHECK(link.State().minecraft);
	CHECK_EQ(link.State().minecraftPid, 77u);
	CHECK(link.State().inWorld);
	CHECK_NEAR(link.State().minecraftFeet.x, -119.5, 1e-9);
	CHECK(sink.Logged("Minecraft is there (process 77)"));
	link.Update(At(5.0), kSettings, sink);  // no heartbeat for 4 s
	CHECK(!link.State().minecraft);
	CHECK(!link.State().inWorld);
	CHECK(sink.Logged("Minecraft has gone"));
}

TEST(LinkReportsMinecraftsEyeAndTheDrainTime)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	CHECK_NEAR(link.State().minecraftEye.x, -119.5, 1e-9);
	CHECK_NEAR(link.State().minecraftEye.y, 7.76, 1e-9);
	CHECK(link.State().drainMs >= 0.0);
}

TEST(LinkNamesTheRenderTypes)
{
	CHECK_STR(RenderTypeName(proto::kRenSection), "section");
	CHECK_STR(RenderTypeName(proto::kRenAtlasRegion), "atlas patch");
	CHECK_STR(RenderTypeName(Link::kTypes - 1), "other");
	CHECK_STR(RenderTypeName(99), "other");
}

TEST(LinkTeleportsMinecraftsEyeToTheCamera)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	link.Update(At(1.0), kSettings, sink);
	const auto sky = minecraft.Sky();
	CHECK_EQ(sky.flags, uint32_t(proto::kSkyInGame | proto::kSkyMenuOpen));  // build mode off: the menu flag
	CHECK_EQ(sky.teleportSeq, 1u);
	CHECK_NEAR(sky.posX, -119.5, 1e-9);
	CHECK_NEAR(sky.posY, 6.12, 1e-9);  // the camera, less Minecraft's standing eye height
	CHECK_NEAR(sky.posZ, 2007.5, 1e-9);
	CHECK_NEAR(sky.yaw, 180.0, 1e-4);
	CHECK_NEAR(sky.pitch, 0.0, 1e-4);
	link.Update(At(2.0, Corner(), false), kSettings, sink);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyMenuOpen));  // unlinked
}

TEST(LinkStartsItsTeleportSequenceFromTheClock)
{
	// A Minecraft that kept running keeps the last teleportSeq it applied, and teleports only when it changes. A new
	// game side starting from 0 again could repeat it: no teleport, and an acknowledgement that looks like an arrival.
	// So the sequence starts from the clock, as SkyCraft's does, and odd, so never 0.
	CHECK(FirstTeleportSeq() % 2 == 1);
	FakeMinecraft  minecraft;
	Link           link(minecraft.base);
	const uint32_t first = link.State().teleportSeq;
	CHECK(first % 2 == 1);
	Sink sink;
	link.Update(At(1.0), kSettings, sink);  // linked, with V: the first teleport
	CHECK_EQ(minecraft.Sky().teleportSeq, first + 1);
}

TEST(LinkTeleportsByStepAndRate)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	link.Update(At(1.00), kSettings, sink);
	link.Update(At(1.50, Corner(0.05)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 1u);  // under the 0.1 m step
	link.Update(At(1.55, Corner(0.25)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 2u);
	link.Update(At(1.60, Corner(0.50)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 2u);  // only 0.05 s since the last: 10 a second at most
	link.Update(At(1.66, Corner(0.50)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 3u);
	CHECK_NEAR(minecraft.Sky().posX, -119.0, 1e-9);
}

TEST(LinkSendsTheFloorOnceMinecraftIsThere)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base);
	Sink                              sink;
	std::vector<std::vector<uint8_t>> payloads;
	link.Update(At(1.0), kSettings, sink);
	CHECK(minecraft.Collision(payloads).empty());  // nobody reads it yet
	minecraft.Beat(77, 2000);
	link.Update(At(2.0), kSettings, sink);
	auto messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(1 + 49 * 4));  // the clear, then each region's triangles and three voxel regions
	CHECK_EQ(messages[0].type, uint32_t(proto::kColClear));
	CHECK(payloads[0] == std::vector<uint8_t>({ 1, 0, 0, 0 }));
	CHECK_EQ(messages[1].type, uint32_t(proto::kColTris));
	CHECK_EQ(messages[2].type, uint32_t(proto::kColRegion));
	CHECK_EQ(messages[3].type, uint32_t(proto::kColRegion));
	CHECK_EQ(messages[4].type, uint32_t(proto::kColRegion));
	CHECK_EQ(messages[5].type, uint32_t(proto::kColTris));
	CHECK(payloads[2] == FloorVoxeler(6.14).Column(-18, 247, 1)[0]);  // the first region's, the floor's own
	CHECK_EQ(minecraft.Sky().collisionEpoch, 1u);
	minecraft.Beat(77, 2100);
	link.Update(At(2.1), kSettings, sink);
	CHECK(minecraft.Collision(payloads).empty());
	minecraft.Beat(77, 2200);
	link.Update(At(2.2, Corner(20.0)), kSettings, sink);  // 20 m along: a new floor
	CHECK_EQ(minecraft.Collision(payloads).size(), size_t(1 + 49 * 4));
	CHECK_EQ(link.State().floorEpoch, 2u);
	minecraft.Beat(77, 2300);
	link.Update(At(2.3, Corner(20.0, 1.5)), kSettings, sink);  // 1.5 m up
	CHECK_EQ(link.State().floorEpoch, 3u);
	link.RequestFloor();
	minecraft.Beat(77, 2400);
	link.Update(At(2.4, Corner(20.0, 1.5)), kSettings, sink);
	CHECK_EQ(link.State().floorEpoch, 4u);
	CHECK_NEAR(link.State().floorCentre.y, 7.64, 1e-9);
}

TEST(LinkDropsSectionsBeforeTheAtlas)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenSection, SectionPayload(1, 3, 3));
	link.Update(At(1.0), kSettings, sink);
	CHECK(sink.sections.empty());
	CHECK_EQ(link.State().sectionsBeforeAtlas, 1ull);
	minecraft.Send(proto::kRenClearAll, {});
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	minecraft.Send(proto::kRenSection, SectionPayload(2, 6, 6));
	link.Update(At(1.1), kSettings, sink);
	CHECK_EQ(sink.clears, 1);
	CHECK_EQ(sink.atlases.size(), size_t(1));
	CHECK_EQ(sink.atlases[0].first, 2u);
	CHECK_EQ(sink.sections.size(), size_t(1));
	CHECK_EQ(sink.sections[0].x, 2);
	CHECK_EQ(sink.sections[0].vertices.size(), size_t(6));
	CHECK(link.State().atlasSeen);
}

TEST(LinkStepsOverOtherMessagesAndKeepsToTheBudget)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	for (int i = 0; i < 3; ++i)
	{
		minecraft.Send(proto::kRenScene, std::vector<uint8_t>(4000));
		minecraft.Send(proto::kRenSection, SectionPayload(i, 3, 3));
	}
	LinkSettings settings;
	settings.ringBudget = 100;  // the atlas (32 bytes) and one section (120 bytes) reach it
	link.Update(At(1.0), settings, sink);
	CHECK_EQ(sink.sections.size(), size_t(1));
	CHECK_EQ(link.State().messages[proto::kRenScene], 1ull);  // stepped over, without counting toward the budget
	link.Update(At(1.1), kSettings, sink);
	CHECK_EQ(sink.sections.size(), size_t(3));
	CHECK_EQ(link.State().messages[proto::kRenScene], 3ull);
	CHECK_EQ(link.State().messages[proto::kRenSection], 3ull);
}

TEST(LinkRejectsABadPayloadAndCarriesOn)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	minecraft.Send(proto::kRenSection, SectionPayload(1, 3, 100));  // claims 100 vertices, carries 3
	minecraft.Send(proto::kRenSection, SectionPayload(2, 3, 3));
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(link.State().rejected, 1ull);
	CHECK(link.State().lastRejected.find("claims 100 vertices") != std::string::npos);
	CHECK_EQ(sink.sections.size(), size_t(1));
	CHECK_EQ(sink.sections[0].x, 2);
	CHECK_EQ(link.State().resyncs, 0ull);
}

TEST(LinkTypesCommandsThroughTheInputRing)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	std::string   error;
	CHECK(link.Type("/gamemode creative", error));
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	auto inputs = minecraft.Inputs();
	CHECK_EQ(inputs.size(), size_t(2));
	CHECK(inputs[0].type == proto::kInKey && inputs[0].code == kSdlT && inputs[0].a == 1);
	minecraft.Beat(77, 1100);
	minecraft.State(proto::kMcInWorld | proto::kMcScreenOpen);
	link.Update(At(1.1), kSettings, sink);
	inputs = minecraft.Inputs();
	CHECK_EQ(inputs.size(), size_t(18 + 2));
	CHECK(link.State().lastEnter.has_value());
	CHECK_NEAR(*link.State().lastEnter, 1.1, 1e-9);
	minecraft.Beat(77, 1200);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.2), kSettings, sink);
	CHECK(sink.Logged("typed: /gamemode creative"));
	CHECK_EQ(link.State().commandsWaiting, size_t(0));
}

TEST(LinkPassesOnExplosionsAndHudImages)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.Explode(-117.0f, 6.0f, 2010.0f, 4.0f);
	link.Update(At(1.0), kSettings, sink);  // Minecraft connects
	CHECK_EQ(sink.explosions.size(), size_t(1));
	CHECK_EQ(sink.explosions[0].a, -117.0f);
	CHECK_EQ(sink.explosions[0].d, 4.0f);
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 1100);
	link.Update(At(1.1), kSettings, sink);
	CHECK(sink.huds.empty());  // not asked for, so it waits
	auto frame = At(1.2);
	frame.takeHud = true;
	minecraft.Beat(77, 1200);
	link.Update(frame, kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(1));
	CHECK_EQ(sink.huds[0].first, 1280u);
	CHECK_EQ(link.State().hudImages, 1ull);
}

TEST(LinkKeepsAnImagePublishedBeforeItSawMinecraft)
{
	// SkyCraft ships an image left from before as soon as it relinks, which can be before the link first sees it.
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.PublishHud(1280, 720);
	link.Update(HudAt(1.0), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(1));
	CHECK(SlotsApart(minecraft, sink));
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 1100);
	link.Update(HudAt(1.1), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(2));
	CHECK(SlotsApart(minecraft, sink));
}

TEST(LinkKeepsTheSwapWhenMinecraftComesBack)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	link.Update(HudAt(1.0), kSettings, sink);
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 1100);
	link.Update(HudAt(1.1), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(1));
	link.Update(HudAt(5.0), kSettings, sink);  // no heartbeat for 3.9 s: gone
	CHECK(!link.State().minecraft);
	minecraft.Beat(77, 5100);  // back: the same process, its writer's slot where it was
	link.Update(HudAt(5.1), kSettings, sink);
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 5200);
	link.Update(HudAt(5.2), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(2));
	CHECK(SlotsApart(minecraft, sink));
}

TEST(LinkStartsTheSwapOverForANewMinecraft)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	link.Update(HudAt(1.0), kSettings, sink);
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 1100);
	link.Update(HudAt(1.1), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(1));
	minecraft.hudBack = 1;  // Minecraft restarted: a new process, its writer starting over
	minecraft.Beat(78, 2000);
	link.Update(HudAt(2.0), kSettings, sink);
	CHECK(sink.Logged("Minecraft is there (process 78)"));
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(78, 2100);
	link.Update(HudAt(2.1), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(2));
	CHECK(SlotsApart(minecraft, sink));
}

TEST(LinkLogsRingProblemsOnceASecond)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	for (int i = 0; i < 3; ++i)
	{
		minecraft.Send(proto::kRenSection, SectionPayload(i, 3, 100));  // each claims 100 vertices, and carries 3
	}
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(link.State().rejected, 3ull);
	CHECK_EQ(sink.Count("render ring:"), 1);
	CHECK(sink.Logged("render ring: 3 rejected messages or resyncs since the last report; the last: section 2 0 0"));
	minecraft.Send(proto::kRenSection, SectionPayload(5, 3, 100));
	link.Update(At(1.5), kSettings, sink);
	CHECK_EQ(link.State().rejected, 4ull);
	CHECK_EQ(sink.Count("render ring:"), 1);  // within the second: counted, not logged
	link.Update(At(2.1), kSettings, sink);
	CHECK_EQ(sink.Count("render ring:"), 2);  // logged once the second has passed
	CHECK(sink.Logged("render ring: section 5 0 0 claims 100 vertices"));
}

TEST(LinkMenuFlagFollowsBuildMode)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	auto          frame = At(1.0);
	frame.buildMode = false;
	link.Update(frame, kSettings, sink);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame | proto::kSkyMenuOpen));
	frame.buildMode = true;
	link.Update(frame, kSettings, sink);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame));
}

TEST(LinkCountsDroppedInputs)
{
	FakeMinecraft           minecraft;
	Link                    link(minecraft.base);
	Sink                    sink;
	const proto::InputEvent click{ proto::kInMouseButton, 3, 1, 0, 0 };
	for (uint32_t i = 0; i < proto::kInputRingEntries; ++i)
	{
		CHECK(link.Push(click));
	}
	CHECK(!link.Push(click));
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(link.State().inputsDropped, 1ull);
}

TEST(LinkEntersBuildModeWithOneTeleportToVsFeet)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 1u);  // outside build mode: Minecraft's eye at V's camera
	CHECK_EQ(link.State().floorEpoch, 1u);
	minecraft.Collision(payloads);
	link.Update(InBuild(1.1), kSettings, sink);
	// One teleport, to V's feet, and a fresh floor there.
	CHECK_EQ(minecraft.Sky().teleportSeq, 2u);
	CHECK_NEAR(minecraft.Sky().posY, 6.14, 1e-9);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame));
	CHECK_EQ(link.State().floorEpoch, 2u);
	CHECK_EQ(minecraft.Collision(payloads).size(), size_t(1 + 49 * 4));
	CHECK(link.State().drive == DriveState::Waiting);
	CHECK(sink.Logged("build mode: Minecraft's player goes to V's feet"));
	// Minecraft hasn't let go: V is held where build mode started, though it moved (a key held from before).
	link.Update(InBuild(1.2, Corner(3.0)), kSettings, sink);
	CHECK(link.State().placeV.has_value());
	CHECK_NEAR(link.State().placeV->x, -119.5, 1e-9);
	CHECK_NEAR(link.State().placeV->z, 6.14, 1e-9);
	// No teleports in build mode, and the position is V's feet, every frame.
	CHECK_EQ(minecraft.Sky().teleportSeq, 2u);
	CHECK_NEAR(minecraft.Sky().posX, -116.5, 1e-9);
	CHECK_NEAR(minecraft.Sky().posY, 6.14, 1e-9);
}

TEST(LinkPlacesVUnderMinecraftsEyeOnceItLetsGo)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(At(1.05), kSettings, sink);  // a second level frame: V's eye height, 1.60
	CHECK_NEAR(link.State().eyeHeight, 1.60, 1e-9);
	link.Update(InBuild(1.1), kSettings, sink);
	// Minecraft has applied teleport 2 and let its player go, and it has walked half a block east.
	minecraft.Ticked(2, { -119.0, 6.14, 2007.5 }, 1.15);
	link.Update(InBuild(1.2), kSettings, sink);
	CHECK(link.State().drive == DriveState::Driving);
	CHECK(sink.Logged("Minecraft let its player go"));
	CHECK(link.State().placeV.has_value());
	CHECK_NEAR(link.State().placeV->x, -119.0, 1e-9);
	CHECK_NEAR(link.State().placeV->y, -2007.5, 1e-9);
	CHECK_NEAR(link.State().placeV->z, 6.16, 1e-6);  // V's camera 1.60 up, at Minecraft's eye 1.62 up
	CHECK_EQ(link.State().teleportAck, 2u);
	CHECK(link.State().minecraftFlags & proto::kMcOnGround);
	CHECK_EQ(link.State().waitedFrames, 0ull);
	// Placed where it was sent: V's camera is right where Minecraft's eye was. No newer tick: this frame waited.
	link.Update(InBuild(1.25, Corner(0.5, 0.02)), kSettings, sink);
	CHECK(link.State().eyeDistance.has_value());
	CHECK_NEAR(*link.State().eyeDistance, 0.0, 1e-6);
	CHECK_EQ(link.State().waitedFrames, 1ull);
}

TEST(LinkKeepsItsFloorWhileMinecraftDrives)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(InBuild(1.1), kSettings, sink);
	minecraft.Ticked(2, { -119.5, 6.14, 2007.5 }, 1.15);
	link.Update(InBuild(1.2), kSettings, sink);
	minecraft.Collision(payloads);
	// V placed 1.5 m up, Minecraft's player having jumped: the floor stays.
	link.Update(InBuild(1.3, Corner(0.0, 1.5)), kSettings, sink);
	CHECK(minecraft.Collision(payloads).empty());
	// Minecraft's player walked 20 m east: the floor grows around it at its height, with no clear, so no new epoch.
	minecraft.Ticked(2, { -99.5, 6.14, 2007.5 }, 1.35);
	link.Update(InBuild(1.4, Corner(20.0)), kSettings, sink);
	const auto messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(49 * 4));
	CHECK_EQ(messages[0].type, uint32_t(proto::kColTris));
	CHECK_EQ(link.State().floorEpoch, 2u);
	CHECK_NEAR(link.State().floorCentre.x, -99.5, 1e-9);
	CHECK_NEAR(link.State().floorCentre.y, 6.14, 1e-9);
	CHECK(sink.Logged("floor 2 grows"));
}

TEST(LinkWaitsAgainWhenMinecraftHoldsItsPlayer)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(InBuild(1.1), kSettings, sink);
	minecraft.Ticked(2, { -119.5, 6.14, 2007.5 }, 1.15);
	link.Update(InBuild(1.2), kSettings, sink);
	// Minecraft teleported its player by itself, and holds it: its acknowledgement falls behind. V stays where it is.
	minecraft.Ticked(1, { -50.0, 6.14, 2007.5 }, 1.25);
	link.Update(InBuild(1.3, Corner(1.0)), kSettings, sink);
	CHECK(link.State().drive == DriveState::Waiting);
	CHECK_NEAR(link.State().placeV->x, -118.5, 1e-9);
	CHECK(sink.Logged("Minecraft holds its player again"));
}

TEST(LinkFollowsVAgainAfterBuildMode)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(InBuild(1.1), kSettings, sink);
	minecraft.Ticked(2, { -119.5, 6.14, 2007.5 }, 1.15);
	link.Update(InBuild(1.2), kSettings, sink);
	link.Update(At(1.3, Corner(1.0)), kSettings, sink);  // build mode off
	CHECK(link.State().drive == DriveState::Off);
	CHECK(!link.State().placeV.has_value());
	CHECK_EQ(minecraft.Sky().teleportSeq, 3u);  // a teleport at once, to V's camera
	CHECK_NEAR(minecraft.Sky().posX, -118.5, 1e-9);
	CHECK_NEAR(minecraft.Sky().posY, 6.12, 1e-9);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame | proto::kSkyMenuOpen));
	CHECK(sink.Logged("build mode ended"));
	// In and out again without V moving: V's camera is 2 cm from where build mode put Minecraft's player, inside the
	// teleport step, and the teleport still goes at once.
	link.Update(InBuild(2.0, Corner(1.0)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 4u);  // build mode's own
	link.Update(At(2.2, Corner(1.0)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 5u);
}

TEST(LinkEndsBuildModeWhenMinecraftStaysTooFar)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(InBuild(1.1), kSettings, sink);
	minecraft.Ticked(2, { -94.5, 6.14, 2007.5 }, 1.15);  // 25 m east of V, which stays put
	for (int frame = 0; frame <= 10; ++frame)
	{
		link.Update(InBuild(1.2 + frame * 0.1), kSettings, sink);
		CHECK(link.State().endBuild.empty() == (frame < 10));
	}
	CHECK_STR(link.State().endBuild, "Minecraft's player stayed over 20 m from V for a second");
	CHECK_EQ(link.State().driveSkipped, 11ull);
}

TEST(LinkMeasuresVsEyeHeightOutsideBuildModeOnly)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	GamePose      pose = Corner();
	pose.camera.z = 6.14 + 1.70;
	link.Update(At(1.0, pose), kSettings, sink);
	link.Update(At(1.1, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
	pose.camera.z = 6.14 + 1.20;
	link.Update(InBuild(1.2, pose), kSettings, sink);
	link.Update(InBuild(1.3, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
}

TEST(LinkMeasuresVsEyeHeightOnlyWhileVCanBePlaced)
{
	// In a car's seat, V's camera is 1.18 m above its feet (spike 5's step 2): that isn't V's eye height.
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	GamePose      pose = Corner();
	link.Update(At(1.0, pose), kSettings, sink);  // getting into the car: V's feet are noted
	pose.camera.z = 6.14 + 1.18;
	Link::Frame seated = At(1.1, pose);
	seated.placeable = false;
	link.Update(seated, kSettings, sink);
	seated.now = 1.2;
	link.Update(seated, kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.60, 1e-9);
	// Out of the car and standing, at the same height: the first frame only notes V's feet, the second measures.
	pose.camera.z = 6.14 + 1.70;
	link.Update(At(1.3, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.60, 1e-9);
	link.Update(At(1.4, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
}
