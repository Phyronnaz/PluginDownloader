// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderApi.h"
#include "PluginDownloaderTokens.h"

void FPluginDownloaderApi::GetRepoAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived, bool bIsOrganization)
{
	const UPluginDownloaderTokens* Tokens = GetDefault<UPluginDownloaderTokens>();
	if (!Tokens->HasValidToken())
	{
		return;
	}

	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://api.github.com" / FString(bIsOrganization ? "orgs" : "users") / Info.User / "repos?per_page=100");
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		if (!bSucceeded || HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
		{
			if (bIsOrganization)
			{
				GetRepoAutocomplete(Info, OnAutocompleteReceived, false);
			}
			return;
		}

		TSharedPtr<FJsonValue> ParsedValue;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, ParsedValue))
		{
			return;
		}

		TArray<FString> Result;
		ensure(ParsedValue->AsArray().Num() < 99);
		for (const TSharedPtr<FJsonValue>& JsonValue : ParsedValue->AsArray())
		{
			if (!JsonValue)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
			if (!JsonObject)
			{
				continue;
			}

			Result.Add(JsonObject->GetStringField("name"));
		}

		OnAutocompleteReceived.ExecuteIfBound(Result);
	});

	Tokens->AddAuthToRequest(*Request);
	Request->ProcessRequest();
}

void FPluginDownloaderApi::GetBranchAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived)
{
	const UPluginDownloaderTokens* Tokens = GetDefault<UPluginDownloaderTokens>();
	if (!Tokens->HasValidToken())
	{
		return;
	}

	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://api.github.com/repos/" + Info.User / Info.Repo / "/branches");
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		if (!bSucceeded || HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
		{
			return;
		}

		TSharedPtr<FJsonValue> ParsedValue;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, ParsedValue))
		{
			return;
		}

		TArray<FString> Result;
		ensure(ParsedValue->AsArray().Num() < 99);
		for (const TSharedPtr<FJsonValue>& JsonValue : ParsedValue->AsArray())
		{
			if (!JsonValue)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
			if (!JsonObject)
			{
				continue;
			}

			Result.Add(JsonObject->GetStringField("name"));
		}

		OnAutocompleteReceived.ExecuteIfBound(Result);
	});

	Tokens->AddAuthToRequest(*Request);
	Request->ProcessRequest();
}

void FPluginDownloaderApi::GetTagAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived)
{
	const UPluginDownloaderTokens* Tokens = GetDefault<UPluginDownloaderTokens>();
	if (!Tokens->HasValidToken())
	{
		return;
	}

	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://api.github.com/repos/" + Info.User / Info.Repo / "/tags");
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		if (!bSucceeded || HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
		{
			return;
		}

		TSharedPtr<FJsonValue> ParsedValue;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, ParsedValue))
		{
			return;
		}

		TArray<FString> Result;
		ensure(ParsedValue->AsArray().Num() < 99);
		for (const TSharedPtr<FJsonValue>& JsonValue : ParsedValue->AsArray())
		{
			if (!JsonValue)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
			if (!JsonObject)
			{
				continue;
			}

			Result.Add(JsonObject->GetStringField("name"));
		}

		OnAutocompleteReceived.ExecuteIfBound(Result);
	});

	Tokens->AddAuthToRequest(*Request);
	Request->ProcessRequest();
}

void FPluginDownloaderApi::GetBranchAndTagAutocomplete(const FPluginDownloaderInfo& Info, FOnAutocompleteReceived OnAutocompleteReceived)
{
	struct FData
	{
		bool bBranchDone = false;
		bool bTagDone = false;
		TArray<FString> BranchResult;
		TArray<FString> TagResult;

		TArray<FString> GetResult() const
		{
			TArray<FString> Result = BranchResult;
			Result.Append(TagResult);
			return Result;
		}
	};
	const TSharedRef<FData> Data = MakeShared<FData>();

	GetBranchAutocomplete(Info, FOnAutocompleteReceived::CreateLambda([=](const TArray<FString>& Result)
	{
		Data->bBranchDone = true;
		Data->BranchResult = Result;

		if (Data->bTagDone)
		{
			OnAutocompleteReceived.ExecuteIfBound(Data->GetResult());
		}
	}));

	GetTagAutocomplete(Info, FOnAutocompleteReceived::CreateLambda([=](const TArray<FString>& Result)
	{
		Data->bTagDone = true;
		Data->TagResult = Result;

		if (Data->bBranchDone)
		{
			OnAutocompleteReceived.ExecuteIfBound(Data->GetResult());
		}
	}));
}