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

	SelectedPluginBranch = "2.0";
	SelectedPluginCounter = -1;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("Voxel");
	if (!Plugin)
	{
		return;
	}

	const FString Branch = Plugin->GetDescriptor().VersionName;

	int32 Major = 0;
	int32 Minor = 0;
	int32 Hotfix = 0;
	bool bIsPreview = false;
	int32 PreviewWeek = 0;
	int32 PreviewHotfix = 0;
	{
		TArray<FString> VersionAndPreview;
		Branch.ParseIntoArray(VersionAndPreview, TEXT("p-"));
		ensure(VersionAndPreview.Num() == 1 || VersionAndPreview.Num() == 2);

		if (VersionAndPreview.Num() == 1)
		{
			TArray<FString> MinorMajorHotfix;
			VersionAndPreview[0].ParseIntoArray(MinorMajorHotfix, TEXT("."));
			ensure(MinorMajorHotfix.Num() == 3);

			Major = FCString::Atoi(*MinorMajorHotfix[0]);
			Minor = FCString::Atoi(*MinorMajorHotfix[1]);
			Hotfix = FCString::Atoi(*MinorMajorHotfix[2]);
			ensure(FString::FromInt(Major) == MinorMajorHotfix[0]);
			ensure(FString::FromInt(Minor) == MinorMajorHotfix[1]);
			ensure(FString::FromInt(Hotfix) == MinorMajorHotfix[2]);
		}
		else
		{
			bIsPreview = true;

			TArray<FString> MinorMajor;
			VersionAndPreview[0].ParseIntoArray(MinorMajor, TEXT("."));
			ensure(MinorMajor.Num() == 2);

			TArray<FString> PreviewAndHotfix;
			VersionAndPreview[1].ParseIntoArray(PreviewAndHotfix, TEXT("."));
			ensure(PreviewAndHotfix.Num() == 1 || PreviewAndHotfix.Num() == 2);

			Major = FCString::Atoi(*MinorMajor[0]);
			Minor = FCString::Atoi(*MinorMajor[1]);
			ensure(FString::FromInt(Major) == MinorMajor[0]);
			ensure(FString::FromInt(Minor) == MinorMajor[1]);

			PreviewWeek = FCString::Atoi(*PreviewAndHotfix[0]);
			ensure(FString::FromInt(PreviewWeek) == PreviewAndHotfix[0]);

			if (PreviewAndHotfix.Num() == 2)
			{
				PreviewHotfix = FCString::Atoi(*PreviewAndHotfix[1]);
				ensure(FString::FromInt(PreviewHotfix) == PreviewAndHotfix[1]);
			}
		}
	}

	int32 Counter = Major;
	{
		Counter *= 10;
		Counter += Minor;

		Counter *= 10;
		Counter += bIsPreview ? 0 : Hotfix;

		Counter *= 1000;
		Counter += bIsPreview ? PreviewWeek : 999;

		Counter *= 10;
		Counter += bIsPreview ? PreviewHotfix : 0;

		ensure(Plugin->GetDescriptor().Version == Counter);
	}

	PluginBranch = FString::FromInt(Major) + "." + FString::FromInt(Minor);
	PluginCounter = Counter;

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

FString FVoxelAuthApi::GetCounterName(int32 Counter) const
{
	const int32 PreviewHotfix = Counter % 10;
	Counter /= 10;

	const int32 PreviewWeek = Counter % 1000;
	Counter /= 1000;

	const int32 Hotfix = Counter % 10;
	Counter /= 10;

	const int32 Minor = Counter % 10;
	Counter /= 10;

	const int32 Major = Counter % 10;
	ensure(Counter == Major);

	if (PreviewWeek != 999)
	{
		ensure(Hotfix == 0);
		FString Result = FString::FromInt(Major) + "." + FString::FromInt(Minor) + "p-" + FString::FromInt(PreviewWeek);
		if (PreviewHotfix != 0)
		{
			Result += "." + FString::FromInt(PreviewHotfix);
		}
		return Result;
	}
	else
	{
		ensure(PreviewHotfix == 0);
		return FString::FromInt(Major) + "." + FString::FromInt(Minor) + "." + FString::FromInt(Hotfix);
	}
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