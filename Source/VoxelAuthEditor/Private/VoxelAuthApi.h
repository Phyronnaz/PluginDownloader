// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Interfaces/IHttpRequest.h"

class FVoxelAuthApi;

extern FVoxelAuthApi* GVoxelAuthApi;

class FVoxelAuthApi
{
public:
	FString SelectedPluginBranch;
	int32 SelectedPluginCounter = 0;

	TArray<TSharedPtr<FString>> Branches;
	TArray<TSharedPtr<int32>> Counters;

	void UpdateComboBoxes();

public:
    FVoxelAuthApi() = default;

	void Initialize();

	void UpdateVersions(bool bForce = false);
    void UpdateIsPro(bool bForce = false);
	void VerifyGumroadKey(const FString& GumroadKey);
	void VerifyMarketplace();

    bool IsPro() const;
    bool IsProUpdated() const;

    bool IsVerifyingGumroadKey() const
    {
	    return bIsVerifyingGumroadKey;
    }
    bool IsVerifyingMarketplace() const
    {
	    return bIsVerifyingMarketplace;
    }
	const TArray<TSharedPtr<FString>>& GetBranches() const
	{
		return Branches;
	}
	const TMap<FString, TArray<int32>>& GetVersions() const
	{
		return Versions;
	}

	const FString& GetPluginBranch() const
	{
		return PluginBranch;
	}
	int32 GetPluginCounter() const
	{
		return PluginCounter;
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
	TMap<FString, TArray<int32>> Versions;

	FString PluginBranch;
	int32 PluginCounter = 0;

	void UpdateVersions(const FString& VersionsString);
};