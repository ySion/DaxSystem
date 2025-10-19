// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DaxSystem : ModuleRules {
    public DaxSystem(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] { }
        );


        PrivateIncludePaths.AddRange(
            new string[] { }
        );
        
        PrivateIncludePaths.Add(PluginDirectory);

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "AngelscriptCode",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "NetCore",
            }
        );
    }
}