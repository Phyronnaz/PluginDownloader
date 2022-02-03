// Copyright Voxel Plugin, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class PluginDownloaderMiniz : ModuleRules
{
    public PluginDownloaderMiniz(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePaths.Add(ModuleDirectory);
        
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
            });
    }
}