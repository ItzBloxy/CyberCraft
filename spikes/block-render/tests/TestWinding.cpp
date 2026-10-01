#include "Harness.h"

#include "AtlasFixture.h"
#include "render/Camera.h"
#include "render/Pipeline.h"
#include "world/Mesher.h"

// The mesher's faces, through the camera, wind counter-clockwise in clip space (x right, y up) when they face the
// camera, as Minecraft's do. Flipping y for the render target doesn't change how a triangle winds on screen, so
// Direct3D takes such a triangle as a front face only when the rasterizer says FrontCounterClockwise. Its default is
// the opposite: the first run in the game showed a cube seen from the inside.
namespace
{
double SignedArea(const std::array<double, 3>& aA, const std::array<double, 3>& aB, const std::array<double, 3>& aC)
{
	return (aB[0] - aA[0]) * (aC[1] - aA[1]) - (aC[0] - aA[0]) * (aB[1] - aA[1]);
}

void CheckWinding(const std::array<float, 16>& aViewToClip)
{
	// The camera at world (0.5, 0, 0.5), looking north (+y), +z up.
	cpmc::SlConstants constants;
	constants.viewToClip = aViewToClip;
	constants.position = { 0.5f, 0.0f, 0.5f };
	constants.forward = { 0.0f, 1.0f, 0.0f };
	constants.right = { 1.0f, 0.0f, 0.0f };
	constants.up = { 0.0f, 0.0f, 1.0f };
	const auto frame = cpmc::MakeCameraFrame(constants, {});
	CHECK(frame.has_value());
	if (!frame)
	{
		return;
	}
	// Minecraft block (0, 0, -10) fills world x 0..1, y 9..10, z 0..1: ahead of the camera.
	cpmc::BlockWorld world;
	const cpmc::Atlas atlas = fixture::Atlas();
	world.Set({ 0, 0, -10 }, *atlas.BlockId("stone"));
	const cpmc::SectionPos section{ 0, 0, -1 };
	const auto             vertices = cpmc::MeshSection(world, section, atlas, {});
	for (size_t first = 0; first < vertices.size(); first += 3)
	{
		std::array<std::array<double, 3>, 3> ndc{};
		for (size_t k = 0; k < 3; ++k)
		{
			const cpmc::Vertex& v = vertices[first + k];
			const cpmc::Vec3d   worldPos = cpmc::WorldFromMc({ section.x * 16.0 + v.x, section.y * 16.0 + v.y, section.z * 16.0 + v.z });
			const auto          clip = cpmc::Transform({ worldPos.x - frame->origin.x, worldPos.y - frame->origin.y, worldPos.z - frame->origin.z, 1.0 }, frame->cameraToClip);
			ndc[k] = { clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3] };
		}
		const auto face = cpmc::Face(((vertices[first].flags >> 4) & 15) - 1);
		const double area = SignedArea(ndc[0], ndc[1], ndc[2]);
		// Direct3D's rule: a front face winds clockwise on screen, or counter-clockwise with FrontCounterClockwise.
		const bool front = cpmc::BlockRasterizer(1).FrontCounterClockwise ? area > 0.0 : area < 0.0;
		if (face == cpmc::Face::South)  // world -y: towards the camera
		{
			CHECK(area > 0.0);
			CHECK(front);
		}
		if (face == cpmc::Face::North)  // away from the camera
		{
			CHECK(area < 0.0);
			CHECK(!front);
		}
	}
}
}

TEST(WindingFacesTheCameraWithALeftHandedProjection)
{
	CheckWinding({ 0.8f, 0, 0, 0, 0, 1.4f, 0, 0, 0, 0, 0, 1, 0, 0, 0.1f, 0 });
}

TEST(WindingFacesTheCameraWithARightHandedColumnProjection)
{
	CheckWinding({ 0.8f, 0, 0, 0, 0, 1.4f, 0, 0, 0, 0, 0, 0.1f, 0, 0, -1, 0 });
}

TEST(WindingCullSettingsPickTheirFaces)
{
	CHECK(cpmc::BlockRasterizer(0).CullMode == D3D12_CULL_MODE_NONE);
	CHECK(cpmc::BlockRasterizer(1).CullMode == D3D12_CULL_MODE_BACK);
	CHECK(cpmc::BlockRasterizer(2).CullMode == D3D12_CULL_MODE_FRONT);
	CHECK(cpmc::BlockRasterizer(1).FrontCounterClockwise == TRUE);
	CHECK(cpmc::BlockRasterizer(1).DepthClipEnable == TRUE);
}
