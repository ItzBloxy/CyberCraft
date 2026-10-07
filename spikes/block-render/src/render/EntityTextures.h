#pragma once

#include <cstdint>
#include <map>
#include <optional>

// Spike 7: where Minecraft's entity textures go in the renderer's one buffer (the spec's "Entity textures on the GPU"):
// each after the last, one uint per texel as the atlas has them, 64 MB in all at most. A texture is placed as it comes,
// before its upload, so the cap counts those waiting for drawing to come on too. Pure; the renderer locks around it.
namespace cpmc
{
class EntityTextures
{
public:
	static constexpr uint64_t kCapTexels = (64ull << 20) / 4;   // 64 MB
	static constexpr uint64_t kFirstTexels = (4ull << 20) / 4;  // the buffer's first size, 4 MB

	struct Place
	{
		uint32_t start = 0;  // its first texel in the buffer
		uint32_t width = 0;
		uint32_t height = 0;
	};

	// Texture aId placed after the last: nothing if it would take the textures past 64 MB in all. An id placed again
	// gets a new place, its old one unused until a clear.
	std::optional<Place> Add(uint32_t aId, uint32_t aWidth, uint32_t aHeight);
	const Place*         Find(uint32_t aId) const;
	void                 Clear();  // Minecraft's clear: every place goes, and the next texture starts the buffer again

	uint64_t Used() const { return used_; }  // texels placed
	size_t   Count() const { return places_.size(); }
	// The buffer's size in texels to hold what's placed, from aCurrent's: aCurrent if it holds it, else the first of
	// 4 MB doubled that does, at most 64 MB.
	uint64_t Capacity(uint64_t aCurrent) const;

private:
	std::map<uint32_t, Place> places_;
	uint64_t                  used_ = 0;
};
}
