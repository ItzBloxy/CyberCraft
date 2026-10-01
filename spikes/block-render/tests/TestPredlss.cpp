#include "Harness.h"

#include "render/Predlss.h"

#include <string>

using cpmc::ListState;
using cpmc::Move;

namespace
{
D3D12_RESOURCE_DESC Texture(DXGI_FORMAT aFormat, D3D12_RESOURCE_FLAGS aFlags)
{
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = 1707;
	desc.Height = 960;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = aFormat;
	desc.SampleDesc.Count = 1;
	desc.Flags = aFlags;
	return desc;
}

// The tags step 1 found.
const D3D12_RESOURCE_DESC kColour = Texture(DXGI_FORMAT_R16G16B16A16_FLOAT,
                                            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
const D3D12_RESOURCE_DESC kMotion = Texture(DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
const D3D12_RESOURCE_DESC kDepth = Texture(DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

bool Refused(const D3D12_RESOURCE_DESC& aColour, const D3D12_RESOURCE_DESC& aMotion, const D3D12_RESOURCE_DESC& aDepth,
             const char* aWhy)
{
	std::string why;
	return !cpmc::CheckPredlssTargets(aColour, aMotion, aDepth, why) && why.find(aWhy) != std::string::npos;
}
}

TEST(PredlssTakesTheTagsStep1Found)
{
	std::string why;
	CHECK(cpmc::CheckPredlssTargets(kColour, kMotion, kDepth, why));
	CHECK_STR(why, "");
	D3D12_RESOURCE_DESC typedDepth = kDepth;
	typedDepth.Format = DXGI_FORMAT_D32_FLOAT;
	CHECK(cpmc::CheckPredlssTargets(kColour, kMotion, typedDepth, why));
}

TEST(PredlssRefusesOtherTargets)
{
	D3D12_RESOURCE_DESC computeOnly = kColour;
	computeOnly.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	CHECK(Refused(computeOnly, kMotion, kDepth, "the colour isn't a render target"));
	D3D12_RESOURCE_DESC otherFormat = kMotion;
	otherFormat.Format = DXGI_FORMAT_R32G32_FLOAT;
	CHECK(Refused(kColour, otherFormat, kDepth, "the motion vectors are R32G32_FLOAT"));
	D3D12_RESOURCE_DESC noDepth = kDepth;
	noDepth.Flags = D3D12_RESOURCE_FLAG_NONE;
	CHECK(Refused(kColour, kMotion, noDepth, "the depth isn't a depth buffer"));
	D3D12_RESOURCE_DESC smaller = kDepth;
	smaller.Width = 853;
	CHECK(Refused(kColour, kMotion, smaller, "sizes differ"));
	D3D12_RESOURCE_DESC mipped = kColour;
	mipped.MipLevels = 2;
	CHECK(Refused(mipped, kMotion, kDepth, "the colour has more than one subresource"));
}

TEST(PredlssViewsTheDepthAsD32)
{
	CHECK(cpmc::DepthViewFormat(DXGI_FORMAT_R32_TYPELESS) == DXGI_FORMAT_D32_FLOAT);
	CHECK(cpmc::DepthViewFormat(DXGI_FORMAT_D32_FLOAT) == DXGI_FORMAT_D32_FLOAT);
	CHECK(cpmc::DepthViewFormat(DXGI_FORMAT_R32G8X24_TYPELESS) == DXGI_FORMAT_UNKNOWN);
}

TEST(PredlssFindsEachBuffersStateBeforeTheDlssList)
{
	constexpr uint32_t all = ListState::kAllSubresources;
	ListState list;
	list.Begin(1, false);
	int colour = 0, depth = 0, motion = 0, part = 0, split = 0;
	list.Transition(&colour, 0x4, 0x8, all, 0);  // render target, to unordered access for the evaluate
	list.Transition(&depth, 0x10, 0xC0, 0, 0);   // subresource 0, the only one a tagged buffer has
	list.Transition(&part, 0x4, 0x8, 1, 0);      // another subresource only
	list.Transition(&split, 0x4, 0x8, all, ListState::kEndOnly);  // ends a split begun in an earlier list
	CHECK_EQ(cpmc::StateBeforeList(list, &colour, 0x8).value_or(0), 0x4u);
	CHECK_EQ(cpmc::StateBeforeList(list, &depth, 0xC0).value_or(0), 0x10u);
	CHECK_EQ(cpmc::StateBeforeList(list, &motion, 0xC0).value_or(0), 0xC0u);  // never moved: as tagged
	CHECK(!cpmc::StateBeforeList(list, &motion, 0xFFFFFFFF));               // never moved, and the tag didn't say
	CHECK(!cpmc::StateBeforeList(list, &part, 0x8));
	CHECK(!cpmc::StateBeforeList(list, &split, 0x8));
}

TEST(PredlssMovesOnlyWhatIsntThereAndPutsItBack)
{
	int colour = 0, motion = 0, depth = 0;
	const std::vector<Move> into = cpmc::MovesInto({ { &colour, 0x8, 0x4 }, { &motion, 0x4, 0x4 }, { &depth, 0xC0, 0x10 } });
	CHECK_EQ(into.size(), size_t(2));  // the motion vectors are already a render target
	CHECK(into[0].resource == &colour && into[0].before == 0x8 && into[0].after == 0x4);
	CHECK(into[1].resource == &depth && into[1].before == 0xC0 && into[1].after == 0x10);
	const std::vector<Move> back = cpmc::MovesBack(into);
	CHECK_EQ(back.size(), size_t(2));
	CHECK(back[0].resource == &depth && back[0].before == 0x10 && back[0].after == 0xC0);
	CHECK(back[1].resource == &colour && back[1].before == 0x4 && back[1].after == 0x8);
	const std::vector<D3D12_RESOURCE_BARRIER> barriers = cpmc::Barriers(into);
	CHECK_EQ(barriers.size(), size_t(2));
	CHECK(barriers[1].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
	CHECK(barriers[1].Transition.pResource == reinterpret_cast<ID3D12Resource*>(&depth));
	CHECK(barriers[1].Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	CHECK(barriers[1].Transition.StateBefore == D3D12_RESOURCE_STATES(0xC0));
	CHECK(barriers[1].Transition.StateAfter == D3D12_RESOURCE_STATE_DEPTH_WRITE);
}
