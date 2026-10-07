#pragma once

#include "FMaterial.h"
#include "FMesh.h"
#include "FRenderPipeline.h"
#include "FRenderResourceLibrary.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Core/TMap.h"
#include "Runtime/CoreUObject/FStatsManager.h"
#include "Runtime/Material/FTextureSamplerDesc.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Rendering/FLineBatcher.h"
#include "Runtime/Rendering/FViewportRenderSurface.h"
#include "ShaderConstants.h"
#include "Vertices.h"

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl/client.h>
#include <initializer_list>
#include <span>

class FTexture;
struct FTextureDesc;
class FCamera;
class UTextInstanceComponent;
struct FDrawCommand;
struct FFogConstants;

#include "Runtime/Engine/ShowFlags.h"

struct FStructuredBuffer
{
    ID3D11DeviceContext* DeviceContext;

    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
    UINT ElementSize;
    UINT ElementCount;

    void UpdateStructuredBuffer(const void* Data, uint32 DataCount)
    {
        D3D11_BOX Box = {};
        Box.left = 0;
        Box.right = DataCount * ElementSize;
        Box.top = 0;
        Box.bottom = 1;
        Box.front = 0;
        Box.back = 1;

        DeviceContext->UpdateSubresource(Buffer.Get(), 0, &Box, Data, 0, 0);
    }
};



struct FFrameResource
{
	Microsoft::WRL::ComPtr<ID3D11Buffer> FrameConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ViewConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ObjectConstantBuffer;
};

class FRenderer final {
public:
  bool Initialize(HWND Window);
  void Shutdown();
  void BeginFrame();
  // 현재 렌더 대상(뷰포트 표면이 있으면 그 표면, 없으면 창 크기 깊이 버퍼)의 깊이/스텐실을 지운다
  void ClearDepth();
  void SwapBuffer();
  void FlushDrawStats();
  void OnWindowSize(UINT Width, UINT Height);

  EViewModeIndex GetRenderMode() const { return CurrentRenderMode; }
  void SetRenderMode(EViewModeIndex InMode) { CurrentRenderMode = InMode; }

  [[nodiscard]]
  TSharedPtr<FMesh> CreateMesh(const FMeshDesc &Desc);
  [[nodiscard]]
  TSharedPtr<FMesh> CreateDynamicMesh(const FMeshDesc &Desc); // 텍스트 렌더링용
  void GetDeviceAndContext_ImplDX11(ID3D11Device *&DeviceOut,
                                    ID3D11DeviceContext *&ContextOut);
  [[nodiscard]] ID3D11Device *GetDevice() const { return Device.Get(); }
  [[nodiscard]] ID3D11DeviceContext *GetContext() const {
    return Context.Get();
  }

  [[nodiscard]]
  TSharedPtr<FRenderPipeline>
  CreateRenderPipeline(const FRenderPipelineDesc &Desc,
                       EViewModeIndex RenderMode = EViewModeIndex::VMI_Lit);
  [[nodiscard]]
  TSharedPtr<FTexture> CreateTexture(const wchar_t* path);
  TSharedPtr<FTexture> CreateSolidTexture(const FVector4& Color);
  // 파이프라인 조회
  [[nodiscard]]
  TSharedPtr<FRenderPipeline> GetPipeline(const FName& Id) const;

  FLineBatcher &GetLineBatcher() { return LineBatcher; }

  void UpdateLightConstants(const FLightConstants &Constants, const EViewModeIndex InMode);
  void UpdateFrameConstants(const FFrameConstants &Constants);
  void UpdateViewConstants(const FViewConstants &Constants);
  void UpdateSceneDepthConstants(const FSceneDepthConstants &Constants);
  void UpdateFogConstants(const FFogConstants&Constants);

  // 텍스트 인스턴싱
  void AddTextInstanceArray(const FDrawCommand& Command);
  void DrawInstances(const FCamera& Camera);
  void DrawTextInstances(const FDrawCommand& Command);
  void ClearTextInstances();

  void Draw(const FDrawCommand& Command, uint32 Slot = 2,
            bool bApplyViewMode = true);

  void DrawPrimitiveBatch(std::span<const FDrawCommand> Commands, TArray<FPointLightConstants>& PointLightConstants);

  bool UploadObjectConstants(std::span<const FDrawCommand> Commands);

  void BindObjectConstantRange(uint32 Slot, uint32 ByteOffset);
  void BindDrawResources(
      const FMesh& Mesh,
      const FMaterial& Material,
      bool bApplyViewMode
  );

  void DrawUploadedCommand(const FDrawCommand& Command, bool bApplyViewMode = true);

  ID3D11RenderTargetView* GetBackBuffer() { return BackBufferRTV.Get(); }
  ID3D11DepthStencilView* GetDepthStencilView() { return DepthStencilView.Get(); }

  float GetWidth() const { return Viewport.Width; }
  float GetHeight() const { return Viewport.Height; }

  void ClearLastRenderState();

  // 뷰포트 표면
  // 백버퍼 기준 UV 영역을 정수 픽셀 영역으로 바꾼다. 표면 크기와 합성 영역이 같은 값을 쓴다.
  [[nodiscard]] D3D11_VIEWPORT GetViewportPixelRect(FVector2 TopLeftUV, FVector2 LengthUV) const;
  // 표면을 현재 렌더 대상으로 지정하고 컬러/깊이를 지운다
  void BeginViewportSurface(FViewportRenderSurface& Surface);
  void EndViewportSurface() { CurrentSurface = nullptr; }
  // 현재 표면의 씬 컬러 + 깊이를 다시 바인딩한다. 후처리가 바인딩을 바꾼 뒤에 쓴다.
  void BindViewportSurfaceTargets();
  [[nodiscard]] const FViewportRenderSurface* GetCurrentSurface() const { return CurrentSurface; }

  // 후처리 (현재 표면 대상)
  //Scene Depth View 그리기
  void RenderSceneDepthView();
  //NDC -> World View Mode 그리기
  void RenderNDCtoWorldView();
  //Fog Rendering
  void RenderPostProcessFog();
  // 씬 컬러에 FXAA를 적용한다. 표면의 두 컬러를 번갈아 써서 추가 RT 없이 처리한다.
  void RenderFXAA();
  // 씬 컬러와 스텐실을 읽어 선택 외곽선을 입힌다. 결과는 표면의 다른 컬러에 쓰고 교체한다.
  void RenderOutline();

  // 표면의 최종 컬러를 백버퍼의 뷰포트 영역으로 옮긴다. sRGB 변환은 여기서 한 번만 일어난다.
  void CompositeViewportSurface(const FViewportRenderSurface& Surface, FVector2 TopLeftUV, FVector2 LengthUV);

private:
  // 풀스크린 삼각형 패스의 공통 흐름.
  // 출력 RT 바인딩(DSV 없음) → 입력 SRV 바인딩 → Draw(3) → 입력 SRV 해제 → 렌더 상태 캐시 무효화
  void DrawFullScreenPass(const FName& PipelineId, ID3D11RenderTargetView* Target,
                          const D3D11_VIEWPORT& TargetViewport,
                          std::initializer_list<ID3D11ShaderResourceView*> Inputs);
  // 깊이를 읽어 현재 표면의 씬 컬러에 그리는 후처리
  void RenderDepthPostProcess(const FName& PipelineId);

  bool InitializeDeviceAndSwapChain(HWND Window);
  bool InitializeBackBufferAndDepthStencil();
  bool InitializeConstantBuffers();
  bool InitializeGPUTimerQueries();

  // GPU 타임스탬프. 결과를 같은 프레임에 바로 읽으면 CPU가 GPU를 기다리게 되므로
  // 쿼리 세트를 돌려 쓰고 가장 오래된 것만 회수한다.
  void BeginGPUTimer();
  void EndGPUTimer();
  void ResolveGPUTimer();

  Microsoft::WRL::ComPtr<ID3D11RasterizerState>
  GetOrCreateRasterizerState(const FRasterizerDesc& Desc);
  Microsoft::WRL::ComPtr<ID3D11DepthStencilState>
  GetOrCreateDepthStencilState(const FDepthStencilDesc& Desc);
  Microsoft::WRL::ComPtr<ID3D11BlendState>
  GetOrCreateBlendState(const FBlendDesc& Desc);
  Microsoft::WRL::ComPtr<ID3D11SamplerState>
  GetOrCreateSamplerState(const FTextureSamplerDesc& Desc);

  FLineBatcher LineBatcher;
  Microsoft::WRL::ComPtr<ID3D11Device> Device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
  Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
  D3D11_VIEWPORT Viewport{};

  // D3D11_1 Extension
  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> Context1;

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> BackBufferRTV;
  // 창 크기 깊이 버퍼. 백버퍼에 직접 그리는 경로(ObjViewer)가 쓴다.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;

  // 지금 그리고 있는 뷰포트 표면. 소유자는 FEditor다.
  FViewportRenderSurface* CurrentSurface = nullptr;

  // 모든 ConstantBuffer의 최대 크기
  static constexpr UINT ConstantBufferSize = 256u;

  // 상수 버퍼들
  /*Microsoft::WRL::ComPtr<ID3D11Buffer> FrameConstantBuffer;
  Microsoft::WRL::ComPtr<ID3D11Buffer> ViewConstantBuffer;
  Microsoft::WRL::ComPtr<ID3D11Buffer> ObjectConstantBuffer;*/
  Microsoft::WRL::ComPtr<ID3D11Buffer> LightConstantBuffer;
  //TODO : 일단은 상수 버퍼로 하겠지만 멀티 뷰포트가 멀티 RTV로 수정된다면 카메라마다 다를 경우를 대비해 늘려야한다.
  Microsoft::WRL::ComPtr<ID3D11Buffer> SceneDepthConstantBuffer;
  Microsoft::WRL::ComPtr<ID3D11Buffer> FogConstantBuffer;

  TSharedPtr<FStructuredBuffer> PointLightBuffer;

  // 임시 상수버퍼
  Microsoft::WRL::ComPtr<ID3D11Buffer> ObjectConstantUploadBuffer;

  TMap<FRasterizerDesc, Microsoft::WRL::ComPtr<ID3D11RasterizerState>> RasterizerStateMap;
  TMap<FDepthStencilDesc, Microsoft::WRL::ComPtr<ID3D11DepthStencilState>> DepthStencilStateMap;
  TMap<FBlendDesc, Microsoft::WRL::ComPtr<ID3D11BlendState>> BlendStateMap;
  TMap<FTextureSamplerDesc, Microsoft::WRL::ComPtr<ID3D11SamplerState>> SamplerStateMap;

  // 텍스트 인스턴싱 버퍼

  Microsoft::WRL::ComPtr<ID3D11Buffer> InstanceBuffer;
  UINT TextInstanceBufferSize = 0;

  EViewModeIndex CurrentRenderMode = EViewModeIndex::VMI_Lit;

  struct FGPUTimerQuery {
    Microsoft::WRL::ComPtr<ID3D11Query> Disjoint;
    Microsoft::WRL::ComPtr<ID3D11Query> Start;
    Microsoft::WRL::ComPtr<ID3D11Query> End;
    // 이 프레임이 반영한 입력의 QPC 시각. 입력이 없었으면 0.
    int64 InputStartTick = 0;
    bool bPending = false;
  };

  static constexpr uint32 GPUTimerFrameCount = 3u;
  FGPUTimerQuery GPUTimerQueries[GPUTimerFrameCount];
  uint32 GPUTimerFrameIndex = 0u;

  // 회수에 실패한 프레임에 0을 넣으면 평균이 눌리므로 직전 값을 들고 있는다.
  double LastGPUTimeMs = 0.0;
  
  const FMesh* LastMesh = nullptr;
  const FTexture* LastTexture = nullptr;
  bool bHasLastTexture = false;
  const FRenderPipeline* LastRenderPipeline = nullptr;

  // Draw/DrawSection이 드로우마다 통계 매크로를 부르지 않도록 여기에 모았다가
  // FlushDrawStats에서 한 번에 반영한다.
  uint32 PendingDrawCount = 0u;
  uint32 PendingPrimCount = 0u;

	static constexpr uint32 NumFrameResourceCount = 3;
	FFrameResource FrameResources[NumFrameResourceCount];
	uint32 CurrentFrameResourceIndex = 0;

    FFrameResource* GetCurrentFrameResource() { return &FrameResources[CurrentFrameResourceIndex]; }
	FFrameResource* GetNextFrameResource() { return &FrameResources[(CurrentFrameResourceIndex + 1) % NumFrameResourceCount]; }
public:
  template <typename TConstants>
  void FlushLineBatch(
      const TConstants &Constants,
      const FName& PipelineId = FName("Simple_Line")
  ) {
    UpdateBuffer(Constants, 2);
    LineBatcher.Flush(*Context.Get(), GetPipeline(PipelineId));
  }

  // bApplyViewMode=false면 뷰모드(와이어프레임) 오버라이드를 건너뛴다
  template <typename TConstants>
  void Draw(
      const FMesh &Mesh,
      const FMaterial &Material,
      const TConstants &Constants,
      uint32 Slot = 2,
      bool bApplyViewMode = true
  )
  {
    UpdateBuffer(Constants, 2);

    BindDrawResources(
        Mesh,
        Material,
        bApplyViewMode
    );

    if (Mesh.HasIndices()) {
      Context->DrawIndexed(Mesh.IndexCount, 0, 0);
      PendingPrimCount += Mesh.IndexCount / 3u;
    } else {
      Context->Draw(Mesh.VertexCount, 0);
      PendingPrimCount += Mesh.VertexCount / 3u;
    }
    ++PendingDrawCount;
  }

  template <typename TConstants>
  void DrawSection(
      const FMesh& Mesh,
      const FMaterial& Material,
      const TConstants& Constants,
      uint32 StartIndex,
      uint32 IndexCount,
      uint32 Slot = 2,
      bool bApplyViewMode = true
  )
  {
      UpdateBuffer(Constants, Slot);

      BindDrawResources(
          Mesh,
          Material,
          bApplyViewMode
      );

      if (Mesh.HasIndices()) {
          Context->DrawIndexed(IndexCount, StartIndex, 0);
          PendingPrimCount += IndexCount / 3u;
      }
      else {
          Context->Draw(Mesh.VertexCount, 0);
          PendingPrimCount += Mesh.VertexCount / 3u;
      }
      ++PendingDrawCount;
  }

  // Constant Buffer를 갱신한다.
  // 크기가 맞는지는 컴파일 타임에 검사한다.
/*  template <typename TConstants>
  void UpdateBuffer(const TConstants &Constants, uint32 Slot) {
    static_assert(sizeof(TConstants) <= ConstantBufferSize);
    static_assert(sizeof(TConstants) % 16 == 0);

    // 언리얼 Clip -> D3D Clip 좌표 변환.
    // MVP, VP를 가진 상수 타입에만 적용한다(없는 타입은 그대로 통과).
    TConstants ShaderConstants = Constants;
    if constexpr (requires { ShaderConstants.MVP; }) {
        ShaderConstants.MVP = ShaderConstants.MVP.ToD3DMatrix();
    }

    D3D11_MAPPED_SUBRESOURCE Mapped{};
    if (FAILED(Context->Map(ObjectConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
                            0, &Mapped))) {
      return;
    }
    std::memcpy(Mapped.pData, &ShaderConstants, sizeof(TConstants));
    Context->Unmap(ObjectConstantBuffer.Get(), 0);

    Context->VSSetConstantBuffers(Slot, 1u, ObjectConstantBuffer.GetAddressOf());
    Context->PSSetConstantBuffers(Slot, 1u, ObjectConstantBuffer.GetAddressOf());
  }*/

  template <typename TConstants>
  void UpdateBuffer(const TConstants& Constants, uint32 Slot) {
      static_assert(sizeof(TConstants) <= ConstantBufferSize);
      static_assert(sizeof(TConstants) % 16 == 0);

      // 언리얼 Clip -> D3D Clip 좌표 변환.
      // MVP, VP를 가진 상수 타입에만 적용한다(없는 타입은 그대로 통과).
      TConstants ShaderConstants = Constants;
      if constexpr (requires { ShaderConstants.MVP; }) {
          ShaderConstants.MVP = ShaderConstants.MVP.ToD3DMatrix();
      }

      D3D11_MAPPED_SUBRESOURCE Mapped{};
      if (FAILED(Context->Map(GetCurrentFrameResource()->ObjectConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
          0, &Mapped))) {
          return;
      }
      std::memcpy(Mapped.pData, &ShaderConstants, sizeof(TConstants));
      Context->Unmap(GetCurrentFrameResource()->ObjectConstantBuffer.Get(), 0);

      Context->VSSetConstantBuffers(Slot, 1u, GetCurrentFrameResource()->ObjectConstantBuffer.GetAddressOf());
      Context->PSSetConstantBuffers(Slot, 1u, GetCurrentFrameResource()->ObjectConstantBuffer.GetAddressOf());
  }

  template <typename T>
  TSharedPtr<FStructuredBuffer> CreateStructuredBuffer(uint32 ElementCount)
  {
      TSharedPtr<FStructuredBuffer> StructuredBuffer = MakeShared<FStructuredBuffer>();
      StructuredBuffer->DeviceContext = Context.Get();

      D3D11_BUFFER_DESC StructuredBufferDesc = {};
      StructuredBufferDesc.ByteWidth = sizeof(T) * ElementCount;
      StructuredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
      StructuredBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      StructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
      StructuredBufferDesc.StructureByteStride = sizeof(T);

      Device->CreateBuffer(&StructuredBufferDesc, nullptr, StructuredBuffer->Buffer.GetAddressOf());

      D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
      SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
      SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
      SRVDesc.Buffer.FirstElement = 0;
      SRVDesc.Buffer.NumElements = ElementCount;

      Device->CreateShaderResourceView(StructuredBuffer->Buffer.Get(), &SRVDesc, StructuredBuffer->SRV.GetAddressOf());

      StructuredBuffer->ElementSize = sizeof(T);
      StructuredBuffer->ElementCount = ElementCount;

      return StructuredBuffer;
  }
public:
  //현재 깊이 버퍼 기준으로 각 명령이 실제로 보이는 픽셀 수를 GPU에 묻는다.
  //GPU가 끝날 때까지 기다리므로 느리다. 디버깅에서 쓰는 한 프레임 측정 전용
  void QueryVisibility(const TArray<const FDrawCommand*>& Commands, TArray<uint64>& OutSamples);

private:
    // 오클루전 오라클 (측정 도구)
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> OracleDepthState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> OracleBlendState;
    TArray<Microsoft::WRL::ComPtr<ID3D11Query>> OracleQueries;
};
