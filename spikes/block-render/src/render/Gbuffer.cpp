#include "render/Gbuffer.h"

#include <d3d12.h>

namespace cpmc
{
namespace
{
// The G-buffer's render targets in order, as step 1 found them; the fourth, the motion vectors, only in some passes.
constexpr DXGI_FORMAT kTargetFormats[4] = { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM,
	                                        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16_FLOAT };

bool FirstMip(const ViewInfo& aView)
{
	return aView.mip == 0 && aView.firstSlice == 0;
}
}

std::optional<GbufferTargets> MainGbuffer(const Targets& aTargets, uint64_t aWidth, uint32_t aHeight)
{
	// The candidate rule checks there are three or more render targets and a depth buffer, all known and render-sized.
	if (!IsGbufferCandidate(aTargets, aWidth, aHeight) || aTargets.rtvs.size() > 4)
	{
		return std::nullopt;
	}
	for (size_t i = 0; i < aTargets.rtvs.size(); ++i)
	{
		const ViewInfo& view = *aTargets.rtvs[i].view;
		if (view.viewFormat != uint32_t(kTargetFormats[i]) || !FirstMip(view))
		{
			return std::nullopt;
		}
	}
	const ViewInfo& depth = *aTargets.dsv->view;
	if (depth.viewFormat != uint32_t(DXGI_FORMAT_D32_FLOAT_S8X24_UINT) || !FirstMip(depth))
	{
		return std::nullopt;
	}
	return GbufferTargets{ aTargets.rtvs[0].view->resource, aTargets.rtvs[1].view->resource, aTargets.rtvs[2].view->resource,
		                   depth.resource };
}

bool EndsGbuffer(const ListState& aList, const GbufferTargets& aGbuffer)
{
	const auto leaves = [&](void* aResource) {
		const std::optional<ListState::FirstTransition> first = aList.FirstTransitionOf(aResource);
		return first && first->subresource == ListState::kAllSubresources && (first->flags & ListState::kEndOnly) == 0 &&
		       first->before == uint32_t(D3D12_RESOURCE_STATE_RENDER_TARGET);
	};
	return leaves(aGbuffer.normals) && leaves(aGbuffer.material);
}

void KnownStates::Watch(const std::vector<void*>& aResources)
{
	states_.clear();
	for (void* resource : aResources)
	{
		states_.emplace_back(resource, std::nullopt);
	}
}

void KnownStates::Forget()
{
	for (auto& [resource, state] : states_)
	{
		state.reset();
	}
}

void KnownStates::Apply(const ListState& aList)
{
	for (auto& [resource, state] : states_)
	{
		if (aList.FirstTransitionOf(resource))
		{
			state = aList.StateOf(resource);
		}
	}
}

std::optional<uint32_t> KnownStates::Before(const ListState& aNext, void* aResource) const
{
	if (const std::optional<ListState::FirstTransition> first = aNext.FirstTransitionOf(aResource))
	{
		const bool whole = first->subresource == ListState::kAllSubresources && (first->flags & ListState::kEndOnly) == 0;
		return whole ? std::optional<uint32_t>(first->before) : std::nullopt;
	}
	for (const auto& [resource, state] : states_)
	{
		if (resource == aResource)
		{
			return state;
		}
	}
	return std::nullopt;
}

Vec3d FaceNormal(Face aFace)
{
	// Minecraft's directions in its own axes (x east, y up, z south), in Face's order.
	constexpr Vec3d kMinecraft[6] = { { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { -1, 0, 0 }, { 1, 0, 0 } };
	return WorldFromMc(kMinecraft[size_t(aFace)]);
}
}
