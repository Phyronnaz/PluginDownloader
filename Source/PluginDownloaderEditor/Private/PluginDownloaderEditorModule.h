// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "miniz.h"
#include "HttpModule.h"
#include "UnrealEdMisc.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PluginDownloaderEditorModule.generated.h"

UCLASS()
class UPluginDownloaderInfo : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Settings")
	FString Domain = "https://github.com";

	UPROPERTY(EditAnywhere, Category = "Settings")
	FString User = "Phyronnaz";

	UPROPERTY(EditAnywhere, Category = "Settings")
	FString Repo = "HLSLMaterial";

	UPROPERTY(EditAnywhere, Category = "Settings")
	FString Branch = "master";

public:
	UPROPERTY(VisibleAnywhere, Category = "Output")
	FString URL;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FString Response;

public:
	void Download()
	{
		FixupURL();

		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetURL(URL);
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPluginDownloaderInfo::OnDownloadFinished);
		HttpRequest->OnRequestProgress().BindWeakLambda(this, [=](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
		{
			Response = FString::Printf(TEXT("%f MB received"), BytesReceived / float(1 << 20));
		});
		HttpRequest->ProcessRequest();

		UE_LOG(LogTemp, Log, TEXT("Downloading %s"), *HttpRequest->GetURL());
	}

private:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);

		FixupURL();
	}

	void FixupURL()
	{
		URL = Domain / User / Repo / "archive" / "refs" / "heads" / Branch + ".zip";
	}
	void OnDownloadFinished(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
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
};