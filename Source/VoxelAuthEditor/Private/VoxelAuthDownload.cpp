// Copyright Voxel Plugin, Inc. All Rights Reserved.

#if ENGINE_VERSION >= 500
#include "VoxelAuthDownload.h"
#include "VoxelAuthApi.h"
#include "PluginDownloaderUtilities.h"
#include "Compression/OodleDataCompressionUtil.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

FVoxelAuthDownload* GVoxelAuthDownload = nullptr;

void FVoxelAuthDownload::Download(const FString& Branch, int32 Counter)
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("VoxelPro"))
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString("VoxelPro found in " + FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()) + ". Please uninstall it first."));
		return;
	}
	
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("VoxelFree"))
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString("VoxelFree found in " + FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()) + ". Please uninstall it first."));
		return;
	}

	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("Voxel"))
	{
		TArray<FString> Array;
		Plugin->GetDescriptor().VersionName.ParseIntoArray(Array, TEXT("-"));

		if (Array.Num() != 4)
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::FromString("Unknown version of Voxel found in " + FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()) + ". Please uninstall it first."));
			return;
		}

		const FString ExpectedPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / "Voxel");
		const FString Path = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
		if (!FPaths::IsSamePath(ExpectedPath, Path))
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::FromString("Voxel found in " + Path + ", but should be " + ExpectedPath + ". Please uninstall it or move it."));
			return;
		}
	}

	if (Notification)
	{
		FMessageDialog::Open(EAppMsgType::Ok, INVTEXT("Already downloading. You might need to restart the engine."));
		return;
	}

	FNotificationInfo Info(FText::FromString("Downloading Voxel Plugin " + Branch + "-" + FString::FromInt(Counter)));
	Info.bFireAndForget = false;
	Notification = FSlateNotificationManager::Get().AddNotification(Info);

	GVoxelAuthApi->MakeRequest(
		"version/download",
		{
			{ "branch", Branch },
			{ "counter", FString::FromInt(Counter) },
			{ "unrealVersion", FString::FromInt(ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION) },
		},
		FVoxelAuthApi::ERequestVerb::Get,
		true,
		FHttpRequestCompleteDelegate::CreateLambda([=](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully ||
				!Response ||
				Response->GetResponseCode() != 200)
			{
				Fail("Failed get download URL for Voxel Plugin");
				return;
			}
			
			TSharedPtr<FJsonValue> ParsedValue;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (!ensure(FJsonSerializer::Deserialize(Reader, ParsedValue)) ||
				!ensure(ParsedValue))
			{
				return;
			}

			const TSharedPtr<FJsonObject> Object = ParsedValue->AsObject();
			if (!ensure(Object))
			{
				return;
			}

			const FString Url = Object->GetStringField("url");
			const int64 Size = Object->GetNumberField("size");
			const FString Hash = Object->GetStringField("hash");
			const FString AesKey = Object->GetStringField("aesKey");
			if (!ensure(!Url.IsEmpty()) ||
				!ensure(Size > 0) ||
				!ensure(!Hash.IsEmpty()) ||
				!ensure(!AesKey.IsEmpty()))
			{
				return;
			}
			UE_LOG(LogPluginDownloader, Log, TEXT("Hash: %s"), *Hash);

			const FString Path = GVoxelAuthApi->GetAppDataPath() / "Cache" / Hash;

			TArray<uint8> Result;
			if (FFileHelper::LoadFileToArray(Result, *Path))
			{
				if (ensure(FSHA1::HashBuffer(Result.GetData(), Result.Num()).ToString() == Hash))
				{
					IFileManager::Get().SetTimeStamp(*Path, FDateTime::UtcNow());
					FinalizeDownload(Result, AesKey);
					return;
				}
			}

			const TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
			Request->SetURL(Url);
			Request->SetVerb(TEXT("GET"));
			Request->OnRequestProgress().BindLambda([=](FHttpRequestPtr, int32 BytesSent, int32 BytesReceived)
			{
				if (ensure(Notification))
				{
					Notification->SetSubText(FText::FromString(FString::Printf(
						TEXT("%.1f/%.1fMB"),
						BytesReceived / double(1 << 20),
						Size / double(1 << 20))));
				}
			});
			Request->OnProcessRequestComplete().BindLambda([this, Path, Hash, AesKey](FHttpRequestPtr, FHttpResponsePtr NewResponse, bool bNewConnectedSuccessfully)
			{
				if (!bNewConnectedSuccessfully ||
					!NewResponse ||
					NewResponse->GetResponseCode() != 200)
				{
					Fail("Failed to download Voxel Plugin");
					return;
				}

				const TArray<uint8> Result = NewResponse->GetContent();
				
				if (!ensure(FSHA1::HashBuffer(Result.GetData(), Result.Num()).ToString() == Hash))
				{
					Fail("Failed to download Voxel Plugin: invalid hash");
					return;
				}

				ensure(FFileHelper::SaveArrayToFile(Result, *Path));
				IFileManager::Get().SetTimeStamp(*Path, FDateTime::UtcNow());

				TArray<FString> Files;
				IFileManager::Get().FindFiles(Files, *FPaths::GetPath(Path), true, false);

				int64 TotalSize = 0;
				for (const FString& File : Files)
				{
					TotalSize += IFileManager::Get().FileSize(*File);
				}

				constexpr int64 MaxSize = 1024 * 1024 * 1024;
				while (TotalSize > MaxSize)
				{
					FString OldestFile;
					FDateTime OldestFileTimestamp;
					for (const FString& File : Files)
					{
						const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*File);

						if (OldestFile.IsEmpty() ||
							Timestamp < OldestFileTimestamp)
						{
							OldestFile = File;
							OldestFileTimestamp = Timestamp;
						}
					}
					UE_LOG(LogPluginDownloader, Log, TEXT("Deleting %s"), *OldestFile);

					TotalSize -= IFileManager::Get().FileSize(*OldestFile);
					ensure(IFileManager::Get().Delete(*OldestFile));
				}

				FinalizeDownload(Result, AesKey);
			});
			Request->ProcessRequest();
		}));
}

void FVoxelAuthDownload::Fail(const FString& Error)
{
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));

	if (ensure(Notification))
	{
		Notification->ExpireAndFadeout();
		Notification.Reset();
	}
}

void FVoxelAuthDownload::FinalizeDownload(const TArray<uint8>& Data, const FString& AesKey)
{
	const FString InstallLocation = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / "Voxel");
	UE_LOG(LogPluginDownloader, Log, TEXT("Installing to %s"), *InstallLocation);

	TArray<uint8> AesKeyBits;
	if (!ensure(FBase64::Decode(AesKey, AesKeyBits)) ||
		!ensure(AesKeyBits.Num() == 32))
	{
		Fail("Failed to download Voxel Plugin: invalid AES key");
		return;
	}

	FAES::FAESKey FinalAesKey;
	FMemory::Memcpy(FinalAesKey.Key, AesKeyBits.GetData(), 32);

	TArray<uint8> DecryptedData = Data;
	FAES::DecryptData(DecryptedData.GetData(), DecryptedData.Num(), FinalAesKey);

	TArray<uint8> UncompressedData;
	if (!FOodleCompressedArray::DecompressToTArray(UncompressedData, DecryptedData))
	{
		Fail("Failed to download Voxel Plugin: decompression failed");
		return;
	}

	TMap<FString, TArray<uint8>> Files;
	const FString ZipError = FPluginDownloaderUtilities::Unzip(UncompressedData, Files);
	if (!ZipError.IsEmpty())
	{
		Fail("Failed to unzip Voxel Plugin: " + ZipError);
		return;
	}
	
	const FString IntermediateDir = FPluginDownloaderUtilities::GetIntermediateDir();
	const FString TrashDir = IntermediateDir / "Trash" / "OldVoxelPlugin";
	const FString PackagedDir = IntermediateDir / "Voxel";

	IFileManager::Get().DeleteDirectory(*TrashDir, false, true);
	IFileManager::Get().DeleteDirectory(*PackagedDir, false, true);

	IFileManager::Get().MakeDirectory(*TrashDir, true);
	IFileManager::Get().MakeDirectory(*PackagedDir, true);

	for (const auto& It : Files)
	{
		const FString Path = PackagedDir / It.Key;

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);

		if (!ensure(FFileHelper::SaveArrayToFile(It.Value, *Path)))
		{
			Fail("Failed to write " + Path);
			return;
		}
	}

	if (!FPluginDownloaderUtilities::WriteInstallPluginBatch())
	{
		Fail("Failed to write InstallPlugin.bat");
		return;
	}
	
	if (!FPluginDownloaderUtilities::WriteRestartEngineBatch())
	{
		Fail("Failed to write RestartEngine.bat");
		return;
	}

	const FString BatchFile = IntermediateDir / "InstallPlugin_VoxelPlugin.bat";

	// InstallPlugin_VoxelPlugin.bat: calls InstallPlugin.bat with the right parameters
	{
		FString Batch = "cd /D \"" + IntermediateDir + "\"\r\n";
		Batch += FString::Printf(TEXT("start InstallPlugin.bat %u \"%s\" \"%s\" \"%s\" \"%s\" %s"),
			FPlatformProcess::GetCurrentProcessId(),
			FPaths::DirectoryExists(InstallLocation) ? *InstallLocation : TEXT(""),
			*TrashDir,
			*PackagedDir,
			*InstallLocation,
			TEXT("Voxel Plugin"));

		if (!FFileHelper::SaveStringToFile(Batch, *BatchFile))
		{
			Fail("Failed to write " + BatchFile);
			return;
		}
	}

	if (!FPluginDownloaderUtilities::ExecuteDetachedBatch(BatchFile))
	{
		Fail("Failed to execute bat file");
		return;
	}

	const TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (ActiveWindow.IsValid())
	{
		ActiveWindow->HACK_ForceToFront();
	}

	if (FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::FromString("Voxel Plugin successfully downloaded. Do you want to restart now to reload the plugin?")) == EAppReturnType::Yes)
	{
		GEngine->DeferredCommands.Add(TEXT("CLOSE_SLATE_MAINFRAME"));
	}
}
#endif