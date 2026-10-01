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
	return BlockShaders{ std::move(*vertex), std::move(*present) };
}
}
