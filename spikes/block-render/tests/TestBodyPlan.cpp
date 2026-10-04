#include "Harness.h"

#include "physx/BodyPlan.h"

#include <cmath>
#include <format>
#include <initializer_list>
#include <limits>
#include <string>

using namespace cpmc;
using namespace cpmc::px;

namespace
{
// Sets aSection's solids: aBlocks solid blocks, from its first.
void SetSolids(link::Solids& aSolids, const SectionPos& aSection, uint32_t aBlocks = 1)
{
	link::SolidsMessage message{ aSection, aBlocks, {} };
	for (uint32_t bit = 0; bit < aBlocks; ++bit)
	{
		message.bits[bit >> 6] |= uint64_t(1) << (bit & 63);
	}
	aSolids.Set(message);
}

link::Solids With(std::initializer_list<SectionPos> aSections)
{
	link::Solids solids;
	for (const SectionPos& section : aSections)
	{
		SetSolids(solids, section);
	}
	return solids;
}

// "make 0 0 0, remove 1 0 0": the changes, in order.
std::string Describe(const std::vector<BodyPlan::Change>& aChanges)
{
	std::string out;
	for (const BodyPlan::Change& change : aChanges)
	{
		out += std::format("{}{} {} {} {}", out.empty() ? "" : ", ", change.make ? "make" : "remove", change.section.x, change.section.y,
		                   change.section.z);
	}
	return out;
}

// What Bodies does when every change works: the plan's next state.
std::string Apply(BodyPlan& aPlan, const SectionPos& aV, const link::Solids& aSolids, bool aOn = true)
{
	const std::vector<BodyPlan::Change> changes = aPlan.Changes(aV, aOn, aSolids);
	for (const BodyPlan::Change& change : changes)
	{
		change.make ? aPlan.Made(change.section, change.version) : aPlan.Removed(change.section);
	}
	return Describe(changes);
}

bool Near(const Vec3d& aA, const Vec3d& aB)
{
	return std::abs(aA.x - aB.x) < 1e-9 && std::abs(aA.y - aB.y) < 1e-9 && std::abs(aA.z - aB.z) < 1e-9;
}
}

TEST(BodyPlanMakesTheSectionsAroundV)
{
	// Solids in five sections, V in section 0 0 0: bodies for the three within one section of it, not for those two away.
	const link::Solids solids = With({ { 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 0, -1, 1 }, { 0, 2, 0 } });
	BodyPlan           plan;
	const auto         changes = plan.Changes({ 0, 0, 0 }, true, solids);
	CHECK_STR(Describe(changes), "make 0 -1 1, make 0 0 0, make 1 0 0");
	CHECK(changes.size() == 3 && changes[1].version == solids.Find({ 0, 0, 0 })->version);
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "make 0 -1 1, make 0 0 0, make 1 0 0");  // nothing done yet
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 -1 1, make 0 0 0, make 1 0 0");
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "");  // all made
}

TEST(BodyPlanKeepsBodiesUntilTheyAreTwoSectionsAway)
{
	// Walking along a boundary doesn't churn bodies: one is made within a section of V's, and removed only past two.
	const link::Solids solids = With({ { 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 0, -1, 1 } });
	BodyPlan           plan;
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 -1 1, make 0 0 0, make 1 0 0");
	CHECK_STR(Apply(plan, { 1, 0, 0 }, solids), "make 2 0 0");
	CHECK_STR(Apply(plan, { 2, 0, 0 }, solids), "");  // 0 0 0 is two away: kept
	CHECK_STR(Apply(plan, { 3, 0, 0 }, solids), "remove 0 -1 1, remove 0 0 0");
	CHECK_STR(Apply(plan, { 2, 0, 0 }, solids), "");  // 0 0 0 is two away again: not made
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 -1 1, make 0 0 0");
}

TEST(BodyPlanRebuildsABodyWhenItsSolidsChange)
{
	link::Solids solids = With({ { 0, 0, 0 } });
	BodyPlan     plan;
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 0 0");
	SetSolids(solids, { 0, 0, 0 }, 2);  // a block placed
	const auto changes = plan.Changes({ 0, 0, 0 }, true, solids);
	CHECK_STR(Describe(changes), "remove 0 0 0, make 0 0 0");
	CHECK(changes.size() == 2 && changes[1].version == solids.Find({ 0, 0, 0 })->version);
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "remove 0 0 0, make 0 0 0");
	SetSolids(solids, { 0, 0, 0 }, 0);  // every block broken
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "remove 0 0 0");
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "");
}

TEST(BodyPlanRemovesEveryBodyWhileBodiesAreOff)
{
	// Bodies stand only while blocks and drawing are both on: off, every body goes, and none is made.
	const link::Solids solids = With({ { 0, 0, 0 }, { 1, 0, 0 } });
	BodyPlan           plan;
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 0 0, make 1 0 0");
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids, false), "remove 0 0 0, remove 1 0 0");
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids, false), "");
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 0 0, make 1 0 0");  // on again
}

TEST(BodyPlanTriesAFailedBodyAgainOnlyWhenItsSolidsChange)
{
	link::Solids solids = With({ { 0, 0, 0 } });
	BodyPlan     plan;
	const auto   changes = plan.Changes({ 0, 0, 0 }, true, solids);
	CHECK(changes.size() == 1);
	plan.Failed({ 0, 0, 0 }, changes.empty() ? 0 : changes[0].version);
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "");  // not every frame
	SetSolids(solids, { 0, 0, 0 }, 3);
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "remove 0 0 0, make 0 0 0");  // the removal finds no body: harmless
}

TEST(BodyPlanStartsOverWhenCleared)
{
	const link::Solids solids = With({ { 0, 0, 0 }, { 1, 1, 1 } });
	BodyPlan           plan;
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 0 0, make 1 1 1");
	plan.Removed({ 1, 1, 1 });  // forgotten: its actor was lost in a load, say
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "make 1 1 1");
	plan.Clear();
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "make 0 0 0, make 1 1 1");
}

TEST(BodyPlanPutsBoxesInTheWorld)
{
	const auto section = SectionAt({ -0.5, 15.99, 16.0 });
	CHECK((section.has_value() && *section == SectionPos{ -1, 0, 1 }));
	CHECK(!SectionAt({ std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0 }).has_value());
	CHECK(!SectionAt({ 2.0e6, 0.0, 0.0 }).has_value());
	// Section 1 0 -2 holds Minecraft blocks 16 to 31, 0 to 15 and -32 to -17: its centre, in the world, is 24 24 8.
	const Vec3d centre = SectionCentre({ 1, 0, -2 });
	CHECK(Near(centre, { 24.0, 24.0, 8.0 }));
	// Its block 15 0 0, Minecraft block 31 0 -32, is centred at world 31.5 31.5 0.5.
	const LocalBox block = LocalBoxOf({ 15, 0, 0, 16, 1, 1 });
	CHECK(Near({ centre.x + block.centre.x, centre.y + block.centre.y, centre.z + block.centre.z }, { 31.5, 31.5, 0.5 }));
	CHECK(Near(block.half, { 0.5, 0.5, 0.5 }));
	// A layer at height 3 of the section, from Minecraft's x 0 to 16 and z 0 to 4: wide along the world's x, 4 m along
	// its y, which is Minecraft's -z, and 1 m tall.
	const LocalBox layer = LocalBoxOf({ 0, 3, 0, 16, 4, 4 });
	CHECK(Near(layer.centre, { 0.0, 6.0, -4.5 }));
	CHECK(Near(layer.half, { 8.0, 2.0, 0.5 }));
}

TEST(BodyPlanTellsWhetherABodyMovedWithTheOrigin)
{
	// PhysX = world - origin: an actor at PhysX 10 20 30 stands at world 10 1044 30 with the origin 0 1024 0.
	CHECK(PosedAt({ 10.0, 20.0, 30.0 }, { 10.0, 1044.0, 30.0 }, { 0.0, 1024.0, 0.0 }));
	CHECK(PosedAt({ 10.0, 20.005, 30.0 }, { 10.0, 1044.0, 30.0 }, { 0.0, 1024.0, 0.0 }));
	CHECK(!PosedAt({ 10.0, 20.02, 30.0 }, { 10.0, 1044.0, 30.0 }, { 0.0, 1024.0, 0.0 }));
	CHECK(!PosedAt({ 10.0, 20.0, 30.0 }, { 10.0, 1044.0, 30.0 }, { 0.0, 0.0, 0.0 }));  // not moved when the origin was
}
