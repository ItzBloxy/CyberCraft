#include "link/Link.h"

#include "link/Floor.h"
#include "link/Mapping.h"

#include <algorithm>
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

const char* GroundName(Ground aGround)
{
	return aGround == Ground::City ? "city" : "floor";
}

uint32_t FirstTeleportSeq()
{
	return uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()) | 1u;
}

Link::Link(uint8_t* aBase, uint32_t aFirstTeleportSeq, unsigned aGroundThreads)
    : base_(aBase), overlay_(aBase + proto::kOffOverlayCtl, aBase + proto::kOffOverlaySlotHdr, aBase + proto::kOffOverlayPixels,
                             proto::kOverlaySlotBytes)
    , groundThreads_(aGroundThreads)
{
	status_.teleportSeq = aFirstTeleportSeq;
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
			ticks_.Reset();
		}
		status_.floorSent = false;
		teleportDue_ = true;
		cityClear_ = true;  // the city starts over, with a clear, once its collision has had time to stream in
		areas_.Reset(aFrame.now + GroundAreas::kSettle);
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
	std::optional<TickSample> sample;
	if (mc)
	{
		status_.minecraftFeet = { mc->x, mc->y, mc->z };
		status_.minecraftEye = { mc->eyeX, mc->eyeY, mc->eyeZ };
		status_.minecraftFrames = mc->frameCounter;
		status_.minecraftFlags = mc->flags;
		status_.minecraftTickMs = mc->tickMs;
		status_.teleportAck = mc->teleportAck;
		sample = ticks_.Frame(*mc, aFrame.qpc, aFrame.qpcPerSecond);  // every frame, so it has settled when it drives
	}
	status_.renderDelayMs = ticks_.DelayMs();

	// The ground (spike 5's "Part 2"). Switching clears the last one, with a new epoch: the city's own clear, or the
	// floor's, sent fresh.
	if (aSettings.ground != status_.ground)
	{
		status_.ground = aSettings.ground;
		status_.floorSent = false;
		cityClear_ = aSettings.ground == Ground::City;
		areas_.Reset(aFrame.now);
		outbox_.clear();
		aSink.Log(std::format("the ground is the {} now", GroundName(aSettings.ground)));
	}
	if (std::exchange(cityRestart_, false))
	{
		cityClear_ = true;
		areas_.Reset(aFrame.now);
		outbox_.clear();
	}
	const bool city = status_.ground == Ground::City;
	if (aFrame.linked && !wasLinked_)
	{
		cityClear_ = true;  // the link starts: the city starts over
		areas_.Reset(aFrame.now + GroundAreas::kSettle);
	}
	wasLinked_ = aFrame.linked;

	// In game while linked with V there. Linking teleports Minecraft's player and sends a fresh floor. V found again,
	// after a load say, has the city read again once its collision has streamed in.
	const bool inGame = aFrame.linked && aFrame.pose.has_value();
	if (inGame && !wasInGame_)
	{
		teleportDue_ = true;
		status_.floorSent = false;
		areas_.Reset(aFrame.now + GroundAreas::kSettle);
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

	// Build mode (spike 5): one teleport, of Minecraft's player to V's feet, and then Minecraft drives and V follows.
	const bool build = inGame && aFrame.buildMode;
	status_.placeV.reset();
	status_.endBuild.clear();
	status_.eyeDistance.reset();
	if (aFrame.pose && !build && aFrame.placeable)
	{
		eyeHeight_.Note(aFrame.pose->feet, aFrame.pose->camera);
	}
	else
	{
		eyeHeight_.Break();
	}
	if (build && !wasBuild_)
	{
		++status_.teleportSeq;
		++status_.teleports;
		status_.target = McFromWorld(aFrame.pose->feet);
		lastTeleportAt_ = aFrame.now;
		teleportDue_ = false;
		drive_.Enter(aFrame.pose->feet, aFrame.now);
		lastEye_.reset();
		if (there && !city)
		{
			SendFloor(status_.target, true, aSink);
		}
		aSink.Log("build mode: Minecraft's player goes to V's feet, and V waits there until Minecraft lets it go");
	}
	else if (!build && wasBuild_)
	{
		drive_.Leave();
		teleportDue_ = true;  // Minecraft's player follows V again, from where it is
		aSink.Log("build mode ended: Minecraft's player follows V again");
	}
	wasBuild_ = build;
	if (build)
	{
		const DriveState  before = drive_.State();
		const double      since = drive_.Since();
		const bool        acked = mc && mc->teleportAck == status_.teleportSeq;
		const Drive::Step driveStep = drive_.Update(aFrame.now, acked, sample, aFrame.pose->feet, eyeHeight_.Value());
		status_.placeV = driveStep.place;
		status_.endBuild = driveStep.end;
		if (drive_.State() == DriveState::Driving && before == DriveState::Waiting)
		{
			status_.lastHold = aFrame.now - since;
			aSink.Log(std::format("Minecraft let its player go after {:.2f} s: V follows it", aFrame.now - since));
		}
		else if (drive_.State() == DriveState::Waiting && before == DriveState::Driving)
		{
			aSink.Log("Minecraft holds its player again: V waits where it is");
		}
		if (drive_.State() == DriveState::Driving)
		{
			if (sample && sample->waited)
			{
				++status_.waitedFrames;
			}
			if (lastEye_)
			{
				status_.eyeDistance = Distance(aFrame.pose->camera, *lastEye_);
			}
		}
		lastEye_ = drive_.State() == DriveState::Driving && driveStep.place ? drive_.Eye() : std::nullopt;
		// The floor stays at its height while Minecraft drives, and grows around its player, never with a new epoch,
		// whose clear would take the ground from under it. Until Minecraft has its player at V, it's around V.
		const bool  driving = drive_.State() == DriveState::Driving && mc;
		const Vec3d centre = driving ? Vec3d{ mc->x, mc->y, mc->z } : McFromWorld(aFrame.pose->feet);
		if (there && !city && (!status_.floorSent || floorRequested_))
		{
			SendFloor(McFromWorld(aFrame.pose->feet), true, aSink);
			floorRequested_ = false;
		}
		else if (there && !city && std::hypot(centre.x - status_.floorCentre.x, centre.z - status_.floorCentre.z) > 16.0)
		{
			SendFloor({ centre.x, status_.floorCentre.y, centre.z }, false, aSink);
		}
	}
	else if (inGame)
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
		if (there && !city &&
		    (!status_.floorSent || floorRequested_ || std::hypot(feet.x - status_.floorCentre.x, feet.z - status_.floorCentre.z) > 16.0 ||
		     std::abs(feet.y - status_.floorCentre.y) > 1.0))
		{
			SendFloor(feet, true, aSink);
			floorRequested_ = false;
		}
	}
	status_.drive = drive_.State();
	status_.driveSince = drive_.Since();
	status_.driveSkipped = drive_.Skipped();
	status_.eyeHeight = eyeHeight_.Value();

	// The city: its area goes around Minecraft's player while it drives, and V otherwise. Its messages go into the
	// collision ring only while Minecraft is there to read them.
	centred_ = false;
	if (city)
	{
		const bool                 driving = drive_.State() == DriveState::Driving && mc;
		const std::optional<Vec3d> centre = driving       ? std::optional<Vec3d>(Vec3d{ mc->x, mc->y, mc->z })
		                                    : aFrame.pose ? std::optional<Vec3d>(McFromWorld(aFrame.pose->feet))
		                                                  : std::nullopt;
		if (const std::optional<RegionPos> region = centre ? RegionAt(*centre) : std::nullopt)
		{
			areas_.Centre(*region);
			centred_ = aFrame.linked;
		}
		if (there)
		{
			WriteCity(aSink);
		}
	}
	status_.cityCentre = areas_.CentreRegion();
	status_.cityKnown = areas_.Known();
	status_.cityWaiting = areas_.Waiting();
	status_.workerJobs = worker_ ? worker_->Waiting() : 0;
	status_.workerThreads = worker_ ? worker_->Threads() : 0;
	status_.workerRegions = worker_ ? worker_->Regions() : 0;
	status_.workerTriangles = worker_ ? worker_->Triangles() : 0;
	status_.workerBlocks = worker_ ? worker_->Blocks() : 0;
	status_.workerReplaced = worker_ ? worker_->Replaced() : 0;
	status_.workerMs = worker_ ? worker_->Ms() : 0.0;
	status_.workerCycles = worker_ ? worker_->Cycles() : 0;
	status_.workerTicks = worker_ ? worker_->Ticks() : 0;
	status_.collisionWaiting = outbox_.size();

	// The position: the last teleport's target, or in build mode V's feet, every frame, so that a teleport Minecraft
	// makes by itself (a new player object, or the in-game flag coming back) puts its player where V is.
	const Vec3d     position = build ? McFromWorld(aFrame.pose->feet) : status_.target;
	proto::SkyState state{};
	state.flags = (inGame ? uint32_t(proto::kSkyInGame) : 0u) | (aFrame.buildMode ? 0u : uint32_t(proto::kSkyMenuOpen));
	state.worldId = 1;
	state.collisionEpoch = status_.collisionEpoch;
	state.posX = position.x;
	state.posY = position.y;
	state.posZ = position.z;
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
		else if (aEvent.type == proto::kEvHitActor)
		{
			++status_.hitEvents;
			aSink.HitActor(aEvent);
		}
		else if (aEvent.type == proto::kEvPlayerDied)
		{
			++status_.deathEvents;
			aSink.PlayerDied(aEvent);
		}
		else
		{
			++status_.otherEvents;  // arrows stuck in NPCs, Skyrim's skills
		}
	});

	const auto drainStart = std::chrono::steady_clock::now();
	const auto drained = DrainByteRing(base_ + proto::kOffRenderRing, proto::kRenRingDataBytes, aSettings.ringBudget,
	                                   [&](const RingMessage& aMessage) { return Take(aMessage, aSettings, aSink); });
	status_.drainMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - drainStart).count();
	status_.drainedBytes += drained.bytes;
	status_.solidSections = solids_.Sections();
	status_.solidBlocks = solids_.Blocks();
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

	// Spike 7: the world-entity table, read while Minecraft is in its world, where it writes the table each frame.
	// Otherwise nothing of it is kept, so nothing stale is drawn.
	if (status_.inWorld)
	{
		if (const auto table = ReadWorldEntities(base_))
		{
			world_ = CheckWorldTable(*table);
		}
		else
		{
			++status_.worldTorn;  // mid-write every time: the last good table stays
		}
	}
	else
	{
		world_ = {};
	}
	status_.worldEntities = world_.entities.size();
	status_.worldLeftOut = world_.leftOut;
	status_.outline = world_.outline.has_value();
	aSink.WorldEntities(world_);

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
			status_.atlasWidth = atlas->width;
			status_.atlasHeight = atlas->height;
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
		solids_.Clear();
		aSink.ClearAll();
		return true;
	case proto::kRenSolids:
		if (const auto solids = ParseSolids(aMessage.payload, error))
		{
			solids_.Set(*solids);
		}
		else
		{
			reject();
		}
		return true;
	case proto::kRenTexture:
	case proto::kRenScene:
		if (!status_.atlasSeen)
		{
			++status_.entitiesBeforeAtlas;  // from before Minecraft's clear: SkyCraft gives its ids again after it
			return false;
		}
		if (aMessage.type == proto::kRenTexture)
		{
			if (const auto texture = ParseTexture(aMessage.payload, error))
			{
				aSink.Texture(*texture);
			}
			else
			{
				reject();
			}
		}
		else if (const auto scene = ParseScene(aMessage.payload, error))
		{
			aSink.Scene(*scene);
		}
		else
		{
			reject();
		}
		return true;
	case proto::kRenAtlasRegion:
		if (!status_.atlasSeen)
		{
			++status_.patchesBeforeAtlas;  // a frame for an atlas we never saw, or the one Minecraft's clear dropped
			return false;
		}
		if (const auto patch = ParseAtlasRegion(aMessage.payload, status_.atlasWidth, status_.atlasHeight, error))
		{
			aSink.AtlasRegion(*patch);
		}
		else
		{
			reject();
		}
		return true;
	default:
		return false;  // lights, digging, avatar, ragdoll: stepped over
	}
}

void Link::SendFloor(const Vec3d& aCentre, bool aNewEpoch, LinkSink& aSink)
{
	// A new epoch's clear message drops the last floor, and anything half-sent. Without one, the regions sent replace
	// those already there, and the rest stay.
	const uint32_t epoch = aNewEpoch ? status_.collisionEpoch + 1 : status_.collisionEpoch;
	uint8_t* const ring = base_ + proto::kOffCollisionRing;
	bool           sent = !aNewEpoch || WriteByteRing(ring, proto::kColRingDataBytes, proto::kColClear, FloorClear(epoch));
	const FloorVoxeler voxeler(aCentre.y);
	for (const auto& [x, z] : FloorRegions(aCentre.x, aCentre.z))
	{
		// Each region's triangles first, then its voxels, as SkyCraft sends them.
		sent = sent && WriteByteRing(ring, proto::kColRingDataBytes, proto::kColTris, FloorRegion(x, z, aCentre.y, epoch));
		for (const std::vector<uint8_t>& voxels : voxeler.Column(x, z, epoch))
		{
			sent = sent && WriteByteRing(ring, proto::kColRingDataBytes, proto::kColRegion, voxels);
		}
	}
	status_.collisionEpoch = epoch;
	status_.floorSent = sent;
	if (sent)
	{
		status_.floorCentre = aCentre;
		aSink.Log(aNewEpoch ? std::format("floor {} at height {:.2f} around Minecraft {:.1f} {:.1f}", epoch, aCentre.y, aCentre.x, aCentre.z)
		                    : std::format("floor {} grows to Minecraft {:.1f} {:.1f}, at height {:.2f}", epoch, aCentre.x, aCentre.z, aCentre.y));
	}
	else
	{
		aSink.Log("the floor didn't fit in the collision ring; trying again next frame");
	}
}

void Link::ReadGround(double aNow, GroundSource& aSource, double aBudgetMs)
{
	status_.cityReadMs = 0.0;
	if (status_.ground != Ground::City || !status_.minecraft || !centred_ || cityClear_)
	{
		return;  // nobody would read it, or there's no centre, or the regions would be of the epoch the clear ends
	}
	if (!worker_)
	{
		worker_ = std::make_unique<GroundWorker>(groundThreads_);
	}
	if (focus_ != areas_.CentreRegion())
	{
		// The worker starts the regions nearest the city's centre first: Minecraft's player while it drives, V otherwise.
		focus_ = areas_.CentreRegion();
		worker_->Focus(*focus_);
	}
	const auto              start = std::chrono::steady_clock::now();
	std::vector<McTriangle> triangles;
	while (const std::optional<BatchPos> batch = areas_.Next(aNow))
	{
		triangles.clear();
		if (!aSource.Read(BatchBox(*batch), triangles))
		{
			++status_.cityUnread;
			break;  // the batch waits for the next frame
		}
		std::array<std::vector<McTriangle>, 8> filed = FileBatch(*batch, triangles);
		const std::array<RegionPos, 8>         regions = RegionsOf(*batch);
		std::vector<GroundWorker::Job>         jobs;
		for (const size_t i : areas_.Read(*batch, aNow, filed))
		{
			++status_.cityRegions;
			status_.cityTriangles += filed[i].size();
			jobs.push_back({ status_.collisionEpoch, regions[i], std::move(filed[i]) });
		}
		worker_->Add(std::move(jobs));
		++status_.cityBatches;
		if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= aBudgetMs)
		{
			break;
		}
	}
	status_.cityReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	status_.cityKnown = areas_.Known();
	status_.cityWaiting = areas_.Waiting();
	status_.workerJobs = worker_->Waiting();
}

void Link::WriteCity(LinkSink& aSink)
{
	if (cityClear_)
	{
		// Anything still waiting is of the last epoch, which the clear ends.
		outbox_.clear();
		++status_.collisionEpoch;
		outbox_.push_back({ status_.collisionEpoch, proto::kColClear, FloorClear(status_.collisionEpoch) });
		cityClear_ = false;
		aSink.Log(std::format("the city starts over, as collision epoch {}", status_.collisionEpoch));
	}
	if (worker_)
	{
		for (GroundWorker::Message& message : worker_->Take())
		{
			if (message.epoch == status_.collisionEpoch)
			{
				outbox_.push_back(std::move(message));
			}
		}
	}
	uint8_t* const ring = base_ + proto::kOffCollisionRing;
	while (!outbox_.empty())
	{
		const GroundWorker::Message& next = outbox_.front();
		if (next.payload.size() + 8 > proto::kColRingDataBytes / 2)
		{
			++status_.collisionDropped;  // it would never fit
		}
		else if (!WriteByteRing(ring, proto::kColRingDataBytes, next.type, next.payload))
		{
			break;  // no room until Minecraft reads: the rest wait for the next frame, in order
		}
		else
		{
			status_.collisionBytes += next.payload.size();
		}
		outbox_.pop_front();
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

void Link::WriteActors(std::span<const proto::ActorRecord> aRecords)
{
	WriteActorTable(base_, aRecords);
	status_.actors = std::min<size_t>(aRecords.size(), proto::kMaxActors);
	actorsWritten_ = true;
}

void Link::ClearActors()
{
	if (actorsWritten_)
	{
		WriteActorTable(base_, {});
		status_.actors = 0;
		actorsWritten_ = false;
	}
}
}
