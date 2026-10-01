#pragma once

#include "Coords.h"
#include "track/StateTracker.h"
#include "world/Vertex.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

// Drawing into the G-buffer (step 4): the parts that need no GPU. Our list goes just before the list that ends the
// G-buffer, and draws into its targets as the game's own geometry does.
namespace cpmc
{
// The G-buffer's targets, as step 1 found them.
struct GbufferTargets
{
	void* base = nullptr;      // R10G10B10A2_UNORM: the base colour, sRGB-encoded
	void* normals = nullptr;   // R10G10B10A2_UNORM: world-space normals as n x 0.5 + 0.5; it has mip levels
	void* material = nullptr;  // R8G8B8A8_UNORM: metalness, roughness, a translucency amount and emission
	void* depth = nullptr;     // D32_FLOAT_S8X24_UINT: the depth, and a stencil class for each kind of surface

	bool operator==(const GbufferTargets&) const = default;
};

// The G-buffer, if aTargets are its targets at aWidth x aHeight, the render size (the tagged depth's): three render
// targets in its formats, maybe the motion vectors as a fourth, and its depth-stencil buffer, all views of mip 0.
std::optional<GbufferTargets> MainGbuffer(const Targets& aTargets, uint64_t aWidth, uint32_t aHeight);

// True when aList ends the G-buffer: its first transitions take the whole of the normals and of the material values
// out of RENDER_TARGET. The frame logs found one such list each frame, just after the G-buffer's draws.
bool EndsGbuffer(const ListState& aList, const GbufferTargets& aGbuffer);

// A few resources' states on the main queue between the lists submitted to it, as far as tracked lists show them.
class KnownStates
{
public:
	void Watch(const std::vector<void*>& aResources);  // these from now on, all unknown
	void Forget();                                     // all unknown: a new frame, or a list that wasn't tracked

	// aList was submitted after everything applied so far. Each watched resource it moves ends where its last
	// transition leaves it, which is unknown after a transition of one subresource or the first half of a split.
	void Apply(const ListState& aList);

	// A resource's state just before aNext: where aNext's first transition of it starts, if that covers the whole
	// resource and doesn't end a split barrier; if aNext doesn't move it, its known state; otherwise nothing.
	std::optional<uint32_t> Before(const ListState& aNext, void* aResource) const;

private:
	std::vector<std::pair<void*, std::optional<uint32_t>>> states_;
};

// The world-space normal of a block face: Minecraft's direction for it, in world axes (x east, y north, z up).
// shaders/blocks.hlsl's kFaceNormal holds the same.
Vec3d FaceNormal(Face aFace);
}
