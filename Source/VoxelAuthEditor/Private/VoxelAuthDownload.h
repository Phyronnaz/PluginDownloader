// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelPluginVersion.h"

class FVoxelAuthDownload;

extern FVoxelAuthDownload* GVoxelAuthDownload;

class FVoxelAuthDownload
{
public:
	void Download(const FVoxelPluginVersion& Version);

private:
	TSharedPtr<SNotificationItem> Notification;

	void Fail(const FString& Error);
	void FinalizeDownload(const TArray<uint8>& Data, const FString& AesKey);
};