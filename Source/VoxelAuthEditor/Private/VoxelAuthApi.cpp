// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelAuthApi.h"
#include "VoxelAuth.h"
#include "VoxelAuthDownload.h"
#include "VoxelPluginVersion.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

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
	if (Branch == "Unknown")
	{
		return;
	}

	FVoxelPluginVersion Version;
	if (!ensure(Version.Parse(Branch)))
	{
		return;
	}

	PluginBranch = Version.GetBranch();
	PluginCounter = Version.GetCounter();

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

FString FVoxelAuthApi::GetCounterName(const int32 Counter) const
{
	FVoxelPluginVersion Version;
	Version.ParseCounter(Counter);
	return Version.ToString();
}

void FVoxelAuthApi::OpenReleaseNotes(const int32 Counter) const
{
	const FString Url = "https://docs.voxelplugin.com/release-notes#" + GetCounterName(Counter);
	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
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

	static bool bDisplayedNotification = false;
	if (bDisplayedNotification)
	{
		return;
	}
	bDisplayedNotification = true;

	if (GetPluginState() != EState::HasUpdate ||
		!Versions.Contains(PluginBranch))
	{
		return;
	}

	const int32 Latest = Versions[PluginBranch].Last();

	FString String;
	if (GConfig->GetString(
		TEXT("VoxelPlugin_SkippedVersions"),
		*FString::FromInt(Latest),
		String,
		GEditorPerProjectIni) &&
		String == "1")
	{
		return;
	}

	const TSharedRef<TWeakPtr<SNotificationItem>> WeakNotification = MakeShared<TWeakPtr<SNotificationItem>>();

	FNotificationInfo Info(FText::Format(
		INVTEXT("A new Voxel Plugin release is available: {0}"),
		FText::FromString(GetCounterName(Latest))));

	Info.bFireAndForget = false;
	Info.ExpireDuration = 0.f;
	Info.FadeInDuration = 0.f;
	Info.FadeOutDuration = 0.f;
	Info.WidthOverride = FOptionalSize();

	Info.CheckBoxText = INVTEXT("Skip this update");
	Info.CheckBoxStateChanged = FOnCheckStateChanged::CreateLambda([=](const ECheckBoxState NewState)
	{
		GConfig->SetString(
			TEXT("VoxelPlugin_SkippedVersions"),
			*FString::FromInt(Latest),
			NewState == ECheckBoxState::Checked ? TEXT("1") : TEXT("0"),
			GEditorPerProjectIni);
	});

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		INVTEXT("Update"),
		INVTEXT("Update Voxel Plugin to latest"),
		FSimpleDelegate::CreateLambda([=]
		{
			GVoxelAuthDownload->Download(PluginBranch, Latest);

			const TSharedPtr<SNotificationItem> Notification = WeakNotification->Pin();
			if (!ensure(Notification))
			{
				return;
			}

			Notification->ExpireAndFadeout();
		}),
		SNotificationItem::CS_None));

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		INVTEXT("Release Notes"),
		INVTEXT("Show the new version release notes"),
		FSimpleDelegate::CreateLambda([=]
		{
			GVoxelAuthApi->OpenReleaseNotes(Latest);
		}),
		SNotificationItem::CS_None));

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		INVTEXT("Dismiss"),
		INVTEXT("Dismiss"),
		FSimpleDelegate::CreateLambda([=]
		{
			const TSharedPtr<SNotificationItem> Notification = WeakNotification->Pin();
			if (!ensure(Notification))
			{
				return;
			}

			Notification->ExpireAndFadeout();
		}),
		SNotificationItem::CS_None));

	*WeakNotification = FSlateNotificationManager::Get().AddNotification(Info);
}