// Spike 9's deep review: a dump's static triangles, placed around a stand-in V's feet as mclink's `dump` places them,
// filed under each region as the exporter files them (bounds touching the region's blocks, half a block more each way),
// and voxelized by BlockRender's own VoxelizeRegion. Writes every solid cell as SkyCraft's ColBlock (80 bytes).
#include "DumpObj.h"
#include "link/Voxels.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using cpmc::link::McTriangle;

int main(int aArgc, char** aArgv)
{
	if (aArgc != 9)
	{
		std::puts("usage: voxdump <collision.obj> <feetX> <feetY> <feetZ> <radius> <below> <above> <out.bin>");
		return 2;
	}
	std::ifstream in(aArgv[1], std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	std::string error;
	auto triangles = cpmc::mclink::ParseDumpObj(text.str(), "s0_static_", error);
	if (!triangles)
	{
		std::printf("can't parse: %s\n", error.c_str());
		return 1;
	}
	const double feet[3] = { std::atof(aArgv[2]), std::atof(aArgv[3]), std::atof(aArgv[4]) };
	const int radius = std::atoi(aArgv[5]), below = std::atoi(aArgv[6]), above = std::atoi(aArgv[7]);
	for (McTriangle& t : *triangles)
	{
		for (int k = 0; k < 3; ++k)
		{
			for (int a = 0; a < 3; ++a)
			{
				t.v[k * 3 + a] = float(t.v[k * 3 + a] + feet[a]);
			}
		}
	}
	const int rx0 = int(std::floor((feet[0] - radius) / 8)), rx1 = int(std::floor((feet[0] + radius) / 8));
	const int ry0 = int(std::floor((feet[1] - below) / 8)), ry1 = int(std::floor((feet[1] + above) / 8));
	const int rz0 = int(std::floor((feet[2] - radius) / 8)), rz1 = int(std::floor((feet[2] + radius) / 8));
	std::ofstream out(aArgv[8], std::ios::binary);
	size_t cells = 0;
	for (int rx = rx0; rx <= rx1; ++rx)
	{
		for (int ry = ry0; ry <= ry1; ++ry)
		{
			for (int rz = rz0; rz <= rz1; ++rz)
			{
				const double low[3] = { rx * 8 - 0.5, ry * 8 - 0.5, rz * 8 - 0.5 };
				const double high[3] = { rx * 8 + 8.5, ry * 8 + 8.5, rz * 8 + 8.5 };
				std::vector<McTriangle> filed;
				for (const McTriangle& t : *triangles)
				{
					bool touches = true;
					for (int a = 0; a < 3 && touches; ++a)
					{
						const double lo = std::min({ t.v[a], t.v[3 + a], t.v[6 + a] });
						const double hi = std::max({ t.v[a], t.v[3 + a], t.v[6 + a] });
						touches = hi >= low[a] && lo <= high[a];
					}
					if (touches)
					{
						filed.push_back(t);
					}
				}
				const auto blocks = cpmc::link::VoxelizeRegion(rx, ry, rz, filed);
				out.write(reinterpret_cast<const char*>(blocks.data()), std::streamsize(blocks.size() * sizeof blocks[0]));
				cells += blocks.size();
			}
		}
	}
	std::printf("%zu triangles, regions x %d..%d y %d..%d z %d..%d, %zu solid cells\n", triangles->size(), rx0, rx1, ry0, ry1, rz0,
	            rz1, cells);
	return 0;
}
