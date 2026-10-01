#include "Harness.h"

#include "render/Camera.h"

#include <cmath>

using cpmc::CameraFrame;
using cpmc::Mat4;
using cpmc::SlConstants;

namespace
{
constexpr double kXs = 0.8;   // 1 / (tan(fov / 2) * aspect)
constexpr double kYs = 1.4;   // 1 / tan(fov / 2)
constexpr double kNear = 0.1;

// A camera at (100, 200, 10) in the world, looking north (+y), with +z up and +x to its right.
SlConstants Camera(const std::array<float, 16>& aViewToClip)
{
	SlConstants constants;
	constants.viewToClip = aViewToClip;
	constants.position = { 100.0f, 200.0f, 10.0f };
	constants.forward = { 0.0f, 1.0f, 0.0f };
	constants.right = { 1.0f, 0.0f, 0.0f };
	constants.up = { 0.0f, 0.0f, 1.0f };
	constants.depthInverted = 1;
	return constants;
}

// D3D's usual left-handed view space (x right, y up, z forward), reversed infinite depth, row vectors.
std::array<float, 16> LeftHanded()
{
	return { float(kXs), 0, 0, 0, 0, float(kYs), 0, 0, 0, 0, 0, 1, 0, 0, float(kNear), 0 };
}

// A right-handed view space (z backward) stored for column vectors, as OpenGL-style code writes it.
std::array<float, 16> RightHandedColumns()
{
	return { float(kXs), 0, 0, 0, 0, float(kYs), 0, 0, 0, 0, 0, float(kNear), 0, 0, -1, 0 };
}

// A Z-up view space (x right, y forward, z up), row vectors.
std::array<float, 16> ZUp()
{
	return { float(kXs), 0, 0, 0, 0, 0, 0, 1, 0, float(kYs), 0, 0, 0, 0, float(kNear), 0 };
}

// Where a world point lands in normalised device coordinates, plus its depth.
std::array<double, 3> Ndc(const CameraFrame& aFrame, double aX, double aY, double aZ)
{
	const auto clip = cpmc::Transform({ aX - aFrame.origin.x, aY - aFrame.origin.y, aZ - aFrame.origin.z, 1.0 }, aFrame.cameraToClip);
	return { clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3] };
}

void ChecksOut(const std::array<float, 16>& aViewToClip)
{
	const auto frame = cpmc::MakeCameraFrame(Camera(aViewToClip), {});
	CHECK(frame.has_value());
	if (!frame)
	{
		return;
	}
	// 10 m ahead: the centre of the screen, depth near/10 (reversed).
	const auto ahead = Ndc(*frame, 100.0, 210.0, 10.0);
	CHECK_NEAR(ahead[0], 0.0, 1e-9);
	CHECK_NEAR(ahead[1], 0.0, 1e-9);
	// 10 m ahead, 1 m right and 2 m up.
	const auto offset = Ndc(*frame, 101.0, 210.0, 12.0);
	CHECK_NEAR(offset[0], kXs * 1.0 / 10.0, 1e-6);
	CHECK_NEAR(offset[1], kYs * 2.0 / 10.0, 1e-6);
	CHECK(frame->reversedDepth);
}
}

TEST(CameraHandlesALeftHandedRowVectorProjection)
{
	const auto layout = cpmc::FindProjectionLayout(LeftHanded());
	CHECK(layout.has_value());
	CHECK(!layout->columnVectors);
	CHECK_EQ(layout->forwardAxis, 2);
	CHECK_EQ(layout->forwardSign, 1.0);
	ChecksOut(LeftHanded());
	const auto frame = cpmc::MakeCameraFrame(Camera(LeftHanded()), {});
	CHECK_NEAR(Ndc(*frame, 100.0, 210.0, 10.0)[2], kNear / 10.0, 1e-9);
}

TEST(CameraHandlesARightHandedColumnVectorProjection)
{
	const auto layout = cpmc::FindProjectionLayout(RightHandedColumns());
	CHECK(layout.has_value());
	CHECK(layout->columnVectors);
	CHECK_EQ(layout->forwardAxis, 2);
	CHECK_EQ(layout->forwardSign, -1.0);
	ChecksOut(RightHandedColumns());
}

TEST(CameraHandlesAZUpViewSpace)
{
	const auto layout = cpmc::FindProjectionLayout(ZUp());
	CHECK(layout.has_value());
	CHECK_EQ(layout->forwardAxis, 1);
	CHECK_EQ(layout->upAxis, 2);
	ChecksOut(ZUp());
}

TEST(CameraRefusesWhatIsntAPerspective)
{
	const std::array<float, 16> identity = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	CHECK(!cpmc::FindProjectionLayout(identity));
	SlConstants noAxes = Camera(LeftHanded());
	noAxes.forward = { 0.0f, 0.0f, 0.0f };
	CHECK(!cpmc::MakeCameraFrame(noAxes, {}));
}

TEST(CameraAddsTheRenderOrigin)
{
	const auto frame = cpmc::MakeCameraFrame(Camera(LeftHanded()), { 0.0, 1024.0, 0.0 });
	CHECK_NEAR(frame->origin.y, 1224.0, 1e-9);
	// A point 10 m ahead of the camera's true world position is still the screen's centre.
	CHECK_NEAR(Ndc(*frame, 100.0, 1234.0, 10.0)[0], 0.0, 1e-9);
}

TEST(CameraJitterMovesAPointByThatManyPixels)
{
	const auto frame = cpmc::MakeCameraFrame(Camera(LeftHanded()), {});
	for (const double sign : { 1.0, -1.0 })
	{
		CameraFrame jittered = *frame;
		jittered.cameraToClip = cpmc::AddJitter(frame->cameraToClip, 0.5, 0.25, 1000, 500, sign);
		const auto ndc = Ndc(jittered, 100.0, 210.0, 10.0);
		// NDC spans 2 units over the image, so a pixel is 2 / 1000 across and 2 / 500 down (y runs up).
		CHECK_NEAR(ndc[0] * 1000.0 / 2.0, sign * 0.5, 1e-9);
		CHECK_NEAR(-ndc[1] * 500.0 / 2.0, sign * 0.25, 1e-9);
	}
}

TEST(CameraMapsOutputPixelsIntoTheDepthExtent)
{
	const sl::Extent whole{ 0, 0, 1707, 960 };
	CHECK(cpmc::DepthTexelFor(0, 0, 2560, 1440, whole) == std::make_pair(0u, 0u));
	CHECK(cpmc::DepthTexelFor(2559, 1439, 2560, 1440, whole) == std::make_pair(1706u, 959u));
	CHECK(cpmc::DepthTexelFor(1280, 720, 2560, 1440, whole) == std::make_pair(853u, 480u));
	const sl::Extent part{ 10, 20, 100, 50 };  // top, left, width, height
	CHECK(cpmc::DepthTexelFor(0, 0, 200, 100, part) == std::make_pair(20u, 10u));
	CHECK(cpmc::DepthTexelFor(199, 99, 200, 100, part) == std::make_pair(119u, 59u));
}
