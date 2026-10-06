#include "Harness.h"

#include "link/WorldTable.h"

#include <algorithm>
#include <limits>

using namespace cpmc::link;

namespace
{
// A record as WorldExporter.exportEntities makes one: a kind, an id, a position near spike 5's spot B.
proto::WorldEntity Record(uint32_t aKind, uint32_t aId)
{
	proto::WorldEntity record{};
	record.kind = aKind;
	record.id = aId;
	record.x = 480.5f;
	record.y = 6.14f;
	record.z = 2007.5f;
	record.scale = 0.5f;
	record.uv[0][2] = 0.25f;
	return record;
}

proto::WorldEntities Table(std::initializer_list<proto::WorldEntity> aRecords)
{
	proto::WorldEntities table{};
	for (const proto::WorldEntity& record : aRecords)
	{
		table.entities[table.count++] = record;
	}
	return table;
}
}

TEST(WorldTableKeepsGoodRecordsInOrder)
{
	proto::WorldEntities table = Table({ Record(proto::kWeItem, 7), Record(proto::kWeArrow, 9), Record(proto::kWeShadow, 3) });
	table.hasSelection = 1;
	const float box[6] = { 480.0f, 6.0f, 2007.0f, 481.0f, 7.0f, 2008.0f };
	std::copy(box, box + 3, table.selMin);
	std::copy(box + 3, box + 6, table.selMax);
	const WorldTable checked = CheckWorldTable(table);
	CHECK_EQ(checked.entities.size(), size_t(3));
	CHECK_EQ(checked.leftOut, 0u);
	CHECK(checked.outline.has_value());
	if (checked.entities.size() != 3 || !checked.outline)
	{
		return;
	}
	CHECK_EQ(checked.entities[0].id, 7u);
	CHECK_EQ(checked.entities[1].kind, uint32_t(proto::kWeArrow));
	CHECK_EQ(checked.entities[2].id, 3u);
	CHECK_EQ((*checked.outline)[0], 480.0f);
	CHECK_EQ((*checked.outline)[5], 2008.0f);
}

TEST(WorldTableLeavesOutBadRecords)
{
	proto::WorldEntity unknown = Record(0, 1);
	proto::WorldEntity tooHigh = Record(7, 2);
	proto::WorldEntity lost = Record(proto::kWeItem, 3);
	lost.x = std::numeric_limits<float>::quiet_NaN();
	proto::WorldEntity badUv = Record(proto::kWeBlock, 4);
	badUv.uv[2][3] = std::numeric_limits<float>::infinity();
	proto::WorldEntity badExtent = Record(proto::kWeCrack, 5);
	badExtent.ext[1] = std::numeric_limits<float>::quiet_NaN();
	const WorldTable checked = CheckWorldTable(Table({ unknown, Record(proto::kWeTrident, 6), tooHigh, lost, badUv, badExtent }));
	CHECK_EQ(checked.entities.size(), size_t(1));
	CHECK_EQ(checked.leftOut, 5u);
	if (checked.entities.size() == 1)
	{
		CHECK_EQ(checked.entities[0].id, 6u);
	}
}

TEST(WorldTableHoldsTheCountToItsRecords)
{
	proto::WorldEntities table{};
	for (uint32_t i = 0; i < proto::kMaxWorldEntities; ++i)
	{
		table.entities[i] = Record(proto::kWeShadow, i + 1);
	}
	table.count = 1000;  // more than the table holds: only its records are read
	const WorldTable checked = CheckWorldTable(table);
	CHECK_EQ(checked.entities.size(), size_t(proto::kMaxWorldEntities));
	CHECK_EQ(checked.leftOut, 0u);
}

TEST(WorldTableHasAnOutlineOnlyWhenOneIsWhole)
{
	proto::WorldEntities none = Table({});
	none.selMax[0] = 1.0f;  // a box, but hasSelection is 0
	CHECK(!CheckWorldTable(none).outline.has_value());
	proto::WorldEntities broken = Table({});
	broken.hasSelection = 1;
	broken.selMin[1] = std::numeric_limits<float>::quiet_NaN();
	CHECK(!CheckWorldTable(broken).outline.has_value());
}

TEST(WorldTableNamesTheKinds)
{
	CHECK_STR(WorldKindName(proto::kWeArrow), "arrow");
	CHECK_STR(WorldKindName(proto::kWeItem), "item");
	CHECK_STR(WorldKindName(proto::kWeTrident), "trident");
	CHECK_STR(WorldKindName(proto::kWeBlock), "block");
	CHECK_STR(WorldKindName(proto::kWeCrack), "crack");
	CHECK_STR(WorldKindName(proto::kWeShadow), "shadow");
	CHECK_STR(WorldKindName(0), "other");
	CHECK_STR(WorldKindName(7), "other");
}
