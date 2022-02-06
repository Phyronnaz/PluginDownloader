// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloader.h"
#include "Utilities.h"
#include "HttpModule.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/MessageDialog.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Application/SlateApplication.h"

// Hack to make the marketplace review happy
#include "miniz.h"
#include "miniz.cpp"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <processthreadsapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY(LogPluginDownloader);

void UPluginDownloaderTokens::LoadFromConfig()
{
	FUtilities::LoadConfig(this, "PluginDownloaderTokens");

	GithubAccessToken = FUtilities::DecryptData(GithubAccessToken_Encrypted);
}

void UPluginDownloaderTokens::SaveToConfig()
{
	GithubAccessToken_Encrypted = FUtilities::EncryptData(GithubAccessToken);

	FUtilities::SaveConfig(this, "PluginDownloaderTokens");
}

void UPluginDownloaderTokens::CheckTokens()
{
	if (GithubAccessToken.IsEmpty())
	{
		GithubStatus.Reset();
		return;
	}

	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://api.github.com/");
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindWeakLambda(this, [=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		if (!bSucceeded)
		{
			GithubStatus = "Failed to query github.com";
			return;
		}

		if (HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
		{
			UE_LOG(LogPluginDownloader, Error, TEXT("Invalid token: %s"), *HttpResponse->GetContentAsString());
			GithubStatus = "Invalid token";
			return;
		}

		const FString Scope = HttpResponse->GetHeader("X-OAuth-Scopes");

		TArray<FString> Scopes;
		Scope.ParseIntoArray(Scopes, TEXT(","));
		if (!Scopes.Contains("repo"))
		{
			GithubStatus = "Token needs to have the 'repo' scope";
			return;
		}

		GithubStatus = {};
	});
	Request->SetHeader("Authorization", "token " + GithubAccessToken);
	Request->ProcessRequest();
}

bool UPluginDownloaderTokens::AddAuthToRequest(EPluginDownloaderHost Host, IHttpRequest& Request, FString& OutError) const
{
	check(Host == EPluginDownloaderHost::Github);

	if (!GithubStatus.IsEmpty())
	{
		ensure(!GithubAccessToken.IsEmpty());
		OutError = GithubStatus;
		return false;
	}

	if (!GithubAccessToken.IsEmpty())
	{
		Request.SetHeader("Authorization", "token " + GithubAccessToken);
	}

	return true;
}

void UPluginDownloaderTokens::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SaveToConfig();
	CheckTokens();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FPluginDownloaderInfo::GetURL() const
{
	check(Host == EPluginDownloaderHost::Github);
	return "https://api.github.com/repos/" + User + "/" + Repo + "/zipball/" + Branch;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FPluginDownloaderDownload::FPluginDownloaderDownload(const FHttpRequestRef& Request)
	: Request(Request)
{
}

void FPluginDownloaderDownload::Cancel()
{
	ensure(!bIsCancelled);
	bIsCancelled = true;
	Request->CancelRequest();
	
	UE_LOG(LogPluginDownloader, Log, TEXT("Cancelled %s"), *Request->GetURL());
}

void FPluginDownloaderDownload::Start()
{
	Request->OnRequestProgress().BindSP(this, &FPluginDownloaderDownload::OnProgressImpl);
	Request->OnProcessRequestComplete().BindSP(this, &FPluginDownloaderDownload::OnCompleteImpl);
	Request->ProcessRequest();

	UE_LOG(LogPluginDownloader, Log, TEXT("Downloading %s"), *Request->GetURL());
}

void FPluginDownloaderDownload::OnProgressImpl(FHttpRequestPtr HttpRequest, int32 BytesSent, int32 BytesReceived)
{
	check(HttpRequest == Request);
	Progress = BytesReceived;

	OnProgress.Broadcast();
}

void FPluginDownloaderDownload::OnCompleteImpl(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	ensure(!bIsDone);
	bIsDone = true;

	if (bIsCancelled)
	{
		return;
	}
	check(HttpRequest == Request);

	FPluginDownloader::OnComplete(HttpRequest, HttpResponse, bSucceeded);
	OnComplete.Broadcast();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FPluginDownloaderDownload> FPluginDownloader::DownloadPlugin(const FPluginDownloaderInfo& Info)
{
	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Info.GetURL());
	Request->SetVerb(TEXT("GET"));

	FString Error;
	if (!GetDefault<UPluginDownloaderTokens>()->AddAuthToRequest(Info.Host, *Request, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Can't download: " + Error));
		return nullptr;
	}

	const TSharedRef<FPluginDownloaderDownload> Download = MakeShareable(new FPluginDownloaderDownload(Request));
	Download->Start();
	return Download;
}

void FPluginDownloader::GetRepoAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived, bool bIsOrganization)
{
	check(Info.Host == EPluginDownloaderHost::Github);

	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://api.github.com" / FString(bIsOrganization ? "orgs" : "users") / Info.User / "repos?per_page=100");
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		if (!bSucceeded || HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
		{
			if (!bIsOrganization)
			{
				GetRepoAutocomplete(Info, OnAutocompleteReceived, true);
			}
			return;
		}

		TSharedPtr<FJsonValue> ParsedValue;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, ParsedValue))
		{
			return;
		}

		TArray<FString> Result;
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

			Result.Add(JsonObject->GetStringField("name"));
		}

		OnAutocompleteReceived.ExecuteIfBound(Result);
	});

	FString Error;
	if (GetDefault<UPluginDownloaderTokens>()->AddAuthToRequest(Info.Host, *Request, Error))
	{
		Request->ProcessRequest();
	}
}

void FPluginDownloader::GetBranchAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived)
{
	check(Info.Host == EPluginDownloaderHost::Github);

	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://api.github.com/repos/" + Info.User / Info.Repo / "/branches");
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
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

		TArray<FString> Result;
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

			Result.Add(JsonObject->GetStringField("name"));
		}

		OnAutocompleteReceived.ExecuteIfBound(Result);
	});

	FString Error;
	if (GetDefault<UPluginDownloaderTokens>()->AddAuthToRequest(Info.Host, *Request, Error))
	{
		Request->ProcessRequest();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FPluginDownloader::OnComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	const FString Error = OnComplete_Internal(HttpResponse, bSucceeded);

	FPlatformProcess::Sleep(1);

	const TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (ActiveWindow.IsValid())
	{
		ActiveWindow->HACK_ForceToFront();
	}

	if (Error.IsEmpty())
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString("Download successful. Do you want to restart to reload the plugin?")) == EAppReturnType::Yes)
		{
			GEngine->DeferredCommands.Add(TEXT("CLOSE_SLATE_MAINFRAME"));
		}
	}
	else
	{
		const FString FullError = "Failed to download " + HttpRequest->GetURL() + ":\n" + Error;

		UE_LOG(LogPluginDownloader, Error, TEXT("%s"), *FullError);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FullError));
	}
}

FString FPluginDownloader::OnComplete_Internal(FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (!bSucceeded || !HttpResponse)
	{
		return "Request failed";
	}
	if (HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
	{
		return "Request failed: " + FString::FromInt(HttpResponse->GetResponseCode()) + "\n" + HttpResponse->GetContentAsString();
	}

	UE_LOG(LogPluginDownloader, Log, TEXT("Downloaded %s"), *HttpResponse->GetURL());

	const TArray<uint8> Data = HttpResponse->GetContent();

	mz_zip_archive Zip;
	mz_zip_zero_struct(&Zip);
	ON_SCOPE_EXIT
	{
		mz_zip_end(&Zip);
	};

#define CheckZip(...) \
		if ((__VA_ARGS__) != MZ_TRUE) \
		{ \
			return "Failed to unzip: " + FString(mz_zip_get_error_string(mz_zip_peek_last_error(&Zip))); \
		} \
		{ \
			const mz_zip_error Error = mz_zip_peek_last_error(&Zip); \
			if (Error != MZ_ZIP_NO_ERROR) \
			{ \
				return "Failed to unzip: " + FString(mz_zip_get_error_string(Error)); \
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
				return "More than one .uplugin found: " + Filename + " and " + UPlugin;
			}
			UPlugin = Filename;
		}
	}

	if (UPlugin.IsEmpty())
	{
		return ".uplugin not found";
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
			return PluginDir + " already exists";
		}
	}

	const FString Batch
	{
#include "InstallPluginScript.inl"
	};
	if (!FFileHelper::SaveStringToFile(Batch, *BatchFile))
	{
		return "Failed to write " + BatchFile;
	}

	const FString PluginBatch = FString::Printf(TEXT("start InstallPlugin.bat %u \"%s\" \"%s\" \"%s\" %s"),
		FPlatformProcess::GetCurrentProcessId(),
		*PluginDir,
		*PluginTrashDir,
		*PluginDownloadDir,
		*PluginName);

	if (!FFileHelper::SaveStringToFile(PluginBatch, *PluginBatchFile))
	{
		return "Failed to write " + PluginBatchFile;
	}

	FString RestartBatch = "start \"";
	RestartBatch += FPlatformProcess::ExecutablePath();
	RestartBatch += "\" ";
	RestartBatch += FCommandLine::GetOriginal();
	RestartBatch += "\r\nexit";

	if (!FFileHelper::SaveStringToFile(RestartBatch, *RestartBatchFile))
	{
		return "Failed to write " + RestartBatchFile;
	}

	for (const auto& It : Files)
	{
		const FString File = It.Key;
		const int32 FileIndex = It.Value;

		FString LocalPath = File;
		if (!LocalPath.RemoveFromStart(Prefix))
		{
			UE_LOG(LogPluginDownloader, Warning, TEXT("Skipping %s: not in the uplugin folder"), *File);
			continue;
		}

		const FString TargetPath = PluginDownloadDir / LocalPath;

		UE_LOG(LogPluginDownloader, Log, TEXT("Extracting %s to %s"), *File, *TargetPath);

		mz_zip_archive_file_stat FileStat;
		CheckZip(mz_zip_reader_file_stat(&Zip, FileIndex, &FileStat));

		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(FileStat.m_uncomp_size);

		CheckZip(mz_zip_reader_extract_file_to_mem(&Zip, TCHAR_TO_UTF8(*File), Buffer.GetData(), Buffer.Num(), 0));

		if (!FFileHelper::SaveArrayToFile(Buffer, *TargetPath))
		{
			return "Failed to write " + TargetPath;
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
		return "Failed to create bat process";
	}
#else
	ensure(false);
#endif

	return {};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UPluginDownloaderBase::Download()
{
	ensure(!IsDownloading());
	ActiveDownload = FPluginDownloader::DownloadPlugin(GetInfo());
	if (!ActiveDownload)
	{
		return;
	}

	ActiveDownload->OnProgress.AddWeakLambda(this, [=]
	{
		Progress = FString::Printf(TEXT("%f MB received"), ActiveDownload->GetProgress() / float(1 << 20));
	});

	ActiveDownload->OnComplete.AddWeakLambda(this, [=]
	{
		Progress.Reset();
	});
}

void UPluginDownloaderBase::Cancel()
{
	ensure(IsDownloading());

	Progress = "Cancelled";

	ActiveDownload->Cancel();
	ActiveDownload.Reset();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UPluginDownloaderCustom::FillAutoComplete()
{
	FPluginDownloader::GetRepoAutocomplete(Info, FOnAutocompleteReceived::CreateWeakLambda(this, [=](const TArray<FString>& Result)
	{
		RepoOptions = Result;
	}));

	FPluginDownloader::GetBranchAutocomplete(Info, FOnAutocompleteReceived::CreateWeakLambda(this, [=](const TArray<FString>& Result)
	{
		BranchOptions = Result;
	}));
}

void UPluginDownloaderCustom::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FillAutoComplete();

	FUtilities::SaveConfig(this, "PluginDownloaderCustom");
}