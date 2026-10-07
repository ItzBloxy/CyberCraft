#include "entities/WorldShapes.h"

#include <array>
#include <cmath>
#include <numbers>

namespace cpmc::entities
{
namespace
{
using Point = std::array<float, 3>;

constexpr float    kRadians = std::numbers::pi_v<float> / 180.0f;
constexpr uint32_t kWhite = 0xFFFFFFFFu;

Vertex At(const Point& aPoint, float aU, float aV, uint32_t aRgba)
{
	Vertex vertex;
	vertex.x = aPoint[0];
	vertex.y = aPoint[1];
	vertex.z = aPoint[2];
	vertex.u = aU;
	vertex.v = aV;
	vertex.r = uint8_t(aRgba & 0xFF);
	vertex.g = uint8_t((aRgba >> 8) & 0xFF);
	vertex.b = uint8_t((aRgba >> 16) & 0xFF);
	vertex.a = uint8_t(aRgba >> 24);
	vertex.light = kShapeLight;
	vertex.flags = kShapeFlags;
	return vertex;
}

// Two triangles over four corners, top left, top right, bottom right and bottom left as they're seen, with the atlas
// rectangle aUv {u0, v0, u1, v1}: WorldRender.cpp's Quad.
void Quad(const std::array<Point, 4>& aCorners, const float aUv[4], uint32_t aRgba, std::vector<Vertex>& aOut)
{
	const float uv[4][2] = { { aUv[0], aUv[1] }, { aUv[2], aUv[1] }, { aUv[2], aUv[3] }, { aUv[0], aUv[3] } };
	for (const int k : { 0, 1, 2, 0, 2, 3 })
	{
		aOut.push_back(At(aCorners[k], uv[k][0], uv[k][1], aRgba));
	}
}

// A box from aMin of aSize, turned aYaw (radians) about its vertical centre line: its sides aSide, its top aTop tinted
// aTopTint (RGBA8, red in the low byte; 0 for none) and its bottom aBottom. WorldRender.cpp's Box.
void Box(const Point& aMin, const Point& aSize, float aYaw, const float aSide[4], const float aTop[4], const float aBottom[4],
         uint32_t aTopTint, std::vector<Vertex>& aOut)
{
	const float cx = aMin[0] + aSize[0] * 0.5f;
	const float cz = aMin[2] + aSize[2] * 0.5f;
	const float c = std::cos(aYaw);
	const float s = std::sin(aYaw);
	const auto  corner = [&](int aIndex) {
        const float lx = ((aIndex & 1) ? 0.5f : -0.5f) * aSize[0];
        const float lz = ((aIndex & 4) ? 0.5f : -0.5f) * aSize[2];
        return Point{ cx + lx * c - lz * s, aMin[1] + ((aIndex & 2) ? aSize[1] : 0.0f), cz + lx * s + lz * c };
	};
	// The corners' bits: 1 is +x, 2 is +y, 4 is +z. Each face's top left, top right, bottom right and bottom left, seen
	// from outside: south (+z), north, east (+x), west, top and bottom.
	static constexpr int kFaces[6][4] = { { 6, 7, 5, 4 }, { 3, 2, 0, 1 }, { 7, 3, 1, 5 }, { 2, 6, 4, 0 }, { 2, 3, 7, 6 }, { 4, 5, 1, 0 } };
	for (int face = 0; face < 6; ++face)
	{
		const std::array<Point, 4> corners{ corner(kFaces[face][0]), corner(kFaces[face][1]), corner(kFaces[face][2]), corner(kFaces[face][3]) };
		const float*               uv = face == 4 ? aTop : face == 5 ? aBottom : aSide;
		Quad(corners, uv, face == 4 && aTopTint != 0 ? 0xFF000000u | aTopTint : kWhite, aOut);
	}
}

// Minecraft's arrow, or a trident, at aAt flying along aDir (unit length, Minecraft axes): WorldRender.cpp's BuildArrow,
// at Minecraft's size. Around the flight, aSide is level, and up the cross of it and the flight; the fins sit at 45
// degrees between them.
void Arrow(const Point& aAt, const Point& aDir, const float aUvSide[4], const float aUvBack[4], bool aTrident, std::vector<Vertex>& aOut)
{
	Point side{ aDir[2], 0.0f, -aDir[0] };
	float length = std::sqrt(side[0] * side[0] + side[2] * side[2]);
	if (length < 1e-3f)
	{
		side = { 1.0f, 0.0f, 0.0f };  // flying straight up or down
		length = 1.0f;
	}
	side[0] /= length;
	side[2] /= length;
	const Point     up{ side[1] * aDir[2] - side[2] * aDir[1], side[2] * aDir[0] - side[0] * aDir[2], side[0] * aDir[1] - side[1] * aDir[0] };
	constexpr float r = 0.70710678f;
	const Point     fins[2] = { { (up[0] + side[0]) * r, (up[1] + side[1]) * r, (up[2] + side[2]) * r },
		                        { (up[0] - side[0]) * r, (up[1] - side[1]) * r, (up[2] - side[2]) * r } };
	// aAlong the flight, aAcross aFin, and aAcross2 aFin2 too, if there is one.
	const auto point = [&](float aAlong, const Point& aFin, float aAcross, const Point* aFin2 = nullptr, float aAcross2 = 0.0f) {
		Point p;
		for (int k = 0; k < 3; ++k)
		{
			p[k] = aAt[k] + aDir[k] * aAlong + aFin[k] * aAcross + (aFin2 ? (*aFin2)[k] * aAcross2 : 0.0f);
		}
		return p;
	};
	if (aTrident)
	{
		// The item icon, its diagonal running from the handle (bottom left) to the tip (top right) along the flight.
		constexpr float h = kTridentHalf;
		for (const Point& fin : fins)
		{
			Quad({ point(0.0f, fin, h), point(h, fin, 0.0f), point(0.0f, fin, -h), point(-h, fin, 0.0f) }, aUvSide, kWhite, aOut);
		}
		return;
	}
	// Minecraft's ArrowModel: two fins 16 long and 4 wide from -12 (the fletching) to +4 (the head), and a 4 x 4 back
	// plate at -11.
	constexpr float k = kArrowUnit;
	for (const Point& fin : fins)
	{
		Quad({ point(-12 * k, fin, -2 * k), point(4 * k, fin, -2 * k), point(4 * k, fin, 2 * k), point(-12 * k, fin, 2 * k) }, aUvSide, kWhite, aOut);
	}
	Quad({ point(-11 * k, fins[0], -2 * k, &fins[1], -2 * k), point(-11 * k, fins[0], 2 * k, &fins[1], -2 * k),
	       point(-11 * k, fins[0], 2 * k, &fins[1], 2 * k), point(-11 * k, fins[0], -2 * k, &fins[1], 2 * k) },
	     aUvBack, kWhite, aOut);
}

Point Relative(const proto::WorldEntity& aRecord, const Vec3d& aOrigin)
{
	return { float(aRecord.x - aOrigin.x), float(aRecord.y - aOrigin.y), float(aRecord.z - aOrigin.z) };
}
}

void AppendShape(const proto::WorldEntity& aRecord, const Vec3d& aOrigin, std::vector<Vertex>& aOut)
{
	const Point at = Relative(aRecord, aOrigin);
	switch (aRecord.kind)
	{
	case proto::kWeItem:
	{
		// A flat icon turning about the vertical, centred on the record.
		const float spin = aRecord.yaw * kRadians;
		const float half = aRecord.scale * 0.5f;
		const float rx = std::cos(spin) * half;
		const float rz = std::sin(spin) * half;
		Quad({ Point{ at[0] - rx, at[1] + half, at[2] - rz }, Point{ at[0] + rx, at[1] + half, at[2] + rz },
		       Point{ at[0] + rx, at[1] - half, at[2] + rz }, Point{ at[0] - rx, at[1] - half, at[2] - rz } },
		     aRecord.uv[0], kWhite, aOut);
		break;
	}
	case proto::kWeBlock:
	{
		// A small cube spinning about its centre, the record.
		const float s = aRecord.scale;
		Box({ at[0] - s * 0.5f, at[1] - s * 0.5f, at[2] - s * 0.5f }, { s, s, s }, aRecord.yaw * kRadians, aRecord.uv[0], aRecord.uv[1],
		    aRecord.uv[2], aRecord.tint, aOut);
		break;
	}
	case proto::kWeArrow:
	case proto::kWeTrident:
	{
		// Arrows face (sin yaw cos pitch, sin pitch, cos yaw cos pitch): Minecraft sets their yaw and pitch from their flight.
		const float yaw = aRecord.yaw * kRadians;
		const float pitch = aRecord.pitch * kRadians;
		const Point dir{ std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
		Arrow(at, dir, aRecord.uv[0], aRecord.uv[1], aRecord.kind == proto::kWeTrident, aOut);
		break;
	}
	default: break;
	}
}

void AppendCrack(const proto::WorldEntity& aRecord, const Vec3d& aOrigin, std::vector<Vertex>& aOut)
{
	if (aRecord.kind == proto::kWeCrack)
	{
		Box(Relative(aRecord, aOrigin), { aRecord.ext[0], aRecord.ext[1], aRecord.ext[2] }, 0.0f, aRecord.uv[0], aRecord.uv[0], aRecord.uv[0], 0,
		    aOut);
	}
}

void AppendOutline(const std::array<float, 6>& aBox, const Vec3d& aOrigin, std::vector<Vertex>& aOut)
{
	constexpr float g = kOutlineGrow;
	const Point     low{ float(aBox[0] - aOrigin.x) - g, float(aBox[1] - aOrigin.y) - g, float(aBox[2] - aOrigin.z) - g };
	const Point     high{ float(aBox[3] - aOrigin.x) + g, float(aBox[4] - aOrigin.y) + g, float(aBox[5] - aOrigin.z) + g };
	// The corners' bits: 1 is +x, 2 is +y, 4 is +z. The edges in BuildOutline's order: four along x, four along y, four
	// along z.
	static constexpr int kEdges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 },
		                                   { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
	for (const auto& edge : kEdges)
	{
		for (const int corner : edge)
		{
			aOut.push_back(At({ (corner & 1) ? high[0] : low[0], (corner & 2) ? high[1] : low[1], (corner & 4) ? high[2] : low[2] }, 0.0f, 0.0f,
			                  kOutlineRgba));
		}
	}
}
}
