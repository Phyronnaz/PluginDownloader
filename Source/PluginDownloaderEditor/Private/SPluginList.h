// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/SListView.h"
#include "SPluginList.generated.h"

USTRUCT()
struct FPluginDownloaderRemotePluginInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString IconName;

	UPROPERTY()
	FString User;

	UPROPERTY()
	FString Repo;

	UPROPERTY()
	FString Branch;

	FString GetIconURL() const
	{
		return "https://raw.githubusercontent.com/Phyronnaz/PluginDownloaderData/master/Icons" / IconName + ".png";
	}
};

using FRemotePluginInfo = FPluginDownloaderRemotePluginInfo;

DECLARE_DELEGATE_OneParam(FOnInfoSelected, FRemotePluginInfo);

class SPluginList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPluginList) {}
		SLATE_EVENT(FOnInfoSelected, OnInfoSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

private:
	TSharedPtr<SListView<TSharedRef<FRemotePluginInfo>>> ListView;

	TSharedRef<ITableRow> OnGenerateRow(const TSharedRef<FRemotePluginInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
};