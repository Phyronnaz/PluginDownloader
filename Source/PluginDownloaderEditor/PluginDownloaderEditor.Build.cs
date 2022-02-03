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
    }
}