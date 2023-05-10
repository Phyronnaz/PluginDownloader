// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelAuthApi.h"
#include "VoxelAuth.h"
#include "VoxelAuthDownload.h"
#include "PluginDownloaderSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

FVoxelAuthApi* GVoxelAuthApi = nullptr;

void FVoxelAuthApi::Initialize()
{
	UpdateVersions();

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

	ensure(PluginVersion.Parse(Branch));
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

void FVoxelAuthApi::OpenReleaseNotes(const FVoxelPluginVersion& Version) const
{
	const FString Url = "https://docs.voxelplugin.com/release-notes#" + Version.ToString(false, false, false);
	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelAuthApi::EState FVoxelAuthApi::GetPluginState() const
{
	if (PluginVersion.Type == FVoxelPluginVersion::EType::Unknown)
	{
		return EState::NotInstalled;
	}

	for (const TSharedPtr<FVoxelPluginVersion>& Version : GVoxelAuthApi->AllVersions)
	{
		if (!ensure(Version))
		{
			continue;
		}
		if (Version->GetBranch() == GVoxelAuthApi->SelectedVersion.GetBranch() &&
			Version->GetCounter() > GVoxelAuthApi->SelectedVersion.GetCounter())
		{
			return EState::HasUpdate;
		}
	}
	return EState::NoUpdate;
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

	TArray<TSharedPtr<FVoxelPluginVersion>> NewVersions;
	for (const TSharedPtr<FJsonValue>& Value : ParsedValue->AsArray())
	{
		const FString VersionString = Value->AsString();

		FVoxelPluginVersion Version;
		if (!ensure(Version.Parse(VersionString)))
		{
			continue;
		}

		if (Version.Type == FVoxelPluginVersion::EType::Dev &&
			!GetDefault<UPluginDownloaderSettings>()->bShowVoxelPluginDevVersions)
		{
			continue;
		}

		if (Version.EngineVersion != ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)
		{
			continue;
		}

#if PLATFORM_WINDOWS
		if (Version.Platform != FVoxelPluginVersion::EPlatform::Win64)
		{
			continue;
		}
#endif
#if PLATFORM_MAC
		if (Version.Platform != FVoxelPluginVersion::EPlatform::Mac)
		{
			continue;
		}
#endif

		NewVersions.Add(MakeShared<FVoxelPluginVersion>(Version));
	}

	if (AllVersions.Num() != NewVersions.Num())
	{
		AllVersions = NewVersions;
		OnComboBoxesUpdated.Broadcast();
	}
	else
	{
		for (int32 Index = 0; Index < AllVersions.Num(); Index++)
		{
			if (*AllVersions[Index] == *NewVersions[Index])
			{
				continue;
			}

			AllVersions = NewVersions;
			OnComboBoxesUpdated.Broadcast();
			break;
		}
	}

	static bool bDisplayedNotification = false;
	if (bDisplayedNotification)
	{
		return;
	}
	bDisplayedNotification = true;

	if (GetPluginState() != EState::HasUpdate)
	{
		return;
	}

	FVoxelPluginVersion Latest = GVoxelAuthApi->SelectedVersion;
	for (const TSharedPtr<FVoxelPluginVersion>& Version : GVoxelAuthApi->AllVersions)
	{
		if (!ensure(Version))
		{
			continue;
		}
		if (Version->GetBranch() == Latest.GetBranch() &&
			Version->GetCounter() > Latest.GetCounter())
		{
			Latest = *Version;
		}
	}

	FString String;
	if (GConfig->GetString(
		TEXT("VoxelPlugin_SkippedVersions"),
		*Latest.ToString(),
		String,
		GEditorPerProjectIni) &&
		String == "1")
	{
		return;
	}

	const TSharedRef<TWeakPtr<SNotificationItem>> WeakNotification = MakeShared<TWeakPtr<SNotificationItem>>();

	FNotificationInfo Info(FText::Format(INVTEXT("A new Voxel Plugin release is available: {0}"), Latest.ToDisplayString()));

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
			*Latest.ToString(),
			NewState == ECheckBoxState::Checked ? TEXT("1") : TEXT("0"),
			GEditorPerProjectIni);
	});

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		INVTEXT("Update"),
		INVTEXT("Update Voxel Plugin to latest"),
		FSimpleDelegate::CreateLambda([=]
		{
			GVoxelAuthDownload->Download(Latest);

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