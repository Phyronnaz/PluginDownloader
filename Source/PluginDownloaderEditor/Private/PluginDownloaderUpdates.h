// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PluginDownloaderInfo.h"

class FPluginDownloaderUpdates
{
public:
	static void CheckForUpdate(const FPluginDownloaderRemoteInfo& PluginInfo);
};