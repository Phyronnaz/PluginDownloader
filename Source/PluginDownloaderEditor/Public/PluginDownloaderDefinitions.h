// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

PLUGINDOWNLOADEREDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogPluginDownloader, Log, All);

#define ENGINE_VERSION (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)

#if ENGINE_VERSION >= 426
#define UE_426_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_426_ONLY(...) __VA_ARGS__
#else
#define UE_426_SWITCH(Before, AfterOrEqual) Before
#define UE_426_ONLY(...)
#endif

#if ENGINE_VERSION >= 427
#define UE_427_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_427_ONLY(...) __VA_ARGS__
#else
#define UE_427_SWITCH(Before, AfterOrEqual) Before
#define UE_427_ONLY(...)
#endif

#if ENGINE_VERSION >= 500
#define UE_500_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_500_ONLY(...) __VA_ARGS__
#else
#define UE_500_SWITCH(Before, AfterOrEqual) Before
#define UE_500_ONLY(...)
#endif

#if ENGINE_VERSION >= 501
#define UE_501_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_501_ONLY(...) __VA_ARGS__
#else
#define UE_501_SWITCH(Before, AfterOrEqual) Before
#define UE_501_ONLY(...)
#endif

#if ENGINE_VERSION < 500
#define MarkAsGarbage MarkPendingKill
#endif

using FEditorAppStyle = class UE_501_SWITCH(FEditorStyle, FAppStyle);