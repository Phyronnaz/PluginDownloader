// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "PluginDownloaderEditorModule.generated.h"

UENUM()
enum class EPluginDownloaderHost
{
	GitHub UMETA(DisplayName = "GitHub")
};

UCLASS()
class UPluginDownloaderInfo : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Settings")
	EPluginDownloaderHost Host = EPluginDownloaderHost::GitHub;

	// Token to access private repositories
	// Needs to have the REPO scope
	UPROPERTY(EditAnywhere, Category = "Settings")
	FString AccessToken;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FString User = "Phyronnaz";
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bIsOrganization = false;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (GetOptions=GetRepoOptions))
	FString Repo = "HLSLMaterial";

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (GetOptions=GetBranchOptions))
	FString Branch = "master";

public:
	UPROPERTY(VisibleAnywhere, Category = "Output")
	FString URL;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FString Response;

public:
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingRequest;

	void Download();
	void Cancel();
	void FillAutoComplete();
	void AddAuth(IHttpRequest& Request) const;

	UFUNCTION()
	TArray<FString> GetRepoOptions() const
	{
		return CachedRepos;
	}

	UFUNCTION()
	TArray<FString> GetBranchOptions() const
	{
		return CachedBranches;
	}

	TArray<FString> CachedRepos;
	TArray<FString> CachedBranches;

private:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	void FixupURL()
	{
		URL = "https://api.github.com/repos/" + User + "/" + Repo + "/zipball/" + Branch;
		URL = "https://api.github.com/repos/VoxelPlugin/VoxelPro/zipball/master";
	}

	void OnDownloadFinished(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
};