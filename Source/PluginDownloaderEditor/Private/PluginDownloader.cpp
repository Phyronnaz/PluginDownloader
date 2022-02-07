// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloader.h"
#include "PluginDownloaderApi.h"
#include "PluginDownloaderUtilities.h"

void UPluginDownloaderCustom::FillAutoComplete()
{
	FPluginDownloaderApi::GetRepoAutocomplete(Info, FOnAutocompleteReceived::CreateWeakLambda(this, [=](const TArray<FString>& Result)
	{
		RepoOptions = Result;
	}));

	FPluginDownloaderApi::GetBranchAndTagAutocomplete(Info, FOnAutocompleteReceived::CreateWeakLambda(this, [=](const TArray<FString>& Result)
	{
		BranchOptions = Result;
	}));
}

void UPluginDownloaderCustom::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FillAutoComplete();

	FPluginDownloaderUtilities::SaveConfig(this, "PluginDownloaderCustom");
}