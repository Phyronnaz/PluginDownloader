// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderTokens.h"
#include "PluginDownloaderUtilities.h"
#include "PluginDownloader.h"

static FString GetGithubTokenPath()
{
	return FPluginDownloaderUtilities::GetAppData() / "UnrealEngine" / "PluginDownloader" / "GithubToken.txt";
}

void UPluginDownloaderTokens::LoadFromConfig()
{
	FFileHelper::LoadFileToString(GithubAccessToken_Encrypted, *GetGithubTokenPath());

	GithubAccessToken = FPluginDownloaderUtilities::DecryptData(GithubAccessToken_Encrypted);
}

void UPluginDownloaderTokens::SaveToConfig()
{
	GithubAccessToken_Encrypted = FPluginDownloaderUtilities::EncryptData(GithubAccessToken);

	ensure(FFileHelper::SaveStringToFile(GithubAccessToken_Encrypted, *GetGithubTokenPath()));
}

void UPluginDownloaderTokens::CheckTokens()
{
	if (GithubAccessToken.IsEmpty())
	{
		GithubStatus.Reset();
		return;
	}

	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://api.github.com/");
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindWeakLambda(this, [=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		if (!bSucceeded)
		{
			GithubStatus = "Failed to query github.com";
			return;
		}

		if (HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
		{
			UE_LOG(LogPluginDownloader, Error, TEXT("Invalid token: %s"), *HttpResponse->GetContentAsString());
			GithubStatus = "Invalid token";
			return;
		}

		const FString Scope = HttpResponse->GetHeader("X-OAuth-Scopes");

		TArray<FString> Scopes;
		Scope.ParseIntoArray(Scopes, TEXT(","));
		if (!Scopes.Contains("repo"))
		{
			GithubStatus = "Token needs to have the 'repo' scope";
			return;
		}

		GithubStatus = {};
	});
	Request->SetHeader("Authorization", "token " + GithubAccessToken);
	Request->ProcessRequest();
}

bool UPluginDownloaderTokens::HasValidToken() const
{
	return GithubStatus.IsEmpty();
}

FString UPluginDownloaderTokens::GetTokenError() const
{
	ensure(!HasValidToken());
	return GithubStatus;
}

void UPluginDownloaderTokens::AddAuthToRequest(IHttpRequest& Request) const
{
	ensure(HasValidToken());

	if (!GithubAccessToken.IsEmpty())
	{
		Request.SetHeader("Authorization", "token " + GithubAccessToken);
	}
}

void UPluginDownloaderTokens::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SaveToConfig();
	CheckTokens();

	// Update the custom view autocomplete with the new token
	GetMutableDefault<UPluginDownloaderCustom>()->FillAutoComplete();
}