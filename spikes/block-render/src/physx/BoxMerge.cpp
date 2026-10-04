#include "physx/BoxMerge.h"

namespace cpmc::px
{
namespace
{
// The solid blocks not in a box yet.
class Left
{
public:
	explicit Left(const link::SolidBits& aBits) : bits_(aBits) {}

	bool Has(int aX, int aY, int aZ) const { return (bits_[size_t(Bit(aX, aY, aZ) >> 6)] >> (Bit(aX, aY, aZ) & 63)) & 1; }
	void Take(int aX, int aY, int aZ) { bits_[size_t(Bit(aX, aY, aZ) >> 6)] &= ~(uint64_t(1) << (Bit(aX, aY, aZ) & 63)); }

	// Whether blocks aX0 to aX1, not included, of row (aY, aZ) are all left.
	bool Row(int aX0, int aX1, int aY, int aZ) const
	{
		for (int x = aX0; x < aX1; ++x)
		{
			if (!Has(x, aY, aZ))
			{
				return false;
			}
		}
		return true;
	}

	// And of rows aZ0 to aZ1 of layer aY.
	bool Layer(int aX0, int aX1, int aZ0, int aZ1, int aY) const
	{
		for (int z = aZ0; z < aZ1; ++z)
		{
			if (!Row(aX0, aX1, aY, z))
			{
				return false;
			}
		}
		return true;
	}

private:
	static int Bit(int aX, int aY, int aZ) { return aX + 16 * aZ + 256 * aY; }

	link::SolidBits bits_;
};
}

std::vector<BlockBox> MergeBoxes(const link::SolidBits& aBits)
{
	Left                  left(aBits);
	std::vector<BlockBox> boxes;
	for (int y = 0; y < 16; ++y)
	{
		for (int z = 0; z < 16; ++z)
		{
			for (int x = 0; x < 16; ++x)
			{
				if (!left.Has(x, y, z))
				{
					continue;
				}
				// As far along x as the blocks go, then as many such rows along z, then as many such layers up.
				int x1 = x + 1;
				while (x1 < 16 && left.Has(x1, y, z))
				{
					++x1;
				}
				int z1 = z + 1;
				while (z1 < 16 && left.Row(x, x1, y, z1))
				{
					++z1;
				}
				int y1 = y + 1;
				while (y1 < 16 && left.Layer(x, x1, z, z1, y1))
				{
					++y1;
				}
				for (int by = y; by < y1; ++by)
				{
					for (int bz = z; bz < z1; ++bz)
					{
						for (int bx = x; bx < x1; ++bx)
						{
							left.Take(bx, by, bz);
						}
					}
				}
				boxes.push_back({ uint8_t(x), uint8_t(y), uint8_t(z), uint8_t(x1), uint8_t(y1), uint8_t(z1) });
			}
		}
	}
	return boxes;
}
}
