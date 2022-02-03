// Copyright Voxel Plugin, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class PluginDownloaderEditor : ModuleRules
{
    public PluginDownloaderEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        // Marketplace requires third party deps to be in a ThirdParty folder
        PrivateIncludePaths.Add(ModuleDirectory + "/../ThirdParty");

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "HTTP",
                "Json",
            });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
		    PublicSystemLibraries.Add("crypt32.lib");
        }
    }
}