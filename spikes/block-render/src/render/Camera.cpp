#include "render/Camera.h"

#include <cmath>

namespace cpmc
{
namespace
{
double At(const Mat4& aMatrix, int aRow, int aColumn)
{
	return aMatrix[size_t(aRow * 4 + aColumn)];
}

bool Near(double aValue, double aTarget, double aTolerance)
{
	return std::fabs(aValue - aTarget) <= aTolerance;
}

// True when, for row vectors, exactly one view axis feeds clip w, with a factor of +-1, and nothing else does.
bool PerspectiveForRowVectors(const Mat4& aMatrix)
{
	int ones = 0;
	for (int axis = 0; axis < 3; ++axis)
	{
		const double w = std::fabs(At(aMatrix, axis, 3));
		if (Near(w, 1.0, 1e-3))
		{
			++ones;
		}
		else if (w > 1e-3)
		{
			return false;
		}
	}
	return ones == 1 && Near(At(aMatrix, 3, 3), 0.0, 1e-3);
}

Mat4 Transposed(const Mat4& aMatrix)
{
	Mat4 out{};
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			out[size_t(row * 4 + column)] = At(aMatrix, column, row);
		}
	}
	return out;
}

double Sign(double aValue)
{
	return aValue < 0.0 ? -1.0 : 1.0;
}

double Length(const std::array<float, 3>& aVector)
{
	return std::sqrt(double(aVector[0]) * aVector[0] + double(aVector[1]) * aVector[1] + double(aVector[2]) * aVector[2]);
}
}

Mat4 Multiply(const Mat4& aFirst, const Mat4& aThen)
{
	Mat4 out{};
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			double sum = 0.0;
			for (int k = 0; k < 4; ++k)
			{
				sum += At(aFirst, row, k) * At(aThen, k, column);
			}
			out[size_t(row * 4 + column)] = sum;
		}
	}
	return out;
}

std::array<double, 4> Transform(const std::array<double, 4>& aPoint, const Mat4& aMatrix)
{
	std::array<double, 4> out{};
	for (int column = 0; column < 4; ++column)
	{
		for (int k = 0; k < 4; ++k)
		{
			out[size_t(column)] += aPoint[size_t(k)] * At(aMatrix, k, column);
		}
	}
	return out;
}

std::optional<ProjectionLayout> FindProjectionLayout(const std::array<float, 16>& aViewToClip)
{
	Mat4 matrix{};
	for (size_t i = 0; i < 16; ++i)
	{
		matrix[i] = aViewToClip[i];
	}
	ProjectionLayout layout;
	if (!PerspectiveForRowVectors(matrix))
	{
		matrix = Transposed(matrix);
		if (!PerspectiveForRowVectors(matrix))
		{
			return std::nullopt;
		}
		layout.columnVectors = true;
	}
	for (int axis = 0; axis < 3; ++axis)
	{
		if (Near(std::fabs(At(matrix, axis, 3)), 1.0, 1e-3))
		{
			layout.forwardAxis = axis;
			layout.forwardSign = Sign(At(matrix, axis, 3));
		}
	}
	const int first = (layout.forwardAxis + 1) % 3;
	const int second = (layout.forwardAxis + 2) % 3;
	layout.rightAxis = std::fabs(At(matrix, first, 0)) >= std::fabs(At(matrix, second, 0)) ? first : second;
	layout.upAxis = layout.rightAxis == first ? second : first;
	layout.rightSign = Sign(At(matrix, layout.rightAxis, 0));
	layout.upSign = Sign(At(matrix, layout.upAxis, 1));
	if (std::fabs(At(matrix, layout.rightAxis, 0)) < 1e-3 || std::fabs(At(matrix, layout.upAxis, 1)) < 1e-3)
	{
		return std::nullopt;
	}
	return layout;
}

std::optional<CameraFrame> MakeCameraFrame(const SlConstants& aConstants, const Vec3d& aRenderOrigin)
{
	const std::optional<ProjectionLayout> layout = FindProjectionLayout(aConstants.viewToClip);
	if (!layout || !Near(Length(aConstants.forward), 1.0, 0.01) || !Near(Length(aConstants.right), 1.0, 0.01) ||
	    !Near(Length(aConstants.up), 1.0, 0.01))
	{
		return std::nullopt;
	}
	// World (camera-relative) to the game's view space: each view axis is the matching camera vector, signed.
	Mat4 view{};
	for (int i = 0; i < 3; ++i)
	{
		view[size_t(i * 4 + layout->forwardAxis)] = layout->forwardSign * aConstants.forward[size_t(i)];
		view[size_t(i * 4 + layout->rightAxis)] = layout->rightSign * aConstants.right[size_t(i)];
		view[size_t(i * 4 + layout->upAxis)] = layout->upSign * aConstants.up[size_t(i)];
	}
	view[15] = 1.0;
	Mat4 projection{};
	for (size_t i = 0; i < 16; ++i)
	{
		projection[i] = aConstants.viewToClip[i];
	}
	if (layout->columnVectors)
	{
		projection = Transposed(projection);
	}
	CameraFrame frame;
	frame.cameraToClip = Multiply(view, projection);
	frame.origin = { aConstants.position[0] + aRenderOrigin.x, aConstants.position[1] + aRenderOrigin.y,
		             aConstants.position[2] + aRenderOrigin.z };
	frame.reversedDepth = aConstants.depthInverted == 1;
	return frame;
}

Mat4 AddJitter(const Mat4& aCameraToClip, double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign)
{
	const std::array<double, 2> offset = ClipJitter(aJitterX, aJitterY, aWidth, aHeight, aSign);
	Mat4 jitter{};
	jitter[0] = jitter[5] = jitter[10] = jitter[15] = 1.0;
	jitter[12] = offset[0];  // clip x += this * clip w
	jitter[13] = offset[1];
	return Multiply(aCameraToClip, jitter);
}

std::array<double, 2> ClipJitter(double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign)
{
	return { aSign * 2.0 * aJitterX / aWidth, -aSign * 2.0 * aJitterY / aHeight };  // pixel rows run down, clip y up
}

std::array<double, 2> MotionVector(const std::array<double, 4>& aClip, const Mat4& aClipToPrevClip, double aSign)
{
	const std::array<double, 4> before = Transform(aClip, aClipToPrevClip);
	// Normalised device coordinates span 2 across the screen, and run up where the screen's y runs down.
	return { aSign * (before[0] / before[3] - aClip[0] / aClip[3]) * 0.5,
	         -aSign * (before[1] / before[3] - aClip[1] / aClip[3]) * 0.5 };
}

std::pair<uint32_t, uint32_t> DepthTexelFor(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, const sl::Extent& aExtent)
{
	const auto scale = [](uint32_t aPixel, uint32_t aSize, uint32_t aStart, uint32_t aSpan) {
		const auto texel = uint32_t((double(aPixel) + 0.5) * aSpan / aSize);
		return aStart + (texel < aSpan ? texel : aSpan - 1);
	};
	return { scale(aX, aWidth, aExtent.left, aExtent.width), scale(aY, aHeight, aExtent.top, aExtent.height) };
}
}
