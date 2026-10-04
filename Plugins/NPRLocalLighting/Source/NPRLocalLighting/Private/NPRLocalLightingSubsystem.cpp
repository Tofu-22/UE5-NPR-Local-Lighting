#include "NPRLocalLightingSubsystem.h"
#include "NPRLocalLightReceiverComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "WorldCollision.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
    constexpr double CellSize = 500.0;
    FIntVector CellAt(const FVector& P)
    { return FIntVector(FMath::FloorToInt(P.X / CellSize), FMath::FloorToInt(P.Y / CellSize), FMath::FloorToInt(P.Z / CellSize)); }

    bool LiveLight(const UPointLightComponent* Light)
    {
        return IsValid(Light) && Light->IsRegistered() && Light->IsVisible() && Light->bAffectsWorld &&
            Light->GetOwner() && !Light->GetOwner()->IsHidden();
    }
    float Luminance(const FLinearColor& C) { return FMath::Max(0.f, C.R * .2126f + C.G * .7152f + C.B * .0722f); }
    float SpotAt(const FNPRLightFrame& L, const FVector& P)
    {
        if (!L.bSpot) return 1.f;
        const float Cos = float(FVector::DotProduct((P - L.Position).GetSafeNormal(), L.Direction));
        const float T = FMath::Clamp((Cos - L.OuterCos) / FMath::Max(L.InnerCos - L.OuterCos, .00001f), 0.f, 1.f);
        return T * T * (3.f - 2.f * T);
    }
    float MappedBrightness(const FNPRLightFrame& Light, const UNPRLocalLightReceiverComponent& Receiver, const FVector& Head)
    {
        const float Energy = Luminance(Light.Energy);
        const float Approximate = Light.bInverseSquare ? Energy / FMath::Max(float(FVector::DistSquared(Light.Position, Head)), 25.f) : Energy;
        return NPRLocalLighting::MapEnergy(Approximate, Light.bInverseSquare ? Receiver.PhysicalReference : Receiver.UnitlessReference);
    }
}

bool UNPRLocalLightingSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Type == EWorldType::Game || Type == EWorldType::PIE; }

void UNPRLocalLightingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bShuttingDown = false;
    SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::OnActorSpawned));
    LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ThisClass::OnLevelAdded);
    LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &ThisClass::OnLevelRemoved);
    // Initialize can precede PIE's placed-actor/component duplication. Subscribe here,
    // but perform the one initial scan only once the world is ready for gameplay.
}

void UNPRLocalLightingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (bShuttingDown || &InWorld != GetWorld()) return;
    for (ULevel* Level : InWorld.GetLevels()) ScanLevel(Level);
    LastSelection = -1.e10;
}

void UNPRLocalLightingSubsystem::Deinitialize()
{
    bShuttingDown = true;
    GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
    FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
    FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
    // Copy first: DisableReceiver unregisters itself from the map.
    TArray<TWeakObjectPtr<UNPRLocalLightReceiverComponent>> Keys;
    Receivers.GetKeys(Keys);
    for (auto Receiver : Keys) if (Receiver.IsValid()) Receiver->DisableReceiver();
    Receivers.Empty(); MeshOwners.Empty(); Lights.Empty(); Grid.Empty(); GlobalLights.Empty(); PendingSpawned.Empty();
    Super::Deinitialize();
}

TStatId UNPRLocalLightingSubsystem::GetStatId() const
{ RETURN_QUICK_DECLARE_CYCLE_STAT(UNPRLocalLightingSubsystem, STATGROUP_Tickables); }

void UNPRLocalLightingSubsystem::ScanActor(AActor* Actor)
{
    if (!IsValid(Actor) || Actor->GetWorld() != GetWorld()) return;
    TInlineComponentArray<UPointLightComponent*> Components;
    Actor->GetComponents(Components);
    for (auto* Light : Components) RegisterLight(Light);
}

void UNPRLocalLightingSubsystem::ScanLevel(ULevel* Level)
{ if (Level) for (AActor* Actor : Level->Actors) ScanActor(Actor); }

void UNPRLocalLightingSubsystem::OnActorSpawned(AActor* Actor)
{ PendingSpawned.Add(Actor); } // Actor construction/components may not be finished inside spawn delegate.

void UNPRLocalLightingSubsystem::OnLevelAdded(ULevel* Level, UWorld* World)
{ if (World == GetWorld()) ScanLevel(Level); }

void UNPRLocalLightingSubsystem::OnLevelRemoved(ULevel* Level, UWorld* World)
{
    if (World != GetWorld()) return;
    for (auto It = Lights.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || !Level || It.Key()->GetComponentLevel() == Level)
        { RemoveFromIndex(It.Key(), It.Value()); It.RemoveCurrent(); }
    }
}

void UNPRLocalLightingSubsystem::RegisterLight(UPointLightComponent* Light)
{
    if (!bShuttingDown && IsValid(Light) && Light->GetWorld() == GetWorld())
    { Lights.FindOrAdd(Light); LastSelection = -1.e10; }
}

void UNPRLocalLightingSubsystem::UnregisterLight(UPointLightComponent* Light)
{
    if (auto* Record = Lights.Find(Light)) { RemoveFromIndex(Light, *Record); Lights.Remove(Light); }
}

void UNPRLocalLightingSubsystem::RegisterReceiver(UNPRLocalLightReceiverComponent* Receiver)
{
    if (!bShuttingDown && IsValid(Receiver) && Receiver->GetWorld() == GetWorld())
    { Receivers.FindOrAdd(Receiver); LastSelection = -1.e10; }
}

void UNPRLocalLightingSubsystem::UnregisterReceiver(UNPRLocalLightReceiverComponent* Receiver)
{ Receivers.Remove(Receiver); }

bool UNPRLocalLightingSubsystem::CanClaimReceiverMesh(const UNPRLocalLightReceiverComponent* Receiver, UMeshComponent* Mesh) const
{
    const auto* Owner = MeshOwners.Find(Mesh);
    return !Owner || !Owner->IsValid() || Owner->Get() == Receiver;
}

bool UNPRLocalLightingSubsystem::ClaimReceiverMesh(UNPRLocalLightReceiverComponent* Receiver, UMeshComponent* Mesh)
{
    if (bShuttingDown || !IsValid(Receiver) || !IsValid(Mesh) || Receiver->GetWorld() != GetWorld() ||
        Mesh->GetWorld() != GetWorld() || !CanClaimReceiverMesh(Receiver, Mesh)) return false;
    MeshOwners.Add(Mesh, Receiver);
    return true;
}

void UNPRLocalLightingSubsystem::ReleaseReceiverMesh(UNPRLocalLightReceiverComponent* Receiver, UMeshComponent* Mesh)
{
    const auto* Owner = MeshOwners.Find(Mesh);
    if (Owner && Owner->Get() == Receiver) MeshOwners.Remove(Mesh);
}

const FNPRLightFrame* UNPRLocalLightingSubsystem::ReadLight(TWeakObjectPtr<UPointLightComponent> Light)
{
    auto* Record = Lights.Find(Light);
    if (!Record) return nullptr;
    FNPRLightFrame& D = Record->Data;
    if (D.Frame == DataFrame) return &D;
    D.Frame = DataFrame;
    D.bValid = LiveLight(Light.Get());
    if (!D.bValid) return &D;
    const auto* C = Light.Get();
    D.Position = C->GetComponentLocation();
    D.Direction = C->GetDirection().GetSafeNormal();
    D.Radius = C->AttenuationRadius;
    D.Channels = GetLightingChannelMaskForStruct(C->LightingChannels);
    D.bShadows = C->CastShadows;
    D.bInverseSquare = C->bUseInverseSquaredFalloff;
    D.Energy = C->GetColoredLightBrightness(); // Units + temperature handled by engine.
    D.bValid = FMath::IsFinite(D.Radius) && D.Radius > 0.f && !D.Position.ContainsNaN() &&
        FMath::IsFinite(D.Energy.R) && FMath::IsFinite(D.Energy.G) && FMath::IsFinite(D.Energy.B) && Luminance(D.Energy) > 0.f;
    D.bSpot = false; D.InnerCos = 1.f; D.OuterCos = -1.f;
    if (const auto* Spot = Cast<USpotLightComponent>(C))
    {
        D.bSpot = true;
        const FVector2f Angles = Spot->GetClampedConeAngles();
        D.InnerCos = FMath::Cos(Angles.X); D.OuterCos = FMath::Cos(Angles.Y);
    }
    return &D;
}

void UNPRLocalLightingSubsystem::RemoveFromIndex(TWeakObjectPtr<UPointLightComponent> Light, FNPRTrackedLight& Record)
{
    for (const FIntVector& Cell : Record.Cells)
        if (auto* Set = Grid.Find(Cell)) { Set->Remove(Light); if (Set->IsEmpty()) Grid.Remove(Cell); }
    GlobalLights.Remove(Light);
    Record.Cells.Empty(); Record.bGlobal = false;
}

void UNPRLocalLightingSubsystem::UpdateSpatialIndex()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(NPR_PL_SpatialIndex);
    for (auto It = Lights.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { RemoveFromIndex(It.Key(), It.Value()); It.RemoveCurrent(); continue; }
        const FNPRLightFrame* D = ReadLight(It.Key());
        if (!D->bValid) { RemoveFromIndex(It.Key(), It.Value()); continue; }
        // CPD keeps the physical radius; material scales it exactly once. Expand only CPU search bounds.
        const double Radius = double(D->Radius) * SpatialRadiusScale;
        const bool bLargeRadius = Radius > CellSize * 8;
        const FIntVector Min = bLargeRadius ? FIntVector::ZeroValue : CellAt(D->Position - FVector(Radius));
        const FIntVector Max = bLargeRadius ? FIntVector::ZeroValue : CellAt(D->Position + FVector(Radius));
        const double Count = (double(Max.X) - Min.X + 1) * (double(Max.Y) - Min.Y + 1) * (double(Max.Z) - Min.Z + 1);
        TArray<FIntVector> Cells;
        const bool bGlobal = bLargeRadius || Count > 512;
        if (D->bValid && !bGlobal)
            for (int32 X = Min.X; X <= Max.X; ++X)
                for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
                    for (int32 Z = Min.Z; Z <= Max.Z; ++Z) Cells.Emplace(X, Y, Z);
        if (Cells == It.Value().Cells && bGlobal == It.Value().bGlobal) continue;
        RemoveFromIndex(It.Key(), It.Value());
        It.Value().Cells = MoveTemp(Cells); It.Value().bGlobal = bGlobal;
        for (const auto& Cell : It.Value().Cells) Grid.FindOrAdd(Cell).Add(It.Key());
        if (bGlobal) GlobalLights.Add(It.Key());
    }
}

float UNPRLocalLightingSubsystem::ScoreLight(const FNPRLightFrame& L, const UNPRLocalLightReceiverComponent& Receiver, const FVector& Head) const
{
    const auto* Mesh = Receiver.GetResolvedTargetMesh();
    if (!L.bValid || !IsValid(Mesh) ||
        !(L.Channels & GetLightingChannelMaskForStruct(Mesh->LightingChannels))) return 0.f;
    const float Distance = float(FVector::Distance(L.Position, Head));
    const float Radius = L.Radius * Receiver.SelectionRadiusScale;
    if (Distance >= Radius) return 0.f;
    return MappedBrightness(L, Receiver, Head) * FMath::Max(0.f, Receiver.ArtisticStrength) *
        (1.f - Distance / Radius) * SpotAt(L, Head);
}

void UNPRLocalLightingSubsystem::UpdateCandidates(UNPRLocalLightReceiverComponent& Receiver, FNPRReceiverState& State)
{
    State.bNeedsSelection = true;
    FTransform Head;
    if (!Receiver.GetHeadTransform(Head)) { State.Candidates.Empty(); return; }
    TSet<TWeakObjectPtr<UPointLightComponent>> Nearby = GlobalLights;
    if (const auto* Cell = Grid.Find(CellAt(Head.GetLocation()))) Nearby.Append(*Cell);
    TArray<TPair<TWeakObjectPtr<UPointLightComponent>, float>> Ranked;
    for (auto Light : Nearby)
        if (const auto* D = ReadLight(Light))
        {
            const float Score = ScoreLight(*D, Receiver, Head.GetLocation());
            if (Score > 0.f) Ranked.Emplace(Light, Score);
        }
    Ranked.Sort([](const auto& A, const auto& B)
    { return A.Value == B.Value ? A.Key->GetUniqueID() < B.Key->GetUniqueID() : A.Value > B.Value; });
    State.Candidates.Empty();
    // Hysteresis can retain lights outside the raw top four. Keep their visibility fresh,
    // without exceeding four scheduled candidates. Pending identities are pinned during fades.
    auto KeepViable = [&](TWeakObjectPtr<UPointLightComponent> Light)
    {
        if (State.Candidates.Num() >= 4 || State.Candidates.Contains(Light)) return;
        const auto* D = ReadLight(Light);
        if (D && ScoreLight(*D, Receiver, Head.GetLocation()) > 0.f)
        { State.Candidates.Add(Light); State.Visibility.FindOrAdd(Light); }
    };
    for (const auto& Slot : State.Slots) KeepViable(Slot.Current);
    for (const auto& Slot : State.Slots) KeepViable(Slot.Pending);
    for (const auto& Entry : Ranked) KeepViable(Entry.Key);
    // Keep current fade-out light's occlusion while dropping abandoned entries.
    for (auto It = State.Visibility.CreateIterator(); It; ++It)
        if (!State.Candidates.Contains(It.Key()) && It.Key() != State.Slots[0].Current && It.Key() != State.Slots[1].Current)
            It.RemoveCurrent();
}

void UNPRLocalLightingSubsystem::ScheduleOcclusion()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(NPR_PL_OcclusionSchedule);
    struct FJob { TWeakObjectPtr<UNPRLocalLightReceiverComponent> Receiver; TWeakObjectPtr<UPointLightComponent> Light; };
    TArray<FJob> Jobs;
    for (auto& Pair : Receivers)
        if (Pair.Key.IsValid() && !Pair.Key->bManualInput)
            for (auto Light : Pair.Value.Candidates) Jobs.Add({ Pair.Key, Light });
    if (Jobs.IsEmpty()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const int32 Start = RayCursor % Jobs.Num();
    for (int32 I = 0; I < Jobs.Num(); ++I)
    {
        const int32 Index = (Start + I) % Jobs.Num();
        const FJob Job = Jobs[Index];
        auto* State = Receivers.Find(Job.Receiver);
        auto* Visibility = State ? State->Visibility.Find(Job.Light) : nullptr;
        const auto* Data = ReadLight(Job.Light);
        if (!Visibility || !Data || !Data->bValid) continue;
        if (!Data->bShadows)
        { Visibility->Target = 1.f; Visibility->Smoothed = 1.f; Visibility->LastResult = Now; Visibility->bPending = false; continue; }
        if (Visibility->bPending && Now - Visibility->LastRequest <= NPRLocalLighting::MaxVisibilityAge) continue;
        if (Now - Visibility->LastRequest < NPRLocalLighting::OcclusionInterval) continue;
        if (LastFrameRayCount + 3 > NPRLocalLighting::MaxRaysPerFrame) { RayCursor = Index; break; }
        FTransform Head;
        if (!Job.Receiver->GetHeadTransform(Head)) continue;
        Visibility->Generation = ++NextGeneration;
        Visibility->Received = 0; Visibility->Visible = 0;
        Visibility->LastRequest = Now; Visibility->bPending = true;
        const uint32 Generation = Visibility->Generation;
        const FVector Position = Head.GetLocation();
        const FVector Offset = Head.GetUnitAxis(EAxis::X) * Job.Receiver->HeadSampleOffsetCm;
        const FVector Starts[] = { Position, Position - Offset, Position + Offset };
        const TWeakObjectPtr<UNPRLocalLightingSubsystem> WeakThis(this);
        for (const FVector& From : Starts)
        {
            FCollisionQueryParams Params(SCENE_QUERY_STAT(NPRLocalLighting), false);
            Params.AddIgnoredActor(Job.Receiver->GetOwner());
            if (auto* Mesh = Job.Receiver->GetResolvedTargetMesh())
                if (Mesh->GetOwner() != Job.Receiver->GetOwner()) Params.AddIgnoredActor(Mesh->GetOwner());
            FTraceDelegate Delegate;
            Delegate.BindLambda([WeakThis, Job, Generation](const FTraceHandle&, FTraceDatum& Result)
            {
                if (!WeakThis.IsValid() || WeakThis->bShuttingDown || !Job.Receiver.IsValid() || !Job.Light.IsValid()) return;
                auto* S = WeakThis->Receivers.Find(Job.Receiver);
                auto* V = S ? S->Visibility.Find(Job.Light) : nullptr;
                if (!V || V->Generation != Generation || !V->bPending) return;
                bool bBlocked = false;
                for (const auto& Hit : Result.OutHits) bBlocked |= Hit.bBlockingHit;
                V->Visible += bBlocked ? 0 : 1;
                if (++V->Received == 3)
                {
                    V->Target = V->Visible / 3.f;
                    V->LastResult = WeakThis->GetWorld()->GetTimeSeconds();
                    V->bPending = false;
                }
            });
            GetWorld()->AsyncLineTraceByChannel(EAsyncTraceType::Test, From, Data->Position, ECC_Visibility,
                Params, FCollisionResponseParams::DefaultResponseParam, &Delegate);
            ++LastFrameRayCount;
        }
        RayCursor = (Index + 1) % Jobs.Num();
    }
}

void UNPRLocalLightingSubsystem::UpdateReceiver(UNPRLocalLightReceiverComponent& Receiver, FNPRReceiverState& State, float DeltaTime)
{
    if (Receiver.bManualInput) return;
    FTransform Head;
    const auto* Mesh = Receiver.GetResolvedTargetMesh();
    if (!Receiver.GetHeadTransform(Head) || !IsValid(Mesh)) { Receiver.ClearSamples(); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    for (auto& Pair : State.Visibility)
    {
        if (Now - Pair.Value.LastResult > NPRLocalLighting::MaxVisibilityAge) Pair.Value.Smoothed = 0.f;
        else Pair.Value.Smoothed = FMath::Lerp(Pair.Value.Smoothed, Pair.Value.Target,
            1.f - FMath::Exp(-DeltaTime / NPRLocalLighting::VisibilitySmoothTime));
    }
    auto Score = [&](TWeakObjectPtr<UPointLightComponent> Light)
    {
        const auto* D = ReadLight(Light);
        const auto* V = State.Visibility.Find(Light);
        return D && V ? ScoreLight(*D, Receiver, Head.GetLocation()) * V->Smoothed : 0.f;
    };
    if (State.bNeedsSelection)
    {
    State.bNeedsSelection = false;
    TArray<TWeakObjectPtr<UPointLightComponent>> Ranked = State.Candidates;
    Ranked.Sort([&](auto A, auto B) { return Score(A) > Score(B); });
    // Keep identities in their existing slots; fill weaker/unoccupied slots with challengers.
    TSet<TWeakObjectPtr<UPointLightComponent>> Used;
    TWeakObjectPtr<UPointLightComponent> Desired[2];
    for (int32 I = 0; I < 2; ++I)
    {
        auto Current = State.Slots[I].Current;
        if (Score(Current) > 0.f && !Used.Contains(Current)) { Desired[I] = Current; Used.Add(Current); }
    }
    for (auto Candidate : Ranked)
    {
        if (Score(Candidate) <= 0.f || Used.Contains(Candidate)) continue;
        int32 Weakest = INDEX_NONE;
        for (int32 I = 0; I < 2; ++I)
            if (Weakest == INDEX_NONE || Score(Desired[I]) < Score(Desired[Weakest])) Weakest = I;
        if (!Desired[Weakest].IsValid() || Score(Candidate) > Score(Desired[Weakest]) * 1.25f)
        { Used.Remove(Desired[Weakest]); Desired[Weakest] = Candidate; Used.Add(Candidate); }
    }
    for (int32 I = 0; I < 2; ++I) State.Slots[I].Pending = Desired[I];
    }
    TArray<FNPRLocalLightSample> Samples;
    Samples.SetNum(2);
    for (int32 I = 0; I < 2; ++I)
    {
        auto& Slot = State.Slots[I];
        const auto* OldData = ReadLight(Slot.Current);
        if (!OldData || !OldData->bValid || ScoreLight(*OldData, Receiver, Head.GetLocation()) <= 0.f)
        { Slot.Current.Reset(); Slot.Fade = 0.f; } // Shut-off/invalid/channel change clears immediately.
        if (Slot.Current != Slot.Pending)
        {
            Slot.Fade = FMath::Max(0.f, Slot.Fade - DeltaTime / NPRLocalLighting::SwitchTime);
            if (Slot.Fade <= 0.f) Slot.Current = Slot.Pending;
        }
        else Slot.Fade = FMath::Min(1.f, Slot.Fade + DeltaTime / NPRLocalLighting::SwitchTime);
        const auto* D = ReadLight(Slot.Current);
        const auto* V = State.Visibility.Find(Slot.Current);
        if (!D || !D->bValid || !V || Slot.Fade <= 0.f) continue;
        // Match shader ActorPositionWS, including cross-actor render attachment roots.
        const FVector Relative = D->Position - Mesh->GetActorPositionForRenderer();
        const float MaxColor = FMath::Max3(D->Energy.R, D->Energy.G, D->Energy.B);
        const FLinearColor Chroma = MaxColor > 0.f ? D->Energy / MaxColor : FLinearColor::Black;
        auto& Sample = Samples[I];
        Sample.PositionRadius = FVector4(Relative, D->Radius);
        Sample.ColorWeight = FVector4(Chroma.R, Chroma.G, Chroma.B,
            MappedBrightness(*D, Receiver, Head.GetLocation()) * FMath::Max(0.f, Receiver.ArtisticStrength) * V->Smoothed * Slot.Fade);
        Sample.DirectionOuter = FVector4(D->Direction, D->OuterCos);
        Sample.Control = FVector4(D->InnerCos, D->bSpot ? 1.f : 0.f, 0.f, 0.f);
    }
    Receiver.WriteSamples(Samples);
}

void UNPRLocalLightingSubsystem::Tick(float DeltaTime)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(NPR_PL_Tick);
    Super::Tick(DeltaTime);
    if (bShuttingDown) return;
    ++DataFrame; // One cache epoch per world subsystem tick; also supports isolated world tests.
    for (auto Actor : PendingSpawned) if (Actor.IsValid()) ScanActor(Actor.Get());
    PendingSpawned.Empty();
    const double Now = GetWorld()->GetTimeSeconds();
    const bool bSelect = Now - LastSelection >= NPRLocalLighting::SelectionInterval;
    // Ownership validation may unregister a receiver, so iterate a stable key snapshot.
    TArray<TWeakObjectPtr<UNPRLocalLightReceiverComponent>> Keys;
    Receivers.GetKeys(Keys);
    for (auto Key : Keys)
    {
        if (!Key.IsValid()) { Receivers.Remove(Key); continue; }
        if (bSelect && !Key->ValidateOwnership()) continue;
    }
    if (bSelect)
    {
        SpatialRadiusScale = 1.f;
        for (auto Key : Keys)
            if (Key.IsValid() && Receivers.Contains(Key) && !Key->bManualInput)
                SpatialRadiusScale = FMath::Max(SpatialRadiusScale, Key->SelectionRadiusScale);
        // Preserve light registration when idle, but avoid rebuilding an unused index.
        if (!Receivers.IsEmpty()) UpdateSpatialIndex();
        LastSelection = Now;
        for (auto Key : Keys)
            if (Key.IsValid() && !Key->bManualInput)
                if (auto* State = Receivers.Find(Key)) UpdateCandidates(*Key, *State);
        for (auto It = MeshOwners.CreateIterator(); It; ++It)
            if (!It.Key().IsValid() || !It.Value().IsValid()) It.RemoveCurrent();
    }
    LastFrameRayCount = 0;
    ScheduleOcclusion();
    for (auto Key : Keys)
        if (Key.IsValid()) if (auto* State = Receivers.Find(Key)) UpdateReceiver(*Key, *State, DeltaTime);
    RegisteredLightCount = Lights.Num(); RegisteredReceiverCount = Receivers.Num();
}
