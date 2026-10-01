#include "Harness.h"

#include "render/FrameConstants.h"
#include "render/Shaders.h"

#include <Windows.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <fstream>
#include <sstream>

namespace
{
// The tests run in spikes/block-render, so the real shader file is here.
std::string ShaderSource()
{
	std::ifstream     file("shaders/blocks.hlsl", std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

UINT ConstantBufferSize(const std::vector<uint8_t>& aCode, const char* aName)
{
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
	if (FAILED(D3DReflect(aCode.data(), aCode.size(), IID_PPV_ARGS(&reflection))))
	{
		return 0;
	}
	D3D12_SHADER_BUFFER_DESC desc{};
	if (FAILED(reflection->GetConstantBufferByName(aName)->GetDesc(&desc)))
	{
		return 0;
	}
	return desc.Size;
}
}

TEST(ShadersCompileFromTheShaderFile)
{
	const std::string source = ShaderSource();
	CHECK(!source.empty());
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(source, error);
	CHECK_STR(error, "");
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK(!shaders->vertex.empty());
		CHECK(!shaders->present.empty());
		// The constant buffers match the C++ structs byte for byte.
		CHECK_EQ(ConstantBufferSize(shaders->vertex, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->vertex, "Section"), UINT(sizeof(cpmc::SectionConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->present, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
	}
}

TEST(ShadersReportCompileErrors)
{
	std::string error;
	CHECK(!cpmc::CompileShader("float4 Broken() : SV_Target0 { return nope; }", "broken.hlsl", "Broken", "ps_5_1", error));
	CHECK(error.find("broken.hlsl(1") != std::string::npos);
	CHECK(error.find("nope") != std::string::npos);
}
