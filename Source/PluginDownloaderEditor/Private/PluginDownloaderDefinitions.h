// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPluginDownloader, Log, All);

#ifndef IS_UE5_EA
#define IS_UE5_EA 0
#endif

#define UE5_EA 499

#if IS_UE5_EA
#define ENGINE_VERSION UE5_EA
#else
#define ENGINE_VERSION (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)
#endif

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

#if ENGINE_VERSION >= UE5_EA
#define UE_5EA_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_5EA_ONLY(...) __VA_ARGS__
#else
#define UE_5EA_SWITCH(Before, AfterOrEqual) Before
#define UE_5EA_ONLY(...)
#endif

#if ENGINE_VERSION >= 500
#define UE_500_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_500_ONLY(...) __VA_ARGS__
#else
#define UE_500_SWITCH(Before, AfterOrEqual) Before
#define UE_500_ONLY(...)
#endif