// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SPluginList.h"
#include "HttpModule.h"
#include "WebImageCache.h"
#include "EditorStyleSet.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Images/SImage.h"

FWebImageCache WebImageCache;

struct FItemListCache
{
	TArray<TSharedRef<FRemotePluginInfo>> Items;
};
FItemListCache ItemListCache;

void SPluginList::Construct(const FArguments& Args)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		.Padding(2)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(2)
			[
				SAssignNew(ListView, SListView<TSharedRef<FRemotePluginInfo>>)
				.ListItemsSource(&ItemListCache.Items)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SPluginList::OnGenerateRow)
				.ItemHeight(32)
				.OnSelectionChanged_Lambda([=](TSharedPtr<FRemotePluginInfo> Info, ESelectInfo::Type)
				{
					if (Info)
					{
						Args._OnInfoSelected.ExecuteIfBound(*Info);
					}
				})
			]
		]
	];

	if (ItemListCache.Items.Num() == 0)
	{
		const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
		Request->SetURL("https://raw.githubusercontent.com/Phyronnaz/PluginDownloaderData/master/Plugins.json");
		Request->SetVerb(TEXT("GET"));
		Request->OnProcessRequestComplete().BindLambda([=, WeakThis = TWeakPtr<SPluginList>(SharedThis(this))](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			if (!bSucceeded ||
				HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok ||
				ItemListCache.Items.Num() > 0)
			{
				return;
			}

			TSharedPtr<FJsonValue> ParsedValue;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, ParsedValue))
			{
				return;
			}
			
			for (const TSharedPtr<FJsonValue>& JsonValue : ParsedValue->AsArray())
			{
				if (!JsonValue)
				{
					continue;
				}
				const TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
				if (!ensure(JsonObject))
				{
					continue;
				}

				const TSharedRef<FPluginDownloaderRemotePluginInfo> Info = MakeShared<FRemotePluginInfo>();

				if (ensure(FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &Info.Get())))
				{
					ItemListCache.Items.Add(Info);
				}
			}
			
			const TSharedRef<FPluginDownloaderRemotePluginInfo> CustomInfo = MakeShared<FRemotePluginInfo>();
			CustomInfo->Name = "Custom";
			CustomInfo->Icon = "https://raw.githubusercontent.com/Phyronnaz/PluginDownloaderData/master/Icons/Custom.png";
			ItemListCache.Items.Add(CustomInfo);

			if (WeakThis.IsValid())
			{
				ListView->RequestListRefresh();
			}
		});
		Request->ProcessRequest();
	}
}

TSharedRef<ITableRow> SPluginList::OnGenerateRow(const TSharedRef<FRemotePluginInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const TSharedRef<const FWebImage> Icon = WebImageCache.Download(Item->Icon);
	
	static FTextBlockStyle TextStyle = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");
	TextStyle.Font.Size = 14;

	return
		SNew(STableRow<TSharedRef<FRemotePluginInfo>>, OwnerTable)
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