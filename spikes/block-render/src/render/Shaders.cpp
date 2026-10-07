#include "render/Shaders.h"

#include <Windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace cpmc
{
std::optional<std::vector<uint8_t>> CompileShader(std::string_view aSource, const char* aName, const char* aEntry,
                                                  const char* aProfile, std::string& aError)
{
	Microsoft::WRL::ComPtr<ID3DBlob> code;
	Microsoft::WRL::ComPtr<ID3DBlob> messages;
	const HRESULT result = D3DCompile(aSource.data(), aSource.size(), aName, nullptr, nullptr, aEntry, aProfile,
	                                  D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &messages);
	if (FAILED(result) || !code)
	{
		aError = messages ? std::string(static_cast<const char*>(messages->GetBufferPointer()), messages->GetBufferSize())
		                  : "D3DCompile failed without saying why";
		while (!aError.empty() && (aError.back() == '\0' || aError.back() == '\n'))
		{
			aError.pop_back();
		}
		return std::nullopt;
	}
	const auto* bytes = static_cast<const uint8_t*>(code->GetBufferPointer());
	return std::vector<uint8_t>(bytes, bytes + code->GetBufferSize());
}

std::optional<BlockShaders> CompileBlockShaders(std::string_view aSource, std::string& aError)
{
	auto vertex = CompileShader(aSource, "blocks.hlsl", "BlocksVS", "vs_5_1", aError);
	if (!vertex)
	{
		return std::nullopt;
	}
	auto present = CompileShader(aSource, "blocks.hlsl", "PresentPS", "ps_5_1", aError);
	if (!present)
	{
		return std::nullopt;
	}
	auto predlss = CompileShader(aSource, "blocks.hlsl", "PredlssPS", "ps_5_1", aError);
	if (!predlss)
	{
		return std::nullopt;
	}
	auto gbuffer = CompileShader(aSource, "blocks.hlsl", "GbufferPS", "ps_5_1", aError);
	if (!gbuffer)
	{
		return std::nullopt;
	}
	auto depth = CompileShader(aSource, "blocks.hlsl", "DepthPS", "ps_5_1", aError);
	if (!depth)
	{
		return std::nullopt;
	}
	auto hudVertex = CompileShader(aSource, "blocks.hlsl", "HudVS", "vs_5_1", aError);
	if (!hudVertex)
	{
		return std::nullopt;
	}
	auto hud = CompileShader(aSource, "blocks.hlsl", "HudPS", "ps_5_1", aError);
	if (!hud)
	{
		return std::nullopt;
	}
	auto entityVertex = CompileShader(aSource, "blocks.hlsl", "EntityVS", "vs_5_1", aError);
	if (!entityVertex)
	{
		return std::nullopt;
	}
	auto entity = CompileShader(aSource, "blocks.hlsl", "EntityPS", "ps_5_1", aError);
	if (!entity)
	{
		return std::nullopt;
	}
	auto crack = CompileShader(aSource, "blocks.hlsl", "CrackPS", "ps_5_1", aError);
	if (!crack)
	{
		return std::nullopt;
	}
	auto entityDepth = CompileShader(aSource, "blocks.hlsl", "EntityDepthPS", "ps_5_1", aError);
	if (!entityDepth)
	{
		return std::nullopt;
	}
	auto translucent = CompileShader(aSource, "blocks.hlsl", "TranslucentPS", "ps_5_1", aError);
	if (!translucent)
	{
		return std::nullopt;
	}
	auto hint = CompileShader(aSource, "blocks.hlsl", "HintPS", "ps_5_1", aError);
	if (!hint)
	{
		return std::nullopt;
	}
	auto outlineVertex = CompileShader(aSource, "blocks.hlsl", "OutlineVS", "vs_5_1", aError);
	if (!outlineVertex)
	{
		return std::nullopt;
	}
	auto outline = CompileShader(aSource, "blocks.hlsl", "OutlinePS", "ps_5_1", aError);
	if (!outline)
	{
		return std::nullopt;
	}
	return BlockShaders{ std::move(*vertex), std::move(*present), std::move(*predlss), std::move(*gbuffer),
		                 std::move(*depth), std::move(*hudVertex), std::move(*hud), std::move(*entityVertex),
		                 std::move(*entity), std::move(*crack), std::move(*entityDepth), std::move(*translucent),
		                 std::move(*hint), std::move(*outlineVertex), std::move(*outline) };
}
}
