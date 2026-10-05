#include "Harness.h"

#include "physx/Characters.h"

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

using namespace cpmc;
using namespace cpmc::px;

namespace
{
// An NPC's feet on the road at spike 3's corner.
constexpr Vec3d kFeet{ -119.5, -2007.5, 6.14 };

// A capsule standing at aX, aY (world axes), its centre aUp metres above the road.
CapsuleSeen Standing(double aX, double aY, double aUp, double aRadius, double aHalfHeight)
{
	CapsuleSeen capsule;
	capsule.centre = { aX, aY, kFeet.z + aUp };
	capsule.radius = aRadius;
	capsule.halfHeight = aHalfHeight;
	return capsule;
}

// aCapsule leaning aDegrees from upright, its top towards the world's +x; with aThroughTheFeet, moved along x so its axis
// passes through the feet.
CapsuleSeen Leaning(CapsuleSeen aCapsule, double aDegrees, bool aThroughTheFeet)
{
	const double angle = aDegrees * std::numbers::pi / 180.0;
	aCapsule.axis = { std::sin(angle), 0.0, std::cos(angle) };
	if (aThroughTheFeet)
	{
		aCapsule.centre.x = kFeet.x + std::tan(angle) * (aCapsule.centre.z - kFeet.z);
	}
	return aCapsule;
}

// V's controller (spike 1): radius 0.32, half-height 0.4, centred 1 m above its feet.
CapsuleSeen Controller()
{
	return Standing(kFeet.x, kFeet.y, 1.0, 0.32, 0.4);
}
}

TEST(CharactersPickTheNpcsOwnCapsule)
{
	// V's pair: its controller, and a wide capsule, radius 1, around it. The narrower is the controller.
	const std::vector<CapsuleSeen>   pair{ Standing(kFeet.x, kFeet.y, 1.0, 1.0, 0.4), Controller() };
	const std::optional<CapsuleSeen> picked = PickCapsule(pair, kFeet);
	CHECK(picked.has_value());
	CHECK_NEAR(picked ? picked->radius : 0.0, 0.32, 1e-9);
	// Another NPC's, 0.25 m off across: not this one's. 0.15 m off: this one's.
	CHECK(!PickCapsule(std::vector{ Standing(kFeet.x + 0.25, kFeet.y, 1.0, 0.32, 0.4) }, kFeet).has_value());
	CHECK(PickCapsule(std::vector{ Standing(kFeet.x, kFeet.y + 0.15, 1.0, 0.32, 0.4) }, kFeet).has_value());
	// Leaning 20 degrees with its axis through the feet: kept. Leaning 30: lying too far over for a controller.
	CHECK(PickCapsule(std::vector{ Leaning(Controller(), 20.0, true) }, kFeet).has_value());
	CHECK(!PickCapsule(std::vector{ Leaning(Controller(), 30.0, true) }, kFeet).has_value());
	// Leaning 20 degrees, centred over the feet: its axis passes 0.34 m from them.
	CHECK(!PickCapsule(std::vector{ Leaning(Controller(), 20.0, false) }, kFeet).has_value());
	CHECK(!PickCapsule(std::vector<CapsuleSeen>{}, kFeet).has_value());
}

TEST(CharactersBoxReachesFromTheFeetOrTheCapsulesBottomToItsTop)
{
	// V's controller spans 0.28 to 1.72 m above the feet, so the box starts at the feet.
	const link::NpcBox v = BoxOf(Controller(), kFeet);
	CHECK_NEAR(v.width, 0.64, 1e-9);
	CHECK_NEAR(v.below, 0.0, 1e-9);
	CHECK_NEAR(v.above, 1.72, 1e-9);
	// A capsule reaching 0.12 m below the feet: the box starts at its bottom.
	const link::NpcBox low = BoxOf(Standing(kFeet.x, kFeet.y, 0.6, 0.32, 0.4), kFeet);
	CHECK_NEAR(low.below, 0.12, 1e-9);
	CHECK_NEAR(low.above, 1.32, 1e-9);
	// Leaning 20 degrees: its top is lower by the lean.
	const link::NpcBox leaning = BoxOf(Leaning(Controller(), 20.0, false), kFeet);
	CHECK_NEAR(leaning.above, 1.0 + 0.4 * std::cos(20.0 * std::numbers::pi / 180.0) + 0.32, 1e-9);
}

TEST(CharactersLookForNpcsAtMostEightAFrameAndAgainUntilFound)
{
	std::vector<uint64_t> listed;
	link::ActorIds        ids;
	for (uint64_t entity = 100; entity < 120; ++entity)
	{
		listed.push_back(entity);
		ids.Id(entity, 0.0);
	}
	NpcSizes                    sizes;
	const std::vector<uint64_t> first = sizes.ToLookFor(listed, 0.0);
	CHECK_EQ(first.size(), size_t(8));
	CHECK(!first.empty() && first.front() == uint64_t(100));  // the nearest first, as listed
	const std::vector<uint64_t> second = sizes.ToLookFor(listed, 0.0);
	CHECK(!second.empty() && second.front() == uint64_t(108));
	CHECK_EQ(sizes.ToLookFor(listed, 0.5).size(), size_t(4));
	CHECK(sizes.ToLookFor(listed, 1.0).empty());  // all looked for, and none due again yet
	sizes.Found(101, Controller(), kFeet);
	CHECK_EQ(sizes.FoundCount(), size_t(1));
	CHECK_EQ(sizes.LookedForCount(), size_t(20));
	CHECK_NEAR(sizes.Boxes().contains(101) ? sizes.Boxes().at(101).width : 0.0, 0.64, 1e-9);
	CHECK(sizes.Capsule(101) != nullptr);
	CHECK(sizes.Capsule(100) == nullptr);  // looked for, and none found: a person's size
	// 2 s after their last look, those without a capsule are looked for again, nearest first; 101's was found.
	const std::vector<uint64_t> again = sizes.ToLookFor(listed, 2.0);
	CHECK_EQ(again.size(), size_t(8));
	CHECK(again.size() >= 2 && again[0] == uint64_t(100) && again[1] == uint64_t(102));
	CHECK_EQ(sizes.ToLookFor(listed, 2.0).size(), size_t(7));  // the rest of those looked for at 0 s
	CHECK(sizes.ToLookFor(listed, 2.0).empty());               // those looked for at 0.5 s are due at 2.5 s
	CHECK_EQ(sizes.ToLookFor(listed, 2.5).size(), size_t(4));
	// A minute on, only 101 is still listed: the others are forgotten with their numbers, and looked for at once if
	// they're listed again.
	ids.Id(101, 59.0);
	ids.Forget(60.0);
	sizes.Forget(ids);
	CHECK_EQ(sizes.LookedForCount(), size_t(1));
	CHECK_EQ(sizes.FoundCount(), size_t(1));
	CHECK_EQ(sizes.ToLookFor(std::vector<uint64_t>{ 100, 101 }, 60.0).size(), size_t(1));
}

TEST(CharactersDescribeAnNpcForTheNpcsRequest)
{
	link::NpcSample npc;
	npc.entity = 7;
	npc.name = "Ganger One";
	npc.health = 80.0f;
	npc.healthMax = 100.0f;
	npc.level = 12.0f;
	npc.flags = link::proto::kActorHostile | link::proto::kActorInCombat;
	NpcSizes sizes;
	CHECK_STR(DescribeNpc(npc, 3, 4.3, sizes), "3 Ganger One, 4.3 m: 0.60 wide, from 0.00 below its position to 1.80 above, a person's "
	                                            "until its capsule is looked for; health 80 of 100, level 12; hostile, in combat");
	sizes.ToLookFor(std::vector<uint64_t>{ 7 }, 0.0);
	CHECK(DescribeNpc(npc, 3, 4.3, sizes).find(" 1.80 above, a person's: no capsule found; ") != std::string::npos);
	CapsuleSeen capsule = Controller();
	capsule.queryFilter = { 0, 0, 0x2000, 0 };
	sizes.Found(7, capsule, kFeet);
	CHECK_STR(DescribeNpc(npc, 3, 4.3, sizes), "3 Ganger One, 4.3 m: 0.64 wide, from 0.00 below its position to 1.72 above, its "
	                                            "capsule's: radius 0.32, half-height 0.40, leaning 0 degrees, query filter "
	                                            "0:0:2000:0; health 80 of 100, level 12; hostile, in combat");
	npc.name.clear();
	npc.flags = link::proto::kActorEssential;
	CHECK(DescribeNpc(npc, 3, 4.3, sizes).starts_with("3 (no name), 4.3 m: "));
	CHECK(DescribeNpc(npc, 3, 4.3, sizes).ends_with("; essential"));
	npc.flags = 0;
	CHECK(DescribeNpc(npc, 3, 4.3, sizes).ends_with("; no flags"));
}
