// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "eos_auth.h"
#include "eos_userinfo.h"
#include "IEOSSDKManager.h"

class FVoxelAuth;

extern FVoxelAuth* GVoxelAuth;
extern FSlateStyleSet* GVoxelAuthStyle;

enum class EVoxelAuthState
{
	Uninitialized,
	LoggedIn,
	LoggedOut,
	LoggingIn,
	LoggingOut,
	LoggingInAutomatically
};

class FVoxelAuth
{
public:
	FVoxelAuth() = default;

	void Transition(EVoxelAuthState NewState);

	EVoxelAuthState GetState() const
	{
		return State;
	}
	const FString& GetDisplayName() const
	{
		return DisplayName;
	}
	const FString& GetAccountId() const
	{
		return AccountIdString;
	}
	FString GetIdToken() const;

private:
	EVoxelAuthState State = EVoxelAuthState::Uninitialized;

	TSharedPtr<IEOSPlatformHandle> Platform;
	EOS_HAuth AuthHandle = nullptr;
	EOS_HUserInfo UserInfoHandle = nullptr;

	FString DisplayName;
	FString AccountIdString;

	void Initialize();
	void OnLogin(const EOS_Auth_LoginCallbackInfo* Data);
};