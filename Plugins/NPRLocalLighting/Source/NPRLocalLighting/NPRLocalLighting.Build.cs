using UnrealBuildTool;

public class NPRLocalLighting : ModuleRules
{
    public NPRLocalLighting(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "LevelEditor" }); // Editor-only anchor transaction/UI refresh.
    }
}
