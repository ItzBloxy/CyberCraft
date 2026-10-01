#pragma once

#include "Coords.h"
#include "sl/Sl.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace cpmc
{
// A 4x4 matrix used with row vectors (p' = p * M), stored row by row, as Streamline and HLSL's mul(vector, matrix)
// use them.
using Mat4 = std::array<double, 16>;

Mat4                  Multiply(const Mat4& aFirst, const Mat4& aThen);  // p * aFirst * aThen
std::array<double, 4> Transform(const std::array<double, 4>& aPoint, const Mat4& aMatrix);

// How cameraViewToClip maps the game's view space to clip space, read from the matrix itself: whether it's stored for
// column vectors (and so needs transposing), and which view axis (0, 1 or 2) points forward, right and up, with which
// sign. Clip space is D3D's: x right, y up, w the distance in front.
struct ProjectionLayout
{
	bool   columnVectors = false;
	int    forwardAxis = 2;
	double forwardSign = 1.0;
	int    rightAxis = 0;
	double rightSign = 1.0;
	int    upAxis = 1;
	double upSign = 1.0;
};

// Nothing if aViewToClip isn't a perspective projection: one view axis must feed clip w with a factor of +-1, and
// clip w must not depend on anything else.
std::optional<ProjectionLayout> FindProjectionLayout(const std::array<float, 16>& aViewToClip);

// One frame's camera, ready for drawing.
struct CameraFrame
{
	Mat4  cameraToClip{};  // a camera-relative world position (metres, world axes) to clip space
	Vec3d origin;          // the camera's world position: cameraPos plus the render origin
	bool  reversedDepth = false;
};

// The frame's camera from Streamline's constants, without jitter. aRenderOrigin is added to cameraPos: zero unless
// step 1 finds the renderer works relative to an origin. Nothing if the projection isn't understood or the camera's
// axes are missing.
std::optional<CameraFrame> MakeCameraFrame(const SlConstants& aConstants, const Vec3d& aRenderOrigin);

// aCameraToClip with a jitter of (aJitterX, aJitterY) pixels added, for a render of aWidth x aHeight pixels: clip x
// moves by aSign * 2 * jitterX / width and clip y by -aSign * 2 * jitterY / height, times clip w.
Mat4 AddJitter(const Mat4& aCameraToClip, double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign);

// The texel of a depth buffer under output pixel (aX, aY) of an aWidth x aHeight image, when the depth image fills
// aExtent of its buffer.
std::pair<uint32_t, uint32_t> DepthTexelFor(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, const sl::Extent& aExtent);
}
