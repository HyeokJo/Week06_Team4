#pragma once
#include "runtime/coreuobject/uscenecomponent.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime\Math\FVector.h"
#include "Runtime\Rendering\ShaderConstants.h"
#include "Runtime/Engine/UWorld.h"

class UPointLightComponent : public USceneComponent {
	GENERATED_BODY()
	DECLARE_UCLASS(UPointLightComponent, USceneComponent)
public:
	using Super::Serialize;
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; }

	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }

	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector4& InColor) { LightColor = InColor; }

	void Serialize(FArchive& Archive) override {
		Super::Serialize(Archive);
		Archive.Field("Intensity", Intensity);
		Archive.Field("AttenuationRadius", AttenuationRadius);
		Archive.Field("LightColor", LightColor);
	}

	void Register(ULevel& InLevel) override {
		if (Level == &InLevel)
			return;
		Super::Register(InLevel);
		RegisterToScene();
	}
	void Unregister() override {
		if (bSceneRegistered)
			UnregisterFromScene();
		Super::Unregister();
	}

	FPointLightConstants GetShaderConstant() const {
		return FPointLightConstants{
			.Position = GetGlobalTransform().GetLocation(),
			.Intensity = Intensity,
			.LightColor = FVector{LightColor.X, LightColor.Y, LightColor.Z},
			.AttenuationRadius = AttenuationRadius
		};
	}
	bool RegisterToScene() {
		if (bSceneRegistered)
			return true;
		if (!IsRegistered()) {
			UE_DEBUG_LOG_WARN("Component is Not Registered Yet");
			return false;
		}
		else {
			bool result = Level->GetWorld()->GetScene()->AddPointLight(this);
			if (result) {
				bSceneRegistered = true; return true;
			}
			else return false;
		}
	};
	bool UnregisterFromScene() {
		if (!bSceneRegistered)
			return true;
		if (!IsRegistered()) {
			UE_DEBUG_LOG_WARN("Component without Level");
			return false;
		}
		else {
			bool result = Level->GetWorld()->GetScene()->RemovePointLight(this);
			if (result) {
				bSceneRegistered = false; return true;
			}
			else return false;
		}
	}
private:
	bool bSceneRegistered = false;

	float Intensity = 1.0f;
	float AttenuationRadius = 1.0f;
	FVector4 LightColor{1.0f, 1.0f, 1.0f};
	//uint32 bUseInverseSquaredFalloff = 1;
	//float LightFalloffExponent = 1.0f;
	//float SourceRadius = 0.0f;
	//float SoftSourceRadius = 0.0f;
	//float SourceLength = 0.0f;
	//uint32 bUseTemperature = 1;
	//float Temperature = 0.0f;
	//uint32 CastShadows = 1;
	//float ShadowBias = 0.0f;
	//float ShadowSlopeBias = 0.0f;
	//float IndirectLightingIntensity = 0.0f;
	//float VolumetricScatteringIntensity = 0.0f;
};
