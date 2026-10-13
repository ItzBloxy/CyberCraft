#include "render/SectionParts.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace cpmc
{
uint32_t SectionParts::First(SectionPart aPart) const
{
	switch (aPart)
	{
	case SectionPart::Solid: return 0;
	case SectionPart::Plant: return solid;
	case SectionPart::Translucent: return solid + plants;
	}
	return 0;
}

uint32_t SectionParts::Count(SectionPart aPart) const
{
	switch (aPart)
	{
	case SectionPart::Solid: return solid;
	case SectionPart::Plant: return plants;
	case SectionPart::Translucent: return translucent;
	}
	return 0;
}

namespace
{
// The texels [from, to) a UV range covers along a side of aSize texels: its ends rounded to the nearest texel edge, and
// at least one texel.
std::pair<uint32_t, uint32_t> TexelSpan(float aLow, float aHigh, uint32_t aSize)
{
	const auto edge = [aSize](float aUv) {
		return uint32_t(std::clamp(std::lround(double(aUv) * double(aSize)), 0l, long(aSize)));
	};
	uint32_t from = edge(aLow);
	uint32_t to = edge(aHigh);
	if (to <= from)
	{
		from = std::min(from, aSize - 1);
		to = from + 1;
	}
	return { from, to };
}

using Box = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>;  // x from, x to, y from, y to

Box TexelBox(const Atlas& aAtlas, std::span<const Vertex, 3> aTriangle)
{
	float uLow = aTriangle[0].u;
	float uHigh = uLow;
	float vLow = aTriangle[0].v;
	float vHigh = vLow;
	for (const Vertex& corner : aTriangle)
	{
		uLow = std::min(uLow, corner.u);
		uHigh = std::max(uHigh, corner.u);
		vLow = std::min(vLow, corner.v);
		vHigh = std::max(vHigh, corner.v);
	}
	const auto [x0, x1] = TexelSpan(uLow, uHigh, aAtlas.width);
	const auto [y0, y1] = TexelSpan(vLow, vHigh, aAtlas.height);
	return { x0, x1, y0, y1 };
}

bool PartlyClearIn(const Atlas& aAtlas, const Box& aBox)
{
	const auto [x0, x1, y0, y1] = aBox;
	const uint32_t start = aAtlas.levelOffsets.empty() ? 0 : aAtlas.levelOffsets[0];
	for (uint32_t y = y0; y < y1; ++y)
	{
		for (uint32_t x = x0; x < x1; ++x)
		{
			const size_t at = size_t(start) + size_t(y) * aAtlas.width + x;
			if (at >= aAtlas.texels.size())
			{
				return false;
			}
			const uint32_t alpha = aAtlas.texels[at] >> 24;
			if (alpha != 0 && alpha != 255)
			{
				return true;
			}
		}
	}
	return false;
}

// PartOf, with each texel box's answer kept in aSeen, if there is one.
SectionPart Classify(std::span<const Vertex, 3> aTriangle, const Atlas* aAtlas, std::map<Box, bool>* aSeen)
{
	if (aTriangle[0].flags & kFlagTranslucent)
	{
		if (!aAtlas || aAtlas->width == 0 || aAtlas->height == 0)
		{
			return SectionPart::Translucent;
		}
		const Box box = TexelBox(*aAtlas, aTriangle);
		bool      partly = false;
		if (!aSeen)
		{
			partly = PartlyClearIn(*aAtlas, box);
		}
		else if (const auto found = aSeen->find(box); found != aSeen->end())
		{
			partly = found->second;
		}
		else
		{
			partly = aSeen->emplace(box, PartlyClearIn(*aAtlas, box)).first->second;
		}
		if (partly)
		{
			return SectionPart::Translucent;
		}
	}
	return ((aTriangle[0].flags >> 4) & 15u) == 0 ? SectionPart::Plant : SectionPart::Solid;
}
}

bool PartlyClear(const Atlas& aAtlas, std::span<const Vertex, 3> aTriangle)
{
	if (aAtlas.width == 0 || aAtlas.height == 0)
	{
		return false;
	}
	return PartlyClearIn(aAtlas, TexelBox(aAtlas, aTriangle));
}

SectionPart PartOf(std::span<const Vertex, 3> aTriangle, const Atlas* aAtlas)
{
	return Classify(aTriangle, aAtlas, nullptr);
}

SectionParts SplitSection(std::vector<Vertex>& aVertices, const Atlas* aAtlas)
{
	const size_t triangles = aVertices.size() / 3;
	// A section's translucent triangles mostly share a few sprites (water's, a glass colour's): each box is read once.
	std::map<Box, bool> seen;
	std::vector<Vertex> parts[3];
	for (size_t t = 0; t < triangles; ++t)
	{
		const std::span<const Vertex, 3> triangle(aVertices.data() + t * 3, 3);
		std::vector<Vertex>&             into = parts[size_t(Classify(triangle, aAtlas, &seen))];
		into.insert(into.end(), triangle.begin(), triangle.end());
	}
	SectionParts counts{ uint32_t(parts[0].size()), uint32_t(parts[1].size()), uint32_t(parts[2].size()) };
	aVertices.clear();
	for (const std::vector<Vertex>& vertices : parts)
	{
		aVertices.insert(aVertices.end(), vertices.begin(), vertices.end());
	}
	return counts;
}
}
