#pragma once

#include "link/Combat.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace cpmc
{
// What the module's damage-pipeline wraps report through BlockRender's natives (spike 6's "Part 2"), on whichever thread
// the game runs its damage pipeline on: hits on V, taken only while the main thread says so, so that V's health stays
// V's otherwise; our hits on NPCs as they land, with what the pipeline made of them and the NPC's health before and
// after; and those the pipeline dealt nothing, with why. The main thread takes them each frame.
class CombatInbox
{
public:
	static constexpr size_t kMaxWaiting = 256;  // of each: a hurt past it is left to V, a landing or an ending is only counted

	// One of our hits, as DealDamages dealt it.
	struct Landed
	{
		uint64_t target = 0;
		double   amount = 0.0;  // what the pipeline made of it, before the NPC's health took it
		double   before = 0.0;  // the NPC's health
		double   after = 0.0;
		uint32_t flags = 0;     // link::LandedFlag
	};

	// One of our hits that the pipeline dealt nothing.
	struct Ended
	{
		uint64_t target = 0;
		int32_t  why = 0;  // link::HitEndedName
	};

	struct Counts
	{
		uint64_t taken = 0;      // hits on V taken from the game
		uint64_t refused = 0;    // offered while not taking, or with kMaxWaiting waiting: left to V
		uint64_t nonLethal = 0;  // offered while taking, but the game won't let them kill V: left to V
		uint64_t landed = 0;     // our hits reported as dealt
		uint64_t ended = 0;      // our hits reported as dealt nothing
		uint64_t dropped = 0;    // landings and endings past kMaxWaiting
	};

	void SetTaking(bool aTaking);  // main thread: with combat and build mode on
	bool Taking() const;
	// Any thread: a hit on V, taken (true), so the wrap deals none of it, or left to V.
	bool Offer(const link::GameHurt& aHurt);
	void Land(const Landed& aLanded);  // any thread
	void End(const Ended& aEnded);     // any thread

	std::vector<link::GameHurt> TakeHurts();  // main thread, oldest first
	std::vector<Landed>         TakeLanded();
	std::vector<Ended>          TakeEnded();
	Counts                      Count() const;

private:
	std::atomic<bool>           taking_{ false };
	mutable std::mutex          mutex_;
	std::vector<link::GameHurt> hurts_;
	std::vector<Landed>         landed_;
	std::vector<Ended>          ended_;
	Counts                      counts_;
};
}
