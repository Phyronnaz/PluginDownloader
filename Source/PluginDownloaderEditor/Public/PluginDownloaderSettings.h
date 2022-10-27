// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PluginDownloaderSettings.generated.h"

UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Plugin Downloader"))
class PLUGINDOWNLOADEREDITOR_API UPluginDownloaderSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPluginDownloaderSettings()
    {
		CategoryName = "Plugins";
		SectionName = "Plugin Downloader";
    }

	UPROPERTY(Config, EditAnywhere, Category = "Plugin Downloader")
	bool bCheckForUpdatesOnStartup = true;

	UPROPERTY(Config, EditAnywhere, Category = "Plugin Downloader")
    bool bShowVoxelPluginMenu = true;

    //~ Begin UDeveloperSettings Interface
    virtual FName GetContainerName() const override;
    virtual void PostInitProperties() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
    //~ End UDeveloperSettings Interface
};