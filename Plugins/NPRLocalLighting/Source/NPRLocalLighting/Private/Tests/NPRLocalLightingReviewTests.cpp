#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "NPRLocalLightReceiverComponent.h"
#include "NPRLocalLightingSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"

namespace
{
    struct FReviewWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        UNPRLocalLightingSubsystem* System = World->GetSubsystem<UNPRLocalLightingSubsystem>();
        UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
            FPackageName::MountPointExists(TEXT("/NPRLocalLighting/")) &&
            FPackageName::DoesPackageExist(TEXT("/NPRLocalLighting/Materials/MI_NPR_Unlit")) ?
            TEXT("/NPRLocalLighting/Materials/MI_NPR_Unlit.MI_NPR_Unlit") :
            TEXT("/Game/HairLab/PLRuntime/MI_Hair_PLRuntime.MI_Hair_PLRuntime"));
        FReviewWorld() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        ~FReviewWorld() { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
        UNPRLocalLightReceiverComponent* Receiver(const FVector& Position = FVector::ZeroVector)
        {
            auto* Actor = World->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
            auto* Mesh = Actor->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetStaticMesh(Sphere); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            auto* MID = UMaterialInstanceDynamic::Create(Material, Actor);
            MID->SetScalarParameterValue(TEXT("PLR_RadiusScale"), 1.f); Mesh->SetMaterial(0, MID);
            auto* R = NewObject<UNPRLocalLightReceiverComponent>(Actor); Actor->AddInstanceComponent(R);
            R->TargetMesh = Mesh; R->bUseLocalHeadOffset = true; R->HeadLocalOffset = FVector::ZeroVector; R->RegisterComponent();
            return R;
        }
        UPointLightComponent* Light(const FVector& Position, float Energy = 8.f)
        {
            auto* Actor = World->SpawnActor<APointLight>(Position, FRotator::ZeroRotator);
            auto* L = Actor->PointLightComponent.Get(); L->SetMobility(EComponentMobility::Movable);
            L->SetUseInverseSquaredFalloff(false); L->SetIntensityUnits(ELightUnits::Unitless);
            L->SetIntensity(Energy); L->SetAttenuationRadius(1000); L->SetCastShadows(false);
            return L;
        }
        void Step(int32 Count)
        {
            for (int32 I = 0; I < Count; ++I) { World->Tick(LEVELTICK_All, .05f); System->Tick(.05f); }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPRReviewFixTest, "NPRLocalLighting.ReviewFixes.OriginOwnershipRadius", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNPRReviewFixTest::RunTest(const FString&)
{
    FReviewWorld T;
    if (!TestNotNull(TEXT("engine sphere"), T.Sphere) || !TestNotNull(TEXT("compatible material"), T.Material)) return false;
    auto* A = T.Receiver(FVector(1000, 0, 0));
    auto* Mesh = CastChecked<UStaticMeshComponent>(A->GetResolvedTargetMesh());
    auto* B = NewObject<UNPRLocalLightReceiverComponent>(A->GetOwner()); A->GetOwner()->AddInstanceComponent(B);
    B->TargetMesh = Mesh; B->bUseLocalHeadOffset = true; B->HeadLocalOffset = FVector::ZeroVector; B->RegisterComponent();
    TestTrue(TEXT("first owner enabled with zero CPD"), A->EnableReceiver());
    TestFalse(TEXT("second owner rejected BEFORE any lighting write"), B->EnableReceiver());
    TestTrue(TEXT("duplicate explains ownership conflict"), B->LastError.Contains(TEXT("owned")));
    B->DisableReceiver();
    TestTrue(TEXT("failed duplicate disable does not release first claim"), A->ValidateOwnership());
    auto* L = T.Light(FVector(1010, 0, 0)); T.Step(12);
    TestTrue(TEXT("unattached light produces weight"), A->GetLastWrittenData()[7] > 0);
    auto* Parent = T.World->SpawnActor<AStaticMeshActor>(FVector(300, 0, 0), FRotator::ZeroRotator);
    Parent->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    A->GetOwner()->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform); T.Step(3);
    auto CheckOrigin = [&](const TCHAR* Label)
    {
        const auto Data = A->GetLastWrittenData();
        const FVector Expected = L->GetComponentLocation() - Mesh->GetActorPositionForRenderer();
        TestTrue(Label, FVector(Data[0], Data[1], Data[2]).Equals(Expected, .001));
        const FVector RelativePixel = Mesh->GetComponentLocation() - Mesh->GetActorPositionForRenderer();
        TestTrue(TEXT("shader distance remains true world distance"), FMath::IsNearlyEqual(
            FVector::Distance(RelativePixel, FVector(Data[0], Data[1], Data[2])),
            FVector::Distance(Mesh->GetComponentLocation(), L->GetComponentLocation()), .001));
    };
    TestNotEqual(TEXT("cross actor attachment has a different renderer origin"), Mesh->GetActorPositionForRenderer(), A->GetOwner()->GetActorLocation());
    CheckOrigin(TEXT("attached CPD matches renderer origin"));
    Parent->SetActorLocation(FVector(350, 20, 0));
    L->GetOwner()->SetActorLocation(A->GetHeadWorldPosition() + FVector(10, 0, 0)); T.Step(3);
    CheckOrigin(TEXT("moving parent origin updates CPD"));
    A->GetOwner()->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); T.Step(3);
    CheckOrigin(TEXT("detach restores own renderer origin"));
    A->DisableReceiver(); TestTrue(TEXT("other receiver can claim after release"), B->EnableReceiver());
    B->ResumeAutomaticLighting(); TestTrue(TEXT("resume preserves exclusive ownership"), B->ValidateOwnership());
    B->DisableReceiver();

    // Light bounds end in cell 0, expanded head lies in cell 1: both index AND score must expand.
    auto* C = T.Receiver(FVector(520, 0, 0));
    auto* CM = CastChecked<UStaticMeshComponent>(C->GetResolvedTargetMesh());
    auto* MID = CastChecked<UMaterialInstanceDynamic>(CM->GetMaterial(0));
    MID->SetScalarParameterValue(TEXT("PLR_RadiusScale"), 2.f);
    L->GetOwner()->SetActorLocation(FVector(400, 0, 0)); L->SetAttenuationRadius(90);
    TestTrue(TEXT("scaled receiver enabled"), C->EnableReceiver()); T.Step(12);
    TestEqual(TEXT("MID radius scale read"), C->SelectionRadiusScale, 2.f);
    TestTrue(TEXT("expanded light found across spatial cell boundary"), C->GetLastWrittenData()[7] > 0);
    TestEqual(TEXT("CPD contains physical radius, not double-scaled radius"), C->GetLastWrittenData()[3], 90.f);
    MID->SetScalarParameterValue(TEXT("PLR_RadiusScale"), 1.f); T.Step(4);
    TestEqual(TEXT("live MID shrink removes out-of-range light"), C->GetLastWrittenData()[7], 0.f);
    MID->SetScalarParameterValue(TEXT("PLR_RadiusScale"), 2.f); T.Step(12);
    TestTrue(TEXT("live MID expansion restores light"), C->GetLastWrittenData()[7] > 0);
    C->DisableReceiver();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPRRetainedVisibilityTest, "NPRLocalLighting.ReviewFixes.RetainedVisibilityBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNPRRetainedVisibilityTest::RunTest(const FString&)
{
    for (bool bShadows : { false, true })
    {
        FReviewWorld T;
        if (!T.Sphere || !T.Material) return false;
        auto* R = T.Receiver(); TestTrue(TEXT("receiver owns CPD"), R->EnableReceiver());
        auto* L0 = T.Light(FVector(10, 0, 0)); auto* L1 = T.Light(FVector(20, 0, 0));
        L0->SetCastShadows(bShadows); L1->SetCastShadows(bShadows); T.Step(20);
        const auto Initial = R->GetLastWrittenData();
        TestTrue(TEXT("both incumbent slots initially lit"), Initial[7] > .1f && Initial[23] > .1f);
        for (int32 I = 0; I < 4; ++I) T.Light(FVector(10, I + 1, 0), 10.f)->SetCastShadows(bShadows);
        // Challengers outrank incumbents, but do not exceed the 25% replacement threshold.
        for (int32 I = 0; I < 40; ++I)
        {
            T.Step(1); const auto Data = R->GetLastWrittenData();
            TestEqual(TEXT("retained slot 0 identity does not expire"), Data[0], Initial[0]);
            TestEqual(TEXT("retained slot 1 identity does not expire"), Data[16], Initial[16]);
            TestTrue(TEXT("retained illumination stays on beyond 0.5 seconds"), Data[7] > .1f && Data[23] > .1f);
            TestTrue(TEXT("four candidates cost at most 12 rays"), T.System->LastFrameRayCount <= 12);
            if (!bShadows) TestEqual(TEXT("non-shadow lights use no collision rays"), T.System->LastFrameRayCount, 0);
        }
        if (bShadows)
        {
            TArray<UNPRLocalLightReceiverComponent*> Extra;
            for (int32 I = 0; I < 11; ++I) { auto* X = T.Receiver(FVector(0, I, 0)); X->EnableReceiver(); Extra.Add(X); }
            int32 PeakRays = 0;
            for (int32 I = 0; I < 40; ++I)
            { T.Step(1); PeakRays = FMath::Max(PeakRays, T.System->LastFrameRayCount); }
            TestTrue(TEXT("budget exercised with 12 receivers"), PeakRays > 12);
            TestTrue(TEXT("global ray budget never exceeded"), PeakRays <= NPRLocalLighting::MaxRaysPerFrame);
            for (auto* X : Extra) { TestTrue(TEXT("round-robin receivers receive light"), X->GetLastWrittenData()[7] > 0.f); X->DisableReceiver(); }
        }
        R->DisableReceiver();
    }
    return true;
}
#endif
