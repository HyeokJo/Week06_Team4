#include "UFogComponent.h"
//#include "Runtime/Asset/FAssetRegistry.h"
//#include "Runtime/Rendering/FRenderResourceLibrary.h"
//#include "Runtime/Rendering/ShaderConstants.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/Engine/UWorld.h"
#include "Runtime/Engine/ULevel.h"
#include "Runtime/Engine/FArchive.h"

IMPLEMENT_UCLASS(UFogComponent, USceneComponent)

void UFogComponent::Initialize()
{
	if (IsInitialized()) return;
	Super::Initialize();
}


void UFogComponent::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);
	Archive.OptionalField("FogColor", FogColor);
	Archive.OptionalField("FogDensity", FogDensity);
	Archive.OptionalField("FogHeightFalloff", FogHeightFalloff);
	Archive.OptionalField("FogMaxOpacity", FogMaxOpacity);
	Archive.OptionalField("StartDistance", StartDistance);
}

void UFogComponent::Register(ULevel& InLevel)
{
	Super::Register(InLevel);
	InLevel.GetWorld()->AddFogComponent(this);
}

void UFogComponent::Unregister()
{
	if (Level)
	{
		Level->GetWorld()->RemoveFogComponent(this);
	}
	Super::Unregister();
}

void UFogComponent::OnTransformChanged()
{
	//Transform이 바뀌면 셰이더에도 Constant Buffer Update
	const FTransform& Transform = GetGlobalTransform();
}

FVector4 UFogComponent::GetFogColor() const
{
	return FogColor;
}

float UFogComponent::GetFogDensity() const
{
	return FogDensity;
}

float UFogComponent::GetFogHeightFalloff() const
{
	return FogHeightFalloff;
}

float UFogComponent::GetFogMaxOpacity() const
{
	return FogMaxOpacity;
}

float UFogComponent::GetStartDistance() const
{
	return StartDistance;
}

void UFogComponent::SetFogColor(FVector& pColor)
{
	FogColor = pColor;
}

void UFogComponent::SetFogDensity(float pDensity)
{
	FogDensity = pDensity;
}

void UFogComponent::SetFogHeightFalloff(float pFalloff)
{
	FogHeightFalloff = pFalloff;
}

void UFogComponent::SetFogMaxOpacity(float pMaxOpacity)
{
	FogMaxOpacity = pMaxOpacity;
}

void UFogComponent::SetStartDistance(float pStartDistance)
{
	StartDistance = pStartDistance;
}

float UFogComponent::Calculaterho0(float CameraLocZ)
{
	float h0_H = CameraLocZ - GetGlobalTransform().GetLocation().Z;
	
	//지수 계산에 float 범위 넘어가지 않도록 clamp
	//h0는 카메라 높이, H는 컴포넌트 높이
	//카메라가 컴포넌트보다 훨씬 아래에 있을 때 h0 - H가 큰 음수일 때
	//지수로 너무 큰 값이 들어가서 float 범위를 넘기다.
	if (h0_H < -80.f)
	{
		h0_H = -80.f;
	}

	return FogDensity * std::exp2(-FogHeightFalloff * h0_H);
}
