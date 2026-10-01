#include "Harness.h"

#include "render/Gbuffer.h"

#include <d3d12.h>

using cpmc::BoundTarget;
using cpmc::GbufferTargets;
using cpmc::KnownStates;
using cpmc::ListState;
using cpmc::Targets;
using cpmc::ViewInfo;

namespace
{
constexpr uint32_t kAll = ListState::kAllSubresources;

BoundTarget View(uintptr_t aResource, DXGI_FORMAT aFormat, ViewInfo::Kind aKind = ViewInfo::Kind::Rtv)
{
	ViewInfo view;
	view.kind = aKind;
	view.resource = reinterpret_cast<void*>(aResource);
	view.viewFormat = uint32_t(aFormat);
	view.width = 1707;
	view.height = 960;
	return { aResource * 100, view };
}

// The G-buffer's targets as the frame logs found them: the base colour is 1, the normals 2, the material values 3,
// the motion vectors 4 and the depth 5.
Targets Gbuffer(bool aMotion)
{
	Targets targets;
	targets.rtvs = { View(1, DXGI_FORMAT_R10G10B10A2_UNORM), View(2, DXGI_FORMAT_R10G10B10A2_UNORM),
		             View(3, DXGI_FORMAT_R8G8B8A8_UNORM) };
	if (aMotion)
	{
		targets.rtvs.push_back(View(4, DXGI_FORMAT_R16G16_FLOAT));
	}
	targets.dsv = View(5, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, ViewInfo::Kind::Dsv);
	return targets;
}

void* Resource(uintptr_t aResource)
{
	return reinterpret_cast<void*>(aResource);
}
}

TEST(GbufferIsFoundByItsTargetsFormats)
{
	const auto three = cpmc::MainGbuffer(Gbuffer(false), 1707, 960);
	CHECK(three.has_value());
	if (three)
	{
		CHECK(three->base == Resource(1) && three->normals == Resource(2) && three->material == Resource(3));
		CHECK(three->depth == Resource(5));
	}
	const auto four = cpmc::MainGbuffer(Gbuffer(true), 1707, 960);  // with the motion vectors
	CHECK(four.has_value() && three && *four == *three);
}

TEST(GbufferRefusesOtherTargets)
{
	CHECK(!cpmc::MainGbuffer(Gbuffer(false), 2560, 1440));  // not the render size
	Targets after = Gbuffer(false);  // the pass after the G-buffer that the frame logs also found
	after.rtvs = { View(6, DXGI_FORMAT_R16G16_SNORM), View(7, DXGI_FORMAT_R8_UNORM), View(8, DXGI_FORMAT_R8_UNORM) };
	CHECK(!cpmc::MainGbuffer(after, 1707, 960));
	Targets depth = Gbuffer(false);
	depth.dsv->view->viewFormat = uint32_t(DXGI_FORMAT_D32_FLOAT);
	CHECK(!cpmc::MainGbuffer(depth, 1707, 960));
	Targets mip = Gbuffer(false);
	mip.rtvs[1].view->mip = 1;
	CHECK(!cpmc::MainGbuffer(mip, 1707, 960));
	Targets five = Gbuffer(true);
	five.rtvs.push_back(View(9, DXGI_FORMAT_R8_UNORM));
	CHECK(!cpmc::MainGbuffer(five, 1707, 960));
	Targets unknown = Gbuffer(false);
	unknown.rtvs[0].view.reset();
	CHECK(!cpmc::MainGbuffer(unknown, 1707, 960));
}

TEST(GbufferEndsWhereTheNormalsAndMaterialLeaveRenderTarget)
{
	const GbufferTargets gbuffer = *cpmc::MainGbuffer(Gbuffer(false), 1707, 960);
	ListState end;  // the one-list submission after the G-buffer
	end.Begin(1, false);
	end.Transition(Resource(4), 0x4, 0x40, kAll, 0);   // the motion vectors
	end.Transition(Resource(3), 0x4, 0x40, kAll, 0);   // the material values
	end.Transition(Resource(2), 0x4, 0x40, kAll, 0);   // the normals
	end.Transition(Resource(5), 0x10, 0x40, kAll, 0);  // the depth
	CHECK(cpmc::EndsGbuffer(end, gbuffer));

	ListState start;  // the G-buffer's first list moves them into RENDER_TARGET
	start.Begin(1, false);
	start.Transition(Resource(2), 0x40, 0x4, kAll, 0);
	start.Transition(Resource(3), 0x40, 0x4, kAll, 0);
	CHECK(!cpmc::EndsGbuffer(start, gbuffer));

	ListState mips;  // a G-buffer list that moves only the normals' first mip
	mips.Begin(1, false);
	mips.Transition(Resource(2), 0x4, 0x8C0, 0, 0);
	mips.Transition(Resource(3), 0x4, 0x40, kAll, 0);
	CHECK(!cpmc::EndsGbuffer(mips, gbuffer));

	ListState normalsOnly;
	normalsOnly.Begin(1, false);
	normalsOnly.Transition(Resource(2), 0x4, 0x40, kAll, 0);
	CHECK(!cpmc::EndsGbuffer(normalsOnly, gbuffer));

	ListState split;  // ends split barriers begun in an earlier list
	split.Begin(1, false);
	split.Transition(Resource(2), 0x4, 0x40, kAll, ListState::kEndOnly);
	split.Transition(Resource(3), 0x4, 0x40, kAll, ListState::kEndOnly);
	CHECK(!cpmc::EndsGbuffer(split, gbuffer));
}

TEST(KnownStatesFollowTheListsAsTheyAreSubmitted)
{
	KnownStates states;
	states.Watch({ Resource(1), Resource(2) });
	ListState first;  // the G-buffer's first list: the base colour and the normals into RENDER_TARGET
	first.Begin(1, false);
	first.Transition(Resource(1), 0x40, 0x4, kAll, 0);
	first.Transition(Resource(2), 0x40, 0x4, kAll, 0);
	first.Transition(Resource(9), 0x0, 0x4, kAll, 0);  // not watched
	ListState mips;  // the normals' first mip moves on its own
	mips.Begin(1, false);
	mips.Transition(Resource(2), 0x4, 0x8C0, 0, 0);
	ListState end;  // moves the whole of the normals out, and leaves the base colour
	end.Begin(1, false);
	end.Transition(Resource(2), 0x4, 0x40, kAll, 0);
	ListState quiet;
	quiet.Begin(1, false);

	CHECK(!states.Before(quiet, Resource(1)));  // nothing known yet
	states.Apply(first);
	CHECK_EQ(states.Before(quiet, Resource(1)).value_or(0), 0x4u);
	CHECK_EQ(states.Before(quiet, Resource(2)).value_or(0), 0x4u);
	CHECK(!states.Before(quiet, Resource(9)));  // not watched
	states.Apply(mips);
	CHECK(!states.Before(quiet, Resource(2)));                    // one subresource moved: the whole is unknown
	CHECK_EQ(states.Before(end, Resource(2)).value_or(0), 0x4u);  // but the next list says where it starts
	CHECK_EQ(states.Before(end, Resource(1)).value_or(0), 0x4u);  // and doesn't move the base colour
	states.Forget();
	CHECK(!states.Before(end, Resource(1)));
	CHECK_EQ(states.Before(end, Resource(2)).value_or(0), 0x4u);  // still known from the list itself
}

TEST(GbufferNormalsAreMinecraftsDirectionsInWorldAxes)
{
	const auto is = [](cpmc::Face aFace, double aX, double aY, double aZ) {
		const cpmc::Vec3d normal = cpmc::FaceNormal(aFace);
		return normal.x == aX && normal.y == aY && normal.z == aZ;
	};
	CHECK(is(cpmc::Face::Down, 0, 0, -1));
	CHECK(is(cpmc::Face::Up, 0, 0, 1));
	CHECK(is(cpmc::Face::North, 0, 1, 0));  // world y is north: Minecraft's -z
	CHECK(is(cpmc::Face::South, 0, -1, 0));
	CHECK(is(cpmc::Face::West, -1, 0, 0));
	CHECK(is(cpmc::Face::East, 1, 0, 0));
}
