// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "Utilities.h"
#include "UObject/UnrealType.h"
#include "Misc/Base64.h"
#include "Misc/ConfigCacheIni.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "dpapi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

void FUtilities::SaveConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename, bool bAppendClassName)
{
	if (!ensure(Object))
	{
		return;
	}

	const UClass* Class = Object->GetClass();
	const UObject* CDO = Class->GetDefaultObject();

	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		FProperty& Property = **It;
		if (Property.HasAnyPropertyFlags(CPF_Transient) ||
			!ensure(Property.ArrayDim == 1))
		{
			continue;
		}

		const FString Section = bAppendClassName ? BaseSectionName + TEXT(".") + Class->GetName() : BaseSectionName;

		FString	Value;
		if (Property.ExportText_InContainer(0, Value, Object, CDO, Object, PPF_None))
		{
			GConfig->SetString(*Section, *Property.GetName(), *Value, Filename);
		}
		else
		{
			GConfig->RemoveKey(*Section, *Property.GetName(), Filename);
		}
	}
}

void FUtilities::LoadConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename, bool bAppendClassName)
{
	if (!ensure(Object))
	{
		return;
	}

	const UClass* Class = Object->GetClass();
	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		FProperty& Property = **It;
		if (Property.HasAnyPropertyFlags(CPF_Transient) ||
			!ensure(Property.ArrayDim == 1))
		{
			continue;
		}
		
		const FString Section = bAppendClassName ? BaseSectionName + TEXT(".") + Class->GetName() : BaseSectionName;

		FString Value;
		if (GConfig->GetString(*Section, *Property.GetName(), Value, Filename))
		{
			Property.ImportText(*Value, Property.ContainerPtrToValuePtr<void>(Object), PPF_None, Object);
		}
	}
}

FString FUtilities::EncryptData(const FString& Data)
{
	if (Data.IsEmpty())
	{
		return {};
	}

#if PLATFORM_WINDOWS
	const FString Base64 = FBase64::Encode(Data);

	const FTCHARToUTF8 UTF8(*Base64);

	static_assert(sizeof(*UTF8.Get()) == sizeof(uint8), "");

	DATA_BLOB DataIn;
	DataIn.pbData = const_cast<uint8*>(reinterpret_cast<const uint8*>(UTF8.Get()));
	DataIn.cbData = UTF8.Length();
	
	DATA_BLOB DataOut;
	if (!ensure(CryptProtectData(&DataIn, nullptr, nullptr, nullptr, nullptr, 0, &DataOut)))
	{
		return Data;
	}

	const TArray<uint8> EncodedData(DataOut.pbData, DataOut.cbData);
	LocalFree(DataOut.pbData);

	return FBase64::Encode(EncodedData);
#else
	return Data;
#endif
}

FString FUtilities::DecryptData(const FString& Data)
{
	if (Data.IsEmpty())
	{
		return {};
	}

#if PLATFORM_WINDOWS
	TArray<uint8> Dest;
	FBase64::Decode(Data, Dest);

	DATA_BLOB DataIn;
	DataIn.pbData = Dest.GetData();
	DataIn.cbData = Dest.Num();
	
	DATA_BLOB DataOut;
	if (!ensure(CryptUnprotectData(&DataIn, nullptr, nullptr, nullptr, nullptr, 0, &DataOut)))
	{
		return Data;
	}

	const FUTF8ToTCHAR Char(reinterpret_cast<char*>(DataOut.pbData), DataOut.cbData);

	LocalFree(DataOut.pbData);

	FString Result;
	FBase64::Decode(FString(Char.Length(), Char.Get()), Result);
	return Result;
#else
	return Data;
#endif
}