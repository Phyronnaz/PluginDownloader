// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderEditorModule.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailCustomization.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSplitter.h"

#define LOCTEXT_NAMESPACE "PluginDownloader"

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

class FPluginDownloaderInfoCustomization : public IDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailLayout.GetObjectsBeingCustomized(Objects);

		DetailLayout.EditCategory("Output", {}, ECategoryPriority::Uncommon)
		.AddCustomRow(LOCTEXT("Download", "Download"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(LOCTEXT("Download", "Download"))
		]
		.ValueContent()
		[
			SNew(SButton)
			.ContentPadding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([=]
			{
				for (const TWeakObjectPtr<UObject>& Object : Objects)
				{
					CastChecked<UPluginDownloaderInfo>(Object.Get())->Download();
				}

				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("Download", "Download"))
			]
		];
	}
};

class FPluginDownloaderEditorModule : public IModuleInterface
{
public:
	FOnSpawnTab OnSpawnTab;

	const FName DownloadPluginTabId = "DownloadPlugin";

	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(UPluginDownloaderInfo::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateLambda([]
		{
			return MakeShared<FPluginDownloaderInfoCustomization>();
		}));

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

		if (!ensure(NewPluginButton))
		{
			return DockTab;
		}

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
				.IsEnabled(true)
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
		TSharedRef<SDockTab> Tab = SNew(SDockTab)
			.TabRole(NomadTab);
		
		FDetailsViewArgs Args;
		Args.bHideSelectionTip = true;
		Args.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		const TSharedRef<IDetailsView> DetailsView = PropertyModule.CreateDetailView(Args);
		DetailsView->SetObject(GetMutableDefault<UPluginDownloaderInfo>());

		Tab->SetContent(DetailsView);

		return Tab;
	}
};
IMPLEMENT_MODULE(FPluginDownloaderEditorModule, PluginDownloaderEditor);

#undef LOCTEXT_NAMESPACE