#include "Harness.h"

#include "physx/ShapeTriangles.h"

using namespace physx;
using cpmc::px::CellRange;
using cpmc::px::HeightfieldView;
using cpmc::px::Tri;

namespace
{
const PxBounds3 kEverywhere(PxVec3(-1e6f), PxVec3(1e6f));

struct Grid
{
	uint32_t                          rows;
	uint32_t                          columns;
	std::vector<PxHeightFieldSample>  samples;

	Grid(uint32_t aRows, uint32_t aColumns)
		: rows(aRows)
		, columns(aColumns)
		, samples(size_t(aRows) * aColumns)
	{
		for (auto& sample : samples)
		{
			sample.height = 0;
			sample.materialIndex0 = PxBitAndByte(0);
			sample.materialIndex1 = PxBitAndByte(0);
		}
	}

	PxHeightFieldSample& At(uint32_t aRow, uint32_t aColumn) { return samples[size_t(aRow) * columns + aColumn]; }

	HeightfieldView View(float aHeightScale = 1.0f, float aRowScale = 1.0f, float aColumnScale = 1.0f)
	{
		HeightfieldView view;
		view.rows = rows;
		view.columns = columns;
		view.heightScale = aHeightScale;
		view.rowScale = aRowScale;
		view.columnScale = aColumnScale;
		view.sample = [this](uint32_t aRow, uint32_t aColumn) { return At(aRow, aColumn); };
		return view;
	}
};

bool Near(const PxVec3& aA, const PxVec3& aB)
{
	return (aA - aB).magnitude() <= 1e-4f;
}

std::vector<Tri> AllTriangles(Grid& aGrid, const HeightfieldView& aView)
{
	std::vector<Tri> tris;
	cpmc::px::HeightfieldTriangles(aView, CellRange{ 0, aGrid.rows - 1, 0, aGrid.columns - 1 }, PxTransform(PxIdentity),
	                               kEverywhere, tris);
	return tris;
}
}

TEST(A3x3GridHas8Triangles)
{
	Grid grid(3, 3);
	CHECK_EQ(AllTriangles(grid, grid.View()).size(), size_t(8));
}

TEST(HeightfieldScalesPlaceSamples)
{
	// Rows along local X, height along local Y, columns along local Z.
	Grid grid(2, 2);
	grid.At(1, 1).height = 4;
	const auto tris = AllTriangles(grid, grid.View(0.5f, 2.0f, 3.0f));
	CHECK_EQ(tris.size(), size_t(2));
	// With the tess flag clear, the second triangle starts at (r+1, c+1).
	CHECK(Near(tris[1].v[0], PxVec3(2.0f, 2.0f, 3.0f)));
}

TEST(TessFlagPicksTheDiagonalLikePhysX)
{
	Grid grid(2, 2);
	const PxVec3 v00(0, 0, 0), v01(0, 0, 1), v10(1, 0, 0), v11(1, 0, 1);

	auto clear = AllTriangles(grid, grid.View());
	CHECK(Near(clear[0].v[0], v00) && Near(clear[0].v[1], v01) && Near(clear[0].v[2], v10));
	CHECK(Near(clear[1].v[0], v11) && Near(clear[1].v[1], v10) && Near(clear[1].v[2], v01));

	grid.At(0, 0).setTessFlag();
	auto set = AllTriangles(grid, grid.View());
	CHECK(Near(set[0].v[0], v10) && Near(set[0].v[1], v00) && Near(set[0].v[2], v11));
	CHECK(Near(set[1].v[0], v01) && Near(set[1].v[1], v11) && Near(set[1].v[2], v00));
}

TEST(HeightfieldTrianglesFaceUp)
{
	Grid grid(2, 2);
	for (bool tess : { false, true })
	{
		if (tess)
		{
			grid.At(0, 0).setTessFlag();
		}
		for (const Tri& t : AllTriangles(grid, grid.View()))
		{
			CHECK((t.v[1] - t.v[0]).cross(t.v[2] - t.v[0]).y > 0.0f);
		}
	}
}

TEST(HoleMaterialRemovesOneTriangle)
{
	const PxU8 hole = PxU8(PxHeightFieldMaterial::eHOLE);
	Grid       grid(3, 3);
	grid.At(0, 0).materialIndex0 = PxBitAndByte(hole);
	CHECK_EQ(AllTriangles(grid, grid.View()).size(), size_t(7));

	Grid second(2, 2);
	second.At(0, 0).materialIndex1 = PxBitAndByte(hole);
	const auto tris = AllTriangles(second, second.View());
	CHECK_EQ(tris.size(), size_t(1));
	CHECK(Near(tris[0].v[0], PxVec3(0, 0, 0)));  // the first triangle survives

	// The tess bit shares materialIndex0's byte but isn't part of the material.
	Grid tessOnly(2, 2);
	tessOnly.At(0, 0).setTessFlag();
	CHECK_EQ(AllTriangles(tessOnly, tessOnly.View()).size(), size_t(2));
}

TEST(OnlyCellsUnderTheRegionAreVisited)
{
	Grid            grid(5, 5);  // 4 x 4 cells
	HeightfieldView view = grid.View();
	const PxBounds3 box(PxVec3(1.5f, -10.0f, 0.2f), PxVec3(2.5f, 10.0f, 0.8f));
	const CellRange cells = cpmc::px::HeightfieldCellsUnder(view, PxTransform(PxIdentity), box);
	CHECK_EQ(cells.r0, uint32_t(1));
	CHECK_EQ(cells.r1, uint32_t(3));
	CHECK_EQ(cells.c0, uint32_t(0));
	CHECK_EQ(cells.c1, uint32_t(1));
	std::vector<Tri> tris;
	cpmc::px::HeightfieldTriangles(view, cells, PxTransform(PxIdentity), kEverywhere, tris);
	CHECK_EQ(tris.size(), size_t(4));

	// A box off the grid gives no cells.
	const PxBounds3 off(PxVec3(10.0f, -1.0f, 10.0f), PxVec3(11.0f, 1.0f, 11.0f));
	CHECK(cpmc::px::HeightfieldCellsUnder(view, PxTransform(PxIdentity), off).Empty());
}
