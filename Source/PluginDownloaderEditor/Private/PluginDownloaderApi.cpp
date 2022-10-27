// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderApi.h"
#include "PluginDownloaderTokens.h"
#include "PluginDownloaderUpdates.h"

TArray<TSharedRef<FPluginDownloaderRemoteInfo>> GPluginDownloaderRemoteInfos;
FSimpleMulticastDelegate GOnPluginDownloaderRemoteInfosChanged;

static FAutoConsoleCommand CheckForUpdatesCmd(
	TEXT("PluginDownloader.CheckUpdates"),
	TEXT(""),
	FConsoleCommandDelegate::CreateLambda([]
	{
		for (const TSharedRef<FPluginDownloaderRemoteInfo>& Info : GPluginDownloaderRemoteInfos)
		{
			if (Info->Name == "Custom")
			{
				continue;
			}

			FPluginDownloaderUpdates::CheckForUpdate(*Info);
		}
	}));

void FPluginDownloaderApi::Initialize()
{
	static bool bInitialized = false;
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;

	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://raw.githubusercontent.com/Phyronnaz/PluginDownloaderData/master/Plugins.json");
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		if (!bSucceeded ||
			HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
		{
			return;
		}
		ensure(GPluginDownloaderRemoteInfos.Num() == 0);

		TSharedPtr<FJsonValue> ParsedValue;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, ParsedValue))
		{
			return;
		}
		
		for (const TSharedPtr<FJsonValue>& JsonValue : ParsedValue->AsArray())
		{
			if (!JsonValue)
			{
				continue;
			}
			const TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
			if (!ensure(JsonObject))
			{
				continue;
			}

			const TSharedRef<FPluginDownloaderRemoteInfo> Info = MakeShared<FPluginDownloaderRemoteInfo>();

			if (ensure(FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &Info.Get())))
			{
				FixupBranchName(Info->StableBranch);
				for (auto& It : Info->Branches)
				{
					FixupBranchName(It.Key);
				}
				GPluginDownloaderRemoteInfos.Add(Info);
			}
		}

		for (const TSharedRef<FPluginDownloaderRemoteInfo>& Info : GPluginDownloaderRemoteInfos)
		{
			FPluginDownloaderUpdates::CheckForUpdate(*Info);
		}
		
		const TSharedRef<FPluginDownloaderRemoteInfo> CustomInfo = MakeShared<FPluginDownloaderRemoteInfo>();
		CustomInfo->Name = "Custom";
		CustomInfo->Icon = "https://raw.githubusercontent.com/Phyronnaz/PluginDownloaderData/master/Icons/Custom.png";
		GPluginDownloaderRemoteInfos.Add(CustomInfo);

		GOnPluginDownloaderRemoteInfosChanged.Broadcast();
	});
	Request->ProcessRequest();
}

void FPluginDownloaderApi::FixupBranchName(FString& BranchName)
{
	const FString VersionName = VERSION_STRINGIFY(ENGINE_MAJOR_VERSION) TEXT(".") VERSION_STRINGIFY(ENGINE_MINOR_VERSION);
	BranchName.ReplaceInline(TEXT("{ENGINE_VERSION}"), *VersionName);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FPluginDownloaderApi::GetDescriptor(const FPluginDownloaderRemoteInfo& Info, FOnDescriptorReceived OnDescriptorReceived)
{
	const UPluginDownloaderTokens* Tokens = GetDefault<UPluginDownloaderTokens>();
	if (!Tokens->HasValidToken())
	{
		OnDescriptorReceived.ExecuteIfBound(nullptr);
		return;
	}
	
	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("https://raw.githubusercontent.com" / Info.User / Info.Repo / Info.StableBranch / Info.Descriptor);
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([=](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		if (!bSucceeded || HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok)
		{
			OnDescriptorReceived.ExecuteIfBound(nullptr);
			return;
		}

		FPluginDescriptor Descriptor;

		TArray<uint8> UTF8Content = HttpResponse->GetContent();
		if (UTF8Content.Num() > 3 && 
			UTF8Content[0] == 0xEF && 
			UTF8Content[1] == 0xBB && 
			UTF8Content[2] == 0xBF)
		{
			// BOM
			UTF8Content.RemoveAt(0, 3);
		}

		const FUTF8ToTCHAR Content(reinterpret_cast<ANSICHAR*>(UTF8Content.GetData()), UTF8Content.Num());

		FText Error;
		if (!Descriptor.Read(FString(Content.Length(), Content.Get()), Error))
		{
			UE_LOG(LogPluginDownloader, Error, TEXT("Failed to parse descriptor %s: %s"), *HttpResponse->GetURL(), *Error.ToString());
			OnDescriptorReceived.ExecuteIfBound(nullptr);
			return;
		}

		OnDescriptorReceived.ExecuteIfBound(&Descriptor);
	});

	Tokens->AddAuthToRequest(*Request);
	Request->ProcessRequest();
}