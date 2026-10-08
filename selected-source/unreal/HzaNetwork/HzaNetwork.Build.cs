using UnrealBuildTool;
public class HzaNetwork : ModuleRules
{
    public HzaNetwork(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[] { "HTTP", "WebSockets", "Json", "InputCore", "Slate", "SlateCore" });
        if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("UnrealEd");
        if (Target.Platform == UnrealTargetPlatform.Win64) PublicSystemLibraries.Add("Advapi32.lib");
    }
}
