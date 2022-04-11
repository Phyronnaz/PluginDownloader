// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PluginDownloaderInfo.generated.h"

UENUM()
enum class EPluginDownloadInstallLocation
{
	// Will only be accessible by the current project
	// Highly recommended
	Project,
	// Will install the plugin engine wide
	// Will require administrator access
	// Not recommended
	Engine
};

USTRUCT()
struct FPluginDownloaderInfo
{
	GENERATED_BODY()
		
	UPROPERTY(EditAnywhere, Category = "Settings")
	FString User;
	
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (GetOptions=RepoOptions))
	FString Repo;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (GetOptions=BranchOptions))
	FString Branch;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EPluginDownloadInstallLocation InstallLocation = EPluginDownloadInstallLocation::Project;
};

USTRUCT()
struct FPluginDownloaderRemoteInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Icon;

	UPROPERTY()
	FString User;

	UPROPERTY()
	FString Repo;
	
	UPROPERTY()
	FString Descriptor;

	UPROPERTY()
	FString StableBranch;
	
	UPROPERTY()
	FString ReleaseNotesURL;

	UPROPERTY()
	TMap<FString, FString> Branches;
};