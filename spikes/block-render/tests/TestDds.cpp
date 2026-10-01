#include "Harness.h"

#include "log/DdsWriter.h"

#include <dxgiformat.h>

#include <cstring>

namespace
{
uint32_t Read32(const std::vector<uint8_t>& aBytes, size_t aAt)
{
	uint32_t value = 0;
	std::memcpy(&value, aBytes.data() + aAt, 4);
	return value;
}
}

TEST(DdsWritesTheHeaderAndPacksRows)
{
	// 3 x 2 texels of RGBA8, rows 16 bytes apart in the source.
	uint8_t source[32];
	for (uint8_t i = 0; i < 32; ++i)
	{
		source[i] = i;
	}
	const auto dds = cpmc::MakeDds(DXGI_FORMAT_R8G8B8A8_UNORM, 3, 2, source, 16);
	CHECK_EQ(dds.size(), size_t(4 + 124 + 20 + 24));
	CHECK(std::memcmp(dds.data(), "DDS ", 4) == 0);
	CHECK_EQ(Read32(dds, 4), 124u);
	CHECK_EQ(Read32(dds, 12), 2u);   // height
	CHECK_EQ(Read32(dds, 16), 3u);   // width
	CHECK_EQ(Read32(dds, 20), 12u);  // bytes per row
	CHECK(std::memcmp(dds.data() + 84, "DX10", 4) == 0);
	CHECK_EQ(Read32(dds, 128), uint32_t(DXGI_FORMAT_R8G8B8A8_UNORM));
	CHECK_EQ(Read32(dds, 132), 3u);  // 2D
	CHECK_EQ(Read32(dds, 140), 1u);  // one slice
	CHECK_EQ(int(dds[148]), 0);
	CHECK_EQ(int(dds[148 + 11]), 11);
	CHECK_EQ(int(dds[148 + 12]), 16);  // the second row starts at the source's pitch
}

TEST(DdsKnowsTexelSizes)
{
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_R10G10B10A2_UNORM), 4u);
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_R16G16B16A16_FLOAT), 8u);
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_R8_UINT), 1u);
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_R16G16_FLOAT), 4u);
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_BC1_UNORM), 0u);
}

TEST(DdsTypesTypelessAndDepthPlanes)
{
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R32G8X24_TYPELESS, 0), uint32_t(DXGI_FORMAT_R32_FLOAT));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 1), uint32_t(DXGI_FORMAT_R8_UINT));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R24G8_TYPELESS, 0), uint32_t(DXGI_FORMAT_R24_UNORM_X8_TYPELESS));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R10G10B10A2_TYPELESS, 0), uint32_t(DXGI_FORMAT_R10G10B10A2_UNORM));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R16G16_TYPELESS, 0), uint32_t(DXGI_FORMAT_R16G16_FLOAT));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R11G11B10_FLOAT, 0), uint32_t(DXGI_FORMAT_R11G11B10_FLOAT));
}
