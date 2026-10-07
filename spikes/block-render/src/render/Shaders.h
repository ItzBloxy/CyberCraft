#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// Compiles entry point aEntry of HLSL source aSource for aProfile (for example "vs_5_1") with D3DCompile, which
// System32's D3DCompiler_47.dll provides. aName labels the source in messages. Nothing, with the compiler's messages
// in aError, if it doesn't compile.
std::optional<std::vector<uint8_t>> CompileShader(std::string_view aSource, const char* aName, const char* aEntry,
                                                  const char* aProfile, std::string& aError);

// The shaders the plugin draws with.
struct BlockShaders
{
	std::vector<uint8_t> vertex;     // BlocksVS
	std::vector<uint8_t> present;    // PresentPS
	std::vector<uint8_t> predlss;    // PredlssPS
	std::vector<uint8_t> gbuffer;    // GbufferPS
	std::vector<uint8_t> depth;      // DepthPS
	std::vector<uint8_t> hudVertex;  // HudVS
	std::vector<uint8_t> hud;        // HudPS
	std::vector<uint8_t> entityVertex;  // EntityVS (spike 7)
	std::vector<uint8_t> entity;        // EntityPS
	std::vector<uint8_t> crack;         // CrackPS
	std::vector<uint8_t> entityDepth;   // EntityDepthPS
	std::vector<uint8_t> translucent;    // TranslucentPS (step B)
	std::vector<uint8_t> hint;           // HintPS
	std::vector<uint8_t> outlineVertex;  // OutlineVS
	std::vector<uint8_t> outline;        // OutlinePS
};

// Every entry point of shaders/blocks.hlsl, from its text. Nothing, with the messages in aError, if any fails.
std::optional<BlockShaders> CompileBlockShaders(std::string_view aSource, std::string& aError);
}
