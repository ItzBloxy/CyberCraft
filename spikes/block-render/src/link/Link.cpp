#include "link/Link.h"

#include "link/Floor.h"
#include "link/Mapping.h"

#include <chrono>
#include <cmath>
#include <format>

namespace cpmc::link
{
namespace
{
template<class T>
std::atomic_ref<T> Atomic(T& aValue)
{
	return std::atomic_ref<T>(aValue);
}

double Distance(const Vec3d& aA, const Vec3d& aB)
{
	return std::sqrt((aA.x - aB.x) * (aA.x - aB.x) + (aA.y - aB.y) * (aA.y - aB.y) + (aA.z - aB.z) * (aA.z - aB.z));
}
}

const char* RenderTypeName(size_t aType)
{
	static const char* const names[Link::kTypes] = { "pad",         "atlas",  "section", "clear-all", "texture", "avatar", "scene",
		                                             "atlas patch", "lights", "ragdoll", "solids",    "dug",     "other" };
	return names[aType < Link::kTypes ? aType : Link::kTypes - 1];
}

Link::Link(uint8_t* aBase)
    : base_(aBase), overlay_(aBase + proto::kOffOverlayCtl, aBase + proto::kOffOverlaySlotHdr, aBase + proto::kOffOverlayPixels,
                             proto::kOverlaySlotBytes)
{
}

void Link::Update(const Frame& aFrame, const LinkSettings& aSettings, LinkSink& aSink)
{
	// The heartbeat, then whether Minecraft is there: its heartbeat under 3 s old.
	auto* header = reinterpret_cast<proto::Header*>(base_);
	Atomic(header->skyrimHeartbeatMs).store(aFrame.tickMs, std::memory_order_release);
	const uint32_t pid = Atomic(header->mcPid).load(std::memory_order_acquire);
	const uint64_t beat = Atomic(header->mcHeartbeatMs).load(std::memory_order_acquire);
	const bool     there = pid != 0 && beat != 0 && (beat >= aFrame.tickMs || aFrame.tickMs - beat < kAliveMs);
	status_.minecraft = there;
	status_.minecraftPid = pid;
	status_.heartbeatAgeMs = beat != 0 && aFrame.tickMs > beat ? aFrame.tickMs - beat : 0;
	if (there && (!wasThere_ || pid != lastPid_))
	{
		// Minecraft (re)connected. A new Minecraft sends its clear, atlas and sections by itself; the game side sends
		// the floor and a teleport.
		if (lastPid_ != 0 && pid != lastPid_)
		{
			// A new Minecraft process: its overlay writer starts over at slot 1, so the swap does too. Its first image
			// comes once it's in its world, long after this. SkyCraft's Game.cpp also resets for the first Minecraft
			// it sees, and for one coming back, but neither needs it: Mapping::Open reset the swap before Minecraft
			// could see this game side, and Minecraft starts its writer over when it does; one coming back kept its
			// slots. A reset then could undo an image Minecraft has just published (it ships one left from before as
			// soon as it relinks), leaving its writer and our reader on one slot.
			overlay_.Reset();
		}
		status_.floorSent = false;
		teleportDue_ = true;
		aSink.Log(std::format("Minecraft is there (process {})", pid));
	}
	if (!there && wasThere_)
	{
		typer_.Clear();
		aSink.Log("Minecraft has gone");
	}
	wasThere_ = there;
	if (there)
	{
		lastPid_ = pid;
	}
	const auto mc = there ? ReadMcState(base_) : std::nullopt;
	status_.inWorld = mc && (mc->flags & proto::kMcInWorld);
	status_.screenOpen = mc && (mc->flags & proto::kMcScreenOpen);
	if (mc)
	{
		status_.minecraftFeet = { mc->x, mc->y, mc->z };
		status_.minecraftEye = { mc->eyeX, mc->eyeY, mc->eyeZ };
		status_.minecraftFrames = mc->frameCounter;
	}

	// In game while linked with V there. Linking teleports Minecraft's player and sends a fresh floor.
	const bool inGame = aFrame.linked && aFrame.pose.has_value();
	if (inGame && !wasInGame_)
	{
		teleportDue_ = true;
		status_.floorSent = false;
	}
	wasInGame_ = inGame;
	status_.inGame = inGame;
	if (aFrame.pose)
	{
		if (const auto look = LookFromForward(aFrame.pose->forward, status_.look.yaw))
		{
			status_.look = *look;
		}
	}
	if (inGame)
	{
		const Vec3d target = TeleportTarget(aFrame.pose->camera);
		const bool  due = teleportDue_ || Distance(target, status_.target) >= aSettings.teleportStep;
		if (due && aFrame.now - lastTeleportAt_ >= 1.0 / aSettings.teleportRate)
		{
			++status_.teleportSeq;
			++status_.teleports;
			status_.target = target;
			lastTeleportAt_ = aFrame.now;
			teleportDue_ = false;
		}
		const Vec3d feet = McFromWorld(aFrame.pose->feet);
		if (there && (!status_.floorSent || floorRequested_ || std::hypot(feet.x - status_.floorCentre.x, feet.z - status_.floorCentre.z) > 16.0 ||
		              std::abs(feet.y - status_.floorCentre.y) > 1.0))
		{
			SendFloor(feet, aSink);
			floorRequested_ = false;
		}
	}

	proto::SkyState state{};
	state.flags = (inGame ? uint32_t(proto::kSkyInGame) : 0u) | (aFrame.buildMode ? 0u : uint32_t(proto::kSkyMenuOpen));
	state.worldId = 1;
	state.collisionEpoch = status_.floorEpoch;
	state.posX = status_.target.x;
	state.posY = status_.target.y;
	state.posZ = status_.target.z;
	state.yaw = status_.look.yaw;
	state.pitch = status_.look.pitch;
	state.teleportSeq = status_.teleportSeq;
	state.viewportW = aSettings.viewportWidth;
	state.viewportH = aSettings.viewportHeight;
	WriteSkyState(base_, state);

	const auto step = typer_.Advance(aFrame.now, { status_.inWorld, status_.screenOpen });
	for (const auto& event : step.events)
	{
		Push(event);
	}
	for (const auto& line : step.log)
	{
		aSink.Log(line);
	}
	if (step.entered)
	{
		status_.lastEnter = aFrame.now;
	}
	status_.commandsWaiting = typer_.Waiting();
	status_.inputsDropped = inputsDropped_.load();

	DrainEvents(base_ + proto::kOffEventRing, [&](const proto::McEvent& aEvent) {
		if (aEvent.type == proto::kEvExplosion)
		{
			++status_.explosions;
			aSink.Explosion(aEvent);
		}
		else
		{
			++status_.otherEvents;
		}
	});

	const auto drainStart = std::chrono::steady_clock::now();
	const auto drained = DrainByteRing(base_ + proto::kOffRenderRing, proto::kRenRingDataBytes, aSettings.ringBudget,
	                                   [&](const RingMessage& aMessage) { return Take(aMessage, aSettings, aSink); });
	status_.drainMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - drainStart).count();
	status_.drainedBytes += drained.bytes;
	if (drained.resynced)
	{
		++status_.resyncs;
		status_.lastResync = drained.why;
		lastProblem_ = drained.why;
	}
	// Resyncs and rejected messages: counted each time, logged at most once a second, so a broken stream can't flood
	// the log.
	const uint64_t problems = status_.resyncs + status_.rejected;
	if (problems > problemsLogged_ && aFrame.now - problemLoggedAt_ >= 1.0)
	{
		const uint64_t fresh = problems - problemsLogged_;
		aSink.Log(fresh == 1 ? "render ring: " + lastProblem_
		                     : std::format("render ring: {} rejected messages or resyncs since the last report; the last: {}", fresh, lastProblem_));
		problemsLogged_ = problems;
		problemLoggedAt_ = aFrame.now;
	}

	if (aFrame.takeHud && there)
	{
		if (const auto frame = overlay_.Acquire())
		{
			++status_.hudImages;
			aSink.Hud(*frame);
		}
		status_.hudRefused = overlay_.Refused();
	}
}

bool Link::Take(const RingMessage& aMessage, const LinkSettings& aSettings, LinkSink& aSink)
{
	const size_t type = aMessage.type < kTypes - 1 ? aMessage.type : kTypes - 1;
	++status_.messages[type];
	status_.bytes[type] += aMessage.payload.size();
	std::string error;
	const auto  reject = [&] {
        ++status_.rejected;
        status_.lastRejected = error;
        lastProblem_ = error;
	};
	switch (aMessage.type)
	{
	case proto::kRenAtlas:
		if (const auto atlas = ParseAtlas(aMessage.payload, error))
		{
			status_.atlasSeen = true;
			aSink.Atlas(*atlas);
		}
		else
		{
			reject();
		}
		return true;
	case proto::kRenSection:
		if (!status_.atlasSeen)
		{
			++status_.sectionsBeforeAtlas;  // left over from before we read: its UVs belong to an atlas we never saw
			return false;
		}
		if (auto mesh = ParseSection(aMessage.payload, aSettings.keepTranslucent, error))
		{
			status_.translucentDropped += mesh->translucentDropped;
			aSink.Section(std::move(*mesh));
		}
		else
		{
			reject();
		}
		return true;
	case proto::kRenClearAll:
		status_.atlasSeen = false;  // Minecraft sends its atlas next
		aSink.ClearAll();
		return true;
	default:
		return false;  // atlas patches, lights, solids, digging, textures, avatar, scene, ragdoll: stepped over
	}
}

void Link::SendFloor(const Vec3d& aFeet, LinkSink& aSink)
{
	// A new epoch each time: its clear message drops the last floor, and anything half-sent.
	const uint32_t epoch = status_.floorEpoch + 1;
	uint8_t* const ring = base_ + proto::kOffCollisionRing;
	bool           sent = WriteByteRing(ring, proto::kColRingDataBytes, proto::kColClear, FloorClear(epoch));
	for (const auto& [x, z] : FloorRegions(aFeet.x, aFeet.z))
	{
		sent = sent && WriteByteRing(ring, proto::kColRingDataBytes, proto::kColTris, FloorRegion(x, z, aFeet.y, epoch));
	}
	status_.floorEpoch = epoch;
	status_.floorSent = sent;
	if (sent)
	{
		status_.floorCentre = aFeet;
		aSink.Log(std::format("floor {} at height {:.2f} around Minecraft {:.1f} {:.1f}", epoch, aFeet.y, aFeet.x, aFeet.z));
	}
	else
	{
		aSink.Log("the floor didn't fit in the collision ring; trying again next frame");
	}
}

bool Link::Type(std::string_view aCommandOrName, std::string& aError)
{
	return typer_.Queue(aCommandOrName, aError);
}

bool Link::Push(const proto::InputEvent& aEvent)
{
	std::lock_guard lock(inputMutex_);
	if (PushInput(base_ + proto::kOffInputRing, aEvent))
	{
		return true;
	}
	++inputsDropped_;
	return false;
}

void Link::RequestFloor()
{
	floorRequested_ = true;
}
}
