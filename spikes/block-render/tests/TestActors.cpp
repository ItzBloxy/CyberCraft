#include "Harness.h"

#include "link/Actors.h"

#include <cstring>
#include <limits>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// An NPC on the road at spike 3's corner, facing north (the world's +y), as the module would list it.
NpcSample Npc(uint64_t aEntity, double aEast = 0.0, double aNorth = 0.0)
{
	NpcSample npc;
	npc.entity = aEntity;
	npc.position = { -119.5 + aEast, -2007.5 + aNorth, 6.14 };
	npc.forward = { 0.0f, 1.0f, 0.0f };
	npc.health = 50.0f;
	npc.healthMax = 200.0f;
	npc.level = 12.6f;
	npc.name = "Tyger Claws Ganger";
	npc.flags = proto::kActorHostile | proto::kActorInCombat;
	return npc;
}

const Vec3d kV{ -119.5, -2007.5, 6.14 };  // V, where the NPCs are measured from
}

TEST(ActorsNumberNpcsFromOneAndNeverTheSameNumberTwice)
{
	ActorIds ids;
	CHECK_EQ(ids.Id(0xA, 0.0), 1u);
	CHECK_EQ(ids.Id(0xB, 0.0), 2u);
	CHECK_EQ(ids.Id(0xA, 30.0), 1u);  // kept while it's listed
	CHECK_EQ(ids.Known(), size_t(2));
	ids.Forget(61.0);  // B, unlisted since 0 s, goes; A, listed at 30 s, stays
	CHECK_EQ(ids.Known(), size_t(1));
	CHECK_EQ(ids.Id(0xB, 61.0), 3u);  // a new number: 2 is never given again
	CHECK_EQ(ids.Given(), 3u);
	CHECK_EQ(ids.Id(0xA, 61.0), 1u);
}

TEST(ActorsMapAHitBackToItsNpcForTwoSeconds)
{
	ActorIds ids;
	CHECK_EQ(ids.Id(0xA, 10.0), 1u);
	CHECK(ids.Entity(1, 11.9) == std::optional<uint64_t>(0xA));  // a hit can trail its stand-in's removal
	CHECK(!ids.Entity(1, 12.1).has_value());                     // but not by long
	CHECK(!ids.Entity(2, 10.0).has_value());                     // never given
	ids.Forget(70.1);
	CHECK(!ids.Entity(1, 10.5).has_value());  // forgotten
}

TEST(ActorsCutNamesAtTheStartOfACharacter)
{
	CHECK_STR(CutName("Tyger Claws Ganger", 23), "Tyger Claws Ganger");
	CHECK_STR(CutName("Maelstrom Cyberpsycho Lieutenant", 23), "Maelstrom Cyberpsycho L");
	// "é" is two bytes, C3 A9: one that would end on its first byte is left out whole.
	CHECK_STR(CutName("Ripperdoc Viktor Vektor\xC3\xA9", 24), "Ripperdoc Viktor Vektor");
	CHECK_STR(CutName("\xE2\x82\xAC\xE2\x82\xAC", 4), "\xE2\x82\xAC");  // two euro signs, three bytes each
	CHECK_STR(CutName("", 23), "");
}

TEST(ActorsMakeARecordFromAnNpc)
{
	const proto::ActorRecord record = MakeActorRecord(Npc(0xA), 7, kPersonBox);
	CHECK_EQ(record.formId, 7u);
	CHECK_EQ(record.flags, uint32_t(proto::kActorHostile | proto::kActorInCombat));
	CHECK_NEAR(record.x, -119.5, 1e-4);  // Minecraft's axes: x east, y up, z south
	CHECK_NEAR(record.y, 6.14, 1e-4);
	CHECK_NEAR(record.z, 2007.5, 1e-3);
	CHECK_NEAR(record.yaw, 180.0, 1e-3);  // facing north
	CHECK_NEAR(record.width, 0.6, 1e-6);
	CHECK_NEAR(record.height, 1.8, 1e-6);
	CHECK_NEAR(record.healthFrac, 0.25, 1e-6);
	CHECK_EQ(record.level, 13);
	CHECK_STR(record.name, "Tyger Claws Ganger");
	CHECK_EQ(record.name[23], '\0');
}

TEST(ActorsTakeAnNpcsBoxFromItsCapsule)
{
	// A controller's capsule doesn't reach the feet: the box goes from the position up to the capsule's top.
	const proto::ActorRecord person = MakeActorRecord(Npc(0xA), 1, NpcBox{ 0.64, 0.0, 1.72 });
	CHECK_NEAR(person.y, 6.14, 1e-4);
	CHECK_NEAR(person.width, 0.64, 1e-6);
	CHECK_NEAR(person.height, 1.72, 1e-6);
	// A drone's capsule can reach below its position: the box starts there.
	const proto::ActorRecord drone = MakeActorRecord(Npc(0xB), 2, NpcBox{ 1.0, 0.3, 0.4 });
	CHECK_NEAR(drone.y, 5.84, 1e-4);
	CHECK_NEAR(drone.height, 0.7, 1e-6);
}

TEST(ActorsHoldOddNumbersToTheRecordsRange)
{
	NpcSample npc = Npc(0xA);
	npc.healthMax = 0.0f;  // no health pool
	npc.level = 70000.0f;
	npc.flags = proto::kActorDead | proto::kActorEssential;  // the dead aren't listed, and never flagged so
	npc.forward = { 0.0f, 0.0f, 0.0f };
	proto::ActorRecord record = MakeActorRecord(npc, 1, kPersonBox);
	CHECK_EQ(record.healthFrac, 0.0f);
	CHECK_EQ(record.level, 65535);
	CHECK_EQ(record.flags, uint32_t(proto::kActorEssential));
	CHECK_EQ(record.yaw, 0.0f);
	npc.health = 300.0f;
	npc.healthMax = 200.0f;
	npc.level = std::numeric_limits<float>::quiet_NaN();
	record = MakeActorRecord(npc, 1, kPersonBox);
	CHECK_EQ(record.healthFrac, 1.0f);
	CHECK_EQ(record.level, 0);
	npc.health = std::numeric_limits<float>::infinity();
	npc.level = -4.0f;
	record = MakeActorRecord(npc, 1, kPersonBox);
	CHECK_EQ(record.healthFrac, 0.0f);
	CHECK_EQ(record.level, 0);
}

TEST(ActorsKeepTheNearest256Within80Metres)
{
	std::vector<NpcSample> npcs;
	for (uint64_t i = 1; i <= 300; ++i)
	{
		npcs.push_back(Npc(i, 0.25 * double(301 - i)));  // 300 NPCs 0.25 to 75 m east: the last listed is the nearest
	}
	npcs.push_back(Npc(1000, 80.0));   // at 80 m: in
	npcs.push_back(Npc(1001, 0.0, 80.01));  // past 80 m: out
	ActorIds                         ids;
	std::unordered_map<uint64_t, NpcBox> boxes{ { 300, NpcBox{ 0.5, 0.0, 1.5 } } };
	const std::vector<proto::ActorRecord> records = ActorRecords(npcs, kV, boxes, ids, 5.0);
	CHECK_EQ(records.size(), size_t(256));
	CHECK_EQ(records.front().formId, 1u);  // the nearest is numbered first
	CHECK_NEAR(records.front().x, -119.25, 1e-4);
	CHECK_NEAR(records.front().width, 0.5, 1e-6);  // its box, by entity
	CHECK_NEAR(records.back().x, -119.5 + 0.25 * 256, 1e-3);
	CHECK_NEAR(records[1].width, 0.6, 1e-6);  // the rest person-sized
	CHECK_EQ(ids.Given(), 256u);  // only those in the table are numbered
	npcs.resize(2);
	npcs.push_back(Npc(1000, 80.0));
	npcs.push_back(Npc(1001, 0.0, 80.01));
	const std::vector<proto::ActorRecord> few = ActorRecords(npcs, kV, {}, ids, 6.0);
	CHECK_EQ(few.size(), size_t(3));
	CHECK_NEAR(few.back().x, -39.5, 1e-4);  // the NPC at exactly 80 m
	CHECK_EQ(ids.Given(), 259u);            // three NPCs the first table had no room for
}
