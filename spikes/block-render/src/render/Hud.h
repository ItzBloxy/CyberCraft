#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

// The HUD pass's rules (the spec's "The HUD"): when Minecraft's HUD image is wanted, the size to ask Minecraft for,
// which texel of the image each back-buffer pixel shows, and how images pass through the three upload buffers.
// shaders/blocks.hlsl's HudPS does HudTexel's arithmetic, so the tests here pin the shader's too.
namespace cpmc
{
// Whether to take Minecraft's HUD images, and draw the newest. aSetting is the `hud` setting: 0 never, 1 in build mode
// (the default), 2 always. Either way only while Minecraft is in its world.
bool HudWanted(int aSetting, bool aBuildMode, bool aMinecraftInWorld);

struct HudSize
{
	uint32_t width = 0;
	uint32_t height = 0;
};

// The image to ask Minecraft for (SkyState's viewport): the back buffer's size over aScale (1, 2 or 4), rounded down, and
// at most an overlay slot's 3840 by 2160. 0 by 0, which leaves Minecraft's window alone, before the back buffer is known
// or for another scale.
HudSize HudViewport(uint32_t aBackWidth, uint32_t aBackHeight, uint32_t aScale);

// The scale an image is drawn at: back-buffer pixels per image texel, each way, as many as fit (at least 1). From the
// image itself, so an image Minecraft drew at the size asked for before `hudscale` changed is still drawn right.
uint32_t HudScaleFor(uint32_t aBackWidth, uint32_t aBackHeight, uint32_t aWidth, uint32_t aHeight);

// The image texel back-buffer pixel (aX, aY) shows, as an index into the image's rows as they're stored: aScale pixels a
// texel from the top left, and rows flipped when they're stored bottom-up. Nothing outside the image, where the pass
// shows nothing.
std::optional<uint32_t> HudTexel(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, uint32_t aScale, bool aBottomUp);

// Spike 8: the cursor's arrow, SkyCraft's, which the HUD pass draws over the image while a Minecraft screen is open in
// build mode (its spec's "Drawing it"). Its height in back-buffer pixels: 18 at 1,080 lines, scaled with the back
// buffer's height and rounded, at least 1.
uint32_t CursorHeight(uint32_t aBackHeight);

enum class CursorPart
{
	None,
	Edge,  // black
	Fill,  // white
};

// The part of the arrow, its tip at (aTipX, aTipY) and aHeight pixels tall (0: no cursor), at back-buffer pixel (aX, aY),
// judged at the pixel's centre: inside where the slope 0.6 a pixel down leaves it, its edges 1.5 pixels wide at 18
// pixels tall and as much wider as it's taller. HudPS does the same arithmetic.
CursorPart CursorAt(uint32_t aX, uint32_t aY, uint32_t aTipX, uint32_t aTipY, uint32_t aHeight);

// Where the cursor is, for the HUD pass: the router writes it as it moves and as screens open and close, and the pass reads
// it at Present. Its spot is one atomic, so x and y never tear.
class HudCursor
{
public:
	struct Spot
	{
		uint32_t x = 0;  // back-buffer pixels, below 2^31 each
		uint32_t y = 0;
		bool     shown = false;
	};

	void Set(const Spot& aSpot);
	Spot Get() const;

private:
	std::atomic<uint64_t> packed_{ 0 };  // x in bits 0 to 30, y in 31 to 61, shown in 62
};

// Which of the three upload buffers each image goes through. The main thread copies an image into a slot the GPU has
// finished with, and it waits there as the newest. The HUD pass takes the newest, once, and the GPU reads it until its
// fence. An image never taken is replaced by the next, as in Minecraft's own triple buffer. Not thread-safe: the
// renderer holds its lock around each call, though not while the main thread copies.
class HudSlots
{
public:
	static constexpr uint32_t kSlots = 3;

	// A slot to copy the next image into: a free one, or else the waiting newest, which the next image replaces. Nothing
	// if the GPU still reads the rest. aCompleted is the renderer's completed fence value.
	std::optional<uint32_t> BeginWrite(uint64_t aCompleted);
	void                    EndWrite(uint32_t aSlot);  // the slot is the newest now
	void                    Abandon(uint32_t aSlot);   // a write that failed: the slot is free again
	std::optional<uint32_t> TakeNewest();              // for the pass: the newest, once
	void                    Reading(uint32_t aSlot, uint64_t aFence);  // the GPU reads the slot taken until aFence

private:
	enum class State
	{
		Free,
		Writing,
		Newest,
		Reading,
	};

	std::array<State, kSlots>    state_{};
	std::array<uint64_t, kSlots> fence_{};
};
}
