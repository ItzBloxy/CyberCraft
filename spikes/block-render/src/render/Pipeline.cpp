#include "render/Pipeline.h"

namespace cpmc
{
D3D12_RASTERIZER_DESC BlockRasterizer(int aCull)
{
	D3D12_RASTERIZER_DESC desc{};
	desc.FillMode = D3D12_FILL_MODE_SOLID;
	desc.CullMode = aCull == 0 ? D3D12_CULL_MODE_NONE : aCull == 1 ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT;
	desc.FrontCounterClockwise = TRUE;
	desc.DepthClipEnable = TRUE;
	return desc;
}
}
