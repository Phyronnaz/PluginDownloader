// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"
#include "CoreGlobals.h"
#include "UnrealEdMisc.h"
#include "EditorStyleSet.h"
#include "HAL/FileManager.h"
#include "IUATHelperModule.h"
#include "UObject/UnrealType.h"
#include "PropertyEditorModule.h"
#include "Launch/Resources/Version.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Misc/Base64.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/ConfigCacheIni.h"

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IPluginManager.h"

#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "IDetailsView.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailCustomization.h"
#include "Styling/SlateStyle.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SEditableTextBox.h"

#include "PluginDownloaderDefinitions.h"

#if ENGINE_VERSION >= 500
#include "Styling/StyleColors.h"
#endif