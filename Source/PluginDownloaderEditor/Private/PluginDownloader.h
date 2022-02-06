// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "PluginDownloader.generated.h"

class IHttpRequest;

DECLARE_LOG_CATEGORY_EXTERN(LogPluginDownloader, Log, All);

UENUM()
enum class EPluginDownloaderHost
{
	Github
};

UCLASS()
class UPluginDownloaderTokens : public UObject
{
	GENERATED_BODY()

public:
	// Token to access github private repositories
	// Needs to have the REPO scope
	UPROPERTY(EditAnywhere, Category = "Authentication", Transient)
	FString GithubAccessToken;

	UPROPERTY(VisibleAnywhere, Category = "Authentication", Transient)
	FString GithubStatus;
	
	UPROPERTY()
	FString GithubAccessToken_Encrypted;

public:
	void LoadFromConfig();
	void SaveToConfig();

	void CheckTokens();

	bool AddAuthToRequest(EPluginDownloaderHost Host, IHttpRequest& Request, FString& OutError) const;

protected:
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
};

USTRUCT()
struct FPluginDownloaderInfo
{
	GENERATED_BODY()
		
public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	EPluginDownloaderHost Host = EPluginDownloaderHost::Github;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FString User;
	
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (GetOptions=RepoOptions))
	FString Repo;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (GetOptions=BranchOptions))
	FString Branch;

	FString GetURL() const;
};

class FPluginDownloaderDownload : public TSharedFromThis<FPluginDownloaderDownload>
{
public:
	void Cancel();

	bool IsDone() const
	{
		return bIsDone;
	}
	int32 GetProgress() const
	{
		return Progress;
	}

	FSimpleMulticastDelegate OnProgress;
	FSimpleMulticastDelegate OnComplete;

private:
	explicit FPluginDownloaderDownload(const FHttpRequestRef& Request);

	const FHttpRequestRef Request;
	int32 Progress = 0;
	bool bIsDone = false;
	bool bIsCancelled = false;

	void Start();

	void OnProgressImpl(FHttpRequestPtr HttpRequest, int32 BytesSent, int32 BytesReceived);
	void OnCompleteImpl(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	friend class FPluginDownloader;
};

DECLARE_DELEGATE_OneParam(FOnAutocompleteReceived, TArray<FString>);
DECLARE_DELEGATE_OneParam(FOnResponseReceived, FString);

class FPluginDownloader
{
public:
	static TSharedPtr<FPluginDownloaderDownload> DownloadPlugin(const FPluginDownloaderInfo& Info);

	static void GetRepoAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived, bool bIsOrganization = false);
	static void GetBranchAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);

private:
	static void OnComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	static FString OnComplete_Internal(FHttpResponsePtr HttpResponse, bool bSucceeded);

	friend class FPluginDownloaderDownload;
};

UCLASS(Abstract)
class UPluginDownloaderBase : public UObject
{
	GENERATED_BODY()

public:
	virtual void FillAutoComplete() {}
	virtual FPluginDownloaderInfo GetInfo() { return {}; }

public:
	UPROPERTY(VisibleAnywhere, Category = "Settings")
	FString Progress;

	bool IsDownloading() const
	{
		return ActiveDownload && !ActiveDownload->IsDone();
	}
	void Download();
	void Cancel();

private:
	TSharedPtr<FPluginDownloaderDownload> ActiveDownload;
};

UCLASS()
class UPluginDownloaderCustom : public UPluginDownloaderBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ShowOnlyInnerProperties))
	FPluginDownloaderInfo Info;

public:
	UPROPERTY(Transient)
	TArray<FString> RepoOptions;

	UPROPERTY(Transient)
	TArray<FString> BranchOptions;

public:
	virtual void FillAutoComplete() override;
	virtual FPluginDownloaderInfo GetInfo() override { return Info; }

protected:
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
};