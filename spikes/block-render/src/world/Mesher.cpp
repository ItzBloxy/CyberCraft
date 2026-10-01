#include "world/Mesher.h"

#include <array>
#include <cmath>

namespace cpmc
{
namespace
{
struct FaceShape
{
	std::array<int, 3>                normal;
	std::array<std::array<int, 3>, 4> corners;  // unit-cube corners, counter-clockwise seen from outside
};

// Corners per face, in Face order. Seen from outside, corner 0 is bottom-left, then bottom-right, top-right and
// top-left (for up and down: the texture's bottom-left onwards), so every face takes the same UVs.
constexpr std::array<FaceShape, 6> kFaces = { {
	{ { 0, -1, 0 }, { { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } } } },  // down
	{ { 0, 1, 0 }, { { { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 0 } } } },   // up
	{ { 0, 0, -1 }, { { { 1, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } } } },  // north
	{ { 0, 0, 1 }, { { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } } } },   // south
	{ { -1, 0, 0 }, { { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 } } } },  // west
	{ { 1, 0, 0 }, { { { 1, 0, 1 }, { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 } } } },   // east
} };

// Texture coordinates of the corners: bottom-left, bottom-right, top-right, top-left.
constexpr std::array<std::array<float, 2>, 4> kCornerUv = { { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } } };

bool Opaque(const Atlas& aAtlas, uint16_t aId)
{
	const AtlasBlock* block = aAtlas.Block(aId);
	return block && !block->cutout;
}

bool FaceHidden(const Atlas& aAtlas, uint16_t aId, uint16_t aNeighbour)
{
	const AtlasBlock* neighbour = aAtlas.Block(aNeighbour);
	return neighbour && (!neighbour->cutout || aNeighbour == aId);
}

uint8_t Scale(uint8_t aValue, float aFactor)
{
	return uint8_t(std::lround(aValue * aFactor));
}
}

std::vector<Vertex> MeshSection(const BlockWorld& aWorld, const SectionPos& aSection, const Atlas& aAtlas,
                                const MeshOptions& aOptions)
{
	std::vector<Vertex>       vertices;
	const BlockWorld::Section* section = aWorld.Find(aSection);
	if (!section)
	{
		return vertices;
	}
	const BlockPos base{ aSection.x * 16, aSection.y * 16, aSection.z * 16 };
	for (int y = 0; y < 16; ++y)
	{
		for (int z = 0; z < 16; ++z)
		{
			for (int x = 0; x < 16; ++x)
			{
				const uint16_t    id = (*section)[size_t(x + 16 * (z + 16 * y))];
				const AtlasBlock* block = aAtlas.Block(id);
				if (!block)
				{
					continue;
				}
				const BlockPos at{ base.x + x, base.y + y, base.z + z };
				for (size_t face = 0; face < 6; ++face)
				{
					const FaceShape& shape = kFaces[face];
					const BlockPos   front{ at.x + shape.normal[0], at.y + shape.normal[1], at.z + shape.normal[2] };
					if (FaceHidden(aAtlas, id, aWorld.Get(front)))
					{
						continue;
					}
					const AtlasSprite& sprite = aAtlas.sprites[block->faces[face]];
					const bool         tinted = (block->tintedFaces >> face) & 1;
					std::array<float, 4> shade{ 1.0f, 1.0f, 1.0f, 1.0f };
					if (aOptions.ambientOcclusion)
					{
						for (size_t c = 0; c < 4; ++c)
						{
							// The two blocks beside this corner, and the one diagonal to it, in the layer the face looks into.
							BlockPos side1 = front, side2 = front, corner = front;
							int      tangent = 0;
							for (int axis = 0; axis < 3; ++axis)
							{
								if (shape.normal[size_t(axis)] != 0)
								{
									continue;
								}
								const int step = shape.corners[c][size_t(axis)] == 0 ? -1 : 1;
								BlockPos& side = tangent++ == 0 ? side1 : side2;
								(axis == 0 ? side.x : axis == 1 ? side.y : side.z) += step;
								(axis == 0 ? corner.x : axis == 1 ? corner.y : corner.z) += step;
							}
							const bool s1 = Opaque(aAtlas, aWorld.Get(side1));
							const bool s2 = Opaque(aAtlas, aWorld.Get(side2));
							const bool c3 = (s1 && s2) || Opaque(aAtlas, aWorld.Get(corner));
							shade[c] = (1.0f + (s1 ? 0.2f : 1.0f) + (s2 ? 0.2f : 1.0f) + (c3 ? 0.2f : 1.0f)) / 4.0f;
						}
					}
					Vertex corners[4];
					for (size_t c = 0; c < 4; ++c)
					{
						Vertex& v = corners[c];
						v.x = float(x + shape.corners[c][0]);
						v.y = float(y + shape.corners[c][1]);
						v.z = float(z + shape.corners[c][2]);
						v.u = (sprite.x + kCornerUv[c][0] * sprite.w) / float(aAtlas.width);
						v.v = (sprite.y + kCornerUv[c][1] * sprite.h) / float(aAtlas.height);
						v.r = Scale(tinted ? block->tint[0] : 255, shade[c]);
						v.g = Scale(tinted ? block->tint[1] : 255, shade[c]);
						v.b = Scale(tinted ? block->tint[2] : 255, shade[c]);
						v.a = 255;
						v.light = 15u << 8;
						v.flags = kFlagAlphaTested | FaceFlags(Face(face));
					}
					// Split along the darker diagonal, so a shaded corner's gradient spreads over both triangles alike.
					const bool flip = shade[0] + shade[2] > shade[1] + shade[3];
					const int  order[6] = { 0, 1, 2, 0, 2, 3 };
					const int  flipped[6] = { 1, 2, 3, 1, 3, 0 };
					for (const int c : flip ? flipped : order)
					{
						vertices.push_back(corners[c]);
					}
				}
			}
		}
	}
	return vertices;
}
}
