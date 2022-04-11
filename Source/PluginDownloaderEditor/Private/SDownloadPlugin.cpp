// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SDownloadPlugin.h"
#include "SPluginList.h"
#include "PluginDownloader.h"
#include "PluginDownloaderApi.h"
#include "PluginDownloaderTokens.h"
#include "PluginDownloaderDownload.h"
#include "PluginDownloaderUtilities.h"

#define LOCTEXT_NAMESPACE "PluginDownloader"

void SDownloadPlugin::Construct(const FArguments& Args)
{
	UPluginDownloaderTokens* Tokens = GetMutableDefault<UPluginDownloaderTokens>();
	Tokens->LoadFromConfig();
	Tokens->CheckTokens();

	UPluginDownloaderCustom* Custom = GetMutableDefault<UPluginDownloaderCustom>();
	FPluginDownloaderUtilities::LoadConfig(Custom, "PluginDownloaderCustom");
	Custom->FillAutoComplete();
		
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	DetailsViewArgs.bAllowSearch = false;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	const TSharedRef<IDetailsView> TokensDetailsView = PropertyModule.CreateDetailView(DetailsViewArgs);
	TokensDetailsView->SetObject(Tokens);

	DetailsView = PropertyModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateLambda([=]
	{
		return GetDefault<UPluginDownloaderTokens>()->HasValidToken();
	}));

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 5, 0)
		[
			SNew(SPluginList)
			.OnInfoSelected_Lambda([=](const FPluginDownloaderRemoteInfo& RemoteInfo)
			{
				if (RemoteInfo.Name == "Custom")
				{
					Downloader = Custom;
					DetailsView->SetObject(Custom);
					return;
				}

				UPluginDownloaderRemote* Remote = GetMutableDefault<UPluginDownloaderRemote>();
				Remote->Info = {};
				Remote->Info.User = RemoteInfo.User;
				Remote->Info.Repo = RemoteInfo.Repo;

				Remote->Branch.Reset();
				Remote->BranchOptions.Reset();
				Remote->BranchDisplayNameToName.Reset();

				if (RemoteInfo.Branches.Num() > 0)
				{
					for (auto& It : RemoteInfo.Branches)
					{
						ensure(!Remote->BranchDisplayNameToName.Contains(It.Value));
						Remote->BranchDisplayNameToName.Add(It.Value, It.Key);
					}

					RemoteInfo.Branches.GenerateValueArray(Remote->BranchOptions);

					if (ensure(Remote->BranchOptions.Num() > 0))
					{
						Remote->Branch = Remote->BranchOptions[0];
					}
				}
				else
				{
					FPluginDownloaderApi::GetBranchAndTagAutocomplete(Remote->Info, FOnAutocompleteReceived::CreateWeakLambda(Remote, [=](const TArray<FString>& Result)
					{
						Remote->BranchOptions = Result;

						if (!Remote->BranchOptions.Contains(Remote->Branch) &&
							Remote->BranchOptions.Num() > 0)
						{
							Remote->Branch = Remote->BranchOptions[0];
						}
					}));
				}

				Downloader = Remote;
				DetailsView->SetObject(Remote);
			})
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				TokensDetailsView
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				DetailsView.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				+ SHorizontalBox::Slot()
#if ENGINE_VERSION >= 500
				.Padding(5)
#endif
				.AutoWidth()
				[
					SNew(SButton)
					.ToolTipText_Lambda([=]
					{
						if (!Downloader)
						{
							return LOCTEXT("SelectTooltip", "You need to select a plugin to download");
						}
						if (GActivePluginDownloaderDownload)
						{
							return LOCTEXT("DownloadInProgressTooltip", "Download already in progress");
						}
						return LOCTEXT("DownloadPluginTip", "Download this plugin");
					})
					.IsEnabled_Lambda([=]
					{
						return Downloader != nullptr && !GActivePluginDownloaderDownload;
					})
					.OnClicked_Lambda([=]
					{
						FPluginDownloaderDownload::StartDownload(Downloader->GetInfo());
						return FReply::Handled();
					})

#if ENGINE_VERSION < 500
					.ContentPadding(5)
					.TextStyle(FEditorStyle::Get(), "LargeText")
					.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("DownloadPluginLabel", "Download Plugin"))
#else
					.ContentPadding(FMargin(0, 5.f, 0, 4.f))
					.Content()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.CircleArrowDown"))
							.ColorAndOpacity(FStyleColors::AccentGreen)
						]
						+ SHorizontalBox::Slot()
						.Padding(FMargin(5, 0, 0, 0))
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "SmallButtonText")
							.Text(LOCTEXT("DownloadPluginLabel", "Download"))
						]
					]
#endif
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE