// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct FVoxelPluginVersion
{
	enum class EType
	{
		Unknown,
		Release,
		Preview,
		Dev
	};
	EType Type = EType::Unknown;

    int32 Major = 0;
    int32 Minor = 0;
    int32 Hotfix = 0;
    int32 PreviewWeek = 0;
    int32 PreviewHotfix = 0;

	int32 DevCounter = 0;

	bool Parse(const FString& Version)
	{
#define CHECK(...) if (!ensure(__VA_ARGS__)) { return false; }

		if (Version.StartsWith("dev"))
		{
			Type = EType::Dev;

			FString DevCounterString = Version;
			CHECK(DevCounterString.RemoveFromStart("dev-"));

			DevCounter = FCString::Atoi(*DevCounterString);
			CHECK(FString::FromInt(DevCounter) == DevCounterString);

			return true;
		}

		TArray<FString> VersionAndPreview;
		Version.ParseIntoArray(VersionAndPreview, TEXT("p-"));
		CHECK(VersionAndPreview.Num() == 1 || VersionAndPreview.Num() == 2);

		if (VersionAndPreview.Num() == 1)
		{
			Type = EType::Release;

			TArray<FString> MinorMajorHotfix;
			VersionAndPreview[0].ParseIntoArray(MinorMajorHotfix, TEXT("."));
			CHECK(MinorMajorHotfix.Num() == 3);

			Major = FCString::Atoi(*MinorMajorHotfix[0]);
			Minor = FCString::Atoi(*MinorMajorHotfix[1]);
			Hotfix = FCString::Atoi(*MinorMajorHotfix[2]);
			CHECK(FString::FromInt(Major) == MinorMajorHotfix[0]);
			CHECK(FString::FromInt(Minor) == MinorMajorHotfix[1]);
			CHECK(FString::FromInt(Hotfix) == MinorMajorHotfix[2]);

			return true;
		}
		else
		{
			Type = EType::Preview;

			TArray<FString> MinorMajor;
			VersionAndPreview[0].ParseIntoArray(MinorMajor, TEXT("."));
			CHECK(MinorMajor.Num() == 2);

			TArray<FString> PreviewAndHotfix;
			VersionAndPreview[1].ParseIntoArray(PreviewAndHotfix, TEXT("."));
			CHECK(PreviewAndHotfix.Num() == 1 || PreviewAndHotfix.Num() == 2);

			Major = FCString::Atoi(*MinorMajor[0]);
			Minor = FCString::Atoi(*MinorMajor[1]);
			CHECK(FString::FromInt(Major) == MinorMajor[0]);
			CHECK(FString::FromInt(Minor) == MinorMajor[1]);

			PreviewWeek = FCString::Atoi(*PreviewAndHotfix[0]);
			CHECK(FString::FromInt(PreviewWeek) == PreviewAndHotfix[0]);

			if (PreviewAndHotfix.Num() == 2)
			{
				PreviewHotfix = FCString::Atoi(*PreviewAndHotfix[1]);
				CHECK(FString::FromInt(PreviewHotfix) == PreviewAndHotfix[1]);
			}

			return true;
		}
#undef CHECK
	}
	void ParseCounter(int32 Counter)
	{
		if (Counter == 0)
		{
			Type = EType::Unknown;
			return;
		}
		if (Counter < 100000)
		{
			Type = EType::Dev;
			DevCounter = Counter;
			return;
		}

		PreviewHotfix = Counter % 10;
		Counter /= 10;

		PreviewWeek = Counter % 1000;
		Counter /= 1000;

		Hotfix = Counter % 10;
		Counter /= 10;

		Minor = Counter % 10;
		Counter /= 10;

		Major = Counter % 10;
		ensure(Counter == Major);

		Type = PreviewWeek == 999 ? EType::Release : EType::Preview;
	}

	FString GetBranch() const
	{
		if (Type == EType::Unknown)
		{
			return "unknown";
		}
		if (Type == EType::Dev)
		{
			return "dev";
		}
		return FString::Printf(TEXT("%d.%d"), Major, Minor);
	}
	int32 GetCounter() const
	{
		if (Type == EType::Unknown)
		{
			return 0;
		}
		if (Type == EType::Dev)
		{
			return DevCounter;
		}

		int32 Counter = Major;

		Counter *= 10;
		Counter += Minor;

		Counter *= 10;
		Counter += Type == EType::Preview ? 0 : Hotfix;

		Counter *= 1000;
		Counter += Type == EType::Preview ? PreviewWeek : 999;

		Counter *= 10;
		Counter += Type == EType::Preview ? PreviewHotfix : 0;

		return Counter;
	}
	FString ToString() const
	{
		if (Type == EType::Unknown)
		{
			return "Unknown";
		}
		else if (Type == EType::Release)
		{
			return FString::Printf(TEXT("%d.%d.%d"), Major, Minor, Hotfix);
		}
		else if (Type == EType::Preview)
		{
			ensure(Hotfix == 0);
			return FString::Printf(TEXT("%d.%dp-%d.%d"), Major, Minor, PreviewWeek, PreviewHotfix);
		}
		else
		{
			check(Type == EType::Dev);
			return FString::Printf(TEXT("dev-%d"), DevCounter);
		}
	}
};