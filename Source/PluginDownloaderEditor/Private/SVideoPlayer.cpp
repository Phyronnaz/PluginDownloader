// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SVideoPlayer.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"

void SVideoPlayer::Construct(const FArguments& Args)
{
	MediaPlayer = NewObject<UMediaPlayer>();
	MediaPlayer->PlayOnOpen = true;
	MediaPlayer->SetLooping(true);

	MediaTexture = NewObject<UMediaTexture>();
	MediaTexture->AutoClear = true;
	MediaTexture->SetMediaPlayer(MediaPlayer);
	MediaTexture->UpdateResource();

	MediaSource = Args._Source;

	MediaPlayer->OpenSource(MediaSource);

	VideoBrush.ImageSize = Args._Size;

	UMaterial* VideoMaterialParent = LoadObject<UMaterial>(nullptr, TEXT("/PluginDownloader/VideoPlayerMaterial"));
	if (ensure(VideoMaterialParent))
	{
		VideoMaterial = UMaterialInstanceDynamic::Create(VideoMaterialParent, nullptr);
		VideoMaterial->SetTextureParameterValue(TEXT("MediaTexture"), MediaTexture);
		VideoBrush.SetResourceObject(VideoMaterial);
	}

	ChildSlot
	[
		SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFit)
		[
			SNew(SImage)
			.Image(&VideoBrush)
		]
	];
}

SVideoPlayer::~SVideoPlayer()
{
	if (VideoMaterial)
	{
		VideoMaterial->MarkAsGarbage();
	}
	if (MediaPlayer)
	{
		MediaPlayer->Close();
		MediaPlayer->MarkAsGarbage();
	}
	if (MediaTexture)
	{
		MediaTexture->MarkAsGarbage();
	}

	// We don't own MediaSource so don't kill it
}

void SVideoPlayer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(VideoMaterial);

	Collector.AddReferencedObject(MediaPlayer);
	Collector.AddReferencedObject(MediaTexture);
	Collector.AddReferencedObject(MediaSource);
}
