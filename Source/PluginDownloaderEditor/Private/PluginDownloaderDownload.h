// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PluginDownloaderInfo.h"

class FPluginDownloaderDownload
{
public:
	static void StartDownload(const FPluginDownloaderInfo& Info);

private:
	const FPluginDownloaderInfo Info;

	explicit FPluginDownloaderDownload(const FPluginDownloaderInfo& Info)
		: Info(Info)
	{
	}
	void Destroy(const FString& Reason);

	FHttpRequestPtr Request;
	TSharedPtr<SWindow> ProgressWindow;

	int32 RequestProgress = 0;
	bool bRequestCancelled = false;

	void Start();

	void OnRequestProgress(FHttpRequestPtr HttpRequest, int32 BytesSent, int32 BytesReceived);
	void OnRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnPackageComplete(const FString& Result, const FString& PluginBatchFile, const FString& PluginAdminBatchFile);
};

extern FPluginDownloaderDownload* GActivePluginDownloaderDownload;