#include "Harness.h"

#include "render/PendingSections.h"

using namespace cpmc;

namespace
{
const SectionPos kNear{ -3, 4, 0 };
const SectionPos kFar{ 1, 0, 2 };

std::vector<Vertex> Mesh(size_t aVertices, float aX)
{
	std::vector<Vertex> vertices(aVertices);
	for (Vertex& vertex : vertices)
	{
		vertex.x = aX;
	}
	return vertices;
}
}

TEST(PendingSectionsKeepTheLatestMesh)
{
	PendingSections pending;
	pending.Put(kFar, Mesh(6, 1.0f));
	pending.Put(kFar, Mesh(12, 2.0f));  // Minecraft changed it again before the upload
	pending.Put(kNear, Mesh(3, 3.0f));
	CHECK_EQ(pending.Size(), size_t(2));
	const auto taken = pending.Take();
	CHECK_EQ(taken.size(), size_t(2));
	CHECK(taken[0].first == kNear);  // in section order
	CHECK(taken[1].first == kFar);
	CHECK_EQ(taken[1].second.size(), size_t(12));
	CHECK_EQ(taken[1].second[0].x, 2.0f);
	CHECK_EQ(pending.Size(), size_t(0));
	CHECK(pending.Take().empty());
}

TEST(PendingSectionsKeepARemoval)
{
	PendingSections pending;
	pending.Put(kFar, Mesh(6, 1.0f));
	pending.Put(kFar, {});  // then Minecraft removed it: the removal waits instead
	const auto taken = pending.Take();
	CHECK_EQ(taken.size(), size_t(1));
	CHECK(taken[0].second.empty());
}

TEST(PendingSectionsClear)
{
	PendingSections pending;
	pending.Put(kNear, Mesh(6, 1.0f));
	pending.Put(kFar, Mesh(6, 1.0f));
	pending.Clear();
	CHECK_EQ(pending.Size(), size_t(0));
	CHECK(pending.Take().empty());
}
