// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

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