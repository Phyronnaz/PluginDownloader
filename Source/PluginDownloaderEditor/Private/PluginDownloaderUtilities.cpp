// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderUtilities.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "dpapi.h"
#include "shlobj_core.h"
#include "processthreadsapi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// Hack to make the marketplace review happy
#include "miniz.h"
#include "miniz.cpp"

DEFINE_LOG_CATEGORY(LogPluginDownloader);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////]

void FPluginDownloaderUtilities::DelayedCall(TFunction<void()> Call, float Delay)
{
	check(IsInGameThread());

	UE_500_SWITCH(FTicker, FTSTicker)::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([=](float)
	{
		Call();
		return false;
	}), Delay);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FPluginDownloaderUtilities::SaveConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename, bool bAppendClassName)
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

void FPluginDownloaderUtilities::LoadConfig(UObject* Object, const FString& BaseSectionName, const FString& Filename, bool bAppendClassName)
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FPluginDownloaderUtilities::EncryptData(const FString& Data)
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

FString FPluginDownloaderUtilities::DecryptData(const FString& Data)
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

bool FPluginDownloaderUtilities::ExecuteDetachedBatch(const FString& BatchFile)
{
#if PLATFORM_WINDOWS
	FString SanitizedBatchFile = BatchFile.Replace(TEXT("/"), TEXT("\\"));

	STARTUPINFOW StartupInfo = {};
	StartupInfo.cb = sizeof(StartupInfo);

	StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
	StartupInfo.wShowWindow = SW_SHOW;

	PROCESS_INFORMATION ProcInfo;
	const BOOL bSuccess = CreateProcessW(
		nullptr,
		SanitizedBatchFile.GetCharArray().GetData(),
		nullptr,
		nullptr,
		Windows::FALSE,
		CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE,
		nullptr,
		*FPaths::GetPath(SanitizedBatchFile),
		&StartupInfo,
		&ProcInfo);

	return bSuccess;
#else
	ensure(false);
	return false;
#endif
}

FString FPluginDownloaderUtilities::GetAppData()
{
#if PLATFORM_WINDOWS
	TCHAR LocalAppDataPath[MAX_PATH];
	SHGetFolderPath(0, CSIDL_LOCAL_APPDATA, NULL, 0, LocalAppDataPath);
	return LocalAppDataPath;
#else
	ensure(false);
	return {};
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FPluginDownloaderUtilities::GetIntermediateDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() / "PluginDownloader");
}

void FPluginDownloaderUtilities::CheckTempFolderSize()
{
	const FString PluginDownloaderDir = GetIntermediateDir();

	int64 TotalSize = 0;

	IFileManager& FileManager = IFileManager::Get();
	FileManager.IterateDirectoryStatRecursively(*PluginDownloaderDir, [&](const TCHAR*, const FFileStatData& StatData)
	{
		TotalSize += StatData.FileSize;
		return true;
	});

	if (TotalSize < 2e9)
	{
		return;
	}

	const FString Message = "The plugin downloader temporary folder is over 2GB. Do you want to clear it?\n\nThis will delete " + PluginDownloaderDir;
	if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Message)) != EAppReturnType::Yes)
	{
		return;
	}

	FileManager.DeleteDirectory(*PluginDownloaderDir, false, true);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FPluginDownloaderUtilities::Unzip(const TArray<uint8>& Data, TMap<FString, TArray<uint8>>& OutFiles)
{
#define CheckZip(...) \
		if ((__VA_ARGS__) != MZ_TRUE) \
		{ \
			return FString(mz_zip_get_error_string(mz_zip_peek_last_error(&Zip))); \
		} \
		{ \
			const mz_zip_error Error = mz_zip_peek_last_error(&Zip); \
			if (Error != MZ_ZIP_NO_ERROR) \
			{ \
				return FString(mz_zip_get_error_string(Error)); \
			} \
		}

#define CheckZipError() CheckZip(MZ_TRUE)

	mz_zip_archive Zip;
	mz_zip_zero_struct(&Zip);
	ON_SCOPE_EXIT
	{
		mz_zip_end(&Zip);
	};

	CheckZip(mz_zip_reader_init_mem(&Zip, Data.GetData(), Data.Num(), 0));

	const int32 NumFiles = mz_zip_reader_get_num_files(&Zip);

	for (int32 FileIndex = 0; FileIndex < NumFiles; FileIndex++)
	{
		const int32 FilenameSize = mz_zip_reader_get_filename(&Zip, FileIndex, nullptr, 0);
		CheckZipError();

		TArray<char> FilenameBuffer;
		FilenameBuffer.SetNumUninitialized(FilenameSize);
		mz_zip_reader_get_filename(&Zip, FileIndex, FilenameBuffer.GetData(), FilenameBuffer.Num());
		CheckZipError();

		// To be extra safe
		FilenameBuffer.Add(0);

		const FString Filename = FString(FilenameBuffer.GetData());
		if (Filename.EndsWith("/"))
		{
			continue;
		}

		mz_zip_archive_file_stat FileStat;
		CheckZip(mz_zip_reader_file_stat(&Zip, FileIndex, &FileStat));

		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(FileStat.m_uncomp_size);

		CheckZip(mz_zip_reader_extract_file_to_mem(&Zip, FilenameBuffer.GetData(), Buffer.GetData(), Buffer.Num(), 0));

		ensure(!OutFiles.Contains(Filename));
		OutFiles.Add(Filename, MoveTemp(Buffer));
	}

	return {};

#undef CheckZipError
#undef CheckZip
}