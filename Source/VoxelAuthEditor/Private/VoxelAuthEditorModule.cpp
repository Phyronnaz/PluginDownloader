// Copyright Voxel Plugin, Inc. All Rights Reserved.

#if ENGINE_VERSION >= 500
#include "VoxelMinimal.h"
#include "ToolMenus.h"
#include "VoxelAuth.h"
#include "VoxelAuthApi.h"
#include "SVoxelAuthWidget.h"
#include "VoxelAuthDownload.h"
#include "PluginDownloaderSettings.h"
#include "PluginDownloaderUtilities.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

TSharedRef<SWidget> GenerateVoxelMenuWidget()
{
	return SNew(SVoxelAuthWidget);
}

FSlateStyleSet* GVoxelAuthStyle = nullptr;

class FVoxelAuthEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GVoxelAuthStyle = new FSlateStyleSet("VoxelAuthStyle");
		GVoxelAuthStyle->SetContentRoot(IPluginManager::Get().FindPlugin("PluginDownloader")->GetBaseDir() / TEXT("Resources"));

#define ICON(Name, Size) GVoxelAuthStyle->Set(Name, new FSlateImageBrush(GVoxelAuthStyle->RootToContentDir(TEXT(Name), TEXT(".png")), FVector2D(Size)));
		ICON("DiscordIcon", 64.f);
		ICON("EpicIcon", 32.f);
		ICON("EmailIcon", 32.f);
		ICON("VoxelIcon", 16.f);
		ICON("VoxelIconWithUpdate", 16.f);
#undef ICON

#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(FPaths::EngineContentDir() / "Editor" / "Slate" / RelativePath + ".png", __VA_ARGS__)

		GVoxelAuthStyle->Set("EpicButton", FButtonStyle()
			.SetNormal(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, FLinearColor(0.007843f, 0.007843f, 0.007843f)))
			.SetHovered(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, FLinearColor(0.035601f, 0.035601f, 0.035601f)))
			.SetPressed(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, FLinearColor(0.066626f, 0.066626f, 0.066626f)))
			.SetNormalPadding(FMargin(2, 2, 2, 2))
			.SetPressedPadding(FMargin(2, 3, 2, 1))
			.SetNormalForeground(FLinearColor::White)
			.SetPressedForeground(FLinearColor::White)
			.SetHoveredForeground(FLinearColor::White)
			.SetDisabledForeground(FLinearColor::White));

#undef BOX_BRUSH

		///////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////

		FSlateStyleRegistry::RegisterSlateStyle(*GVoxelAuthStyle);

		GVoxelAuth = new FVoxelAuth();
		GVoxelAuthApi = new FVoxelAuthApi();
		GVoxelAuthDownload = new FVoxelAuthDownload();
		
		UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(
			"LevelEditor.LevelEditorToolBar.SettingsToolbar", 
			NAME_None, 
			EMultiBoxType::SlimHorizontalToolBar, 
			false);

		FToolMenuSection& Section = ToolBar->FindOrAddSection("ProjectSettings");

		FPluginDownloaderUtilities::DelayedCall([]
		{
			if (GetDefault<UPluginDownloaderSettings>()->bShowVoxelPluginMenu)
			{
				GVoxelAuthApi->Initialize();
			}
		});

		Section.AddDynamicEntry("VoxelPluginMenu", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			if (!GetDefault<UPluginDownloaderSettings>()->bShowVoxelPluginMenu)
			{
				return;
			}

			InSection.AddEntry(FToolMenuEntry::InitComboButton(
				"Voxel",
				FUIAction(),
				FOnGetContent::CreateStatic(&GenerateVoxelMenuWidget),
				INVTEXT("Voxel"),
				INVTEXT("Voxel Plugin configuration"),
				MakeAttributeLambda([]
				{
					switch (GVoxelAuthApi->GetPluginState())
					{
					default: check(false);
					case FVoxelAuthApi::EState::NoUpdate:
					case FVoxelAuthApi::EState::NotInstalled:
					{
						return FSlateIcon("VoxelAuthStyle", "VoxelIcon");
					}
					case FVoxelAuthApi::EState::HasUpdate:
					{
						return FSlateIcon("VoxelAuthStyle", "VoxelIconWithUpdate");
					}
					}
				})
			));
		}));
	}
};

IMPLEMENT_MODULE(FVoxelAuthEditorModule, VoxelAuthEditor);
#else
IMPLEMENT_MODULE(FDefaultModuleImpl, VoxelAuthEditor);
#endif