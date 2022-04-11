// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderUpdates.h"
#include "PluginDownloaderApi.h"
#include "PluginDownloaderDownload.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

void FPluginDownloaderUpdates::CheckForUpdate(const FPluginDownloaderRemoteInfo& PluginInfo)
{
	FPluginDownloaderApi::GetDescriptor(PluginInfo, FOnDescriptorReceived::CreateLambda([=](const FPluginDescriptor* Descriptor)
	{
		if (!Descriptor)
		{
			return;
		}

		const FString PluginName = FPaths::GetBaseFilename(PluginInfo.Descriptor);
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (!Plugin ||
			Plugin->GetDescriptor().Version >= Descriptor->Version)
		{
			return;
		}

		const FString Text = PluginInfo.Name + " can be updated (" + Plugin->GetDescriptor().VersionName + " -> " + Descriptor->VersionName + ")";

		FNotificationInfo Info = FNotificationInfo(FText::FromString(Text));
		Info.CheckBoxState = ECheckBoxState::Undetermined;
		Info.ExpireDuration = 10;

		const TSharedRef<TWeakPtr<SNotificationItem>> PtrToPtr = MakeShared<TWeakPtr<SNotificationItem>>();

		Info.ButtonDetails.Add(FNotificationButtonInfo(
			FText::FromString("Update"),
			FText::FromString("Update the plugin"),
			FSimpleDelegate::CreateLambda([=]
			{
				FPluginDownloaderInfo DownloaderInfo;
				DownloaderInfo.User = PluginInfo.User;
				DownloaderInfo.Repo = PluginInfo.Repo;
				DownloaderInfo.Branch = PluginInfo.StableBranch;
				DownloaderInfo.InstallLocation =
					Plugin->GetLoadedFrom() == EPluginLoadedFrom::Engine
					? EPluginDownloadInstallLocation::Engine
					: EPluginDownloadInstallLocation::Project;

				FPluginDownloaderDownload::StartDownload(DownloaderInfo);
			}),
			SNotificationItem::CS_None));

		if (!PluginInfo.ReleaseNotesURL.IsEmpty())
		{
			Info.ButtonDetails.Add(FNotificationButtonInfo(
				FText::FromString("Show Release Notes"),
				FText::FromString(""),
				FSimpleDelegate::CreateLambda([=]
					{
						FPlatformProcess::LaunchURL(*PluginInfo.ReleaseNotesURL, nullptr, nullptr);
					}),
				SNotificationItem::CS_None));
		}
		
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			FText::FromString("Dismiss"),
			FText::FromString(""),
			FSimpleDelegate::CreateLambda([=]
			{
				if (PtrToPtr->IsValid())
				{
					PtrToPtr->Pin()->SetFadeOutDuration(0);
					PtrToPtr->Pin()->Fadeout();
				}
			}),
			SNotificationItem::CS_None));

		*PtrToPtr = FSlateNotificationManager::Get().AddNotification(Info);
	}));
}