#pragma once

#include <cstdint>

namespace cpmc
{
// shaders/blocks.hlsl's constant buffers, byte for byte. tests/TestShaders.cpp checks the sizes against the compiled
// shader's own reflection.
struct FrameConstants
{
	float    cameraToClip[16];    // camera-relative world position to clip, for row vectors
	float    target[4];           // width, height, 1 / width, 1 / height of the image drawn into
	float    depthExtent[4];      // left, top, width, height of the depth image within its buffer
	uint32_t depthInfo[4];        // floats per row of the depth copy; 1 if there is a copy; 1 if depth is reversed; 0
	uint32_t atlasInfo[4];        // width, height, levels; 0
	uint32_t atlasLevels[8];      // where each level's texels start
	float    tuning[4];           // brightness; depth tolerance; exposure; the motion vectors' sign
	float    clipToPrevClip[16];  // Streamline's, for row vectors: a clip position to where it was a frame before
	float    jitter[4];           // the jitter as an offset per unit of clip w (ClipJitter): x, y; 0; 0
	float    material[4];         // the G-buffer's material values: metalness, roughness, B; 0
	float    entity[4];           // spike 7: a glowing triangle's emission (set glow); the block light it glows from (set
	                              // glowlight); the outline's width in render pixels (step B, OutlinePixels); 0
};
static_assert(sizeof(FrameConstants) == 288);

struct SectionConstants
{
	float offset[3];  // the section's minimum corner: camera-relative, world axes
	float unused;
};
static_assert(sizeof(SectionConstants) == 16);

// Spike 7: one entity draw's root constants, in b1 where the blocks have SectionConstants.
struct EntityConstants
{
	float    offset[3];      // the frame's origin: camera-relative, world axes
	uint32_t first;          // the draw's first vertex in the snapshot
	uint32_t textureStart;   // its texture's first texel in the entity textures' buffer
	uint32_t textureWidth;   // 0 for Minecraft's combined atlas
	uint32_t textureHeight;
	uint32_t unused;
};
static_assert(sizeof(EntityConstants) == 32);

// The HUD pass's root constants, in b1 where the blocks have SectionConstants.
struct HudConstants
{
	uint32_t width = 0;     // the image's
	uint32_t height = 0;
	uint32_t scale = 1;     // back-buffer pixels per image texel, each way
	uint32_t bottomUp = 0;  // 1 if its rows are stored bottom-up
};
static_assert(sizeof(HudConstants) == 16);
}
