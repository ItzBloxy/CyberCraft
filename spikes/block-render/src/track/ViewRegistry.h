#pragma once

#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace cpmc
{
// What a render-target or depth-stencil view was made from.
struct ViewInfo
{
	enum class Kind : uint8_t
	{
		Rtv,
		Dsv,
	};

	Kind     kind = Kind::Rtv;
	void*    resource = nullptr;  // the ID3D12Resource; not referenced
	uint32_t viewFormat = 0;      // the view's DXGI_FORMAT, or the resource's own when the view gave none
	uint32_t resourceFormat = 0;
	uint64_t width = 0;  // the resource's size
	uint32_t height = 0;
	uint32_t resourceFlags = 0;  // D3D12_RESOURCE_FLAGS
	uint32_t mip = 0;
	uint32_t plane = 0;
	uint32_t firstSlice = 0;
	bool     readOnlyDepth = false;
	bool     readOnlyStencil = false;
};

// aCount descriptors copied from aSource onwards to aDest onwards.
struct DescriptorRun
{
	uint64_t dest = 0;
	uint64_t source = 0;
	uint32_t count = 0;
};

// Splits one CopyDescriptors call into runs that are contiguous on both sides. Destination and source ranges needn't
// line up; null sizes mean ranges of one descriptor each, as in D3D12. aIncrement is the heap type's handle step.
std::vector<DescriptorRun> CopyRuns(uint32_t aDestRanges, const uint64_t* aDestStarts, const uint32_t* aDestSizes,
                                    uint32_t aSourceRanges, const uint64_t* aSourceStarts, const uint32_t* aSourceSizes,
                                    uint32_t aIncrement);

// RTV and DSV descriptor handles (D3D12_CPU_DESCRIPTOR_HANDLE::ptr) mapped to what their views were made from, as the
// device creates and copies them. Thread-safe.
class ViewRegistry
{
public:
	void     SetIncrements(uint32_t aRtv, uint32_t aDsv);
	uint32_t Increment(ViewInfo::Kind aKind) const;

	void Record(uint64_t aHandle, const ViewInfo& aView);

	// Copies the views of aCount descriptors from aSource onwards to aDest onwards. A source with no known view
	// leaves its destination unknown too.
	void Copy(ViewInfo::Kind aKind, uint64_t aDest, uint64_t aSource, uint32_t aCount);

	std::optional<ViewInfo> Find(uint64_t aHandle) const;
	size_t                  Count(ViewInfo::Kind aKind) const;

private:
	mutable std::shared_mutex              mutex_;
	std::unordered_map<uint64_t, ViewInfo> views_;
	uint32_t                               rtvIncrement_ = 0;
	uint32_t                               dsvIncrement_ = 0;
};
}
