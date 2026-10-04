#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "NPRLocalLightingTypes.h"
#include "NPRLocalLightReceiverComponent.generated.h"

class UMeshComponent;
class USceneComponent;

UCLASS(ClassGroup=(NPR), meta=(BlueprintSpawnableComponent))
class NPRLOCALLIGHTING_API UNPRLocalLightReceiverComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UNPRLocalLightReceiverComponent();

    // Original inputs stay serialized and callable for old actors, MCP and Blueprint graphs.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="NPR|Setup", meta=(DisplayName="Target Mesh (Direct / MCP)")) TObjectPtr<UMeshComponent> TargetMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="NPR|Setup", meta=(DisplayName="Head Anchor (Direct / MCP)")) TObjectPtr<USceneComponent> HeadAnchor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Setup", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.MeshComponent", DisplayName="Mesh Component", ToolTip="Optional same-actor component picker. Takes priority over the direct reference. Leave empty for automatic discovery."))
    FComponentReference MeshComponent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Setup", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.SceneComponent", DisplayName="Head Anchor Component", ToolTip="Optional same-actor head anchor. Move it to the actual head center. An explicit Head Socket takes priority."))
    FComponentReference HeadAnchorComponent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Setup") bool bAutoFindTargetMesh = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Setup", meta=(ToolTip="Optional explicit skeletal socket or bone. Invalid names are errors, never silently replaced by an offset.")) FName HeadSocket;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Setup", meta=(ToolTip="Use a mesh-local point only if no socket or anchor is assigned. Off by default; head position is not guessed from whole-body bounds.")) bool bUseLocalHeadOffset = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Setup", meta=(ToolTip="Mesh-local centimeters. Also used as the initial location for Create Head Anchor. 150cm is only a starting value for this project's model.")) FVector HeadLocalOffset = FVector(0, 0, 150);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Art", meta=(ClampMin="0")) float ArtisticStrength = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Art", meta=(ClampMin="0.0001")) float PhysicalReference = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Art", meta=(ClampMin="0.0001")) float UnitlessReference = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPR|Occlusion", meta=(ClampMin="0")) float HeadSampleOffsetCm = 6.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPR|Diagnostics") FString LastError;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="NPR|Diagnostics") bool bReceiving = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="NPR|Diagnostics") bool bManualInput = false;
    // Largest effective scale among PL-compatible material slots, refreshed at selection cadence.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="NPR|Diagnostics") float SelectionRadiusScale = 1.f;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="NPR|Diagnostics") TObjectPtr<UMeshComponent> ResolvedTargetMesh;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="NPR|Diagnostics") TObjectPtr<USceneComponent> ResolvedHeadAnchor;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="NPR|Diagnostics") FString SetupStatus;

    // Zero-parameter void functions appear as native Details buttons (no custom UI required).
    UFUNCTION(BlueprintCallable, CallInEditor, Category="NPR|Setup") void ValidateSetup();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="NPR|Setup") void CreateHeadAnchor();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="NPR|Setup") void RestartReceiver();
    UFUNCTION(BlueprintCallable, Category="NPR|Setup") bool ConfigureReceiver(UMeshComponent* Mesh, USceneComponent* Anchor, FName Socket);
    UFUNCTION(BlueprintPure, Category="NPR|Setup") UMeshComponent* GetResolvedTargetMesh() const;
    UFUNCTION(BlueprintPure, Category="NPR|Setup") FVector GetHeadWorldPosition() const;

    UFUNCTION(BlueprintCallable, Category="NPR") bool EnableReceiver();
    UFUNCTION(BlueprintCallable, Category="NPR") void DisableReceiver();
    UFUNCTION(BlueprintCallable, Category="NPR") bool InjectManualSamples(const TArray<FNPRLocalLightSample>& Samples);
    UFUNCTION(BlueprintCallable, Category="NPR") void ResumeAutomaticLighting();
    UFUNCTION(BlueprintPure, Category="NPR") TArray<float> GetLastWrittenData() const { return LastWrittenData; }

    bool GetHeadTransform(FTransform& Out) const;
    bool ValidateOwnership();
    bool WriteSamples(const TArray<FNPRLocalLightSample>& Samples);
    void ClearSamples();

protected:
    virtual void OnRegister() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void OnUnregister() override;

private:
    bool ResolveBindings(FString& Error);
    bool CheckSetup(FString& Error);
    bool AuditMaterials(const UMeshComponent* Mesh, FString& Error, float* OutRadiusScale = nullptr) const;
    void ReleaseData();
    TWeakObjectPtr<UMeshComponent> OwnedMesh;
    TArray<float> SavedData;
    TArray<float> LastWrittenData;
};
