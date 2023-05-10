// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelPluginVersion.h"
#include "Interfaces/IHttpRequest.h"

class FVoxelAuthApi;

extern FVoxelAuthApi* GVoxelAuthApi;

class FVoxelAuthApi
{
public:
	TArray<TSharedPtr<FVoxelPluginVersion>> AllVersions;
	FVoxelPluginVersion SelectedVersion;
	FVoxelPluginVersion PluginVersion;

	FSimpleMulticastDelegate OnComboBoxesUpdated;

public:
    FVoxelAuthApi() = default;

	void Initialize();

	void UpdateVersions();
    void UpdateIsPro(bool bForce = false);
	void VerifyGumroadKey(const FString& GumroadKey);
	void VerifyMarketplace();

    bool IsPro() const;
	bool IsProUpdated() const;
	void OpenReleaseNotes(const FVoxelPluginVersion& Version) const;

	bool IsVerifyingGumroadKey() const
	{
		return bIsVerifyingGumroadKey;
    }
    bool IsVerifyingMarketplace() const
    {
	    return bIsVerifyingMarketplace;
    }

    enum class EState
    {
	    NotInstalled,
		HasUpdate,
		NoUpdate
    };
	EState GetPluginState() const;
	FString GetAppDataPath() const;

    enum class ERequestVerb
    {
	    Get,
        Post
    };
	void MakeRequest(
		const FString& Endpoint,
		const TMap<FString, FString>& Parameters,
		ERequestVerb Verb,
		bool bNeedAuth,
		FHttpRequestCompleteDelegate OnComplete);

private:
	bool bIsVerifyingGumroadKey = false;
	bool bIsVerifyingMarketplace = false;

	bool bIsUpdatingVersions = false;
	bool bIsUpdatingIsPro = false;

	TMap<FString, bool> ProAccountIds;

	void UpdateVersions(const FString& VersionsString);
};