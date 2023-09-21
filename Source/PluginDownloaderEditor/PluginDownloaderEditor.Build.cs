// Copyright Voxel Plugin, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class PluginDownloaderEditor : ModuleRules
{
    public PluginDownloaderEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivatePCHHeaderFile = "Public/VoxelMinimal.h";
        bUseUnity = false;
		CppStandard = CppStandardVersion.Cpp17;

        // Marketplace requires third party deps to be in a ThirdParty folder
        PrivateIncludePaths.Add(ModuleDirectory + "/../ThirdParty");

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "Projects",
                "Slate",
                "SlateCore",
                "InputCore",
                "EditorStyle",
                "HTTP",
                "RHI",
                "Json",
                "JsonUtilities",
                "MediaAssets",
                "UATHelper",
                "DeveloperSettings",
            });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
		    PublicSystemLibraries.Add("crypt32.lib");
        }
    }
}