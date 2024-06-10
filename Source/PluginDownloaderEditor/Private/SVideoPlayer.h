// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class UMaterialInstanceDynamic;
class UMediaTexture;
class UMediaPlayer;
class UMediaSource;

class SVideoPlayer : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SVideoPlayer) {}
		SLATE_ARGUMENT(UMediaSource*, Source)
		SLATE_ARGUMENT(FVector2D, Size)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual ~SVideoPlayer() override;

	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject Interface

private:
	FSlateBrush VideoBrush;
	TObjectPtr<UMaterialInstanceDynamic> VideoMaterial;

	TObjectPtr<UMediaPlayer> MediaPlayer;
	TObjectPtr<UMediaTexture> MediaTexture;
	TObjectPtr<UMediaSource> MediaSource;
};