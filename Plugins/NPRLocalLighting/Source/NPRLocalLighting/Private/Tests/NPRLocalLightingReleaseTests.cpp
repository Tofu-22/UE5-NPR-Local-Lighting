#include "NPRLocalLightingTypes.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "NPRLocalLightReceiverComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPRReleaseMathTest, "NPRLocalLighting.Release.MathContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNPRReleaseMathTest::RunTest(const FString&)
{
    TestEqual(TEXT("ABI v1 owns 32 floats"), NPRLocalLighting::FloatCount, 32);
    TestEqual(TEXT("zero light energy"), NPRLocalLighting::MapEnergy(0, 100), 0.f);
    TestEqual(TEXT("reference energy maps to half"), NPRLocalLighting::MapEnergy(100, 100), .5f);
    TestEqual(TEXT("unitless mapping"), NPRLocalLighting::MapEnergy(8, 8), .5f);
    TestEqual(TEXT("hue-preserving cap"), NPRLocalLighting::CapFill(FVector3f(2, .5f, 1), 1), FVector3f(1, .25f, .5f));
    TestEqual(TEXT("zero cap"), NPRLocalLighting::CapFill(FVector3f(2, 1, 1), 0), FVector3f::ZeroVector);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPRReleaseABITest, "NPRLocalLighting.Release.MaterialABI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNPRReleaseABITest::RunTest(const FString&)
{
    auto* Material = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/NPRLocalLighting/Materials/MI_NPR_Unlit.MI_NPR_Unlit"));
    auto* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (!TestNotNull(TEXT("bundled MI, no project dependency"), Material) ||
        !TestNotNull(TEXT("engine-only geometry"), Sphere)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto MakeReceiver = [&]()
    {
        auto* Actor = World->SpawnActor<AStaticMeshActor>();
        auto* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetStaticMesh(Sphere); Mesh->SetMaterial(0, Material);
        auto* Anchor = NewObject<USceneComponent>(Actor); Actor->AddInstanceComponent(Anchor);
        Anchor->SetupAttachment(Mesh); Anchor->RegisterComponent();
        auto* Receiver = NewObject<UNPRLocalLightReceiverComponent>(Actor);
        Actor->AddInstanceComponent(Receiver);
        Receiver->TargetMesh = Mesh; Receiver->HeadAnchor = Anchor; Receiver->RegisterComponent();
        return Receiver;
    };
    auto* A = MakeReceiver(); auto* B = MakeReceiver();
    TestTrue(TEXT("CPD metadata from nested MF passes receiver audit"), A->EnableReceiver());
    TestTrue(TEXT("second primitive shares MI without sharing CPD"), B->EnableReceiver());
    AddInfo(A->LastError);
    FNPRLocalLightSample Light;
    Light.PositionRadius = FVector4(10, 20, 30, 100);
    Light.ColorWeight = FVector4(1, 0, 0, .5);
    TestTrue(TEXT("manual point sample"), A->InjectManualSamples({Light}));
    const auto Data = A->GetLastWrittenData();
    TestEqual(TEXT("complete sample length"), Data.Num(), 32);
    if (Data.Num() == 32)
    {
        TestEqual(TEXT("radius at float 3"), Data[3], 100.f);
        TestEqual(TEXT("mapped weight at float 7"), Data[7], .5f);
    }
    const auto BData = B->GetLastWrittenData();
    if (BData.Num() == 32) TestEqual(TEXT("other primitive remains zero"), BData[7], 0.f);
    A->TargetMesh->SetCustomPrimitiveDataFloat(35, .75f);
    A->DisableReceiver();
    TestEqual(TEXT("unowned tail survives"), A->TargetMesh->GetCustomPrimitiveData().Data[35], .75f);
    B->DisableReceiver(); World->DestroyWorld(false);
    return true;
}
#endif
