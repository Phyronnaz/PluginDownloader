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

#include "ThirdParty/miniz.h"
#include "HttpModule.h"
#include "UnrealEdMisc.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/ConfigCacheIni.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "PluginDownloader"

inline void SaveConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename = GEditorPerProjectIni, bool bAppendClassName = true)
{
	if (!ensure(Object))
	{
		return;
	}

	UClass* Class = Object->GetClass();
	const UObject* CDO = Class->GetDefaultObject();

	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		auto& Property = **It;
		if (Property.HasAnyPropertyFlags(CPF_Transient)) continue;
		if (!ensure(Property.ArrayDim == 1)) continue;

		const FString Section = bAppendClassName ? BaseSectionName + TEXT(".") + Class->GetName() : BaseSectionName;

		FString	Value;
		if (Property.ExportText_InContainer(0, Value, Object, CDO, Object, PPF_None))
		{
			GConfig->SetString(*Section, *Property.GetName(), *Value, Filename);
		}
		else
		{
			GConfig->RemoveKey(*Section, *Property.GetName(), Filename);
		}
	}
}

inline void LoadConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename = GEditorPerProjectIni, bool bAppendClassName = true)
{
	if (!ensure(Object))
	{
		return;
	}

	UClass* Class = Object->GetClass();
	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		auto& Property = **It;
		if (Property.HasAnyPropertyFlags(CPF_Transient)) continue;
		if (!ensure(Property.ArrayDim == 1)) continue;
		
		const FString Section = bAppendClassName ? BaseSectionName + TEXT(".") + Class->GetName() : BaseSectionName;

		FString Value;
		if (GConfig->GetString(*Section, *Property.GetName(), Value, Filename))
		{
			Property.ImportText(*Value, Property.ContainerPtrToValuePtr<void>(Object), PPF_None, Object);
		}
	}
}

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

class FPluginDownloaderInfoCustomization : public IDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailLayout.GetObjectsBeingCustomized(Objects);
		check(Objects.Num() == 1);
		UPluginDownloaderInfo* Info = CastChecked<UPluginDownloaderInfo>(Objects[0].Get());

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
			.ContentPadding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([=]
			{
				Info->FillAutoComplete();

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

		DetailLayout.EditCategory("Output", {}, ECategoryPriority::Uncommon)
		.AddCustomRow(LOCTEXT("Download", "Download"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([=]
			{
				return Info->PendingRequest.IsValid() ? LOCTEXT("Cancel", "Cancel") : LOCTEXT("Download", "Download");
			})
		]
		.ValueContent()
		[
			SNew(SButton)
			.ContentPadding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([=]
			{
				if (Info->PendingRequest.IsValid())
				{
					Info->Cancel();
				}
				else
				{
					Info->Download();
				}

				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([=]
				{
					return Info->PendingRequest.IsValid() ? LOCTEXT("Cancel", "Cancel") : LOCTEXT("Download", "Download");
				})
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

		GetMutableDefault<UPluginDownloaderInfo>()->FillAutoComplete();

		LoadConfig(GetMutableDefault<UPluginDownloaderInfo>(), "PluginDownloaderInfo");

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		const TSharedRef<IDetailsView> DetailsView = PropertyModule.CreateDetailView(Args);
		DetailsView->SetObject(GetMutableDefault<UPluginDownloaderInfo>());

		Tab->SetContent(DetailsView);

		return Tab;
	}
};
IMPLEMENT_MODULE(FPluginDownloaderEditorModule, PluginDownloaderEditor);

void UPluginDownloaderInfo::Download()
{
	ensure(!PendingRequest);

	FixupURL();
		
	ensure(!PendingRequest);
	PendingRequest = FHttpModule::Get().CreateRequest();
	PendingRequest->SetURL(URL);
	PendingRequest->SetVerb(TEXT("GET"));
	PendingRequest->OnProcessRequestComplete().BindUObject(this, &UPluginDownloaderInfo::OnDownloadFinished);
	PendingRequest->OnRequestProgress().BindWeakLambda(this, [=](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
	{
		if (PendingRequest != Request)
		{
			// Cancelled
			return;
		}

		Response = FString::Printf(TEXT("%f MB received"), BytesReceived / float(1 << 20));
	});
	PendingRequest->ProcessRequest();

	UE_LOG(LogTemp, Log, TEXT("Downloading %s"), *PendingRequest->GetURL());
}

void UPluginDownloaderInfo::Cancel()
{
	ensure(PendingRequest);
	PendingRequest->CancelRequest();
	PendingRequest.Reset();

	Response = "Cancelled";
}

void UPluginDownloaderInfo::FillAutoComplete()
{
	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetURL("https://api.github.com/users/" + User + "/repos?per_page=100");
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->OnProcessRequestComplete().BindWeakLambda(this, [=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			if (!bSucceeded || HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
			{
				return;
			}

			TSharedPtr<FJsonValue> ParsedValue;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, ParsedValue))
			{
				return;
			}

			CachedRepos.Reset();
			ensure(ParsedValue->AsArray().Num() < 99);
			for (const TSharedPtr<FJsonValue>& JsonValue : ParsedValue->AsArray())
			{
				if (!JsonValue)
				{
					continue;
				}

				const TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
				if (!JsonObject)
				{
					continue;
				}

				CachedRepos.Add(JsonObject->GetStringField("name"));
			}
		});
		HttpRequest->ProcessRequest();
	}
	
	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetURL("https://api.github.com/repos/" + User / Repo / "/branches");
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->OnProcessRequestComplete().BindWeakLambda(this, [=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			if (!bSucceeded || HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
			{
				return;
			}

			TSharedPtr<FJsonValue> ParsedValue;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, ParsedValue))
			{
				return;
			}

			CachedBranches.Reset();
			for (const TSharedPtr<FJsonValue>& JsonValue : ParsedValue->AsArray())
			{
				if (!JsonValue)
				{
					continue;
				}

				const TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
				if (!JsonObject)
				{
					continue;
				}

				CachedBranches.Add(JsonObject->GetStringField("name"));
			}
		});
		HttpRequest->ProcessRequest();
	}
}

void UPluginDownloaderInfo::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	FixupURL();
	FillAutoComplete();

	::SaveConfig(GetMutableDefault<UPluginDownloaderInfo>(), "PluginDownloaderInfo");
}

void UPluginDownloaderInfo::OnDownloadFinished(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (PendingRequest != HttpRequest)
	{
		// Cancelled
		return;
	}
	PendingRequest.Reset();

	ON_SCOPE_EXIT
	{
		if (Response == "Success")
		{
			if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString("Download successful. Do you want to restart to reload the plugin?")) == EAppReturnType::Yes)
			{
				FUnrealEdMisc::Get().RestartEditor(false);
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Failed to download:\n" + Response));
		}
	};

	if (!bSucceeded)
	{
		Response = "Request failed";
		return;
	}
	if (HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
	{
		Response = "Request failed: " + FString::FromInt(HttpResponse->GetResponseCode()) + "\n" + HttpResponse->GetContentAsString();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Downloaded %s"), *HttpRequest->GetURL());

	const TArray<uint8> Data = HttpResponse->GetContent();

	Response = "Success";
	mz_zip_archive Zip;
	mz_zip_zero_struct(&Zip);
	ON_SCOPE_EXIT
	{
		mz_zip_end(&Zip);
	};

#define CheckZip(...) \
		if ((__VA_ARGS__) != MZ_TRUE) \
		{ \
			Response = "Failed to unzip: " + FString(mz_zip_get_error_string(mz_zip_peek_last_error(&Zip))); \
			return; \
		} \
		{ \
			const mz_zip_error Error = mz_zip_peek_last_error(&Zip); \
			if (Error != MZ_ZIP_NO_ERROR) \
			{ \
				Response = "Failed to unzip: " + FString(mz_zip_get_error_string(Error)); \
				return; \
			} \
		}

#define CheckZipError() CheckZip(MZ_TRUE)

	CheckZip(mz_zip_reader_init_mem(&Zip, Data.GetData(), Data.Num(), 0));

	const int32 NumFiles = mz_zip_reader_get_num_files(&Zip);

	TMap<FString, int32> Files;
	FString UPlugin;
	for (int32 FileIndex = 0; FileIndex < NumFiles; FileIndex++)
	{
		const int32 FilenameSize = mz_zip_reader_get_filename(&Zip, FileIndex, nullptr, 0);
		CheckZipError();

		TArray<char> FilenameBuffer;
		FilenameBuffer.SetNumUninitialized(FilenameSize);
		mz_zip_reader_get_filename(&Zip, FileIndex, FilenameBuffer.GetData(), FilenameBuffer.Num());
		CheckZipError();

		// To be extra safe
		FilenameBuffer.Add(0);

		const FString Filename = FString(FilenameBuffer.GetData());
		if (Filename.EndsWith("/"))
		{
			continue;
		}

		ensure(!Files.Contains(Filename));
		Files.Add(Filename, FileIndex);

		if (Filename.EndsWith(".uplugin"))
		{
			if (!UPlugin.IsEmpty())
			{
				Response = "More than one .uplugin found: " + Filename + " and " + UPlugin;
				return;
			}
			UPlugin = Filename;
		}
	}

	if (UPlugin.IsEmpty())
	{
		Response = ".uplugin not found";
		return;
	}

	const FString PluginName = FPaths::GetBaseFilename(UPlugin);
	const FString Prefix = FPaths::GetPath(UPlugin);
	const FString PluginDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / PluginName);

	if (FPaths::DirectoryExists(PluginDir))
	{
		Response = PluginDir + " already exists";
		return;
	}

	for (const auto& It : Files)
	{
		const FString File = It.Key;
		const int32 FileIndex = It.Value;

		FString LocalPath = File;
		if (!LocalPath.RemoveFromStart(Prefix))
		{
			UE_LOG(LogTemp, Warning, TEXT("Skipping %s: not in the uplugin folder"), *File);
			continue;
		}

		const FString TargetPath = PluginDir / LocalPath;

		UE_LOG(LogTemp, Log, TEXT("Extracting %s to %s"), *File, *TargetPath);

		mz_zip_archive_file_stat FileStat;
		CheckZip(mz_zip_reader_file_stat(&Zip, FileIndex, &FileStat));

		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(FileStat.m_uncomp_size);

		CheckZip(mz_zip_reader_extract_file_to_mem(&Zip, TCHAR_TO_UTF8(*File), Buffer.GetData(), Buffer.Num(), 0));

		if (!FFileHelper::SaveArrayToFile(Buffer, *TargetPath))
		{
			Response = "Failed to write " + TargetPath;
			return;
		}
	}
#undef CheckZipError
#undef CheckZip
}

#undef LOCTEXT_NAMESPACE