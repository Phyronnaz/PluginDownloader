// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PluginDownloaderInfo.h"
#include "PluginDownloader.generated.h"

UCLASS(Abstract)
class UPluginDownloaderBase : public UObject
{
	GENERATED_BODY()

public:
	virtual FPluginDownloaderInfo GetInfo() { return {}; }
};

UCLASS()
class UPluginDownloaderRemote : public UPluginDownloaderBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPluginDownloaderInfo Info;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (GetOptions=BranchOptions))
	FString Branch;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EPluginDownloadInstallLocation InstallLocation = EPluginDownloadInstallLocation::Project;

	UPROPERTY(Transient)
	TArray<FString> BranchOptions;

	TMap<FString, FString> BranchDisplayNameToName;

public:
	virtual FPluginDownloaderInfo GetInfo() override
	{
		FPluginDownloaderInfo FullInfo = Info;
		FullInfo.Branch = BranchDisplayNameToName.Contains(Branch) ? BranchDisplayNameToName[Branch] : Branch;
		FullInfo.InstallLocation = InstallLocation;
		return FullInfo;
	}
};

UCLASS()
class UPluginDownloaderCustom : public UPluginDownloaderBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ShowOnlyInnerProperties))
	FPluginDownloaderInfo Info;

public:
	UPROPERTY(Transient)
	TArray<FString> RepoOptions;

	UPROPERTY(Transient)
	TArray<FString> BranchOptions;

public:
	void FillAutoComplete();
	virtual FPluginDownloaderInfo GetInfo() override { return Info; }

protected:
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
};