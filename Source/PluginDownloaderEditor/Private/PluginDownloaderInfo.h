// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PluginDownloaderInfo.generated.h"

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
};