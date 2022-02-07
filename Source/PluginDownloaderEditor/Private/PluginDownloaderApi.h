// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PluginDownloaderInfo.h"

DECLARE_DELEGATE_OneParam(FOnAutocompleteReceived, TArray<FString>);
DECLARE_DELEGATE_OneParam(FOnResponseReceived, FString);

struct FPluginDownloaderApi
{
	static void GetRepoAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived, bool bIsOrganization = true);
	static void GetBranchAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);
	static void GetTagAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);

	static void GetBranchAndTagAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);
};