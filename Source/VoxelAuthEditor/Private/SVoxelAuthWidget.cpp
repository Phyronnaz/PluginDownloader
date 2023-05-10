// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SVoxelAuthWidget.h"
#include "VoxelAuth.h"
#include "VoxelAuthApi.h"
#include "VoxelAuthDownload.h"

class IVoxelContentEditorModule : public IModuleInterface
{
public:
	int32 Version = 0;

	virtual void ShowContent() = 0;
};

void SVoxelAuthWidget::Construct(const FArguments& Args)
{
	IVoxelContentEditorModule* ContentEditor = FModuleManager::Get().LoadModulePtr<IVoxelContentEditorModule>("VoxelContentEditor");

	GVoxelAuthApi->UpdateVersions();

	if (GVoxelAuth->GetState() == EVoxelAuthState::Uninitialized)
	{
		GVoxelAuth->Transition(EVoxelAuthState::LoggedOut);
	}

	if (GVoxelAuth->GetState() == EVoxelAuthState::LoggedOut)
	{
		GVoxelAuth->Transition(EVoxelAuthState::LoggingInAutomatically);
	}

	const TSharedRef<SEditableTextBox> GumroadKeyEditableBox = SNew(SEditableTextBox);

	const TSharedRef<SVerticalBox> VerifyLicenseKeyVBox =
		SNew(SVerticalBox)
		.Visibility_Lambda([]
		{
			return GVoxelAuthApi->IsPro() ? EVisibility::Collapsed : EVisibility::Visible;
		})

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 0)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.Text(INVTEXT("Bought on Gumroad"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SHorizontalBox)
			.ToolTipText(INVTEXT("Enter your license key\nShould look like ABCDEFGH-ABCDEFGH-ABCDEFGH-ABCDEFGH"))
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(INVTEXT("Gumroad Key"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10, 0)
			[
				SNew(SBox)
				.MinDesiredWidth(150)
				[
					GumroadKeyEditableBox
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SThrobber)
					.Visibility_Lambda([]
					{
						return GVoxelAuthApi->IsVerifyingGumroadKey() ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
				+ SOverlay::Slot()
				[
					SNew(SButton)
					.Visibility_Lambda([]
					{
						return GVoxelAuthApi->IsVerifyingGumroadKey() ? EVisibility::Collapsed : EVisibility::Visible;
					})
					.IsEnabled_Lambda([=]
					{
						return GumroadKeyEditableBox->GetText().ToString().TrimStartAndEnd().Len() == 35;
					})
					.OnClicked_Lambda([=]
					{
						GVoxelAuthApi->VerifyGumroadKey(GumroadKeyEditableBox->GetText().ToString().TrimStartAndEnd());
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(INVTEXT("Verify"))
					]
				]
			]
		]
	
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.Text(INVTEXT("Bought on the Marketplace"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SThrobber)
					.Visibility_Lambda([]
					{
						return GVoxelAuthApi->IsVerifyingMarketplace() ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
				+ SOverlay::Slot()
				[
					SNew(SButton)
					.Visibility_Lambda([]
					{
						return GVoxelAuthApi->IsVerifyingMarketplace() ? EVisibility::Collapsed : EVisibility::Visible;
					})
					.OnClicked_Lambda([=]
					{
						GVoxelAuthApi->VerifyMarketplace();
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(INVTEXT("Verify"))
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 10);

	const TSharedRef<SComboBox<TSharedPtr<FVoxelPluginVersion>>> VersionComboBox =
		SNew(SComboBox<TSharedPtr<FVoxelPluginVersion>>)
		.OptionsSource(&GVoxelAuthApi->AllVersions)
		.OnGenerateWidget_Lambda([](const TSharedPtr<FVoxelPluginVersion>& Item) -> TSharedRef<SWidget>
		{
			if (!ensure(Item))
			{
				return SNullWidget::NullWidget;
			}
			return SNew(STextBlock).Text(Item->ToDisplayString());
		})
		.OnSelectionChanged_Lambda([](const TSharedPtr<FVoxelPluginVersion>& Item, ESelectInfo::Type)
		{
			if (!ensure(Item))
			{
				return;
			}
			GVoxelAuthApi->SelectedVersion = *Item;
		})
		[
			SNew(STextBlock)
			.Text_Lambda([]
			{
				return GVoxelAuthApi->SelectedVersion.ToDisplayString();
			})
		];

	GVoxelAuthApi->OnComboBoxesUpdated.AddSP(VersionComboBox, &SComboBox<TSharedPtr<FVoxelPluginVersion>>::RefreshOptions);

	const auto IsDownloadEnabled = []
	{
		if (GVoxelAuthApi->SelectedVersion == GVoxelAuthApi->PluginVersion)
		{
			return false;
		}

		for (const TSharedPtr<FVoxelPluginVersion>& Version : GVoxelAuthApi->AllVersions)
		{
			if (!ensure(Version))
			{
				continue;
			}
			if (GVoxelAuthApi->SelectedVersion == *Version)
			{
				return true;
			}
		}
		return false;
	};

	const TSharedRef<SVerticalBox> DownloadVBox =
		SNew(SVerticalBox)
		.Visibility_Lambda([]
		{
			return GVoxelAuthApi->IsPro() && GVoxelAuthApi->AllVersions.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
		})
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(INVTEXT("Installed Version"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([=]
				{
					if (GVoxelAuthApi->PluginVersion.Type == FVoxelPluginVersion::EType::Unknown)
					{
						return INVTEXT("Unknown");
					}

					const bool bIsLatest = GVoxelAuthApi->GetPluginState() == FVoxelAuthApi::EState::NoUpdate;
					return FText::FromString(GVoxelAuthApi->PluginVersion.ToDisplayString().ToString() + (bIsLatest ? " (latest)" : " (not latest)"));
				})
			]
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(INVTEXT("Version"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				VersionComboBox
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.IsEnabled_Lambda(IsDownloadEnabled)
				.OnClicked_Lambda([=]
				{
					GVoxelAuthDownload->Download(GVoxelAuthApi->SelectedVersion);
					return FReply::Handled();
				})
				.ContentPadding(FMargin(0, 5.f, 0, 4.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.CircleArrowDown"))
						.ColorAndOpacity(FStyleColors::AccentGreen)
					]
					+ SHorizontalBox::Slot()
					.Padding(FMargin(5, 0, 0, 0))
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "SmallButtonText")
						.Text(INVTEXT("Download"))
					]
				]
			]

			+ SHorizontalBox::Slot()

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.IsEnabled_Lambda(IsDownloadEnabled)
				.OnClicked_Lambda([=]
				{
					GVoxelAuthApi->OpenReleaseNotes(GVoxelAuthApi->SelectedVersion);
					return FReply::Handled();
				})
				.ContentPadding(FMargin(0, 5.f, 0, 4.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Documentation"))
						.ColorAndOpacity(FStyleColors::AccentGreen)
					]
					+ SHorizontalBox::Slot()
					.Padding(FMargin(5, 0, 0, 0))
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "SmallButtonText")
						.Text(INVTEXT("Release Notes"))
					]
				]
			]
			
			+ SHorizontalBox::Slot()
		];

	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////

	const TSharedRef<SVerticalBox> DetailsVBox =
		SNew(SVerticalBox)
		.Visibility_Lambda([]
		{
			return GVoxelAuthApi->IsProUpdated() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 0)
		[
			VerifyLicenseKeyVBox
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 0)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SThrobber)
				.Visibility_Lambda([]
				{
					return GVoxelAuthApi->AllVersions.Num() == 0 ? EVisibility::Visible : EVisibility::Collapsed;
				})
			]
			+ SOverlay::Slot()
			[
				DownloadVBox
			]
		];

	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////

	ChildSlot
	[
		SNew(SBox)
		.Padding(10)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ToolTipText_Lambda([]
					{
						if (GVoxelAuth->GetState() == EVoxelAuthState::LoggedOut)
						{
							return INVTEXT("Login");
						}
						if (GVoxelAuth->GetState() == EVoxelAuthState::LoggedIn)
						{
							return INVTEXT("Logout");
						}

						return INVTEXT("Logging in");
					})
					.OnClicked_Lambda([]
					{
						if (GVoxelAuth->GetState() == EVoxelAuthState::LoggedOut ||
							GVoxelAuth->GetState() == EVoxelAuthState::LoggingIn)
						{
							GVoxelAuth->Transition(EVoxelAuthState::LoggingIn);
						}
						if (GVoxelAuth->GetState() == EVoxelAuthState::LoggedIn)
						{
							if (FMessageDialog::Open(EAppMsgType::YesNoCancel, INVTEXT("Do you want to log out?")) == EAppReturnType::Yes)
							{
								GVoxelAuth->Transition(EVoxelAuthState::LoggingOut);
							}
						}
						return FReply::Handled();
					})
					.ButtonStyle(GVoxelAuthStyle, "EpicButton")
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(32)
							.HeightOverride(32)
							[
								SNew(SScaleBox)
								.Stretch(EStretch::ScaleToFit)
								[
									SNew(SImage)
									.Image(GVoxelAuthStyle->GetBrush("EpicIcon"))
								]
							]
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Padding(7, 0, 7, 0)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SThrobber)
								.Visibility_Lambda([]
								{
									return
										GVoxelAuth->GetState() == EVoxelAuthState::LoggingIn ||
										GVoxelAuth->GetState() == EVoxelAuthState::LoggingInAutomatically
										? EVisibility::Visible
										: EVisibility::Collapsed;
								})
							]
							+ SOverlay::Slot()
							[
								SNew(STextBlock)
								.Visibility_Lambda([]
								{
									return GVoxelAuth->GetState() == EVoxelAuthState::LoggedOut ? EVisibility::Visible : EVisibility::Collapsed;
								})
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
								.Text(INVTEXT("Login"))
							]
							+ SOverlay::Slot()
							[
								SNew(STextBlock)
								.Visibility_Lambda([]
								{
									return GVoxelAuth->GetState() == EVoxelAuthState::LoggedIn ? EVisibility::Visible : EVisibility::Collapsed;
								})
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
								.Text_Lambda([]
					            {
					                return FText::FromString(GVoxelAuth->GetDisplayName());
					            })
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(10, 0, 0, 0)
					[
						SNew(SButton)
						.ToolTipText(INVTEXT("Open documentation"))
						.OnClicked_Lambda([]
						{
							FPlatformProcess::LaunchURL(TEXT("https://docs.voxelplugin.com"), nullptr, nullptr);
							return FReply::Handled();
						})
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						[
							SNew(SBox)
							.WidthOverride(20)
							.HeightOverride(20)
							[
								SNew(SScaleBox)
								.Stretch(EStretch::ScaleToFit)
								[
									SNew(SImage)
									.Image(FAppStyle::Get().GetBrush("Icons.Documentation"))
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.ToolTipText(INVTEXT("Open contact email"))
						.OnClicked_Lambda([]
						{
							FPlatformProcess::LaunchURL(TEXT("mailto:contact@voxelplugin.com"), nullptr, nullptr);
							return FReply::Handled();
						})
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						[
							SNew(SBox)
							.WidthOverride(20)
							.HeightOverride(20)
							[
								SNew(SScaleBox)
								.Stretch(EStretch::ScaleToFit)
								[
									SNew(SImage)
									.Image(GVoxelAuthStyle->GetBrush("EmailIcon"))
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.ToolTipText(INVTEXT("Open discord"))
						.OnClicked_Lambda([]
						{
							FPlatformProcess::LaunchURL(TEXT("https://discord.voxelplugin.com"), nullptr, nullptr);
							return FReply::Handled();
						})
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						[
							SNew(SBox)
							.WidthOverride(20)
							.HeightOverride(20)
							[
								SNew(SScaleBox)
								.Stretch(EStretch::ScaleToFit)
								[
									SNew(SImage)
									.Image(GVoxelAuthStyle->GetBrush("DiscordIcon"))
								]
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 10, 0, 0)
			[
				SNew(SOverlay)
				.Visibility_Lambda([]
				{
					return
						GVoxelAuth->GetState() == EVoxelAuthState::LoggedIn ||
						GVoxelAuth->GetState() == EVoxelAuthState::LoggingInAutomatically
						? EVisibility::Visible
						: EVisibility::Collapsed;
				})
				+ SOverlay::Slot()
				[
					SNew(SThrobber)
					.Visibility_Lambda([]
					{
						return GVoxelAuthApi->IsProUpdated() ? EVisibility::Collapsed : EVisibility::Visible;
					})
				]
				+ SOverlay::Slot()
				[
					DetailsVBox
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([=]
				{
					return ContentEditor ? EVisibility::Visible : EVisibility::Collapsed;
				})

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 10, 0, 0)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SButton)
						.OnClicked_Lambda([=]
						{
							ContentEditor->ShowContent();
							return FReply::Handled();
						})
						.ContentPadding(FMargin(0, 5.f, 0, 4.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("MainFrame.VisitOnlineLearning"))
								.ColorAndOpacity(FStyleColors::White)
							]
							+ SHorizontalBox::Slot()
							.Padding(FMargin(5, 0, 0, 0))
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(FAppStyle::Get(), "SmallButtonText")
								.Text(INVTEXT("Open Examples"))
							]
						]
					]

					+ SHorizontalBox::Slot()
				]
			]
		]
	];
}