// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderEditorModule.h"
#include "Utilities.h"

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

// Hack to make the marketplace review happy
#include "miniz.h"
#include "miniz.cpp"

#include "HttpModule.h"
#include "HttpManager.h"
#include "UnrealEdMisc.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/ConfigCacheIni.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HTTP/Private/HttpThread.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <processthreadsapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "PluginDownloader"

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

		const TSharedRef<IPropertyHandle> AccessTokenHandle = DetailLayout.GetProperty("AccessToken");
		DetailLayout.EditDefaultProperty(AccessTokenHandle)->CustomWidget()
		.NameContent()
		[
			AccessTokenHandle->CreatePropertyNameWidget()
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
					AccessTokenHandle->CreatePropertyValueWidget()
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
					FPlatformProcess::LaunchURL(TEXT("https://github.com/settings/tokens"), nullptr, nullptr);
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

		UPluginDownloaderInfo* Info = GetMutableDefault<UPluginDownloaderInfo>();
		FUtilities::LoadConfig(Info, "PluginDownloaderInfo");
		Info->AccessToken = FUtilities::DecryptData(Info->AccessToken);

		GetMutableDefault<UPluginDownloaderInfo>()->FillAutoComplete();

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		const TSharedRef<IDetailsView> DetailsView = PropertyModule.CreateDetailView(Args);
		DetailsView->SetObject(Info);

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
	AddAuth(*PendingRequest);
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
	const FString Prefix = bIsOrganization ? "orgs" : "users";

	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetURL("https://api.github.com" / Prefix / User + "/repos?per_page=100");
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
		AddAuth(*HttpRequest);
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
		AddAuth(*HttpRequest);
		HttpRequest->ProcessRequest();
	}
}

void UPluginDownloaderInfo::AddAuth(IHttpRequest& Request) const
{
	Request.SetHeader("Authorization", "token " + AccessToken);
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

	AccessToken = FUtilities::EncryptData(AccessToken);
	FUtilities::SaveConfig(this, "PluginDownloaderInfo");
	AccessToken = FUtilities::DecryptData(AccessToken);
}

void UPluginDownloaderInfo::FixupURL()
{
	URL = "https://api.github.com/repos/" + User + "/" + Repo + "/zipball/" + Branch;
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
		FPlatformProcess::Sleep(1);

		const TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (ActiveWindow.IsValid())
		{
			ActiveWindow->HACK_ForceToFront();
		}

		if (Response == "Success")
		{
			if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString("Download successful. Do you want to restart to reload the plugin?")) == EAppReturnType::Yes)
			{
				GEngine->DeferredCommands.Add(TEXT("CLOSE_SLATE_MAINFRAME"));
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

	const FString Timestamp = FDateTime::Now().ToString();
	const FString PluginName = FPaths::GetBaseFilename(UPlugin);
	const FString Prefix = FPaths::GetPath(UPlugin);
	const FString PluginDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / PluginName);
	const FString PluginDownloaderDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() / "PluginDownloader");

	const FString PluginTrashDir = PluginDownloaderDir / "Trash" /  Timestamp / PluginName;
	const FString PluginDownloadDir = PluginDownloaderDir / "Downloads" / Timestamp / PluginName;
	const FString BatchFile = PluginDownloaderDir / "InstallPlugin.bat";
	const FString RestartBatchFile = PluginDownloaderDir / "RestartEngine.bat";
	const FString PluginBatchFile = PluginDownloaderDir / "InstallPlugin_" + PluginName + ".bat";

	IFileManager::Get().MakeDirectory(*PluginTrashDir, true);

	if (FPaths::DirectoryExists(PluginDir))
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(PluginDir + " already exists. Do you want to overwrite it?")) == EAppReturnType::No)
		{
			Response = PluginDir + " already exists";
			return;
		}
	}

	const FString Batch
	{
#include "InstallPluginScript.inl"
	};
	if (!FFileHelper::SaveStringToFile(Batch, *BatchFile))
	{
		Response = "Failed to write " + BatchFile;
		return;
	}

	const FString PluginBatch = FString::Printf(TEXT("start InstallPlugin.bat %u \"%s\" \"%s\" \"%s\" %s"),
		FPlatformProcess::GetCurrentProcessId(),
		*PluginDir,
		*PluginTrashDir,
		*PluginDownloadDir,
		*PluginName);

	if (!FFileHelper::SaveStringToFile(PluginBatch, *PluginBatchFile))
	{
		Response = "Failed to write " + PluginBatchFile;
		return;
	}

	FString RestartBatch = "start \"";
	RestartBatch += FPlatformProcess::ExecutablePath();
	RestartBatch += "\" ";
	RestartBatch += FCommandLine::GetOriginal();
	RestartBatch += "\r\nexit";

	if (!FFileHelper::SaveStringToFile(RestartBatch, *RestartBatchFile))
	{
		Response = "Failed to write " + RestartBatchFile;
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

		const FString TargetPath = PluginDownloadDir / LocalPath;

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

	FString CommandLine = PluginBatchFile.Replace(TEXT("/"), TEXT("\\"));

#if PLATFORM_WINDOWS
	// initialize startup info
	STARTUPINFOW StartupInfo = {};
	StartupInfo.cb = sizeof(StartupInfo);

	StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
	StartupInfo.wShowWindow = SW_SHOW;

	PROCESS_INFORMATION ProcInfo;
	const BOOL bSuccess = CreateProcessW(
		nullptr,
		CommandLine.GetCharArray().GetData(),
		nullptr,
		nullptr,
		Windows::FALSE,
		CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE,
		nullptr,
		*FPaths::GetPath(BatchFile),
		&StartupInfo,
		&ProcInfo);

	if (!ensure(bSuccess))
	{
		Response = "Failed to create bat process";
		return;
	}
#else
	ensure(false);
#endif
}

#undef LOCTEXT_NAMESPACE