// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct PLUGINDOWNLOADEREDITOR_API FPluginDownloaderUtilities
{
	// Delay until next fire; 0 means "next frame"
	static void DelayedCall(TFunction<void()> Call, float Delay = 0);

	static void SaveConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename = GEditorPerProjectIni, bool bAppendClassName = true);
	static void LoadConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename = GEditorPerProjectIni, bool bAppendClassName = true);

	static FString EncryptData(const FString& Data);
	static FString DecryptData(const FString& Data);

	static bool ExecuteDetachedBatch(const FString& BatchFile);
	static FString GetAppData();

	static FString GetIntermediateDir();
	static void CheckTempFolderSize();

	static FString Unzip(const TArray<uint8>& Data, TMap<FString, TArray<uint8>>& OutFiles);

	static bool WriteInstallPluginBatch();
	static bool WriteRestartEngineBatch();
};