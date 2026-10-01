#include "hooks/Submission.h"

namespace cpmc
{
std::vector<ID3D12CommandList*> WithListsBefore(UINT aCount, ID3D12CommandList* const* aLists,
                                                const std::function<ID3D12CommandList*(ID3D12CommandList*)>& aBefore)
{
	std::vector<ID3D12CommandList*> lists;
	for (UINT i = 0; i < aCount; ++i)
	{
		if (ID3D12CommandList* before = aBefore(aLists[i]))
		{
			if (lists.empty())
			{
				lists.assign(aLists, aLists + i);  // the lists ahead of it, as they were
			}
			lists.push_back(before);
		}
		if (!lists.empty())
		{
			lists.push_back(aLists[i]);
		}
	}
	return lists;
}
}
