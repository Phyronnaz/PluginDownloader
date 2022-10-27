// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderSettings.h"

FName UPluginDownloaderSettings::GetContainerName() const
{
	return "Project";
}

void UPluginDownloaderSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif
}

#if WITH_EDITOR
void UPluginDownloaderSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif