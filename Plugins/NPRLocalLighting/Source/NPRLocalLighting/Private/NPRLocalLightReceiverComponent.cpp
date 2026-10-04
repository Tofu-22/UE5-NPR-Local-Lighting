#include "NPRLocalLightReceiverComponent.h"
#include "NPRLocalLightingSubsystem.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameters.h"
#include "GameFramework/Actor.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#include "LevelEditor.h"
#endif

namespace
{
    const TCHAR* CPDNames[] = { TEXT("PLR_L0_PositionRadius"), TEXT("PLR_L0_ColorWeight"),
        TEXT("PLR_L0_DirectionOuter"), TEXT("PLR_L0_Control"), TEXT("PLR_L1_PositionRadius"),
        TEXT("PLR_L1_ColorWeight"), TEXT("PLR_L1_DirectionOuter"), TEXT("PLR_L1_Control") };

    TArray<float> ReadReservedData(const UMeshComponent* Mesh)
    {
        TArray<float> Data;
        Data.Init(0.f, NPRLocalLighting::FloatCount);
        const auto& Existing = Mesh->GetCustomPrimitiveData().Data;
        for (int32 I = 0; I < FMath::Min(Existing.Num(), Data.Num()); ++I) Data[I] = Existing[I];
        return Data;
    }

    bool HasSelection(const FComponentReference& Reference)
    {
        // An EMPTY FComponentReference resolves to the root, so do not call GetComponent on it.
        return !Reference.ComponentProperty.IsNone() || !Reference.PathToComponent.IsEmpty() ||
            !Reference.OverrideComponent.IsExplicitlyNull() || !Reference.OtherActor.IsExplicitlyNull();
    }
}

UNPRLocalLightReceiverComponent::UNPRLocalLightReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UNPRLocalLightReceiverComponent::GetHeadTransform(FTransform& Out) const
{
    const auto* Mesh = GetResolvedTargetMesh();
    if (!IsValid(Mesh) || Mesh->GetWorld() != GetWorld()) return false;
    if (!HeadSocket.IsNone())
    {
        const auto* Skeletal = Cast<USkeletalMeshComponent>(Mesh);
        if (!Skeletal || !Skeletal->DoesSocketExist(HeadSocket)) return false;
        Out = Skeletal->GetSocketTransform(HeadSocket, RTS_World);
        return !Out.ContainsNaN();
    }
    const auto* Anchor = IsValid(ResolvedHeadAnchor) ? ResolvedHeadAnchor.Get() : HeadAnchor.Get();
    if (IsValid(Anchor))
    {
        if (Anchor->GetWorld() != GetWorld()) return false;
        Out = Anchor->GetComponentTransform();
        return !Out.ContainsNaN();
    }
    if (!bUseLocalHeadOffset || HeadLocalOffset.ContainsNaN()) return false;
    Out = Mesh->GetComponentTransform();
    Out.SetLocation(Out.TransformPosition(HeadLocalOffset));
    return !Out.ContainsNaN();
}

UMeshComponent* UNPRLocalLightReceiverComponent::GetResolvedTargetMesh() const
{
    return IsValid(ResolvedTargetMesh) ? ResolvedTargetMesh.Get() : TargetMesh.Get();
}

FVector UNPRLocalLightReceiverComponent::GetHeadWorldPosition() const
{
    FTransform Head;
    return GetHeadTransform(Head) ? Head.GetLocation() : FVector::ZeroVector;
}

bool UNPRLocalLightReceiverComponent::ResolveBindings(FString& Error)
{
    ResolvedTargetMesh = nullptr;
    ResolvedHeadAnchor = nullptr;
    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) { Error = TEXT("Receiver must belong to an actor."); return false; }
    if (HasSelection(MeshComponent))
    {
        ResolvedTargetMesh = Cast<UMeshComponent>(MeshComponent.GetComponent(Owner));
        if (!IsValid(ResolvedTargetMesh)) { Error = TEXT("Mesh Component selection is invalid. Select a mesh on this actor or clear the picker."); return false; }
    }
    else if (TargetMesh != nullptr)
    {
        ResolvedTargetMesh = TargetMesh;
        if (!IsValid(ResolvedTargetMesh)) { Error = TEXT("Direct TargetMesh is invalid. Clear or rebind it."); return false; }
    }
    else if (bAutoFindTargetMesh)
    {
        TInlineComponentArray<UMeshComponent*> Meshes;
        Owner->GetComponents(Meshes);
        TArray<UMeshComponent*> Compatible;
        for (auto* Mesh : Meshes)
        {
            FString AuditError;
            if (IsValid(Mesh) && AuditMaterials(Mesh, AuditError)) Compatible.Add(Mesh);
        }
        if (Compatible.Num() == 1) ResolvedTargetMesh = Compatible[0];
        else if (Compatible.IsEmpty() && Meshes.Num() == 1) ResolvedTargetMesh = Meshes[0];
        else
        {
            Error = FString::Printf(TEXT("Cannot choose a unique mesh (%d meshes, %d PL-compatible). Select Mesh Component explicitly; no first-mesh guessing."), Meshes.Num(), Compatible.Num());
            return false;
        }
    }
    if (!IsValid(ResolvedTargetMesh)) { Error = TEXT("No target mesh. Enable Auto Find Target Mesh or select Mesh Component."); return false; }
    if (ResolvedTargetMesh->GetOwner() != Owner || ResolvedTargetMesh->GetWorld() != GetWorld())
    { Error = TEXT("Target mesh must be on the receiver's own actor/world."); return false; }
    if (!HeadSocket.IsNone()) return true; // Socket errors are checked explicitly, never offset-fallback.
    if (HasSelection(HeadAnchorComponent))
    {
        ResolvedHeadAnchor = Cast<USceneComponent>(HeadAnchorComponent.GetComponent(Owner));
        if (!IsValid(ResolvedHeadAnchor)) { Error = TEXT("Head Anchor Component selection is invalid. Re-select it or clear the picker."); return false; }
    }
    else if (HeadAnchor != nullptr)
    {
        ResolvedHeadAnchor = HeadAnchor;
        if (!IsValid(ResolvedHeadAnchor)) { Error = TEXT("Direct HeadAnchor is invalid. Rebind or clear it."); return false; }
    }
    if (IsValid(ResolvedHeadAnchor) && (ResolvedHeadAnchor->GetOwner() != Owner || ResolvedHeadAnchor->GetWorld() != GetWorld()))
    { Error = TEXT("Head anchor must be on the receiver's own actor/world."); return false; }
    return true;
}

bool UNPRLocalLightReceiverComponent::AuditMaterials(const UMeshComponent* Mesh, FString& Error, float* OutRadiusScale) const
{
    if (!IsValid(Mesh)) { Error = TEXT("Target mesh is required."); return false; }
    bool bFoundABI = false;
    float RadiusScale = 1.f;
    for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
    {
        const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
        if (!Material) continue;
        int32 FoundVectors = 0;
        for (EMaterialParameterType Type : { EMaterialParameterType::Scalar, EMaterialParameterType::Vector })
        {
            TMap<FMaterialParameterInfo, FMaterialParameterMetadata> Parameters;
            // Use the base's cached metadata; MI scalar overrides must not hide the CPD index.
            Material->GetMaterial()->GetAllParametersOfType(Type, Parameters);
            for (const auto& Pair : Parameters)
            {
                const int32 Index = Pair.Value.PrimitiveDataIndex;
                if (Index == INDEX_NONE || Index >= NPRLocalLighting::FloatCount) continue;
                const bool bABI = Type == EMaterialParameterType::Vector && Index >= 0 && Index % 4 == 0 &&
                    Index / 4 < 8 && Pair.Key.Name == FName(CPDNames[Index / 4]);
                if (!bABI)
                {
                    Error = FString::Printf(TEXT("CPD conflict: slot %d, %s, index %d. Nothing overwritten."), Slot, *Pair.Key.Name.ToString(), Index);
                    return false;
                }
                ++FoundVectors;
            }
        }
        if (FoundVectors == 8)
        {
            bFoundABI = true;
            float Value = 1.f;
            if (OutRadiusScale && Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("PLR_RadiusScale")), Value))
            {
                if (!FMath::IsFinite(Value) || Value < 0.f)
                { Error = TEXT("PLR_RadiusScale must be finite and non-negative."); return false; }
                RadiusScale = FMath::Max(RadiusScale, Value);
            }
        }
    }
    if (!bFoundABI) { Error = TEXT("Target has no complete PL Runtime ABI (8 float4 parameters). Enable after material installation."); return false; }
    if (OutRadiusScale) *OutRadiusScale = RadiusScale;
    return true;
}

bool UNPRLocalLightReceiverComponent::CheckSetup(FString& Error)
{
    if (!ResolveBindings(Error)) return false;
    if (!HeadSocket.IsNone())
    {
        const auto* Skeletal = Cast<USkeletalMeshComponent>(ResolvedTargetMesh);
        if (!Skeletal || !Skeletal->DoesSocketExist(HeadSocket))
        { Error = FString::Printf(TEXT("Head Socket '%s' does not exist on the selected skeletal mesh. No offset fallback."), *HeadSocket.ToString()); return false; }
    }
    FTransform Head;
    if (!GetHeadTransform(Head))
    { Error = TEXT("Head position required: click Create Head Anchor and move it to the head, select an anchor/socket, or explicitly enable Use Local Head Offset."); return false; }
    if (!AuditMaterials(ResolvedTargetMesh, Error, &SelectionRadiusScale)) return false;
    if (GetWorld())
        if (const auto* System = GetWorld()->GetSubsystem<UNPRLocalLightingSubsystem>())
            if (!System->CanClaimReceiverMesh(this, ResolvedTargetMesh))
            { Error = TEXT("This mesh is already owned by another NPR receiver. Disable that receiver first; nothing overwritten."); return false; }
    if (!bReceiving)
        for (float Value : ReadReservedData(ResolvedTargetMesh))
            if (Value != 0.f) { Error = TEXT("Reserved CPD 0..31 already contains data; nothing overwritten."); return false; }
    return true;
}

void UNPRLocalLightReceiverComponent::ValidateSetup()
{
    FString Error;
    const bool bOK = bReceiving ? ValidateOwnership() : CheckSetup(Error);
    if (!bOK)
    {
        if (!Error.IsEmpty()) LastError = Error;
        SetupStatus = TEXT("Not ready: ") + LastError;
        return;
    }
    LastError.Empty();
    const FString HeadMode = !HeadSocket.IsNone() ? TEXT("socket ") + HeadSocket.ToString() :
        (IsValid(ResolvedHeadAnchor) ? TEXT("anchor ") + ResolvedHeadAnchor->GetName() : TEXT("EXPLICIT local offset (verify head position)"));
    SetupStatus = FString::Printf(TEXT("%s | Mesh: %s | Head: %s | %s"), bReceiving ? TEXT("Receiving") : TEXT("Ready"),
        *GetResolvedTargetMesh()->GetName(), *HeadMode, bReceiving ? TEXT("runtime CPD active") : TEXT("Run Simulate/Play; editor preview does not receive light"));
}

void UNPRLocalLightReceiverComponent::CreateHeadAnchor()
{
#if WITH_EDITOR
    if (IsTemplate() || !GetWorld() || GetWorld()->IsGameWorld())
    { LastError = TEXT("Create Head Anchor is an editor-only setup action on a placed actor, outside PIE."); SetupStatus = LastError; return; }
    FString Error;
    if (!ResolveBindings(Error)) { LastError = Error; SetupStatus = TEXT("Not ready: ") + Error; return; }
    if (!HeadSocket.IsNone())
    { LastError = TEXT("Head Socket is configured. Clear it before choosing anchor mode; no existing binding was replaced."); SetupStatus = LastError; return; }
    if (IsValid(ResolvedHeadAnchor)) { ValidateSetup(); return; } // Idempotent; never duplicate or reposition existing anchors.
    if (HeadLocalOffset.ContainsNaN()) { LastError = TEXT("Head Local Offset must be finite."); SetupStatus = LastError; return; }
    FScopedTransaction Transaction(NSLOCTEXT("NPRLocalLighting", "CreateHeadAnchor", "Create NPR Head Anchor"));
    AActor* Owner = GetOwner(); Owner->Modify(); Modify();
    auto* Anchor = NewObject<USceneComponent>(Owner, MakeUniqueObjectName(Owner, USceneComponent::StaticClass(), TEXT("NPR_HeadAnchor")), RF_Transactional);
    Anchor->Modify(); Owner->AddInstanceComponent(Anchor);
    Anchor->SetupAttachment(ResolvedTargetMesh); Anchor->SetRelativeLocation(HeadLocalOffset); Anchor->RegisterComponent();
    HeadAnchorComponent = FComponentReference();
    HeadAnchorComponent.PathToComponent = Anchor->GetPathName(Owner);
    // Keep the old input clear; picker paths duplicate/remap within each actor and persist on save/reload.
    HeadAnchor = nullptr;
    Owner->MarkPackageDirty();
    ValidateSetup();
    if (auto* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
        LevelEditor->BroadcastComponentsEdited(); // Refresh native actor's component tree without reselection.
#else
    LastError = TEXT("Create Head Anchor requires the editor. Use ConfigureReceiver or an explicit local offset at runtime.");
    SetupStatus = LastError;
#endif
}

void UNPRLocalLightReceiverComponent::RestartReceiver()
{
    DisableReceiver();
    if (GetWorld() && GetWorld()->IsGameWorld()) EnableReceiver();
    ValidateSetup(); // Editor validation never registers the subsystem or writes CPD.
}

bool UNPRLocalLightReceiverComponent::ConfigureReceiver(UMeshComponent* Mesh, USceneComponent* Anchor, FName Socket)
{
#if WITH_EDITOR
    if (GetWorld() && !GetWorld()->IsGameWorld()) Modify();
#endif
    DisableReceiver();
    MeshComponent = FComponentReference(); HeadAnchorComponent = FComponentReference();
    TargetMesh = Mesh; HeadAnchor = Anchor; HeadSocket = Socket;
    ValidateSetup();
    if (!LastError.IsEmpty()) return false;
    return GetWorld() && GetWorld()->IsGameWorld() ? EnableReceiver() : true;
}

bool UNPRLocalLightReceiverComponent::EnableReceiver()
{
    if (bReceiving) return ValidateOwnership();
    LastError.Empty();
    if (!GetWorld() || !GetWorld()->IsGameWorld())
    { LastError = TEXT("EnableReceiver requires Play/Simulate. Use Validate Setup in the editor."); SetupStatus = LastError; return false; }
    if (!CheckSetup(LastError)) { SetupStatus = TEXT("Not ready: ") + LastError; return false; }
    SavedData = ReadReservedData(ResolvedTargetMesh);
    for (float Value : SavedData)
    {
        if (Value != 0.f) { LastError = TEXT("Reserved CPD 0..31 already contains data. Receiver refused ownership."); return false; }
    }
    auto* System = GetWorld()->GetSubsystem<UNPRLocalLightingSubsystem>();
    if (!System || !System->ClaimReceiverMesh(this, ResolvedTargetMesh))
    {
        SavedData.Empty();
        LastError = TEXT("Could not claim mesh ownership; another receiver may already own it.");
        SetupStatus = TEXT("Not ready: ") + LastError;
        return false;
    }
    OwnedMesh = ResolvedTargetMesh;
    LastWrittenData = SavedData;
    bReceiving = true;
    System->RegisterReceiver(this);
    SetupStatus = TEXT("Receiving | ") + ResolvedTargetMesh->GetName();
    return true;
}

bool UNPRLocalLightReceiverComponent::ValidateOwnership()
{
    FString Error;
    if (!bReceiving || !OwnedMesh.IsValid() || !CheckSetup(Error) || OwnedMesh.Get() != ResolvedTargetMesh.Get())
    {
        LastError = Error.IsEmpty() ? TEXT("Target or ownership changed.") : Error;
        DisableReceiver();
        SetupStatus = TEXT("Not ready: ") + LastError;
        return false;
    }
    if (ReadReservedData(OwnedMesh.Get()) != LastWrittenData)
    {
        LastError = TEXT("Another system wrote reserved CPD. Receiver stopped without overwriting its data.");
        DisableReceiver();
        SetupStatus = TEXT("Not ready: ") + LastError;
        return false;
    }
    return true;
}

bool UNPRLocalLightReceiverComponent::WriteSamples(const TArray<FNPRLocalLightSample>& Samples)
{
    if (!bReceiving || !OwnedMesh.IsValid()) return false;
    // Detect external writers before every batch, not only at the 10 Hz material audit.
    if (ReadReservedData(OwnedMesh.Get()) != LastWrittenData)
    {
        LastError = TEXT("External CPD writer detected; lighting stopped without overwriting.");
        DisableReceiver();
        return false;
    }
    TArray<float> Data;
    Data.Init(0.f, NPRLocalLighting::FloatCount);
    for (int32 Slot = 0; Slot < FMath::Min(Samples.Num(), NPRLocalLighting::SlotCount); ++Slot)
    {
        const auto& Sample = Samples[Slot];
        const FVector4 Vectors[] = { Sample.PositionRadius, Sample.ColorWeight, Sample.DirectionOuter, Sample.Control };
        for (int32 V = 0; V < 4; ++V)
            for (int32 C = 0; C < 4; ++C)
                Data[Slot * 16 + V * 4 + C] = FMath::IsFinite(Vectors[V][C]) ? float(Vectors[V][C]) : 0.f;
    }
    if (Data != LastWrittenData)
    {
        OwnedMesh->SetCustomPrimitiveDataFloatArray(0, Data);
        LastWrittenData = MoveTemp(Data);
    }
    return true;
}

void UNPRLocalLightReceiverComponent::ClearSamples() { WriteSamples({}); }

void UNPRLocalLightReceiverComponent::ReleaseData()
{
    // Do not restore over a new owner's write. Indices 32+ are never touched.
    if (OwnedMesh.IsValid() && ReadReservedData(OwnedMesh.Get()) == LastWrittenData)
        OwnedMesh->SetCustomPrimitiveDataFloatArray(0, SavedData);
    OwnedMesh.Reset();
    LastWrittenData.Empty();
    SavedData.Empty();
}

void UNPRLocalLightReceiverComponent::DisableReceiver()
{
    if (GetWorld())
        if (auto* System = GetWorld()->GetSubsystem<UNPRLocalLightingSubsystem>())
        {
            System->UnregisterReceiver(this);
            System->ReleaseReceiverMesh(this, OwnedMesh.Get());
        }
    ReleaseData();
    bReceiving = false;
    bManualInput = false;
}

bool UNPRLocalLightReceiverComponent::InjectManualSamples(const TArray<FNPRLocalLightSample>& Samples)
{
    if (Samples.Num() > 2) { LastError = TEXT("At most two manual samples allowed."); return false; }
    if (!bReceiving && !EnableReceiver()) return false;
    bManualInput = true;
    return WriteSamples(Samples);
}

void UNPRLocalLightReceiverComponent::ResumeAutomaticLighting()
{
    bManualInput = false;
    ClearSamples();
    if (bReceiving)
        if (auto* System = GetWorld()->GetSubsystem<UNPRLocalLightingSubsystem>())
        { System->UnregisterReceiver(this); System->RegisterReceiver(this); }
}

void UNPRLocalLightReceiverComponent::OnRegister()
{
    Super::OnRegister();
    if (!IsTemplate() && GetOwner() && GetWorld() && !GetWorld()->IsGameWorld()) ValidateSetup();
}
void UNPRLocalLightReceiverComponent::BeginPlay() { Super::BeginPlay(); EnableReceiver(); }
void UNPRLocalLightReceiverComponent::EndPlay(const EEndPlayReason::Type Reason) { DisableReceiver(); Super::EndPlay(Reason); }
void UNPRLocalLightReceiverComponent::OnUnregister() { DisableReceiver(); Super::OnUnregister(); }
