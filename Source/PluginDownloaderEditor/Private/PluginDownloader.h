// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "PluginDownloader.generated.h"

class SWindow;
class IHttpRequest;

DECLARE_LOG_CATEGORY_EXTERN(LogPluginDownloader, Log, All);

UCLASS()
class UPluginDownloaderTokens : public UObject
{
	GENERATED_BODY()

public:
	// Token to access github private repositories
	// Needs to have the REPO scope
	UPROPERTY(EditAnywhere, Category = "Authentication", Transient, meta = (PasswordField = true))
	FString GithubAccessToken;

	UPROPERTY(VisibleAnywhere, Category = "Authentication", Transient)
	FString GithubStatus;
	
	UPROPERTY()
	FString GithubAccessToken_Encrypted;

public:
	void LoadFromConfig();
	void SaveToConfig();

	void CheckTokens();

	bool HasValidToken() const;
	FString GetTokenError() const;

	void AddAuthToRequest(IHttpRequest& Request) const;

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
	const FHttpRequestRef Request;
	TSharedPtr<SWindow> Window;

	int32 Progress = 0;
	bool bIsDone = false;
	bool bIsCancelled = false;

	~FPluginDownloaderDownload();
	explicit FPluginDownloaderDownload(const FHttpRequestRef& Request);

	void Start();
	void Cancel();

	void OnProgressImpl(FHttpRequestPtr HttpRequest, int32 BytesSent, int32 BytesReceived);
	void OnCompleteImpl(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
};

DECLARE_DELEGATE_OneParam(FOnAutocompleteReceived, TArray<FString>);
DECLARE_DELEGATE_OneParam(FOnResponseReceived, FString);

class FPluginDownloader
{
public:
	static void DownloadPlugin(const FPluginDownloaderInfo& Info);
	static void CheckTempFolderSize();

	static void GetRepoAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived, bool bIsOrganization = true);
	static void GetBranchAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);
	static void GetTagAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);

	static void GetBranchAndTagAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived);

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
	virtual FPluginDownloaderInfo GetInfo() { return {}; }
};

UCLASS()
class UPluginDownloaderRemote : public UPluginDownloaderBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPluginDownloaderInfo Info;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (GetOptions=BranchOptions))
	FString Branch;

	UPROPERTY(Transient)
	TArray<FString> BranchOptions;

	TMap<FString, FString> BranchDisplayNameToName;

public:
	virtual FPluginDownloaderInfo GetInfo() override
	{
		FPluginDownloaderInfo FullInfo = Info;
		FullInfo.Branch = BranchDisplayNameToName.Contains(Branch) ? BranchDisplayNameToName[Branch] : Branch;
		return FullInfo;
	}
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
	void FillAutoComplete();
	virtual FPluginDownloaderInfo GetInfo() override { return Info; }

protected:
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
};