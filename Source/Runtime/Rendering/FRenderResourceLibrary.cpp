#include "FRenderResourceLibrary.h"
#include "Runtime/Resource/FResourceLoader.h"

#include "FRenderer.h"
#include "FTexture.h"
#include <d3dcompiler.h>
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/Log.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Material/FBlendDesc.h"
#include "Runtime/Engine/FJsonArchive.h"
#include "Runtime/Utility/EngineUtil.h"
#include "ThirdParty/Json/json.hpp"

#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/stb/stb_image.h"

FRenderResourceLibrary& FRenderResourceLibrary::Get()
{
	static FRenderResourceLibrary Instance;
	return Instance;
}

bool FRenderResourceLibrary::CreateWireframePipeline(FRenderer& Renderer)
{
	const FWString Path = EngineUtil::GetContentDirectory();
	const FWString VsPath = Path + L"/Shader/ExampleVS.cso";
	const FWString PsPath = Path + L"/Shader/ExamplePS.cso";

	if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath))
	{
		return false;
	}

	FRenderPipelineDesc Desc = {
		.VertexShaderFilePath = std::filesystem::path(VsPath).string(),
		.PixelShaderFilePath = std::filesystem::path(PsPath).string(),
	};

	TSharedPtr<FRenderPipeline> WireframePipeline =
		Renderer.CreateRenderPipeline(Desc, EViewModeIndex::VMI_Wireframe);
	if (WireframePipeline)
	{
		AllPipelineMap[FName("#Simple_Wireframe")] = WireframePipeline;
	}

	return WireframePipeline != nullptr;
}

bool FRenderResourceLibrary::CreateOutlinePipeline(FRenderer& Renderer)
{
	/*const FWString Path = EngineUtil::GetContentDirectory();
	const FWString VsPath = Path + L"/Shader/ExampleVS.cso";
	const FWString PsPath = Path + L"/Shader/ExamplePS.cso";

	if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath))
	{
		return false;
	}

	FRenderPipelineDesc Desc = {
		.VertexShaderFilePath = std::filesystem::path(VsPath).string(),
		.PixelShaderFilePath = std::filesystem::path(PsPath).string(),
		.Blend = { EBlendMode::Translucent },
		.Rasterizer = FRasterizerDesc{},
		.DepthStencil = FDepthStencilDesc{false, true, EDepthWriteMode::Disable},
		.bIsInstancing = false,
		.bCreateInputLayout = false,
		.Sampler = FTextureSamplerDesc{ETextureSamplerFilterMode::Bilinear, ETextureSamplerWrapMode::Wrap}
	};

	TSharedPtr<FRenderPipeline> PPPipeline = CreateRenderPipeline(Renderer, Desc);
	if (PPPipeline)
	{
		AllPipelineMap[FName("#Outline")] = std::move(PPPipeline);
	}

	return AllPipelineMap[FName("#Outline")] != nullptr;*/


	//TODO 위쪽 코드로 교체하고 싶은데 아웃라인이 제대로 동작하지 않음.
	//나중에 좀 더 꼼꼼히 살펴보기 위쪽 코드로 적용할 것



	ID3D11Device* Device = Renderer.GetDevice();
	if (!Device)
	{
		return false;
	}

	const FWString Path = EngineUtil::GetContentDirectory();
	const FWString VsPath = Path + L"/Shader/ExampleVS.cso";
	const FWString PsPath = Path + L"/Shader/ExamplePS.cso";

	if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> SamplerState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> BlendState;

	// 버텍스 셰이더 로드 및 생성
	Microsoft::WRL::ComPtr<ID3DBlob> Blob;
	HRESULT Result = D3DReadFileToBlob(VsPath.c_str(), &Blob);
	if (FAILED(Result))
	{
		return false;
	}

	Result = Device->CreateVertexShader(Blob->GetBufferPointer(),
										Blob->GetBufferSize(), nullptr,
										&VertexShader);
	if (FAILED(Result))
	{
		return false;
	}

	// 입력 레이아웃 생성
	Result = Device->CreateInputLayout(FVertexLayouts::Layout,
									   FVertexLayouts::NumElements,
									   Blob->GetBufferPointer(),
									   Blob->GetBufferSize(),
									   &InputLayout);
	if (FAILED(Result))
	{
		return false;
	}

	// 픽셀 셰이더 로드 및 생성
	Result = D3DReadFileToBlob(PsPath.c_str(), &Blob);
	if (FAILED(Result))
	{
		return false;
	}

	Result = Device->CreatePixelShader(Blob->GetBufferPointer(),
									   Blob->GetBufferSize(), nullptr,
									   &PixelShader);
	if (FAILED(Result))
	{
		return false;
	}

	// 래스터라이저 상태 생성
	D3D11_RASTERIZER_DESC RasterizerDesc{
		.FillMode = D3D11_FILL_SOLID,
		.CullMode = D3D11_CULL_NONE,
		.FrontCounterClockwise = false,
	};
	Result = Device->CreateRasterizerState(&RasterizerDesc, &RasterizerState);
	if (FAILED(Result))
	{
		return false;
	}

	// 스텐실 마스크 기록 설정
	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc{};
	DepthStencilDesc.DepthEnable = FALSE;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	DepthStencilDesc.StencilEnable = TRUE;
	DepthStencilDesc.StencilReadMask = 0xFF;
	DepthStencilDesc.StencilWriteMask = 0xFF;
	DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	DepthStencilDesc.BackFace = DepthStencilDesc.FrontFace;
	Result = Device->CreateDepthStencilState(&DepthStencilDesc,
											 &DepthStencilState);
	if (FAILED(Result))
	{
		return false;
	}

	// 블렌드 상태 생성
	D3D11_BLEND_DESC BlendDesc{};
	BlendDesc.RenderTarget[0].BlendEnable = FALSE;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
	Result = Device->CreateBlendState(&BlendDesc, &BlendState);
	if (FAILED(Result))
	{
		return false;
	}

	// 샘플러 상태 생성
	D3D11_SAMPLER_DESC SamplerDesc{
		.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
		.AddressV = D3D11_TEXTURE_ADDRESS_WRAP,

		.AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
		.ComparisonFunc = D3D11_COMPARISON_NEVER,
		.MaxLOD = D3D11_FLOAT32_MAX,
	};
	Result = Device->CreateSamplerState(&SamplerDesc, &SamplerState);
	if (FAILED(Result))
	{
		return false;
	}

	FRenderPipelineCreateInfo CreateInfo{
		.VertexShader = std::move(VertexShader),
		.PixelShader = std::move(PixelShader),
		.InputLayout = std::move(InputLayout),
		.RasterizerState = std::move(RasterizerState),
		.DepthStencilState = std::move(DepthStencilState),
		.SamplerState = std::move(SamplerState),
		.BlendState = std::move(BlendState),
	};

	AllPipelineMap[FName("#Outline")] = MakeShared<FRenderPipeline>(std::move(CreateInfo));
	return true;
}

bool FRenderResourceLibrary::CreatePostProcessPipeline(FRenderer& Renderer)
{
	/*const FWString Path = EngineUtil::GetContentDirectory();
	const FWString VsPath = Path + L"/Shader/ScreenQuadVS.cso";
	const FWString PsPath = Path + L"/Shader/OutlinePostProcessPS.cso";

	if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath))
	{
		return false;
	}

	FRenderPipelineDesc Desc = {
		.VertexShaderFilePath = std::filesystem::path(VsPath).string(),
		.PixelShaderFilePath = std::filesystem::path(PsPath).string(),
		.Blend = { EBlendMode::Translucent },
		.Rasterizer = FRasterizerDesc{},
		.DepthStencil = FDepthStencilDesc{false, false, EDepthWriteMode::Disable},
		.bIsInstancing = false,
		.bCreateInputLayout = false,
		.Sampler = FTextureSamplerDesc{ETextureSamplerFilterMode::Point, ETextureSamplerWrapMode::Clamp}
	};

	TSharedPtr<FRenderPipeline> PPPipeline = CreateRenderPipeline(Renderer, Desc);
	if (PPPipeline)
	{
		AllPipelineMap[FName("#PostProcess")] = std::move(PPPipeline);
	}

	return AllPipelineMap[FName("#PostProcess")] != nullptr;*/


	//TODO 위쪽 코드로 교체하고 싶은데 아웃라인이 제대로 동작하지 않음.
	//나중에 좀 더 꼼꼼히 살펴보기 위쪽 코드로 적용할 것



	ID3D11Device* Device = Renderer.GetDevice();
	if (!Device)
	{
		return false;
	}

	const FWString Path = EngineUtil::GetContentDirectory();
	const FWString VsPath = Path + L"/Shader/ScreenQuadVS.cso";
	const FWString PsPath = Path + L"/Shader/OutlinePostProcessPS.cso";

	if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> SamplerState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> BlendState;

	// 버텍스 셰이더 로드 및 생성
	Microsoft::WRL::ComPtr<ID3DBlob> Blob;
	HRESULT Result = D3DReadFileToBlob(VsPath.c_str(), &Blob);
	if (FAILED(Result))
	{
		return false;
	}

	Result = Device->CreateVertexShader(Blob->GetBufferPointer(),
										Blob->GetBufferSize(), nullptr,
										&VertexShader);
	if (FAILED(Result))
	{
		return false;
	}

	// 픽셀 셰이더 로드 및 생성
	Result = D3DReadFileToBlob(PsPath.c_str(), &Blob);
	if (FAILED(Result))
	{
		return false;
	}

	Result = Device->CreatePixelShader(Blob->GetBufferPointer(),
									   Blob->GetBufferSize(), nullptr,
									   &PixelShader);
	if (FAILED(Result))
	{
		return false;
	}

	// 래스터라이저 상태 생성
	D3D11_RASTERIZER_DESC RasterizerDesc{
		.FillMode = D3D11_FILL_SOLID,
		.CullMode = D3D11_CULL_NONE,
		.FrontCounterClockwise = false,
	};
	Result = Device->CreateRasterizerState(&RasterizerDesc, &RasterizerState);
	if (FAILED(Result))
	{
		return false;
	}

	// 깊이 스텐실 상태 생성
	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc{
		.DepthEnable = FALSE,
		.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO,
		.DepthFunc = D3D11_COMPARISON_ALWAYS,
		.StencilEnable = FALSE,
	};
	Result = Device->CreateDepthStencilState(&DepthStencilDesc,
											 &DepthStencilState);
	if (FAILED(Result))
	{
		return false;
	}

	// 블렌드 상태 생성
	D3D11_BLEND_DESC BlendDesc{};
	BlendDesc.RenderTarget[0].BlendEnable = FALSE;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D11_COLOR_WRITE_ENABLE_ALL;
	Result = Device->CreateBlendState(&BlendDesc, &BlendState);
	if (FAILED(Result))
	{
		return false;
	}

	// 샘플러 상태 생성
	D3D11_SAMPLER_DESC SamplerDesc{
		.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
		.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
		.ComparisonFunc = D3D11_COMPARISON_NEVER,
		.MaxLOD = D3D11_FLOAT32_MAX,
	};

	Result = Device->CreateSamplerState(&SamplerDesc, &SamplerState);

	if (FAILED(Result))
	{
		return false;
	}

	FRenderPipelineCreateInfo CreateInfo{
		.VertexShader = std::move(VertexShader),
		.PixelShader = std::move(PixelShader),
		.RasterizerState = std::move(RasterizerState),
		.DepthStencilState = std::move(DepthStencilState),
		.SamplerState = std::move(SamplerState),
		.BlendState = std::move(BlendState),
	};

	AllPipelineMap[FName("#PostProcess")] = MakeShared<FRenderPipeline>(std::move(CreateInfo));
	return true;
}

bool FRenderResourceLibrary::CreateSceneDepthViewPipeline(FRenderer& Renderer)
{
	const FWString Path = EngineUtil::GetContentDirectory();
	const FWString VsPath = Path + L"/Shader/FullScreenTriangleVS.cso";
	const FWString PsPath = Path + L"/Shader/SceneDepthViewPS.cso";

	if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath))
	{
		return false;
	}

	FRenderPipelineDesc Desc = {
		.VertexShaderFilePath = std::filesystem::path(VsPath).string(),
		.PixelShaderFilePath = std::filesystem::path(PsPath).string(),
		.Blend = { EBlendMode::Opaque },
		.Rasterizer = FRasterizerDesc{},
		.DepthStencil = FDepthStencilDesc{false, false, EDepthWriteMode::Disable},
		.bIsInstancing = false,
		.bCreateInputLayout = false,
		.Sampler = FTextureSamplerDesc{ETextureSamplerFilterMode::Point, ETextureSamplerWrapMode::Clamp}
	};

	TSharedPtr<FRenderPipeline> SceneDepthViewPipeline = CreateRenderPipeline(Renderer, Desc);
	if (SceneDepthViewPipeline)
	{
		AllPipelineMap[FName("#SceneDepthView")] = std::move(SceneDepthViewPipeline);
	}

	return AllPipelineMap[FName("#SceneDepthView")] != nullptr;
}

bool FRenderResourceLibrary::CreateNDCtoWorldViewPipeline(FRenderer& Renderer)
{
	const FWString Path = EngineUtil::GetContentDirectory();
	const FWString VsPath = Path + L"/Shader/FullScreenTriangleVS.cso";
	const FWString PsPath = Path + L"/Shader/NDCtoWorldViewPs.cso";

	if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath))
	{
		return false;
	}

	FRenderPipelineDesc Desc = {
		.VertexShaderFilePath = std::filesystem::path(VsPath).string(),
		.PixelShaderFilePath = std::filesystem::path(PsPath).string(),
		.Blend = { EBlendMode::Opaque },
		.Rasterizer = FRasterizerDesc{},
		.DepthStencil = FDepthStencilDesc{false, false, EDepthWriteMode::Disable},
		.bIsInstancing = false,
		.bCreateInputLayout = false,
		.Sampler = FTextureSamplerDesc{ETextureSamplerFilterMode::Point, ETextureSamplerWrapMode::Clamp}
	};

	TSharedPtr<FRenderPipeline> NDCtoWorldPipeline = CreateRenderPipeline(Renderer, Desc);
	if (NDCtoWorldPipeline)
	{
		AllPipelineMap[FName("#NDCtoWorldView")] = std::move(NDCtoWorldPipeline);
	}

	return AllPipelineMap[FName("#NDCtoWorldView")] != nullptr;
}

bool FRenderResourceLibrary::CreatePPFog_AlphaBlendingPipeline(FRenderer& Renderer)
{
	const FWString Path = EngineUtil::GetContentDirectory();
	const FWString VsPath = Path + L"/Shader/FullScreenTriangleVS.cso";
	const FWString PsPath = Path + L"/Shader/PostProcessFogPS.cso";

	if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath))
	{
		return false;
	}

	FRenderPipelineDesc Desc = {
		.VertexShaderFilePath = std::filesystem::path(VsPath).string(),
		.PixelShaderFilePath = std::filesystem::path(PsPath).string(),
		.Blend = { EBlendMode::PremultipliedAlpha },
		.Rasterizer = FRasterizerDesc{},
		.DepthStencil = FDepthStencilDesc{false, false, EDepthWriteMode::Disable},
		.bIsInstancing = false,
		.bCreateInputLayout = false,
		.Sampler = FTextureSamplerDesc{ETextureSamplerFilterMode::Point, ETextureSamplerWrapMode::Clamp}
	};

	TSharedPtr<FRenderPipeline> PPFogPipeline =	CreateRenderPipeline(Renderer, Desc);
	if (PPFogPipeline)
	{
		AllPipelineMap[FName("#PostProcessFog")] = std::move(PPFogPipeline);
	}

	return AllPipelineMap[FName("#PostProcessFog")] != nullptr;
}

bool FRenderResourceLibrary::CreateFXAAPipelines(FRenderer& Renderer)
{
	const std::filesystem::path Path{EngineUtil::GetContentDirectory()};
	FRenderPipelineDesc Desc = {
		.VertexShaderFilePath = (Path / "Shader/FullScreenTriangleVS.cso").string(),
		.PixelShaderFilePath = (Path / "Shader/FXAAInputPS.cso").string(),
		.Blend = {EBlendMode::Opaque},
		.Rasterizer = {ERasterizerFillMode::Solid, ERasterizerCullMode::None},
		.DepthStencil = {false, false, EDepthWriteMode::Disable},
		.Sampler = {ETextureSamplerFilterMode::Bilinear, ETextureSamplerWrapMode::Clamp}
	};
	auto InputPipeline = CreateRenderPipeline(Renderer, Desc);
	Desc.PixelShaderFilePath = (Path / "Shader/FXAAPS.cso").string();
	auto FXAAPipeline = CreateRenderPipeline(Renderer, Desc);
	if (!InputPipeline || !FXAAPipeline) return false;
	AllPipelineMap[FName("#FXAAInput")] = std::move(InputPipeline);
	AllPipelineMap[FName("#FXAA")] = std::move(FXAAPipeline);
	return true;
}

TSharedPtr<FRenderPipeline> FRenderResourceLibrary::CreateRenderPipeline(FRenderer& Renderer, const FRenderPipelineDesc& Desc)
{
	ID3D11Device* Device = Renderer.GetDevice();
	if (!Device)
	{
		return nullptr;
	}

	Microsoft::WRL::ComPtr<ID3DBlob> Blob;

	namespace fs = std::filesystem;

	//Vertex Shader
	const fs::path VertexShaderPath{ Desc.VertexShaderFilePath };
	HRESULT Result = D3DReadFileToBlob(VertexShaderPath.wstring().c_str(), &Blob);
	if (FAILED(Result))
	{
		return nullptr;
	}

	Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
	Result = Device->CreateVertexShader(Blob->GetBufferPointer(),
										Blob->GetBufferSize(), nullptr,
										&VertexShader);
	if (FAILED(Result))
	{
		return nullptr;
	}

	size_t VSSize = Blob->GetBufferSize();
	INC_MEMORY_STAT_BY("VertexShaderMemory", Blob->GetBufferSize());

	//Input Layout
	Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;
	if (Desc.bCreateInputLayout)
	{
		if (Desc.bIsInstancing)
		{
			Result = Device->CreateInputLayout(
				FVertexInstanceLayouts::Layout, FVertexInstanceLayouts::NumElements,
				Blob->GetBufferPointer(), Blob->GetBufferSize(),
				&InputLayout);
		}
		else
		{
			Result = Device->CreateInputLayout(
				FVertexLayouts::Layout, FVertexLayouts::NumElements,
				Blob->GetBufferPointer(), Blob->GetBufferSize(),
				&InputLayout);
		}

		if (FAILED(Result))
		{
			return nullptr;
		}
	}
	
	//Pixel Shader
	const fs::path PixelShaderPath{ Desc.PixelShaderFilePath };
	Result = D3DReadFileToBlob(PixelShaderPath.wstring().c_str(), &Blob);
	if (FAILED(Result))
	{
		return nullptr;
	}

	Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
	Result = Device->CreatePixelShader(Blob->GetBufferPointer(), Blob->GetBufferSize(), nullptr, &PixelShader);
	if (FAILED(Result))
	{
		return nullptr;
	}

	size_t PSSize = Blob->GetBufferSize();
	INC_MEMORY_STAT_BY("PixelShaderMemory", Blob->GetBufferSize());

	//RasterizerState
	auto RasterizerState = GetOrCreateRasterizerState(Device, Desc.Rasterizer);

	//Depth Stencil State
	auto DepthStencilState = GetOrCreateDepthStencilState(Device, Desc.DepthStencil);

	//BlendState
	auto BlendState = GetOrCreateBlendState(Device, Desc.Blend);

	//SamplerState
	//auto SamplerState = GetOrCreateSamplerState(Device, FTextureSamplerDesc{});
	auto SamplerState = GetOrCreateSamplerState(Device, Desc.Sampler);

	if (!RasterizerState || !DepthStencilState || !BlendState || !SamplerState)
	{
		return nullptr;
	}

	FRenderPipelineCreateInfo CreateInfo{
		.Desc = Desc,
		.VertexShader = std::move(VertexShader),
		.PixelShader = std::move(PixelShader),
		.InputLayout = std::move(InputLayout),
		.RasterizerState = std::move(RasterizerState),
		.DepthStencilState = std::move(DepthStencilState),
		.SamplerState = std::move(SamplerState),
		.BlendState = std::move(BlendState),
	};

	TSharedPtr<FRenderPipeline> Pipeline = MakeShared<FRenderPipeline>(std::move(CreateInfo));

	Pipeline->SetPixelShaderSize(VSSize);
	Pipeline->SetPixelShaderSize(PSSize);

	return Pipeline;
}

Microsoft::WRL::ComPtr<ID3D11RasterizerState>
FRenderResourceLibrary::GetOrCreateRasterizerState(ID3D11Device* Device, const FRasterizerDesc& Desc)
{
	if (const auto It = RasterizerStateMap.find(Desc); It != RasterizerStateMap.end())
	{
		return It->second;
	}

	static const TMap<ERasterizerFillMode, D3D11_FILL_MODE> FillModeMap{
		{ERasterizerFillMode::Solid, D3D11_FILL_SOLID},
		{ERasterizerFillMode::Wireframe, D3D11_FILL_WIREFRAME},
	};

	static const TMap<ERasterizerCullMode, D3D11_CULL_MODE> CullModeMap{
		{ERasterizerCullMode::None, D3D11_CULL_NONE},
		{ERasterizerCullMode::Front, D3D11_CULL_FRONT},
		{ERasterizerCullMode::Back, D3D11_CULL_BACK},
	};
	static const TMap<ERasterizerFrontFaceMode, BOOL> FrontFaceMap{
		{ERasterizerFrontFaceMode::CounterClockwise, TRUE},
		{ERasterizerFrontFaceMode::Clockwise, FALSE},
	};

	const D3D11_RASTERIZER_DESC NativeDesc{
		.FillMode = FillModeMap.at(Desc.FillMode),
		.CullMode = CullModeMap.at(Desc.CullMode),
		.FrontCounterClockwise = FrontFaceMap.at(Desc.FrontFace),
		.MultisampleEnable = Desc.bUseMultisample,
		.AntialiasedLineEnable = Desc.bUseAntialiasedLine,
	};

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> State;
	if (FAILED(Device->CreateRasterizerState(&NativeDesc, &State)))
	{
		return nullptr;
	}
	RasterizerStateMap.emplace(Desc, State);
	return State;
}

Microsoft::WRL::ComPtr<ID3D11DepthStencilState>
FRenderResourceLibrary::GetOrCreateDepthStencilState(ID3D11Device* Device, const FDepthStencilDesc& Desc)
{
	if (const auto It = DepthStencilStateMap.find(Desc); It != DepthStencilStateMap.end())
	{
		return It->second;
	}

	static const TMap<EDepthWriteMode, D3D11_DEPTH_WRITE_MASK> DepthWriteMap{
		{EDepthWriteMode::Disable, D3D11_DEPTH_WRITE_MASK_ZERO},
		{EDepthWriteMode::Enable, D3D11_DEPTH_WRITE_MASK_ALL},
	};

	//더 필요한 사항이 있다면 EDepthWriteMode에 추가할 것.

	D3D11_DEPTH_STENCIL_DESC NativeDesc{};
	NativeDesc.DepthEnable = Desc.bDepthEnable;
	NativeDesc.DepthWriteMask = DepthWriteMap.at(Desc.DepthWrite);
	NativeDesc.DepthFunc = D3D11_COMPARISON_LESS;
	NativeDesc.StencilEnable = Desc.bStencilEnable;
	NativeDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	NativeDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	NativeDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	NativeDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	NativeDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	NativeDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	NativeDesc.BackFace = NativeDesc.FrontFace;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> State;
	if (FAILED(Device->CreateDepthStencilState(&NativeDesc, &State)))
	{
		return nullptr;
	}
	DepthStencilStateMap.emplace(Desc, State);
	return State;
}

Microsoft::WRL::ComPtr<ID3D11BlendState>
FRenderResourceLibrary::GetOrCreateBlendState(ID3D11Device* Device, const FBlendDesc& Desc)
{
	if (const auto It = BlendStateMap.find(Desc); It != BlendStateMap.end())
	{
		return It->second;
	}

	static const TMap<EBlendMode, D3D11_RENDER_TARGET_BLEND_DESC> BlendModeMap{
		{EBlendMode::Opaque,
		 {.BlendEnable = FALSE,
		  .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL}},
		{EBlendMode::Masked,
		 {.BlendEnable = FALSE,
		  .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL}},
		{EBlendMode::Translucent,
		 {.BlendEnable = TRUE,
		  .SrcBlend = D3D11_BLEND_SRC_ALPHA,
		  .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
		  .BlendOp = D3D11_BLEND_OP_ADD,
		  .SrcBlendAlpha = D3D11_BLEND_ONE,
		  .DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
		  .BlendOpAlpha = D3D11_BLEND_OP_ADD,
		  .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL}},
		{EBlendMode::Additive,
		 {.BlendEnable = TRUE,
		  .SrcBlend = D3D11_BLEND_ONE,
		  .DestBlend = D3D11_BLEND_ONE,
		  .BlendOp = D3D11_BLEND_OP_ADD,
		  .SrcBlendAlpha = D3D11_BLEND_ONE,
		  .DestBlendAlpha = D3D11_BLEND_ZERO,
		  .BlendOpAlpha = D3D11_BLEND_OP_ADD,
		  .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL}},
		{EBlendMode::PremultipliedAlpha,
		 {.BlendEnable = TRUE,
		  .SrcBlend = D3D11_BLEND_ONE,
		  .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
		  .BlendOp = D3D11_BLEND_OP_ADD,
		  .SrcBlendAlpha = D3D11_BLEND_ONE,
		  .DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
		  .BlendOpAlpha = D3D11_BLEND_OP_ADD,
		  .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL}},
	};

	D3D11_BLEND_DESC NativeDesc{};
	NativeDesc.RenderTarget[0] = BlendModeMap.at(Desc.BlendMode);

	Microsoft::WRL::ComPtr<ID3D11BlendState> State;

	if (FAILED(Device->CreateBlendState(&NativeDesc, &State)))
	{
		return nullptr;
	}

	BlendStateMap.emplace(Desc, State);
	return State;
}

Microsoft::WRL::ComPtr<ID3D11SamplerState>
FRenderResourceLibrary::GetOrCreateSamplerState(ID3D11Device* Device, const FTextureSamplerDesc& Desc)
{
	if (const auto It = SamplerStateMap.find(Desc); It != SamplerStateMap.end())
	{
		return It->second;
	}

	static const TMap<ETextureSamplerFilterMode, D3D11_FILTER> FilterModeMap{
		{ETextureSamplerFilterMode::Point, D3D11_FILTER_MIN_MAG_MIP_POINT},
		{ETextureSamplerFilterMode::Bilinear,
		 D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT},
		{ETextureSamplerFilterMode::Trilinear, D3D11_FILTER_MIN_MAG_MIP_LINEAR},
		{ETextureSamplerFilterMode::Anisotropic, D3D11_FILTER_ANISOTROPIC},
	};
	static const TMap<ETextureSamplerWrapMode, D3D11_TEXTURE_ADDRESS_MODE>
		WrapModeMap{
			{ETextureSamplerWrapMode::Wrap, D3D11_TEXTURE_ADDRESS_WRAP},
			{ETextureSamplerWrapMode::Mirror, D3D11_TEXTURE_ADDRESS_MIRROR},
			{ETextureSamplerWrapMode::Clamp, D3D11_TEXTURE_ADDRESS_CLAMP},
	};
	static const TMap<ETextureSamplerFilterMode, UINT> MaxAnisotropyMap{
		{ETextureSamplerFilterMode::Point, 1u},
		{ETextureSamplerFilterMode::Bilinear, 1u},
		{ETextureSamplerFilterMode::Trilinear, 1u},
		{ETextureSamplerFilterMode::Anisotropic, 16u},
	};

	const D3D11_TEXTURE_ADDRESS_MODE Address = WrapModeMap.at(Desc.WrapMode);
	const D3D11_SAMPLER_DESC NativeDesc{
		.Filter = FilterModeMap.at(Desc.FilterMode),
		.AddressU = Address,
		.AddressV = Address,
		.AddressW = Address,
		.MaxAnisotropy = MaxAnisotropyMap.at(Desc.FilterMode),
		.ComparisonFunc = D3D11_COMPARISON_NEVER,
		.MaxLOD = D3D11_FLOAT32_MAX,
	};

	Microsoft::WRL::ComPtr<ID3D11SamplerState> State;
	if (FAILED(Device->CreateSamplerState(&NativeDesc, &State)))
	{
		return nullptr;
	}
	SamplerStateMap.emplace(Desc, State);
	return State;
}

bool FRenderResourceLibrary::InitializePipelines(FRenderer& Renderer)
{
	return CreateWireframePipeline(Renderer) &&
		CreateOutlinePipeline(Renderer) &&
		CreatePostProcessPipeline(Renderer) &&
		CreateSceneDepthViewPipeline(Renderer) &&
		CreateNDCtoWorldViewPipeline(Renderer)&&
		CreatePPFog_AlphaBlendingPipeline(Renderer) &&
		CreateFXAAPipelines(Renderer);
}

bool FRenderResourceLibrary::Initialize(FRenderer& Renderer)
{
	RendererRef = &Renderer;
	if (!InitializePipelines(Renderer))
	{ // 파이프라인을 먼저 생성해야 뒤에 material 할당가능
		return false;
	}

	if (!CreateInstancingArrayMap())
	{
		return false;
	}

	return true;
}

bool FRenderResourceLibrary::CreateInstancingArrayMap()
{
	AllInstancingArrayMap.clear();
	return true;
}

TSharedPtr<FMaterial> FRenderResourceLibrary::RegisterMaterial(const FName& Id, TSharedPtr<FMaterial> inMaterial)
{
	AllMaterialMap[Id] = inMaterial;
	return inMaterial;
}

TSharedPtr<FMesh> FRenderResourceLibrary::GetOrCreateMesh(const FName& ID, const TArray<FVertexData>& vertices)
{
	auto it = AllMeshMap.find(ID);
	if (it != AllMeshMap.end())
		return it->second;

	FMeshDesc Desc{ .VertexData = vertices.data(),
				   .VertexDataSize =
					   static_cast<uint32>(sizeof(FVertexData) * vertices.size()),
				   .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
				   .VertexCount = static_cast<uint32>(vertices.size()) };
	TSharedPtr<FMesh> newMesh =
		RendererRef ? RendererRef->CreateMesh(Desc) : nullptr;
	if (newMesh)
	{
		AllMeshMap[ID] = newMesh;
	}
	return newMesh;
}
