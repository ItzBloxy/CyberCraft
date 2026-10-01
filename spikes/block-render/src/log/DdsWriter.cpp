#include "log/DdsWriter.h"

#include <dxgiformat.h>

#include <cstring>

namespace cpmc
{
namespace
{
void Put32(std::vector<uint8_t>& aOut, uint32_t aValue)
{
	for (int i = 0; i < 4; ++i)
	{
		aOut.push_back(uint8_t(aValue >> (8 * i)));
	}
}
}

uint32_t BytesPerTexel(uint32_t aDxgiFormat)
{
	switch (DXGI_FORMAT(aDxgiFormat))
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 16;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
		return 8;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		return 4;
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
		return 2;
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
		return 1;
	default:
		return 0;
	}
}

uint32_t DumpFormat(uint32_t aResourceFormat, uint32_t aPlane)
{
	switch (DXGI_FORMAT(aResourceFormat))
	{
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return aPlane == 0 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R8_UINT;
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return aPlane == 0 ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R8_UINT;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_UNORM;
	case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case DXGI_FORMAT_R32G32_TYPELESS: return DXGI_FORMAT_R32G32_FLOAT;
	case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
	case DXGI_FORMAT_R16G16_TYPELESS: return DXGI_FORMAT_R16G16_FLOAT;
	case DXGI_FORMAT_R8G8_TYPELESS: return DXGI_FORMAT_R8G8_UNORM;
	case DXGI_FORMAT_R8_TYPELESS: return DXGI_FORMAT_R8_UNORM;
	default: return aResourceFormat;
	}
}

std::vector<uint8_t> MakeDds(uint32_t aFormat, uint32_t aWidth, uint32_t aHeight, const uint8_t* aData, size_t aPitch)
{
	const uint32_t       rowBytes = aWidth * BytesPerTexel(aFormat);
	std::vector<uint8_t> out;
	out.reserve(148 + size_t(rowBytes) * aHeight);
	const char magic[] = "DDS ";
	out.insert(out.end(), magic, magic + 4);
	Put32(out, 124);                      // header size
	Put32(out, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000);  // caps, height, width, pitch, pixel format
	Put32(out, aHeight);
	Put32(out, aWidth);
	Put32(out, rowBytes);
	Put32(out, 0);  // depth
	Put32(out, 0);  // mip count
	for (int i = 0; i < 11; ++i)
	{
		Put32(out, 0);
	}
	Put32(out, 32);   // pixel format size
	Put32(out, 0x4);  // a four-character code follows
	out.insert(out.end(), { 'D', 'X', '1', '0' });
	for (int i = 0; i < 5; ++i)
	{
		Put32(out, 0);
	}
	Put32(out, 0x1000);  // a texture
	for (int i = 0; i < 4; ++i)
	{
		Put32(out, 0);
	}
	Put32(out, aFormat);
	Put32(out, 3);  // a 2D texture
	Put32(out, 0);
	Put32(out, 1);  // one array slice
	Put32(out, 0);
	for (uint32_t row = 0; row < aHeight; ++row)
	{
		const uint8_t* from = aData + row * aPitch;
		out.insert(out.end(), from, from + rowBytes);
	}
	return out;
}
}
