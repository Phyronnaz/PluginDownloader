// Copyright Voxel Plugin, Inc. All Rights Reserved.

#if ENGINE_VERSION >= 500
#include "SVoxelAuthWidget.h"
#include "VoxelAuth.h"
#include "VoxelAuthApi.h"
#include "VoxelAuthDownload.h"

void SVoxelAuthWidget::Construct(const FArguments& Args)
{
	GVoxelAuthApi->UpdateVersions(true);

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

	GVoxelAuthApi->UpdateComboBoxes();

	FString IsLatest;
	if (GVoxelAuthApi->GetVersions().Contains(GVoxelAuthApi->GetPluginBranch()) &&
		GVoxelAuthApi->GetVersions()[GVoxelAuthApi->GetPluginBranch()].Last() == GVoxelAuthApi->GetPluginCounter())
	{
		IsLatest = " (latest)";
	}

	const TSharedRef<SWidget> BranchComboBox =
		SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&GVoxelAuthApi->Branches)
		.OnGenerateWidget_Lambda([](const TSharedPtr<FString>& Item) -> TSharedRef<SWidget>
		{
			if (!ensure(Item))
			{
				return SNullWidget::NullWidget;
			}

			return SNew(STextBlock).Text(FText::FromString(*Item));
		})
		.OnSelectionChanged_Lambda([](const TSharedPtr<FString>& Item, ESelectInfo::Type)
		{
			if (!ensure(Item))
			{
				return;
			}

			GVoxelAuthApi->SelectedPluginBranch = *Item;
			GVoxelAuthApi->UpdateComboBoxes();
		})
		[
			SNew(STextBlock)
			.Text_Lambda([]
			{
				return FText::FromString(GVoxelAuthApi->SelectedPluginBranch);
			})
		];

	const TSharedRef<SWidget> VersionComboBox =
		SNew(SComboBox<TSharedPtr<int32>>)
		.OptionsSource(&GVoxelAuthApi->Counters)
		.OnGenerateWidget_Lambda([](const TSharedPtr<int32>& Item)
		{
			if (*Item == -1)
			{
				return SNew(STextBlock).Text(INVTEXT("latest"));
			}

			return SNew(STextBlock).Text(FText::FromString(FString::FromInt(*Item)));
		})
		.OnSelectionChanged_Lambda([](const TSharedPtr<int32>& Item, ESelectInfo::Type)
		{
			GVoxelAuthApi->SelectedPluginCounter = *Item;
		})
		[
			SNew(STextBlock)
			.Text_Lambda([]
			{
				if (GVoxelAuthApi->SelectedPluginCounter == -1)
				{
					return INVTEXT("latest");
				}

				return FText::FromString(FString::FromInt(GVoxelAuthApi->SelectedPluginCounter));
			})
		];

	const TSharedRef<SVerticalBox> DownloadVBox =
		SNew(SVerticalBox)
		.Visibility_Lambda([]
		{
			return GVoxelAuthApi->IsPro() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(INVTEXT("Installed Branch"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(GVoxelAuthApi->GetPluginBranch().IsEmpty() ? "Unknown" : GVoxelAuthApi->GetPluginBranch()))
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
				.Text(INVTEXT("Installed Version"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::FromInt(GVoxelAuthApi->GetPluginCounter()) + IsLatest))
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
				.Text(INVTEXT("Branch"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				BranchComboBox
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
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.IsEnabled_Lambda([]
				{
					const TArray<int32>* Versions = GVoxelAuthApi->GetVersions().Find(GVoxelAuthApi->SelectedPluginBranch);
					if (!Versions)
					{
						return false;
					}

					if (GVoxelAuthApi->SelectedPluginCounter == -1)
					{
						if (GVoxelAuthApi->SelectedPluginBranch == GVoxelAuthApi->GetPluginBranch() &&
							Versions->Last() == GVoxelAuthApi->GetPluginCounter())
						{
							return false;
						}

						return true;
					}
					else
					{
						if (GVoxelAuthApi->SelectedPluginBranch == GVoxelAuthApi->GetPluginBranch() &&
							GVoxelAuthApi->SelectedPluginCounter == GVoxelAuthApi->GetPluginCounter())
						{
							return false;
						}

						return Versions->Contains(GVoxelAuthApi->SelectedPluginCounter);
					}
				})
				.OnClicked_Lambda([=]
				{
					const TArray<int32>* Versions = GVoxelAuthApi->GetVersions().Find(GVoxelAuthApi->SelectedPluginBranch);
					if (!Versions)
					{
						return FReply::Handled();
					}

					if (GVoxelAuthApi->SelectedPluginCounter == -1)
					{
						GVoxelAuthDownload->Download(GVoxelAuthApi->SelectedPluginBranch, Versions->Last());
					}
					else
					{
						GVoxelAuthDownload->Download(GVoxelAuthApi->SelectedPluginBranch, (*Versions)[GVoxelAuthApi->SelectedPluginCounter]);
					}

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
			DownloadVBox
		];;

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
						if (GVoxelAuth->GetState() == EVoxelAuthState::LoggedOut)
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
						.ButtonStyle(FEditorAppStyle::Get(), "HoverHintOnly")
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
						.ButtonStyle(FEditorAppStyle::Get(), "HoverHintOnly")
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
						.ButtonStyle(FEditorAppStyle::Get(), "HoverHintOnly")
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
		]
	];
}
#endif