// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SDownloadPlugin.h"
#include "SPluginList.h"
#include "PluginDownloader.h"
#include "Utilities.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "PluginDownloader"

void SDownloadPlugin::Construct(const FArguments& Args)
{
	UPluginDownloaderTokens* Tokens = GetMutableDefault<UPluginDownloaderTokens>();
	Tokens->LoadFromConfig();
	Tokens->CheckTokens();

	UPluginDownloaderCustom* Custom = GetMutableDefault<UPluginDownloaderCustom>();
	FUtilities::LoadConfig(Custom, "PluginDownloaderCustom");
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
			.OnInfoSelected_Lambda([=](const FRemotePluginInfo& RemoteInfo)
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

				Remote->BranchDisplayNameToName.Reset();
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
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(5)
					.ToolTipText_Lambda([=]
					{
						if (!Downloader)
						{
							return LOCTEXT("SelectTooltip", "You need to select a plugin to download");
						}
						if (!GetDefault<UPluginDownloaderTokens>()->HasValidToken())
						{
							return LOCTEXT("InvalidTokenTooltip", "Your github token is invalid");
						}
						return LOCTEXT("DownloadPluginTip", "Download this plugin");
					})
					.TextStyle(FEditorStyle::Get(), "LargeText")
					.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("DownloadPluginLabel", "Download Plugin"))
					.IsEnabled_Lambda([=]
					{
						return Downloader != nullptr && GetDefault<UPluginDownloaderTokens>()->HasValidToken();
					})
					.OnClicked_Lambda([=]
					{
						FPluginDownloader::DownloadPlugin(Downloader->GetInfo());
						return FReply::Handled();
					})
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE