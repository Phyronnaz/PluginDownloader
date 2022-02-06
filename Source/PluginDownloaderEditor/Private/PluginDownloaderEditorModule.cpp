// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "Utilities.h"
#include "SVideoPlayer.h"
#include "SDownloadPlugin.h"
#include "PluginDownloader.h"

#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailCustomization.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSplitter.h"

#include "HttpModule.h"
#include "HttpManager.h"
#include "UnrealEdMisc.h"
#include "FileMediaSource.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "HTTP/Private/HttpThread.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "PluginDownloader"

class FPluginDownloaderBaseCustomization : public IDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailLayout.GetObjectsBeingCustomized(Objects);
		check(Objects.Num() == 1);
		UPluginDownloaderBase* Base = CastChecked<UPluginDownloaderBase>(Objects[0].Get());

		if (UPluginDownloaderCustom* Custom = Cast<UPluginDownloaderCustom>(Base))
		{
			DetailLayout.EditCategory("Settings", {}, ECategoryPriority::Uncommon)
			.AddCustomRow(LOCTEXT("Refresh", "Refresh"))
			.NameContent()
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([=]
				{
					return LOCTEXT("Refresh", "Refresh");
				})
			]
			.ValueContent()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("RefreshTooltip", "Queries the github API to fill the dropdowns"))
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([=]
				{
					Custom->FillAutoComplete();

					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text_Lambda([=]
					{
						return LOCTEXT("Refresh", "Refresh");
					})
				]
			];
		}
	}
};

class FPluginDownloaderTokensCustomization : public IDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailLayout.GetObjectsBeingCustomized(Objects);
		check(Objects.Num() == 1);
		UPluginDownloaderTokens* Tokens = CastChecked<UPluginDownloaderTokens>(Objects[0].Get());

		{
			const TSharedRef<IPropertyHandle> Handle = DetailLayout.GetProperty("GithubAccessToken");
			DetailLayout.EditDefaultProperty(Handle)->CustomWidget()
			.NameContent()
			[
				Handle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(120)
					[
						Handle->CreatePropertyValueWidget()
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(2, 0)
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(2)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.OnClicked_Lambda([=]
					{
						UFileMediaSource* MediaSource = NewObject<UFileMediaSource>();
						MediaSource->FilePath = IPluginManager::Get().FindPlugin("PluginDownloader")->GetContentDir() / "HowToCreateToken.mp4";

						const TSharedRef<SWindow> Window =
							SNew(SWindow)
							.Title(LOCTEXT("CreateTokenHowTo", "How to create a new token"))
							.ClientSize(FVector2D(1280, 720))
							.IsTopmostWindow(true);

						Window->SetContent(
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.FillHeight(1)
							[
								SNew(SVideoPlayer)
								.Source(MediaSource)
								.Size(FVector2D(1580, 826))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0, 5)
							[
								SNew(SBox)
								.HAlign(HAlign_Center)
								[
									SNew(SButton)
									.ContentPadding(5)
									.IsEnabled(true)
									.ToolTip(SNew(SToolTip).Text(LOCTEXT("OpenBrowswer", "Click here to open the web browser")))
									.TextStyle(FEditorStyle::Get(), "LargeText")
									.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
									.HAlign(HAlign_Center)
									.Text(LOCTEXT("OpenWebBrowser", "Open Web Browser"))
									.OnClicked_Lambda([=]
									{
										FPlatformProcess::LaunchURL(TEXT("https://github.com/settings/tokens"), nullptr, nullptr);
										return FReply::Handled();
									})
								]
							]
						);

						FSlateApplication::Get().AddWindow(Window);

						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.ToolTipText(LOCTEXT("TokenTooltip", "Create a new token from your github account. Make sure to tick the REPO scope"))
						.Text_Lambda([=]
						{
							return LOCTEXT("Create token", "Create token");
						})
					]
				]
			];
		}

		{
			const TSharedRef<IPropertyHandle> Handle = DetailLayout.GetProperty("GithubStatus");

			DetailLayout.EditDefaultProperty(Handle)->CustomWidget()
			.NameContent()
			[
				Handle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(300)
			.MaxDesiredWidth(300)
			[
				SNew(SBox)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.ColorAndOpacity_Lambda([=]
					{
						return Tokens->GithubStatus.IsEmpty()
							? Tokens->GithubAccessToken.IsEmpty()
							? FSlateColor(FColor::Orange)
							: FSlateColor::UseForeground()
							: FSlateColor(FColor::Red);
					})
					.Text_Lambda([=]
					{
						return FText::FromString(Tokens->GithubStatus.IsEmpty()
							? Tokens->GithubAccessToken.IsEmpty()
							? "Empty token"
							: "Valid token"
							: Tokens->GithubStatus);
					})
				]
			];
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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

			PropertyModule.RegisterCustomClassLayout(UPluginDownloaderBase::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateLambda([]
			{
				return MakeShared<FPluginDownloaderBaseCustomization>();
			}));
			
			PropertyModule.RegisterCustomClassLayout(UPluginDownloaderTokens::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateLambda([]
			{
				return MakeShared<FPluginDownloaderTokensCustomization>();
			}));
		}

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

		OnSpawnTab = reinterpret_cast<FTabSpawnerEntryHack&>(*TabSpawner).OnSpawnTab;

		FGlobalTabmanager::Get()->UnregisterTabSpawner(TabId);
		FGlobalTabmanager::Get()->RegisterTabSpawner(TabId, FOnSpawnTab::CreateRaw(this, &FPluginDownloaderEditorModule::HandleSpawnPluginBrowserTab))
			.SetDisplayName(TabSpawner->GetDisplayName())
			.SetTooltipText(TabSpawner->GetTooltipText())
			.SetIcon(TabSpawner->GetIcon());

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			DownloadPluginTabId,
			FOnSpawnTab::CreateRaw(this, &FPluginDownloaderEditorModule::HandleDownloadPluginTab))
			.SetDisplayName(LOCTEXT("Download PluginTabHeader", "Download Plugin"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
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
		TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(NomadTab);
		Tab->SetContent(SNew(SDownloadPlugin));
		return Tab;
	}
};
IMPLEMENT_MODULE(FPluginDownloaderEditorModule, PluginDownloaderEditor);

#undef LOCTEXT_NAMESPACE