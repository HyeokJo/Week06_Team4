#include "FViewportRenderSurface.h"

bool FViewportRenderSurface::Create(ID3D11Device& Device, uint32 InWidth, uint32 InHeight)
{
	Release();

	if (InWidth == 0u || InHeight == 0u)
	{
		return false;
	}

	Width = InWidth;
	Height = InHeight;

	if (!CreateColorTarget(Device, Colors[0]) ||
		!CreateColorTarget(Device, Colors[1]) ||
		!CreateDepthStencil(Device))
	{
		Release();
		return false;
	}

	return true;
}

void FViewportRenderSurface::Release()
{
	for (FColorTarget& Color : Colors)
	{
		Color.SRV.Reset();
		Color.RTV.Reset();
		Color.Texture.Reset();
	}
	CurrentColor = 0u;

	StencilSRV.Reset();
	DepthSRV.Reset();
	DepthStencilView.Reset();
	DepthStencilTexture.Reset();

	Width = 0u;
	Height = 0u;
}

D3D11_VIEWPORT FViewportRenderSurface::GetFullViewport() const
{
	return D3D11_VIEWPORT{
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width = static_cast<float>(Width),
		.Height = static_cast<float>(Height),
		.MinDepth = 0.0f,
		.MaxDepth = 1.0f,
	};
}

bool FViewportRenderSurface::CreateColorTarget(ID3D11Device& Device, FColorTarget& Target) const
{
	// sRGB 변환은 백버퍼로 합성할 때 한 번만 일어나도록 표면은 UNORM으로 둔다.
	const D3D11_TEXTURE2D_DESC ColorDesc{
		.Width = Width,
		.Height = Height,
		.MipLevels = 1u,
		.ArraySize = 1u,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {.Count = 1u},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
	};

	if (FAILED(Device.CreateTexture2D(&ColorDesc, nullptr, &Target.Texture)))
	{
		return false;
	}

	if (FAILED(Device.CreateRenderTargetView(Target.Texture.Get(), nullptr, &Target.RTV)))
	{
		return false;
	}

	return SUCCEEDED(Device.CreateShaderResourceView(Target.Texture.Get(), nullptr, &Target.SRV));
}

bool FViewportRenderSurface::CreateDepthStencil(ID3D11Device& Device)
{
	// 같은 텍스처를 DSV와 깊이/스텐실 SRV로 함께 쓰기 위해 TYPELESS로 만든다.
	const D3D11_TEXTURE2D_DESC DepthDesc{
		.Width = Width,
		.Height = Height,
		.MipLevels = 1u,
		.ArraySize = 1u,
		.Format = DXGI_FORMAT_R24G8_TYPELESS,
		.SampleDesc = {.Count = 1u},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
	};

	if (FAILED(Device.CreateTexture2D(&DepthDesc, nullptr, &DepthStencilTexture)))
	{
		return false;
	}

	const D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc{
		.Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
		.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
	};
	if (FAILED(Device.CreateDepthStencilView(DepthStencilTexture.Get(), &DsvDesc, &DepthStencilView)))
	{
		return false;
	}

	const D3D11_SHADER_RESOURCE_VIEW_DESC DepthSrvDesc{
		.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
		.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
		.Texture2D = {.MostDetailedMip = 0, .MipLevels = 1},
	};
	if (FAILED(Device.CreateShaderResourceView(DepthStencilTexture.Get(), &DepthSrvDesc, &DepthSRV)))
	{
		return false;
	}

	const D3D11_SHADER_RESOURCE_VIEW_DESC StencilSrvDesc{
		.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT,
		.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
		.Texture2D = {.MostDetailedMip = 0, .MipLevels = 1},
	};
	return SUCCEEDED(Device.CreateShaderResourceView(DepthStencilTexture.Get(), &StencilSrvDesc, &StencilSRV));
}
