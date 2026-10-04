#pragma once

#include "CoreMinimal.h"
#include "NPRLocalLightingTypes.generated.h"

// ABI v1: two slots, sixteen floats per slot, absolute CPD indices 0..31.
namespace NPRLocalLighting
{
    constexpr int32 SlotCount = 2;
    constexpr int32 FloatsPerSlot = 16;
    constexpr int32 FloatCount = 32;
    constexpr float SelectionInterval = 0.1f;
    constexpr float OcclusionInterval = 0.1f;
    constexpr float VisibilitySmoothTime = 0.15f;
    constexpr float MaxVisibilityAge = 0.5f;
    constexpr float SwitchTime = 0.15f;
    constexpr int32 MaxRaysPerFrame = 32;

    inline float MapEnergy(float Energy, float Reference)
    {
        if (!FMath::IsFinite(Energy) || Energy <= 0.f) return 0.f;
        return Energy / (Energy + FMath::Max(Reference, 0.0001f));
    }

    inline FVector3f CapFill(const FVector3f& Value, float Limit)
    {
        const float MaxRGB = FMath::Max3(Value.X, Value.Y, Value.Z);
        return Value * (MaxRGB > 0.f ? FMath::Min(1.f, FMath::Max(0.f, Limit) / MaxRGB) : 0.f);
    }

    // Mirrors UE's wired SphereMask hardness input, INCLUDING tutorial Hardness * 100.
    inline float TutorialMask(float Distance, float Radius, float AO, float Hardness, float Smooth, float Offset)
    {
        const float Sphere = FMath::Clamp((1.f - Distance / FMath::Max(Radius, 0.00001f)) /
            FMath::Max(1.f - Hardness * 100.f, 0.00001f), 0.f, 1.f);
        const float Input = Sphere * AO;
        return Smooth <= 0.f ? (Input >= Offset ? 1.f : 0.f) : FMath::Clamp(Input / Smooth - Offset, 0.f, 1.f);
    }
}

USTRUCT(BlueprintType)
struct NPRLOCALLIGHTING_API FNPRLocalLightSample
{
    GENERATED_BODY()

    // XYZ relative to TargetMesh->GetActorPositionForRenderer() (shader ActorPositionWS), W physical radius in cm.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR") FVector4 PositionRadius = FVector4(0, 0, 0, 0);
    // RGB normalized scene chroma, W mapped brightness * art * visibility * fade.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR") FVector4 ColorWeight = FVector4(0, 0, 0, 0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR") FVector4 DirectionOuter = FVector4(1, 0, 0, -1);
    // X inner cosine, Y 0=point/1=spot, ZW reserved zero.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR") FVector4 Control = FVector4(1, 0, 0, 0);
};
