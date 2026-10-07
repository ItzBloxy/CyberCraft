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

UINT Outputs(const std::vector<uint8_t>& aCode)
{
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
	D3D12_SHADER_DESC                              desc{};
	if (FAILED(D3DReflect(aCode.data(), aCode.size(), IID_PPV_ARGS(&reflection))) || FAILED(reflection->GetDesc(&desc)))
	{
		return 0;
	}
	return desc.OutputParameters;
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
		CHECK(!shaders->predlss.empty());
		CHECK(!shaders->gbuffer.empty());
		CHECK(!shaders->depth.empty());
		// The constant buffers match the C++ structs byte for byte.
		CHECK_EQ(ConstantBufferSize(shaders->vertex, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->vertex, "Section"), UINT(sizeof(cpmc::SectionConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->present, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->predlss, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->gbuffer, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->depth, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK(!shaders->hudVertex.empty());
		CHECK(!shaders->hud.empty());
		CHECK_EQ(ConstantBufferSize(shaders->hud, "Hud"), UINT(sizeof(cpmc::HudConstants)));
	}
}

TEST(ShadersWriteColourAndMotionBeforeDlss)
{
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(ShaderSource(), error);
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK_EQ(Outputs(shaders->present), 1u);
		CHECK_EQ(Outputs(shaders->predlss), 2u);  // the colour, and the motion vectors
	}
}

TEST(ShadersWriteTheGbuffer)
{
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(ShaderSource(), error);
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK_EQ(Outputs(shaders->gbuffer), 4u);  // the base colour, normals, material values and motion vectors
		CHECK_EQ(Outputs(shaders->depth), 0u);    // depth only
	}
}

TEST(ShadersDrawTheHud)
{
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(ShaderSource(), error);
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK_EQ(Outputs(shaders->hud), 1u);  // the back buffer
	}
}

TEST(ShadersDrawMinecraftsEntities)
{
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(ShaderSource(), error);
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK(!shaders->entityVertex.empty());
		CHECK_EQ(ConstantBufferSize(shaders->entityVertex, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->entityVertex, "Entity"), UINT(sizeof(cpmc::EntityConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->entity, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->entity, "Entity"), UINT(sizeof(cpmc::EntityConstants)));
		CHECK_EQ(Outputs(shaders->entity), 4u);       // the G-buffer's four targets, as GbufferPS
		CHECK_EQ(Outputs(shaders->crack), 1u);        // the base colour alone
		CHECK_EQ(Outputs(shaders->entityDepth), 0u);  // depth only
	}
}

TEST(ShadersDrawStepBsForwardPass)
{
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(ShaderSource(), error);
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK(!shaders->outlineVertex.empty());
		CHECK_EQ(ConstantBufferSize(shaders->outlineVertex, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->outlineVertex, "Entity"), UINT(sizeof(cpmc::EntityConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->translucent, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->translucent, "Entity"), UINT(sizeof(cpmc::EntityConstants)));
		CHECK_EQ(Outputs(shaders->translucent), 1u);  // the scene's colour
		CHECK_EQ(Outputs(shaders->hint), 1u);         // the bias hint
		CHECK_EQ(Outputs(shaders->outline), 1u);      // the scene's colour
	}
}

TEST(ShadersReportCompileErrors)
{
	std::string error;
	CHECK(!cpmc::CompileShader("float4 Broken() : SV_Target0 { return nope; }", "broken.hlsl", "Broken", "ps_5_1", error));
	CHECK(error.find("broken.hlsl(1") != std::string::npos);
	CHECK(error.find("nope") != std::string::npos);
}
