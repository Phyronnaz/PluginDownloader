// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "PluginDownloaderCustomization.h"
#include "PluginDownloader.h"
#include "PluginDownloaderTokens.h"
#include "SVideoPlayer.h"
#include "StreamMediaSource.h"

#define LOCTEXT_NAMESPACE "PluginDownloader"

void FPluginDownloaderCustomCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	check(Objects.Num() == 1);
	UPluginDownloaderCustom* Custom = CastChecked<UPluginDownloaderCustom>(Objects[0].Get());

	DetailLayout.EditCategory("Settings", {}, ECategoryPriority::Uncommon)
	.AddCustomRow(LOCTEXT("Refresh", "Refresh"))
	.NameContent()
	[
		SNew(STextBlock)
		.Font(FEditorAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.Text_Lambda([=]
		{
			return LOCTEXT("Refresh", "Refresh");
		})
	]
	.ValueContent()
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("RefreshTooltip", "Queries the github API to fill the dropdowns"))
		.ContentPadding(2)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.OnClicked_Lambda([=]
		{
			Custom->FillAutoComplete();

			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Font(FEditorAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([=]
			{
				return LOCTEXT("Refresh", "Refresh");
			})
		]
	];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FPluginDownloaderTokensCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	check(Objects.Num() == 1);
	UPluginDownloaderTokens* Tokens = CastChecked<UPluginDownloaderTokens>(Objects[0].Get());

	{
		const TSharedRef<IPropertyHandle> Handle = DetailLayout.GetProperty("GithubAccessToken");
		DetailLayout.EditDefaultProperty(Handle)->CustomWidget()
		.NameContent()
		[
			Handle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.MinDesiredWidth(120)
				[
					Handle->CreatePropertyValueWidget()
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(2, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([=]
				{
					// See FWmfMediaTracks::AddTrackToTopology
					if (GDynamicRHI->GetName() == FString("D3D12"))
					{
						// Can't play video
						FPlatformProcess::LaunchURL(TEXT("https://github.com/settings/tokens"), nullptr, nullptr);
						return FReply::Handled();
					}

					UStreamMediaSource* MediaSource = NewObject<UStreamMediaSource>();
					MediaSource->StreamUrl = "https://raw.githubusercontent.com/Phyronnaz/PluginDownloaderData/master/Videos/HowToCreateToken.mp4";

					const TSharedRef<SWindow> Window =
						SNew(SWindow)
						.Title(LOCTEXT("CreateTokenHowTo", "How to create a new token"))
						.ClientSize(FVector2D(1280, 760))
						.IsTopmostWindow(true);

					Window->SetContent(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(1)
						[
							SNew(SVideoPlayer)
							.Source(MediaSource)
							.Size(FVector2D(1920, 1080))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 5)
						[
							SNew(SBox)
							.HAlign(HAlign_Center)
							[
								SNew(SButton)
								.IsEnabled(true)
								.ToolTip(SNew(SToolTip).Text(LOCTEXT("OpenBrowswer", "Click here to open github in your web browser")))
								.OnClicked_Lambda([=]
								{
									FPlatformProcess::LaunchURL(TEXT("https://github.com/settings/tokens"), nullptr, nullptr);
									return FReply::Handled();
								})
#if ENGINE_VERSION < 500
								.ContentPadding(5)
								.TextStyle(FEditorStyle::Get(), "LargeText")
								.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
								.HAlign(HAlign_Center)
								.Text(LOCTEXT("OpenGithub", "Open Github"))
#else
								.ContentPadding(FMargin(0, 5.f, 0, 4.f))
								.Content()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									[
										SNew(SImage)
										.Image(FAppStyle::Get().GetBrush("Icons.Link"))
										.ColorAndOpacity(FStyleColors::AccentGreen)
									]
									+ SHorizontalBox::Slot()
									.Padding(FMargin(5, 0, 0, 0))
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(STextBlock)
										.TextStyle(FAppStyle::Get(), "SmallButtonText")
										.Text(LOCTEXT("OpenGithub", "Open Github"))
									]
								]
#endif
							]
						]
					);

					FSlateApplication::Get().AddWindow(Window);

					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(FEditorAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.ToolTipText(LOCTEXT("TokenTooltip", "Create a new token from your github account. Make sure to tick the REPO scope"))
					.Text_Lambda([=]
					{
						return LOCTEXT("Create token", "Create token");
					})
				]
			]
		];
	}

	{
		const TSharedRef<IPropertyHandle> Handle = DetailLayout.GetProperty("GithubStatus");

		DetailLayout.EditDefaultProperty(Handle)->CustomWidget()
		.NameContent()
		[
			Handle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(300)
		.MaxDesiredWidth(300)
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FEditorAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ColorAndOpacity_Lambda([=]
				{
					return Tokens->GithubStatus.IsEmpty()
						? Tokens->GithubAccessToken.IsEmpty()
						? FSlateColor(FColor::Orange)
						: FSlateColor::UseForeground()
						: FSlateColor(FColor::Red);
				})
				.Text_Lambda([=]
				{
					return FText::FromString(Tokens->GithubStatus.IsEmpty()
						? Tokens->GithubAccessToken.IsEmpty()
						? "Empty token"
						: "Valid token"
						: Tokens->GithubStatus);
				})
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE