// Copyright Voxel Plugin, Inc. All Rights Reserved.

#if ENGINE_VERSION >= 500
#include "VoxelAuthApi.h"
#include "VoxelAuth.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

FVoxelAuthApi* GVoxelAuthApi = nullptr;

void FVoxelAuthApi::UpdateComboBoxes()
{
	// Make sure to preserve shared ptrs for combo boxes

	TMap<FString, TSharedPtr<FString>> BranchPtrs;
	TMap<int32, TSharedPtr<int32>> CounterPtrs;

	for (const TSharedPtr<FString>& Branch : Branches)
	{
		BranchPtrs.Add(*Branch, Branch);
	}
	for (const TSharedPtr<int32>& Counter : Counters)
	{
		CounterPtrs.Add(*Counter, Counter);
	}

	const auto GetBranch = [&](const FString& Branch)
	{
		if (!BranchPtrs.Contains(Branch))
		{
			BranchPtrs.Add(Branch, MakeShared<FString>(Branch));
		}
		return BranchPtrs[Branch].ToSharedRef();
	};
	const auto GetCounter = [&](const int32 Counter)
	{
		if (!CounterPtrs.Contains(Counter))
		{
			CounterPtrs.Add(Counter, MakeShared<int32>(Counter));
		}
		return CounterPtrs[Counter].ToSharedRef();
	};

	Branches.Reset();
	Counters.Reset();

	Counters.Add(GetCounter(-1));

	for (const auto& It : Versions)
	{
		Branches.Add(GetBranch(It.Key));

		if (It.Key == SelectedPluginBranch)
		{
			for (const int32 Counter : It.Value)
			{
				Counters.Add(GetCounter(Counter));
			}
		}
	}

	OnComboBoxesUpdated.Broadcast();
}

void FVoxelAuthApi::Initialize()
{
	UpdateVersions();

	SelectedPluginBranch = "master";
	SelectedPluginCounter = -1;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("Voxel");
	if (!Plugin)
	{
		return;
	}

	const FString Version = Plugin->GetDescriptor().VersionName;

	TArray<FString> Array;
	Version.ParseIntoArray(Array, TEXT("-"));

	if (Array.Num() != 4)
	{
		return;
	}
	ensure(Array[2] == FString::FromInt(Plugin->GetDescriptor().Version));

	PluginBranch = Array[1];
	PluginCounter = Plugin->GetDescriptor().Version;

	SelectedPluginBranch = PluginBranch;
	SelectedPluginCounter = -1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelAuthApi::UpdateVersions()
{
	if (bIsUpdatingVersions)
	{
		return;
	}
	bIsUpdatingVersions = true;

	MakeRequest(
		"version/list",
		{},
		ERequestVerb::Get,
		false,
		FHttpRequestCompleteDelegate::CreateLambda([=](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			ensure(bIsUpdatingVersions);
			bIsUpdatingVersions = false;

			if (!bConnectedSuccessfully ||
				!Response ||
				Response->GetResponseCode() != 200)
			{
				UE_LOG(LogPluginDownloader, Error, TEXT("Failed to query versions: %d %s"), Response->GetResponseCode(), *Response->GetContentAsString());
				return;
			}

			UpdateVersions(Response->GetContentAsString());
		}));
}

void FVoxelAuthApi::UpdateIsPro(bool bForce)
{
	const FString AccountId = GVoxelAuth->GetAccountId();
	if (!ensure(!AccountId.IsEmpty()))
	{
		return;
	}

	if (ProAccountIds.Contains(AccountId) && !bForce)
	{
		return;
	}

	if (bIsUpdatingIsPro)
	{
		return;
	}
	bIsUpdatingIsPro = true;

	MakeRequest(
		"user/isPro",
		{},
		ERequestVerb::Get,
		true,
		FHttpRequestCompleteDelegate::CreateLambda([=](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			ensure(bIsUpdatingIsPro);
			bIsUpdatingIsPro = false;

			if (!bConnectedSuccessfully ||
				!Response ||
				Response->GetResponseCode() != 200)
			{
				UE_LOG(LogPluginDownloader, Error, TEXT("Failed to query IsPro: %d %s"), Response->GetResponseCode(), *Response->GetContentAsString());
				return;
			}

			ProAccountIds.FindOrAdd(AccountId) |= Response->GetContentAsString() == "true";
		}));
}

void FVoxelAuthApi::VerifyGumroadKey(const FString& GumroadKey)
{
	const FString AccountId = GVoxelAuth->GetAccountId();
	if (!ensure(!AccountId.IsEmpty()) ||
		!ensure(!ProAccountIds.FindRef(AccountId)) ||
		!ensure(!bIsVerifyingGumroadKey))
	{
		return;
	}

	bIsVerifyingGumroadKey = true;

	MakeRequest(
		"user/verifyGumroadKey",
		{ { "key", GumroadKey } },
		ERequestVerb::Post,
		true,
		FHttpRequestCompleteDelegate::CreateLambda([=](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			ensure(bIsVerifyingGumroadKey);
			bIsVerifyingGumroadKey = false;

			if (!bConnectedSuccessfully ||
				!Response ||
				Response->GetResponseCode() != 201)
			{
				UE_LOG(LogPluginDownloader, Error, TEXT("Failed to verify license key: %d %s"), Response->GetResponseCode(), *Response->GetContentAsString());

				const FText Title = INVTEXT("Failed to verify license key");
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Response->GetContentAsString()), &Title);
				return;
			}
			
			ProAccountIds.FindOrAdd(AccountId) |= Response->GetContentAsString() == "true";
		}));
}

void FVoxelAuthApi::VerifyMarketplace()
{
	const FString AccountId = GVoxelAuth->GetAccountId();
	if (!ensure(!AccountId.IsEmpty()) ||
		!ensure(!ProAccountIds.FindRef(AccountId)) ||
		!ensure(!bIsVerifyingMarketplace))
	{
		return;
	}

	bIsVerifyingMarketplace = true;

	MakeRequest(
		"user/verifyMarketplace",
		{},
		ERequestVerb::Post,
		true,
		FHttpRequestCompleteDelegate::CreateLambda([=](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			ensure(bIsVerifyingMarketplace);
			bIsVerifyingMarketplace = false;

			if (!bConnectedSuccessfully ||
				!Response ||
				Response->GetResponseCode() != 201)
			{
				UE_LOG(LogPluginDownloader, Error, TEXT("Failed to verify marketplace: %d %s"), Response->GetResponseCode(), *Response->GetContentAsString());

				const FText Title = INVTEXT("Failed to verify marketplace");
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Response->GetContentAsString()), &Title);
				return;
			}

			if (Response->GetContentAsString() != "true")
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Failed to verify marketplace purchase. Please leave a dummy question on the marketplace page from the account who bought the plugin."));
				FPlatformProcess::LaunchURL(TEXT("https://www.unrealengine.com/marketplace/en-US/product/voxel-plugin-pro/questions"), nullptr, nullptr);
				return;
			}

			ProAccountIds.FindOrAdd(AccountId) |= true;
		}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelAuthApi::IsPro() const
{
	return ProAccountIds.FindRef(GVoxelAuth->GetAccountId());
}

bool FVoxelAuthApi::IsProUpdated() const
{
	return ProAccountIds.Contains(GVoxelAuth->GetAccountId());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelAuthApi::EState FVoxelAuthApi::GetPluginState() const
{
	if (PluginBranch.IsEmpty())
	{
		return EState::NotInstalled;
	}

	if (!Versions.Contains(PluginBranch))
	{
		return EState::NoUpdate;
	}

	return Versions[PluginBranch].Last() > PluginCounter ? EState::HasUpdate : EState::NoUpdate;
}

FString FVoxelAuthApi::GetAppDataPath() const
{
	return FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA")) / "UnrealEngine" / "VoxelPlugin";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelAuthApi::MakeRequest(
	const FString& Endpoint,
	const TMap<FString, FString>& Parameters,
	ERequestVerb Verb,
	bool bNeedAuth,
	FHttpRequestCompleteDelegate OnComplete)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	FString Url = "https://api.voxelplugin.dev/" + Endpoint + "?";

	if (bNeedAuth)
	{
		Url += "token=" + GVoxelAuth->GetIdToken() + "&";
	}

	for (auto& It : Parameters)
	{
		Url += It.Key + "=" + It.Value + "&";
	}

	Url.RemoveFromEnd("?");
	Url.RemoveFromEnd("&");

	HttpRequest->OnProcessRequestComplete() = OnComplete;
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(Verb == ERequestVerb::Get ? "GET" : "POST");
	HttpRequest->ProcessRequest();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelAuthApi::UpdateVersions(const FString& VersionsString)
{
	TSharedPtr<FJsonValue> ParsedValue;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(VersionsString);
	if (!ensure(FJsonSerializer::Deserialize(Reader, ParsedValue)) ||
		!ensure(ParsedValue))
	{
		return;
	}

	const TSharedPtr<FJsonObject> ArrayObject = ParsedValue->AsObject();
	if (!ensure(ArrayObject))
	{
		return;
	}

	Versions.Reset();

	for (const auto& It : ArrayObject->Values)
	{
		const TArray<TSharedPtr<FJsonValue>> Array = It.Value->AsArray();
		for (const TSharedPtr<FJsonValue>& Value : Array)
		{
			const TSharedPtr<FJsonObject> VersionObject = Value->AsObject();
			if (!ensure(VersionObject))
			{
				continue;
			}

			const int32 Counter = VersionObject->GetNumberField("counter");
			const int32 UnrealVersion = VersionObject->GetNumberField("unrealVersion");

			if (!ensure(Counter != 0) ||
				UnrealVersion != ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)
			{
				continue;
			}

			Versions.FindOrAdd(It.Key).Add(Counter);
		}
	}

	for (auto& It : Versions)
	{
		It.Value.Sort();
	}

	UpdateComboBoxes();
}
#endif