// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.h"
#include "SDownloadPlugin.h"
#include "PluginDownloader.h"
#include "PluginDownloaderTokens.h"
#include "PluginDownloaderUtilities.h"
#include "PluginDownloaderCustomization.h"

#include "HTTP/Private/HttpThread.h"

#define LOCTEXT_NAMESPACE "PluginDownloader"

class FPluginDownloaderEditorModule : public IModuleInterface
{
public:
	FOnSpawnTab OnSpawnTab;

	const FName DownloadPluginTabId = "DownloadPlugin";

	virtual void StartupModule() override
	{
		// Hack to increase the HTTP tick rate
		// Makes downloads much faster
		{
			FHttpManager& HttpManager = FHttpModule::Get().GetHttpManager();

			struct FHttpThreadHack : FHttpThread
			{
				void Fixup()
				{
					HttpThreadActiveFrameTimeInSeconds = 1 / 100000.f;
				}
			};

			struct FHttpManagerHack : FHttpManager
			{
				void Fixup()
				{
					if (!Thread)
					{
						return;
					}

					static_cast<FHttpThreadHack*>(Thread)->Fixup();
				}
			};

			static_cast<FHttpManagerHack&>(HttpManager).Fixup();
		}

		// Custom layouts
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

			PropertyModule.RegisterCustomClassLayout(UPluginDownloaderCustom::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateLambda([]
			{
				return MakeShared<FPluginDownloaderCustomCustomization>();
			}));
			
			PropertyModule.RegisterCustomClassLayout(UPluginDownloaderTokens::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateLambda([]
			{
				return MakeShared<FPluginDownloaderTokensCustomization>();
			}));
		}

		// Register Download Plugin tab
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			DownloadPluginTabId,
			FOnSpawnTab::CreateRaw(this, &FPluginDownloaderEditorModule::HandleDownloadPluginTab))
			.SetDisplayName(LOCTEXT("Download PluginTabHeader", "Download Plugin"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		// Hook into the plugin manager window

		struct FTabManagerAccessor : FGlobalTabmanager
		{
			using FGlobalTabmanager::FindTabSpawnerFor;
		};

		const FName TabId = "PluginsEditor";

		const TSharedPtr<FTabSpawnerEntry> TabSpawner = static_cast<FTabManagerAccessor&>(*FGlobalTabmanager::Get()).FindTabSpawnerFor(TabId);
		if (!ensure(TabSpawner))
		{
			return;
		}

		struct FTabSpawnerEntryHack : public FWorkspaceItem
		{
			FName TabType;
			FOnSpawnTab OnSpawnTab;
			FCanSpawnTab CanSpawnTab;
			/** When this method is not provided, we assume that the tab should only allow 0 or 1 instances */
			FOnFindTabToReuse OnFindTabToReuse;
			/** Whether this menu item should be enabled, disabled, or hidden */
			TAttribute<ETabSpawnerMenuType::Type> MenuType;
			/** Whether to automatically generate a menu entry for this tab spawner */
			bool bAutoGenerateMenuEntry;

			TWeakPtr<SDockTab> SpawnedTabPtr;
		};
		static_assert(sizeof(FTabSpawnerEntryHack) == sizeof(FTabSpawnerEntry), "");
		
		OnSpawnTab = reinterpret_cast<FTabSpawnerEntryHack&>(*TabSpawner).OnSpawnTab;

		FGlobalTabmanager::Get()->UnregisterTabSpawner(TabId);
		FGlobalTabmanager::Get()->RegisterTabSpawner(TabId, FOnSpawnTab::CreateRaw(this, &FPluginDownloaderEditorModule::HandleSpawnPluginBrowserTab))
			.SetDisplayName(TabSpawner->GetDisplayName())
			.SetTooltipText(TabSpawner->GetTooltipText())
			.SetIcon(TabSpawner->GetIcon());
	}

	template<typename T>
	static TSharedPtr<T> GetChild(const FString& Type, const TSharedPtr<SWidget>& Widget, int32 Index)
	{
		if (!ensure(Widget))
		{
			return nullptr;
		}

		FChildren* Children = Widget->GetChildren();
		if (!ensure(Children) ||
			!ensure(Index < Children->Num()))
		{
			return nullptr;
		}

		const TSharedRef<SWidget> Result = Children->GetChildAt(Index);
		if (!ensure(Result->GetTypeAsString() == Type))
		{
			return nullptr;
		}

		return StaticCastSharedRef<T>(Result);
	}

	TSharedRef<SDockTab> HandleSpawnPluginBrowserTab(const FSpawnTabArgs& SpawnTabArgs) const
	{
		const TSharedRef<SDockTab> DockTab = OnSpawnTab.Execute(SpawnTabArgs);

#define GET_CHILD(Type, Widget, Index) GetChild<Type>(#Type, Widget, Index)

		const TSharedPtr<SWidget> PluginBrowser = DockTab->GetContent();
		const TSharedPtr<SVerticalBox> VerticalBox1 = GET_CHILD(SVerticalBox, PluginBrowser, 0);
		const TSharedPtr<SSplitter> Splitter = GET_CHILD(SSplitter, VerticalBox1, 0);
		const TSharedPtr<SVerticalBox> FinalVerticalBox = GET_CHILD(SVerticalBox, Splitter, 1);
		const TSharedPtr<SButton> NewPluginButton = GET_CHILD(SButton, FinalVerticalBox, 3);
		const TSharedPtr<STextBlock> NewPluginText = GET_CHILD(STextBlock, NewPluginButton, 0);

#undef GET_CHILD

		if (!ensure(NewPluginText))
		{
			return DockTab;
		}

		NewPluginText->SetText(LOCTEXT("CreateNewPlugin", "Create New Plugin"));

		FinalVerticalBox->RemoveSlot(NewPluginButton.ToSharedRef());
		
		FinalVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(2.0f, 2.0f, 2.0f, 0.0f))
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				NewPluginButton.ToSharedRef()
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(5, 0, 0, 0))
			.AutoWidth()
			[
				SNew(SButton)
				.ContentPadding(5)
				.ToolTip(SNew(SToolTip).Text(LOCTEXT("DownloadPluginEnabled", "Click here to open the Download Plugin dialog.")))
				.TextStyle(FEditorStyle::Get(), "LargeText")
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("DownloadPluginLabel", "Download Plugin"))
				.OnClicked_Lambda([=]
				{
					FGlobalTabmanager::Get()->TryInvokeTab(DownloadPluginTabId);
					return FReply::Handled();
				})
			]
		];

		return DockTab;
	}

	TSharedRef<SDockTab> HandleDownloadPluginTab(const FSpawnTabArgs& SpawnTabArgs) const
	{
		FPluginDownloaderUtilities::CheckTempFolderSize();

		TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(NomadTab);
		Tab->SetContent(SNew(SDownloadPlugin));
		return Tab;
	}
};
IMPLEMENT_MODULE(FPluginDownloaderEditorModule, PluginDownloaderEditor);

#undef LOCTEXT_NAMESPACE