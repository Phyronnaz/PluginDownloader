// Copyright Voxel Plugin, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class VoxelAuthEditor : ModuleRules
{
    public VoxelAuthEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivatePCHHeaderFile = "../PluginDownloaderEditor/Public/VoxelMinimal.h";
        bUseUnity = false;

        // Marketplace requires third party deps to be in a ThirdParty folder
        PrivateIncludePaths.Add(ModuleDirectory + "/../ThirdParty");

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "PluginDownloaderEditor",
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
#if UE_5_0_OR_LATER
				"EOSSDK",
				"EOSShared",
				"ToolMenus",
#endif
            });
    }
}