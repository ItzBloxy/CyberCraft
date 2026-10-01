#pragma once

#include <d3d12.h>

#include <functional>
#include <vector>

namespace cpmc
{
// A submission's command lists with lists of ours put in: aBefore(list) names one to go just before that list, or
// gives null. Empty when nothing is put in, so the submission can go through untouched.
std::vector<ID3D12CommandList*> WithListsBefore(UINT aCount, ID3D12CommandList* const* aLists,
                                                const std::function<ID3D12CommandList*(ID3D12CommandList*)>& aBefore);
}
