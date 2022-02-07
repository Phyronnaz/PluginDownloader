// Copyright Voxel Plugin, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class PluginDownloaderEditor : ModuleRules
{
    public PluginDownloaderEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = false;

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

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
                "Json",
                "JsonUtilities",
                "ImageDownload",
                "MediaAssets",
            });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
		    PublicSystemLibraries.Add("crypt32.lib");
        }

        BuildVersion Version;
        if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version) &&
            Version.BranchName == "++UE5+Release-5.0-EarlyAccess")
        {
            PublicDefinitions.Add("UE5_EA=1");
        }
        else
        {
            PublicDefinitions.Add("UE5_EA=0");
        }
    }
}