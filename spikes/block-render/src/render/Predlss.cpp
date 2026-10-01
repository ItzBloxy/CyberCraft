#include "render/Predlss.h"

#include "log/FrameLogData.h"

#include <format>

namespace cpmc
{
namespace
{
// Why aDesc isn't a plain 2D texture in aFormat that allows aFlag, or nothing if it is. aName takes aVerb: "is" or
// "are".
std::string Problem(const char* aName, const char* aVerb, const D3D12_RESOURCE_DESC& aDesc, DXGI_FORMAT aFormat,
                    D3D12_RESOURCE_FLAGS aFlag, const char* aRole)
{
	const std::string_view verb = aVerb;
	if (aDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		return std::format("the {} {}n't a 2D texture", aName, verb);
	}
	if (aDesc.Format != aFormat && !(aFormat == DXGI_FORMAT_R32_TYPELESS && aDesc.Format == DXGI_FORMAT_D32_FLOAT))
	{
		return std::format("the {} {} {}", aName, verb, DxgiFormatName(aDesc.Format));
	}
	if ((aDesc.Flags & aFlag) == 0)
	{
		return std::format("the {} {}n't {}", aName, verb, aRole);
	}
	if (aDesc.MipLevels != 1 || aDesc.DepthOrArraySize != 1 || aDesc.SampleDesc.Count != 1)
	{
		return std::format("the {} {} more than one subresource", aName, verb == "is" ? "has" : "have");
	}
	return {};
}
}

bool CheckPredlssTargets(const D3D12_RESOURCE_DESC& aColour, const D3D12_RESOURCE_DESC& aMotion, const D3D12_RESOURCE_DESC& aDepth,
                         std::string& aWhy)
{
	aWhy = Problem("colour", "is", aColour, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, "a render target");
	if (aWhy.empty())
	{
		aWhy = Problem("motion vectors", "are", aMotion, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		               "a render target");
	}
	if (aWhy.empty())
	{
		aWhy = Problem("depth", "is", aDepth, DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, "a depth buffer");
	}
	if (aWhy.empty() && (aMotion.Width != aColour.Width || aMotion.Height != aColour.Height || aDepth.Width != aColour.Width ||
	                     aDepth.Height != aColour.Height))
	{
		aWhy = std::format("the tagged buffers' sizes differ: colour {} x {}, motion vectors {} x {}, depth {} x {}", aColour.Width,
		                   aColour.Height, aMotion.Width, aMotion.Height, aDepth.Width, aDepth.Height);
	}
	return aWhy.empty();
}

DXGI_FORMAT DepthViewFormat(DXGI_FORMAT aFormat)
{
	return aFormat == DXGI_FORMAT_R32_TYPELESS || aFormat == DXGI_FORMAT_D32_FLOAT ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
}

std::optional<uint32_t> StateBeforeList(const ListState& aList, void* aResource, uint32_t aTaggedState)
{
	const std::optional<ListState::FirstTransition> first = aList.FirstTransitionOf(aResource);
	if (!first)
	{
		return aTaggedState == ListState::kUnknownState ? std::nullopt : std::optional<uint32_t>(aTaggedState);
	}
	const bool wholeBuffer = first->subresource == ListState::kAllSubresources || first->subresource == 0;
	if (!wholeBuffer || (first->flags & ListState::kEndOnly) != 0)
	{
		return std::nullopt;
	}
	return first->before;
}

std::vector<Move> MovesInto(const std::vector<Move>& aWanted)
{
	std::vector<Move> moves;
	for (const Move& move : aWanted)
	{
		if (move.before != move.after)
		{
			moves.push_back(move);
		}
	}
	return moves;
}

std::vector<Move> MovesBack(const std::vector<Move>& aMoves)
{
	std::vector<Move> back;
	for (auto move = aMoves.rbegin(); move != aMoves.rend(); ++move)
	{
		back.push_back({ move->resource, move->after, move->before });
	}
	return back;
}

std::vector<D3D12_RESOURCE_BARRIER> Barriers(const std::vector<Move>& aMoves)
{
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	for (const Move& move : aMoves)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = static_cast<ID3D12Resource*>(move.resource);
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATES(move.before);
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATES(move.after);
		barriers.push_back(barrier);
	}
	return barriers;
}
}
