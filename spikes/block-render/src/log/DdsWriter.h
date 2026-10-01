#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cpmc
{
// Bytes per texel of the uncompressed DXGI formats a dump saves; 0 for any other format.
uint32_t BytesPerTexel(uint32_t aDxgiFormat);

// The typed format a dump saves for one plane of a resource whose format may be typeless or a depth format: a depth
// plane becomes R32_FLOAT (or the 24-bit depth's R24_UNORM_X8_TYPELESS), a stencil plane R8_UINT, other typeless
// formats their UNORM or FLOAT form (R16G16 as FLOAT, as motion vectors are). Typed formats stay as they are.
uint32_t DumpFormat(uint32_t aResourceFormat, uint32_t aPlane);

// A DDS file (with the DX10 header) for one 2D image of aFormat, from aHeight rows of aWidth texels, each row
// starting aPitch bytes after the last in aData. The file's rows are packed.
std::vector<uint8_t> MakeDds(uint32_t aFormat, uint32_t aWidth, uint32_t aHeight, const uint8_t* aData, size_t aPitch);
}
