#pragma once

#include "Runtime/Core/IntTypes.h"

#include <d3d11.h>
#include <wrl/client.h>

// 뷰포트 하나가 그려지는 렌더 표면.
// "무엇을 어떻게 볼지"(카메라, 뷰모드)는 FEditorViewportClient가,
// "어디에 그릴지"(GPU 리소스)는 이 객체가 맡는다.
//
// 컬러 타깃은 2장을 번갈아 쓴다(핑퐁). 씬 컬러를 읽어 다른 컬러에 쓰는 후처리(외곽선)가
// 같은 텍스처를 SRV와 RTV로 동시에 바인딩하지 않도록 하기 위함이다.
// 깊이/스텐실은 뷰포트마다 따로 가지므로 후처리가 다른 뷰포트의 값을 읽지 않는다.
class FViewportRenderSurface final
{
public:
	FViewportRenderSurface() = default;
	~FViewportRenderSurface() = default;

	// GPU 리소스를 소유하므로 복사하면 두 표면이 같은 텍스처에 그리게 된다.
	FViewportRenderSurface(const FViewportRenderSurface&) = delete;
	FViewportRenderSurface& operator=(const FViewportRenderSurface&) = delete;

	FViewportRenderSurface(FViewportRenderSurface&&) noexcept = default;
	FViewportRenderSurface& operator=(FViewportRenderSurface&&) noexcept = default;

	// 기존 리소스를 해제하고 주어진 크기로 다시 만든다. 실패하면 빈 표면으로 남는다.
	bool Create(ID3D11Device& Device, uint32 InWidth, uint32 InHeight);
	void Release();

	[[nodiscard]] bool IsValid() const { return DepthStencilView != nullptr; }
	[[nodiscard]] uint32 GetWidth() const { return Width; }
	[[nodiscard]] uint32 GetHeight() const { return Height; }
	[[nodiscard]] D3D11_VIEWPORT GetFullViewport() const;

	// 지금까지 그려진 최신 씬 이미지를 담은 컬러
	[[nodiscard]] ID3D11RenderTargetView* GetSceneColorRTV() const { return Colors[CurrentColor].RTV.Get(); }
	[[nodiscard]] ID3D11ShaderResourceView* GetSceneColorSRV() const { return Colors[CurrentColor].SRV.Get(); }

	// 씬 컬러를 읽는 후처리의 출력 대상. 출력 후 SwapColor()로 최신 이미지를 교체한다.
	[[nodiscard]] ID3D11RenderTargetView* GetSpareColorRTV() const { return Colors[1u - CurrentColor].RTV.Get(); }
	void SwapColor() { CurrentColor = 1u - CurrentColor; }
	void ResetColor() { CurrentColor = 0u; }

	[[nodiscard]] ID3D11DepthStencilView* GetDepthStencilView() const { return DepthStencilView.Get(); }
	[[nodiscard]] ID3D11ShaderResourceView* GetDepthSRV() const { return DepthSRV.Get(); }
	[[nodiscard]] ID3D11ShaderResourceView* GetStencilSRV() const { return StencilSRV.Get(); }

private:
	struct FColorTarget
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RTV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	};

	bool CreateColorTarget(ID3D11Device& Device, FColorTarget& Target) const;
	bool CreateDepthStencil(ID3D11Device& Device);

	FColorTarget Colors[2];
	uint32 CurrentColor = 0u;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilTexture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> DepthSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> StencilSRV;

	uint32 Width = 0u;
	uint32 Height = 0u;
};
