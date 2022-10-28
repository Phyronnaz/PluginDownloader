// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SPluginList.h"
#include "PluginDownloaderApi.h"
#include "ImageDownload/WebImageCache.h"

FWebImageCache WebImageCache;

void SPluginList::Construct(const FArguments& Args)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorAppStyle::GetBrush("NoBorder"))
		.Padding(2)
		[
			SNew(SBorder)
			.BorderImage(FEditorAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(2)
			[
				SAssignNew(ListView, SListView<TSharedRef<FPluginDownloaderRemoteInfo>>)
				.ListItemsSource(&GPluginDownloaderRemoteInfos)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SPluginList::OnGenerateRow)
				.ItemHeight(32)
				.OnSelectionChanged_Lambda([=](TSharedPtr<FPluginDownloaderRemoteInfo> Info, ESelectInfo::Type)
				{
					if (Info)
					{
						Args._OnInfoSelected.ExecuteIfBound(*Info);
					}
				})
			]
		]
	];

	GOnPluginDownloaderRemoteInfosChanged.AddSP(ListView.Get(), &SListView<TSharedRef<FPluginDownloaderRemoteInfo>>::RequestListRefresh);
}

TSharedRef<ITableRow> SPluginList::OnGenerateRow(const TSharedRef<FPluginDownloaderRemoteInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const TSharedRef<const FWebImage> Icon = WebImageCache.Download(Item->Icon);
	
	static FTextBlockStyle TextStyle = FEditorAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");
	TextStyle.Font.Size = 14;

	return
		SNew(STableRow<TSharedRef<FPluginDownloaderRemoteInfo>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.MaxDesiredWidth(64)
				.MaxDesiredHeight(64)
				[
					SNew(SImage)
					.Image_Lambda([=]
					{
						return Icon->GetBrush();
					})
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(5, 0, 5, 0)
			[
				SNew(SBox)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->Name))
					.TextStyle(&TextStyle)
				]
			]
		];
}