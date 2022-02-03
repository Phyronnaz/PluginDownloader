// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreGlobals.h"

struct FUtilities
{
	static void SaveConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename = GEditorPerProjectIni, bool bAppendClassName = true);
	static void LoadConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename = GEditorPerProjectIni, bool bAppendClassName = true);

	static FString EncryptData(const FString& Data);
	static FString DecryptData(const FString& Data);
};