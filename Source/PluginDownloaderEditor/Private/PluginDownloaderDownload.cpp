// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderDownload.h"
#include "PluginDownloaderTokens.h"
#include "PluginDownloaderUtilities.h"

bool GPluginDownloaderRestartPending = false;
FPluginDownloaderDownload* GActivePluginDownloaderDownload = nullptr;

void FPluginDownloaderDownload::StartDownload(const FPluginDownloaderInfo& Info)
{
	if (GActivePluginDownloaderDownload)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Can't download: another plugin is already downloading"));
		return;
	}

	if (GPluginDownloaderRestartPending)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Can't download: another plugin is pending install, please restart the engine"));
		return;
	}

	const UPluginDownloaderTokens* Tokens = GetDefault<UPluginDownloaderTokens>();
	if (!Tokens->HasValidToken())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Can't download: " + Tokens->GetTokenError()));
		return;
	}

	GActivePluginDownloaderDownload = new FPluginDownloaderDownload(Info);
	GActivePluginDownloaderDownload->Start();
}

void FPluginDownloaderDownload::Destroy(const FString& Reason)
{
	ensure(GActivePluginDownloaderDownload == this);
	GActivePluginDownloaderDownload = nullptr;

	if (!Reason.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Reason));
	}

	// Delay the deletion until two frames to be safe
	FPluginDownloaderUtilities::DelayedCall([=]
	{
		FPluginDownloaderUtilities::DelayedCall([=]
		{
			delete this;
		});
	});
}

void FPluginDownloaderDownload::Start()
{
	check(GActivePluginDownloaderDownload == this);

	Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://api.github.com/repos" / Info.User / Info.Repo / "zipball" / Info.Branch);
	Request->SetVerb(TEXT("GET"));
	GetDefault<UPluginDownloaderTokens>()->AddAuthToRequest(*Request);
	
	ProgressWindow =
		SNew(SWindow)
		.Title(NSLOCTEXT("PluginDownloader", "DownloadingPlugin", "Downloading Plugin"))
		.ClientSize(FVector2D(400, 100))
		.IsTopmostWindow(true);

	ProgressWindow->SetContent(
		SNew(SBorder)
		.BorderImage(FEditorAppStyle::GetBrush("NoBorder"))
		.Padding(2)
		[
			SNew(SBorder)
			.BorderImage(FEditorAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(2)
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([=]
					{
						return FText::FromString(FString::Printf(TEXT("%f MB received"), RequestProgress / float(1 << 20)));
					})
				]
			]
		]
	);

	ProgressWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([=](const TSharedRef<SWindow>&)
	{
		if (!Request)
		{
			return;
		}

		Request->CancelRequest();

		ensure(!bRequestCancelled);
		bRequestCancelled = true;

		UE_LOG(LogPluginDownloader, Log, TEXT("Cancelled %s"), *Request->GetURL());
	}));

	FSlateApplication::Get().AddWindow(ProgressWindow.ToSharedRef());

	Request->OnRequestProgress().BindRaw(this, &FPluginDownloaderDownload::OnRequestProgress);
	Request->OnProcessRequestComplete().BindRaw(this, &FPluginDownloaderDownload::OnRequestComplete);
	Request->ProcessRequest();

	UE_LOG(LogPluginDownloader, Log, TEXT("Downloading %s"), *Request->GetURL());
}

void FPluginDownloaderDownload::OnRequestProgress(FHttpRequestPtr HttpRequest, int32 BytesSent, int32 BytesReceived)
{
	ensure(HttpRequest == Request);
	RequestProgress = BytesReceived;
}

void FPluginDownloaderDownload::OnRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	ensure(GActivePluginDownloaderDownload == this);
	ensure(HttpRequest == Request);

	// Make sure OnWindowClosed exits early
	Request.Reset();

	check(ProgressWindow);
	// Make sure the dialog is on top
	ProgressWindow->Minimize();
	ProgressWindow->RequestDestroyWindow();
	ProgressWindow.Reset();

	if (bRequestCancelled)
	{
		return Destroy("Download cancelled");
	}

	if (!bSucceeded || !ensure(HttpResponse))
	{
		return Destroy("Query failed");
	}
	
	if (HttpResponse->GetResponseCode() == EHttpResponseCodes::NotFound)
	{
		return Destroy("Repository or branch not found. Make sure you have a valid access token.\nYour engine version might also not be supported by the plugin");
	}
	if (HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
	{
		return Destroy("Request failed: " + FString::FromInt(HttpResponse->GetResponseCode()) + "\n" + HttpResponse->GetContentAsString());
	}

	UE_LOG(LogPluginDownloader, Log, TEXT("Downloaded %s"), *HttpResponse->GetURL());

	TMap<FString, TArray<uint8>> Files;
	const FString ZipError = FPluginDownloaderUtilities::Unzip(HttpResponse->GetContent(), Files);
	if (!ZipError.IsEmpty())
	{
		return Destroy("Failed to unzip: " + ZipError);
	}
	
	FString UPlugin;
	for (auto& It : Files)
	{
		const FString& Filename = It.Key;
		if (!Filename.EndsWith(".uplugin"))
		{
			continue;
		}

		if (!UPlugin.IsEmpty())
		{
			return Destroy("More than one .uplugin found: " + Filename + " and " + UPlugin);
		}
		UPlugin = Filename;
	}

	if (UPlugin.IsEmpty())
	{
		return Destroy(".uplugin not found");
	}

	// Trim files
	{
		const FString Prefix = FPaths::GetPath(UPlugin);
		for (auto It = Files.CreateIterator(); It; ++It)
		{
			if (!It.Key().RemoveFromStart(Prefix))
			{
				UE_LOG(LogPluginDownloader, Warning, TEXT("Skipping %s: not in the uplugin folder"), *It.Key());
				It.RemoveCurrent();
			}
		}
	}

	const FString PluginName = FPaths::GetBaseFilename(UPlugin);
	const FString RepoName = Info.Repo;

	// Manually find uplugin files to handle cases where a plugin is installed both in the project and in engine

	TArray<FString> ExistingPluginsDir;
	{
		const FString UPluginFilename = FPaths::GetCleanFilename(UPlugin);

		TArray<FString> PluginRoots;
		PluginRoots.Add(FPaths::EnginePluginsDir());
		PluginRoots.Add(FPaths::ProjectPluginsDir());

		TArray<FString> PluginPaths;
		for (const FString& PluginRoot : PluginRoots)
		{
			IFileManager::Get().IterateDirectoryRecursively(*PluginRoot, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
			{
				if (!bIsDirectory && FPaths::GetCleanFilename(FilenameOrDirectory) == UPluginFilename)
				{
					PluginPaths.Add(FPaths::GetPath(FPaths::ConvertRelativePathToFull(FilenameOrDirectory)));
				}
				return true;
			});
		}

		for (const FString& PluginPath : PluginPaths)
		{
			if (FPaths::IsUnderDirectory(PluginPath, FPaths::EnginePluginsDir()))
			{
				if (Info.InstallLocation == EPluginDownloadInstallLocation::Engine)
				{
					ExistingPluginsDir.Add(PluginPath);
				}
				else
				{
					ensure(Info.InstallLocation == EPluginDownloadInstallLocation::Project);
					// Nothing to do - project plugins overriden engine ones
				}
			}
			else
			{
				ensure(FPaths::IsUnderDirectory(PluginPath, FPaths::ProjectPluginsDir()));
				if (Info.InstallLocation == EPluginDownloadInstallLocation::Engine)
				{
					const FString Message =
						"Installing plugin in engine, but there's already a project plugin with the same name.\n"
						"Delete the project plugin to use the engine plugin\n\n"
						"Path: " + PluginPath;

					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
				}
				else
				{
					ensure(Info.InstallLocation == EPluginDownloadInstallLocation::Project);
					ExistingPluginsDir.Add(PluginPath);
				}
			}
		}
	}

	FString ExistingPluginDir;
	if (ExistingPluginsDir.Num() > 0)
	{
		if (ExistingPluginsDir.Num() > 1)
		{
			return Destroy("Multiple plugins with the same name found:\n\n" + FString::Join(ExistingPluginsDir, TEXT("\n")));
		}

		ExistingPluginDir = ExistingPluginsDir[0];

		FString Path = ExistingPluginsDir[0];
		const FString Message = "There is already a " + PluginName + ".uplugin in " + ExistingPluginDir + ". Do you want to delete it?";
		if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Message)) != EAppReturnType::Yes)
		{
			return Destroy("Download cancelled");
		}
	}

	const FString Timestamp = FDateTime::Now().ToString();
	const FString IntermediateDir = FPluginDownloaderUtilities::GetIntermediateDir();

	const FString InstallDir = FPaths::ConvertRelativePathToFull(
		Info.InstallLocation == EPluginDownloadInstallLocation::Project
		? FPaths::ProjectPluginsDir() / RepoName
		: FPaths::EnginePluginsDir() / "Marketplace" / RepoName);

	const FString TrashDir = IntermediateDir / "Trash" / PluginName + "_" + Timestamp;
	const FString DownloadDir = IntermediateDir / "Download";
	const FString PackagedDir = IntermediateDir / "Packaged";

	// Delete download/packaged directories left over from previous installs
	IFileManager::Get().DeleteDirectory(*DownloadDir, false, true);
	IFileManager::Get().DeleteDirectory(*PackagedDir, false, true);

	const FString PluginBatchFile = IntermediateDir / "InstallPlugin_" + PluginName + ".bat";
	const FString PluginAdminBatchFile = IntermediateDir / "InstallPlugin_" + PluginName + "_Admin.bat";

	const FString UPluginDownloadPath = DownloadDir / FPaths::GetCleanFilename(UPlugin);
	const FString UPluginPackagedPath = PackagedDir / FPaths::GetCleanFilename(UPlugin);

	if (FPaths::DirectoryExists(InstallDir) &&
		!FPaths::IsSamePath(InstallDir, ExistingPluginDir) &&
		!IFileManager::Get().DeleteDirectory(*InstallDir, false, false))
	{
		return Destroy("Can't install plugin into " + InstallDir + ": folder already exists but is a different plugin");
	}
	
	IFileManager::Get().MakeDirectory(*TrashDir, true);
	
	// InstallPlugin.bat
	if (!FPluginDownloaderUtilities::WriteInstallPluginBatch())
	{
		return Destroy("Failed to write InstallPlugin.bat");
	}

	// InstallPlugin_XXXX.bat: calls InstallPlugin.bat with the right parameters
	{
		FString Batch = "cd /D \"" + IntermediateDir + "\"\r\n";
		Batch += FString::Printf(TEXT("start InstallPlugin.bat %u \"%s\" \"%s\" \"%s\" \"%s\" %s"),
			FPlatformProcess::GetCurrentProcessId(),
			*ExistingPluginDir,
			*TrashDir,
			*PackagedDir,
			*InstallDir,
			*RepoName);

		if (!FFileHelper::SaveStringToFile(Batch, *PluginBatchFile))
		{
			return Destroy("Failed to write " + PluginBatchFile);
		}
	}

	// InstallPlugin_XXXX_Admin.bat: calls InstallPlugin_XXXX.bat as administrator
	{
		const FString Batch = "powershell Start -File InstallPlugin_" + PluginName + ".bat -Verb RunAs";

		if (!FFileHelper::SaveStringToFile(Batch, *PluginAdminBatchFile))
		{
			return Destroy("Failed to write " + PluginAdminBatchFile);
		}
	}

	// RestartEngine.bat: restarts the engine with the same parameters
	if (!FPluginDownloaderUtilities::WriteRestartEngineBatch())
	{
		return Destroy("Failed to write RestartEngine.bat");
	}

	for (const auto& It : Files)
	{
		const FString TargetPath = DownloadDir / It.Key;

		if (!FFileHelper::SaveArrayToFile(It.Value, *TargetPath))
		{
			return Destroy("Failed to write " + TargetPath);
		}
	}

	// Don't compile targets for project plugins as it takes forever
	const bool bOnlyCompileEditor = Info.InstallLocation == EPluginDownloadInstallLocation::Project;

	const FString UatCommandLine = FString::Printf(TEXT("BuildPlugin %s -Plugin=\"%s\" -Package=\"%s\" -VS2019"), bOnlyCompileEditor ? TEXT("-NoTargetPlatforms") : TEXT(""), *UPluginDownloadPath, *PackagedDir);

	IUATHelperModule::Get().CreateUatTask(
		UatCommandLine,
		INVTEXT("Windows"),
		INVTEXT("Packaging Plugin"),
		INVTEXT("Package Plugin Task"),
		FEditorAppStyle::GetBrush(TEXT("MainFrame.CookContent")),
		UE_501_ONLY(nullptr,)
		[=](const FString& Result, double)
	{
		// Is called from an async thread
		AsyncTask(ENamedThreads::GameThread, [=]
		{
			if (!IFileManager::Get().FileExists(*UPluginPackagedPath))
			{
				return Destroy("Packaging failed. Check log for errors.");
			}

			if (FPaths::GetBaseFilename(UPlugin) == "PluginDownloader")
			{
				FText Error;
				FPluginDescriptor Descriptor;
				if (ensureMsgf(Descriptor.Load(*UPluginPackagedPath, Error), TEXT("%s"), *Error.ToString()))
				{
					Descriptor.EnabledByDefault = EPluginEnabledByDefault::Enabled;
					ensureMsgf(Descriptor.Save(*UPluginPackagedPath, Error), TEXT("%s"), *Error.ToString());
				}
			}

			OnPackageComplete(Result, PluginBatchFile, PluginAdminBatchFile);
		});
	});
}

void FPluginDownloaderDownload::OnPackageComplete(const FString& Result, const FString& PluginBatchFile, const FString& PluginAdminBatchFile)
{
	ensure(GActivePluginDownloaderDownload == this);
	check(IsInGameThread());
	
	if (Result != "Completed")
	{
		if (Result == "Canceled")
		{
			return Destroy("Packaging cancelled");
		}
		else
		{
			ensure(Result == "Failed");
			return Destroy("Packaging failed. Check log for errors.");
		}
	}

	const FString BatchFile = Info.InstallLocation == EPluginDownloadInstallLocation::Project ? PluginBatchFile : PluginAdminBatchFile;
	if (Info.InstallLocation == EPluginDownloadInstallLocation::Engine)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Administrator rights will be asked to install the plugin in engine"));
	}

	if (!FPluginDownloaderUtilities::ExecuteDetachedBatch(BatchFile))
	{
		return Destroy("Failed to execute bat file");
	}

	ensure(!GPluginDownloaderRestartPending);
	GPluginDownloaderRestartPending = true;

	const TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (ActiveWindow.IsValid())
	{
		ActiveWindow->HACK_ForceToFront();
	}

	if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString("Download successful. Do you want to restart to reload the plugin?")) == EAppReturnType::Yes)
	{
		GEngine->DeferredCommands.Add(TEXT("CLOSE_SLATE_MAINFRAME"));
	}

	Destroy("");
}