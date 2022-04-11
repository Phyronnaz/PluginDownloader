// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PluginDownloaderInfo.h"

DECLARE_DELEGATE_OneParam(FOnAutocompleteReceived, TArray<FString>);
DECLARE_DELEGATE_OneParam(FOnResponseReceived, FString);
DECLARE_DELEGATE_OneParam(FOnDescriptorReceived, const FPluginDescriptor*);

extern TArray<TSharedRef<FPluginDownloaderRemoteInfo>> GPluginDownloaderRemoteInfos;
extern FSimpleMulticastDelegate GOnPluginDownloaderRemoteInfosChanged;

struct FPluginDownloaderApi
{
	static void Initialize();
	static void FixupBranchName(FString& BranchName);

	static void GetRepoAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived, bool bIsOrganization = true);
	static void GetBranchAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);
	static void GetTagAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);

	static void GetBranchAndTagAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);

	static void GetDescriptor(const FPluginDownloaderRemoteInfo& Info, FOnDescriptorReceived OnDescriptorReceived);
};