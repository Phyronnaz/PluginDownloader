// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.h"
#include "SDownloadPlugin.h"
#include "PluginDownloader.h"
#include "PluginDownloaderApi.h"
#include "PluginDownloaderTokens.h"
#include "PluginDownloaderSettings.h"
#include "PluginDownloaderUtilities.h"
#include "PluginDownloaderCustomization.h"

#include "HttpModule.h"
#include "HttpManager.h"
#include "Runtime/Online/HTTP/Private/HttpThread.h"

#define LOCTEXT_NAMESPACE "PluginDownloader"

class FPluginDownloaderEditorModule : public IModuleInterface
{
public:
	FOnSpawnTab OnSpawnTab;

	const FName DownloadPluginTabId = "DownloadPlugin";

	virtual void StartupModule() override
	{
		// Increase the HTTP tick rate
		// Makes downloads much faster
		{
			FHttpManager& HttpManager = FHttpModule::Get().GetHttpManager();

			struct FHttpThreadHack : UE_503_SWITCH(FHttpThread, FLegacyHttpThread)
			{
				void Fixup()
				{
					HttpThreadActiveFrameTimeInSeconds = 1 / 100000.f;
				}
			};

			struct FHttpManagerHack : FHttpManager
			{
				void Fixup() const
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

		// Download the plugin list & check for updates
		FPluginDownloaderUtilities::DelayedCall([]
		{
			if (GetDefault<UPluginDownloaderSettings>()->bCheckForUpdatesOnStartup)
			{
				FPluginDownloaderApi::Initialize();
			}
		});

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

#if ENGINE_VERSION >= 504
			/** Whether or not this tab can ever be in a sidebar */
			bool bCanSidebarTab;
#endif

			TWeakPtr<SDockTab> SpawnedTabPtr;

#if ENGINE_VERSION >= 504
			/** How this tab behaves when the tab manager is in a read only mode */
			ETabReadOnlyBehavior ReadOnlyBehavior;
#endif
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
		const TSharedPtr<SBorder> Border = GET_CHILD(SBorder, PluginBrowser, 0);
		const TSharedPtr<SVerticalBox> VerticalBox = GET_CHILD(SVerticalBox, Border, 0);
		const TSharedPtr<SHorizontalBox> HorizontalBox = GET_CHILD(SHorizontalBox, VerticalBox, 0);
		const TSharedPtr<SButton> AddPluginButton = GET_CHILD(SButton, HorizontalBox, 0);

		if (!ensure(AddPluginButton))
		{
			return DockTab;
		}

		HorizontalBox->GetSlot(0).SetPadding(FMargin(12, 7, 12, 7));

		HorizontalBox->InsertSlot(1)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(FMargin(0, 7, 18, 7))
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTip(SNew(SToolTip).Text(LOCTEXT("DownloadPluginEnabled", "Click here to open the Download Plugin dialog.")))
			.OnClicked_Lambda([=]
			{
				FGlobalTabmanager::Get()->TryInvokeTab(DownloadPluginTabId);
				return FReply::Handled();
			})
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
		];

#undef GET_CHILD

		return DockTab;
	}

	TSharedRef<SDockTab> HandleDownloadPluginTab(const FSpawnTabArgs& SpawnTabArgs) const
	{
		FPluginDownloaderApi::Initialize();
		FPluginDownloaderUtilities::CheckTempFolderSize();

		TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(NomadTab);
		Tab->SetContent(SNew(SDownloadPlugin));
		return Tab;
	}
};
IMPLEMENT_MODULE(FPluginDownloaderEditorModule, PluginDownloaderEditor);

#undef LOCTEXT_NAMESPACE
