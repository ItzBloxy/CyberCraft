#pragma once

#include <d3d12.h>

namespace cpmc
{
// The rasterizer state the blocks draw with. The mesher winds faces counter-clockwise seen from the front, as
// Minecraft does, so counter-clockwise triangles are the front faces: Direct3D's default is the opposite. aCull: 0
// culls nothing, 1 back faces, 2 front faces.
D3D12_RASTERIZER_DESC BlockRasterizer(int aCull);
}
