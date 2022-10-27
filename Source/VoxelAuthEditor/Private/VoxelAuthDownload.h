// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelAuthDownload;

extern FVoxelAuthDownload* GVoxelAuthDownload;

class FVoxelAuthDownload
{
public:
	void Download(const FString& Branch, int32 Counter);

private:
	TSharedPtr<SNotificationItem> Notification;

	void Fail(const FString& Error);
	void FinalizeDownload(const TArray<uint8>& Data, const FString& AesKey);
};