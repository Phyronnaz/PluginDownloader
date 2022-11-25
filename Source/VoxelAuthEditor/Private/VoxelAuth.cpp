// Copyright Voxel Plugin, Inc. All Rights Reserved.

#if ENGINE_VERSION >= 500
#include "VoxelAuth.h"
#include "VoxelAuthApi.h"
#include "eos_sdk.h"

FVoxelAuth* GVoxelAuth = nullptr;

void FVoxelAuth::Transition(const EVoxelAuthState NewState)
{
	const EVoxelAuthState OldState = State;
	State = NewState;

	static const TSet<TPair<EVoxelAuthState, EVoxelAuthState>> ValidTransitions = 
	{
		{ EVoxelAuthState::Uninitialized, EVoxelAuthState::LoggedOut },
		{ EVoxelAuthState::LoggedIn, EVoxelAuthState::LoggingOut },
		{ EVoxelAuthState::LoggedOut, EVoxelAuthState::LoggingIn },
		{ EVoxelAuthState::LoggedOut, EVoxelAuthState::LoggingInAutomatically },
		{ EVoxelAuthState::LoggingIn, EVoxelAuthState::LoggedIn },
		{ EVoxelAuthState::LoggingIn, EVoxelAuthState::LoggedOut },
		{ EVoxelAuthState::LoggingIn, EVoxelAuthState::LoggingIn },
		{ EVoxelAuthState::LoggingInAutomatically, EVoxelAuthState::LoggedIn },
		{ EVoxelAuthState::LoggingInAutomatically, EVoxelAuthState::LoggedOut },
		{ EVoxelAuthState::LoggingOut, EVoxelAuthState::LoggedOut },
	};
	ensure(ValidTransitions.Contains({ OldState, NewState }));

	if (OldState == EVoxelAuthState::Uninitialized)
	{
		Initialize();
		return;
	}
	if (!ensure(Platform) ||
		!ensure(AuthHandle) ||
		!ensure(UserInfoHandle))
	{
		return;
	}

	if (NewState == EVoxelAuthState::LoggedOut ||
		NewState == EVoxelAuthState::LoggingIn ||
		NewState == EVoxelAuthState::LoggingInAutomatically)
	{
		DisplayName.Reset();
		AccountIdString.Reset();
	}

	if (NewState == EVoxelAuthState::LoggingOut)
	{
		EOS_Auth_DeletePersistentAuthOptions DeletePersistentAuthOptions = {};
		DeletePersistentAuthOptions.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
		EOS_Auth_DeletePersistentAuth(AuthHandle, &DeletePersistentAuthOptions, nullptr, [](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
		{
			EOS_Auth_LogoutOptions LogoutOptions = {};
			LogoutOptions.ApiVersion = EOS_AUTH_LOGOUT_API_LATEST;
			LogoutOptions.LocalUserId = EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*GVoxelAuth->AccountIdString));
			ensure(EOS_EpicAccountId_IsValid(LogoutOptions.LocalUserId));

			EOS_Auth_Logout(GVoxelAuth->AuthHandle, &LogoutOptions, nullptr, [](const EOS_Auth_LogoutCallbackInfo* Data)
			{
				GVoxelAuth->Transition(EVoxelAuthState::LoggedOut);
			});
		});

		return;
	}

	if (NewState == EVoxelAuthState::LoggingIn)
	{
		EOS_Auth_LoginOptions LoginOptions = {};
		LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
		LoginOptions.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;

		EOS_Auth_Credentials Credentials = {};
		Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
		Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
		LoginOptions.Credentials = &Credentials;

		EOS_Auth_Login(AuthHandle, &LoginOptions, nullptr, [](const EOS_Auth_LoginCallbackInfo* Data)
		{
			if (Data && Data->ResultCode == EOS_EResult::EOS_Success)
			{
				GVoxelAuth->OnLogin(Data);
				return;
			}

			EOS_Auth_LoginOptions NewLoginOptions = {};
			NewLoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
			NewLoginOptions.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;

			EOS_Auth_Credentials NewCredentials = {};
			NewCredentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
			NewCredentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
			NewLoginOptions.Credentials = &NewCredentials;

			EOS_Auth_Login(GVoxelAuth->AuthHandle, &NewLoginOptions, nullptr, [](const EOS_Auth_LoginCallbackInfo* Data)
			{
				GVoxelAuth->OnLogin(Data);
			});
		});

		return;
	}

	if (NewState == EVoxelAuthState::LoggingInAutomatically)
	{
		EOS_Auth_LoginOptions LoginOptions = {};
		LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
		LoginOptions.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;

		EOS_Auth_Credentials Credentials = {};
		Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
		Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
		LoginOptions.Credentials = &Credentials;

		EOS_Auth_Login(AuthHandle, &LoginOptions, nullptr, [](const EOS_Auth_LoginCallbackInfo* Data)
		{
			GVoxelAuth->OnLogin(Data);
		});

		return;
	}
}

FString FVoxelAuth::GetIdToken() const
{
	EOS_Auth_CopyIdTokenOptions CopyIdTokenOptions = {};
	CopyIdTokenOptions.ApiVersion = EOS_AUTH_COPYIDTOKEN_API_LATEST;
	CopyIdTokenOptions.AccountId = EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*AccountIdString));

	EOS_Auth_IdToken* IdToken = nullptr;
	if (!ensure(EOS_Auth_CopyIdToken(AuthHandle, &CopyIdTokenOptions, &IdToken) == EOS_EResult::EOS_Success) ||
		!ensure(IdToken) ||
		!ensure(IdToken->JsonWebToken))
	{
		return {};
	}

	return IdToken->JsonWebToken;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelAuth::Initialize()
{
	IEOSSDKManager* Manager = IEOSSDKManager::Get();
	if (!ensure(Manager) ||
		!ensure(Manager->Initialize() == EOS_EResult::EOS_Success))
	{
		return;
	}

	const FTCHARToUTF8 ProductId(TEXT("64567e503d6843658380f2697d096926"));
	const FTCHARToUTF8 SandboxId(TEXT("983dd304db1e4b51b9c3b3f6060d3d92"));
	const FTCHARToUTF8 DeploymentId(TEXT("04b47d9ae3cb4a85b024df1b2505f5ec"));
	const FTCHARToUTF8 ClientId(TEXT("xyza7891YZpqcpFOJEWn3g6M606jjpSg"));
	const FTCHARToUTF8 ClientSecret(TEXT("cXi7xIyfO/ZdjR3od19OYtz4EuaeGt10fhi/YT9Fdzc"));
	const FTCHARToUTF8 CacheDirectory(*(Manager->GetCacheDirBase() / TEXT("OnlineServicesEOS")));

	EOS_Platform_Options PlatformOptions = {};
	PlatformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
	PlatformOptions.Flags = EOS_PF_LOADING_IN_EDITOR;

	PlatformOptions.ProductId = ProductId.Get();
	PlatformOptions.SandboxId = SandboxId.Get();
	PlatformOptions.DeploymentId = DeploymentId.Get();
	PlatformOptions.ClientCredentials.ClientId = ClientId.Get();
	PlatformOptions.ClientCredentials.ClientSecret = ClientSecret.Get();
	PlatformOptions.CacheDirectory = CacheDirectory.Get();
	
	ensure(!Platform);
	Platform = Manager->CreatePlatform(PlatformOptions);
	if (!ensure(Platform))
	{
		return;
	}

	ensure(!AuthHandle);
	AuthHandle = EOS_Platform_GetAuthInterface(*Platform);

	ensure(!UserInfoHandle);
	UserInfoHandle = EOS_Platform_GetUserInfoInterface(*Platform);
}

void FVoxelAuth::OnLogin(const EOS_Auth_LoginCallbackInfo* Data)
{
	ensure(State == EVoxelAuthState::LoggingIn || State == EVoxelAuthState::LoggingInAutomatically);

	if (!ensure(Data) ||
		Data->ResultCode != EOS_EResult::EOS_Success)
	{
		Transition(EVoxelAuthState::LoggedOut);
		return;
	}

	const EOS_EpicAccountId AccountId = Data->LocalUserId;
	if (!ensure(EOS_EpicAccountId_IsValid(AccountId)))
	{
		Transition(EVoxelAuthState::LoggedOut);
		return;
	}

	{
		char Buffer[EOS_EPICACCOUNTID_MAX_LENGTH + 1] = { 0 };
		int32 BufferLength = sizeof(Buffer);
		if (!ensure(EOS_EpicAccountId_ToString(AccountId, Buffer, &BufferLength) == EOS_EResult::EOS_Success))
		{
			Transition(EVoxelAuthState::LoggedOut);
			return;
		}
		ensure(BufferLength == 33);

		AccountIdString = Buffer;
	}

	UE_LOG(LogPluginDownloader, Log, TEXT("EOS Connected"));

	GVoxelAuthApi->UpdateIsPro();

	if (!ensure(UserInfoHandle))
	{
		Transition(EVoxelAuthState::LoggedOut);
		return;
	}

	EOS_UserInfo_QueryUserInfoOptions UserInfoOptions = {};
	UserInfoOptions.ApiVersion = EOS_USERINFO_QUERYUSERINFO_API_LATEST;
	UserInfoOptions.LocalUserId = AccountId;
	UserInfoOptions.TargetUserId = AccountId;
	EOS_UserInfo_QueryUserInfo(UserInfoHandle, &UserInfoOptions, nullptr, [](const EOS_UserInfo_QueryUserInfoCallbackInfo* Data)
	{
		if (!ensure(Data) ||
			!ensure(Data->ResultCode == EOS_EResult::EOS_Success))
		{
			GVoxelAuth->Transition(EVoxelAuthState::LoggedOut);
			return;
		}

		EOS_UserInfo_CopyUserInfoOptions CopyUserInfoOptions = {};
		CopyUserInfoOptions.ApiVersion = EOS_USERINFO_COPYUSERINFO_API_LATEST;
		CopyUserInfoOptions.LocalUserId = Data->LocalUserId;
		CopyUserInfoOptions.TargetUserId = Data->TargetUserId;

		EOS_UserInfo* UserInfo = nullptr;
		if (!ensure(EOS_UserInfo_CopyUserInfo(GVoxelAuth->UserInfoHandle, &CopyUserInfoOptions, &UserInfo) == EOS_EResult::EOS_Success))
		{
			return;
		}

		GVoxelAuth->DisplayName = UserInfo->DisplayName;
		GVoxelAuth->Transition(EVoxelAuthState::LoggedIn);
	});
}
#endif