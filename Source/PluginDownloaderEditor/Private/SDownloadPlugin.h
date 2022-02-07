// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class IDetailsView;
class UPluginDownloaderBase;

class SDownloadPlugin : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDownloadPlugin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

private:
	TSharedPtr<IDetailsView> DetailsView;
	UPluginDownloaderBase* Downloader = nullptr;
};