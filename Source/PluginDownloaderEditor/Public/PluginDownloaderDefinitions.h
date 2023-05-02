// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

PLUGINDOWNLOADEREDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogPluginDownloader, Log, All);

#define ENGINE_VERSION (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)

#if ENGINE_VERSION >= 502
#define UE_502_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_502_ONLY(...) __VA_ARGS__
#else
#define UE_502_SWITCH(Before, AfterOrEqual) Before
#define UE_502_ONLY(...)
#endif