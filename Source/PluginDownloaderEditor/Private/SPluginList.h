// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PluginDownloaderInfo.h"

DECLARE_DELEGATE_OneParam(FOnInfoSelected, FPluginDownloaderRemoteInfo);

class SPluginList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPluginList) {}
		SLATE_EVENT(FOnInfoSelected, OnInfoSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

private:
	TSharedPtr<SListView<TSharedRef<FPluginDownloaderRemoteInfo>>> ListView;

	TSharedRef<ITableRow> OnGenerateRow(const TSharedRef<FPluginDownloaderRemoteInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
};