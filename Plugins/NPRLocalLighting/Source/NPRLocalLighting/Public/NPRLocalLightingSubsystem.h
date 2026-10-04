#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NPRLocalLightingTypes.h"
#include "NPRLocalLightingSubsystem.generated.h"

class UPointLightComponent;
class UNPRLocalLightReceiverComponent;
class ULevel;
class UMeshComponent;

struct FNPRLightFrame
{
    FVector Position = FVector::ZeroVector;
    FVector Direction = FVector::ForwardVector;
    FLinearColor Energy = FLinearColor::Black;
    float Radius = 0.f;
    float InnerCos = 1.f;
    float OuterCos = -1.f;
    uint8 Channels = 0;
    bool bSpot = false;
    bool bInverseSquare = false;
    bool bShadows = false;
    bool bValid = false;
    uint64 Frame = MAX_uint64;
};

struct FNPRTrackedLight
{
    FNPRLightFrame Data;
    TArray<FIntVector> Cells;
    bool bGlobal = false;
};

struct FNPRVisibility
{
    float Target = 0.f;
    float Smoothed = 0.f;
    double LastResult = -1.e10;
    double LastRequest = -1.e10;
    uint32 Generation = 0;
    int32 Received = 0;
    int32 Visible = 0;
    bool bPending = false;
};

struct FNPRLightSlot
{
    TWeakObjectPtr<UPointLightComponent> Current;
    TWeakObjectPtr<UPointLightComponent> Pending;
    float Fade = 0.f;
};

struct FNPRReceiverState
{
    TArray<TWeakObjectPtr<UPointLightComponent>> Candidates;
    TMap<TWeakObjectPtr<UPointLightComponent>, FNPRVisibility> Visibility;
    FNPRLightSlot Slots[2];
    bool bNeedsSelection = true;
};

UCLASS()
class NPRLOCALLIGHTING_API UNPRLocalLightingSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    UFUNCTION(BlueprintCallable, Category="NPR") void RegisterLight(UPointLightComponent* Light);
    UFUNCTION(BlueprintCallable, Category="NPR") void UnregisterLight(UPointLightComponent* Light);
    void RegisterReceiver(UNPRLocalLightReceiverComponent* Receiver);
    void UnregisterReceiver(UNPRLocalLightReceiverComponent* Receiver);
    bool ClaimReceiverMesh(UNPRLocalLightReceiverComponent* Receiver, UMeshComponent* Mesh);
    void ReleaseReceiverMesh(UNPRLocalLightReceiverComponent* Receiver, UMeshComponent* Mesh);
    bool CanClaimReceiverMesh(const UNPRLocalLightReceiverComponent* Receiver, UMeshComponent* Mesh) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPR|Diagnostics") int32 LastFrameRayCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPR|Diagnostics") int32 RegisteredLightCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPR|Diagnostics") int32 RegisteredReceiverCount = 0;

private:
    void ScanActor(AActor* Actor);
    void ScanLevel(ULevel* Level);
    void OnActorSpawned(AActor* Actor);
    void OnLevelAdded(ULevel* Level, UWorld* World);
    void OnLevelRemoved(ULevel* Level, UWorld* World);
    void UpdateSpatialIndex();
    void RemoveFromIndex(TWeakObjectPtr<UPointLightComponent> Light, FNPRTrackedLight& Record);
    const FNPRLightFrame* ReadLight(TWeakObjectPtr<UPointLightComponent> Light);
    float ScoreLight(const FNPRLightFrame& Light, const UNPRLocalLightReceiverComponent& Receiver, const FVector& Head) const;
    void UpdateCandidates(UNPRLocalLightReceiverComponent& Receiver, FNPRReceiverState& State);
    void ScheduleOcclusion();
    void UpdateReceiver(UNPRLocalLightReceiverComponent& Receiver, FNPRReceiverState& State, float DeltaTime);

    TMap<TWeakObjectPtr<UPointLightComponent>, FNPRTrackedLight> Lights;
    TMap<FIntVector, TSet<TWeakObjectPtr<UPointLightComponent>>> Grid;
    TSet<TWeakObjectPtr<UPointLightComponent>> GlobalLights;
    TMap<TWeakObjectPtr<UNPRLocalLightReceiverComponent>, FNPRReceiverState> Receivers;
    TMap<TWeakObjectPtr<UMeshComponent>, TWeakObjectPtr<UNPRLocalLightReceiverComponent>> MeshOwners;
    TArray<TWeakObjectPtr<AActor>> PendingSpawned;
    FDelegateHandle SpawnHandle, LevelAddedHandle, LevelRemovedHandle;
    double LastSelection = -1.e10;
    int32 RayCursor = 0;
    uint32 NextGeneration = 0;
    uint64 DataFrame = 0;
    float SpatialRadiusScale = 1.f;
    bool bShuttingDown = false;
};
