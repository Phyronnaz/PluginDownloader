// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PluginDownloaderTokens.generated.h"

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