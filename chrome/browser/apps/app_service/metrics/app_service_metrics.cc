// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "extensions/common/constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

void RecordDefaultAppLaunch(apps::DefaultAppName default_app_name,
                            apps::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::LaunchSource::kUnknown:
    case apps::LaunchSource::kFromParentalControls:
    case apps::LaunchSource::kFromTest:
      return;
    case apps::LaunchSource::kFromAppListGrid:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromAppListGrid",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromAppListGridContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListGridContextMenu", default_app_name);
      break;
    case apps::LaunchSource::kFromAppListQuery:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromAppListQuery",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromAppListQueryContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListQueryContextMenu",
          default_app_name);
      break;
    case apps::LaunchSource::kFromAppListRecommendation:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListRecommendation", default_app_name);
      break;
    case apps::LaunchSource::kFromShelf:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromShelf",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromFileManager:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFileManager",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromLink:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromLink",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromOmnibox:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOmnibox",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromChromeInternal:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromChromeInternal",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromKeyboard:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromKeyboard",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromOtherApp:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOtherApp",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromMenu:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromMenu",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromInstalledNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromInstalledNotification", default_app_name);
      break;
    case apps::LaunchSource::kFromArc:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromArc",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromSharesheet:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromSharesheet",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromReleaseNotesNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromReleaseNotesNotification",
          default_app_name);
      break;
    case apps::LaunchSource::kFromFullRestore:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFullRestore",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromSmartTextContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromSmartTextContextMenu", default_app_name);
      break;
    case apps::LaunchSource::kFromDiscoverTabNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromDiscoverTabNotification",
          default_app_name);
      break;
    case apps::LaunchSource::kFromManagementApi:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromManagementApi",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromKiosk:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromKiosk",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromNewTabPage:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromNewTabPage",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromIntentUrl:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromIntentUrl",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromOsLogin:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOsLogin",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromProtocolHandler:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromProtocolHandler",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromUrlHandler:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromUrlHandler",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromLockScreen:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromLockScreen",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromSysTrayCalendar:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromSysTrayCalendar",
                                    default_app_name);
      break;
    case apps::LaunchSource::kFromCommandLine:
    case apps::LaunchSource::kFromBackgroundMode:
    case apps::LaunchSource::kFromAppHomePage:
    case apps::LaunchSource::kFromReparenting:
    case apps::LaunchSource::kFromProfileMenu:
      NOTREACHED();
      break;
  }
}

}  // namespace

namespace apps {

void RecordAppLaunch(const std::string& app_id,
                     apps::LaunchSource launch_source) {
  if (const absl::optional<apps::DefaultAppName> app_name =
          PreinstalledWebAppIdToName(app_id)) {
    RecordDefaultAppLaunch(app_name.value(), launch_source);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (const absl::optional<apps::DefaultAppName> app_name =
          SystemWebAppIdToName(app_id)) {
    RecordDefaultAppLaunch(app_name.value(), launch_source);
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (app_id == extension_misc::kCalculatorAppId) {
    // Launches of the legacy calculator chrome app.
    RecordDefaultAppLaunch(DefaultAppName::kCalculatorChromeApp, launch_source);
  } else if (app_id == extension_misc::kTextEditorAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kText, launch_source);
  } else if (app_id == app_constants::kChromeAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kChrome, launch_source);
  } else if (app_id == extension_misc::kGoogleDocsAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kDocs, launch_source);
  } else if (app_id == extension_misc::kGoogleDriveAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kDrive, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == arc::kGoogleDuoAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kDuo, launch_source);
  } else if (app_id == extension_misc::kFilesManagerAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kFiles, launch_source);
  } else if (app_id == extension_misc::kGmailAppId ||
             app_id == arc::kGmailAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kGmail, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kGoogleKeepAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kKeep, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kGooglePhotosAppId ||
             app_id == arc::kGooglePhotosAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPhotos, launch_source);
  } else if (app_id == arc::kPlayBooksAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayBooks, launch_source);
  } else if (app_id == arc::kPlayGamesAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayGames, launch_source);
  } else if (app_id == arc::kPlayMoviesAppId ||
             app_id == extension_misc::kGooglePlayMoviesAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayMovies, launch_source);
  } else if (app_id == arc::kPlayMusicAppId ||
             app_id == extension_misc::kGooglePlayMusicAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayMusic, launch_source);
  } else if (app_id == arc::kPlayStoreAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayStore, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kGoogleSheetsAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kSheets, launch_source);
  } else if (app_id == extension_misc::kGoogleSlidesAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kSlides, launch_source);
  } else if (app_id == extensions::kWebStoreAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kWebStore, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kYoutubeAppId ||
             app_id == arc::kYoutubeAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kYouTube, launch_source);
  } else if (app_id == arc::kGoogleTVAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kGoogleTv, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void RecordBuiltInAppSearchResult(const std::string& app_id) {
  if (app_id == ash::kInternalAppIdKeyboardShortcutViewer) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kKeyboardShortcutViewer);
  } else if (app_id == ash::kInternalAppIdSettings) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kSettings);
  } else if (app_id == ash::kInternalAppIdContinueReading) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kContinueReading);
  } else if (app_id == plugin_vm::kPluginVmShelfAppId) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kPluginVm);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void RecordAppsPerNotification(int count) {
  if (count <= 0) {
    return;
  }
  base::UmaHistogramBoolean("ChromeOS.Apps.NumberOfAppsForNotification",
                            (count > 1));
}

const absl::optional<apps::DefaultAppName> PreinstalledWebAppIdToName(
    const std::string& app_id) {
  if (app_id == web_app::kCalculatorAppId) {
    return apps::DefaultAppName::kCalculator;
  } else if (app_id == web_app::kCanvasAppId) {
    return apps::DefaultAppName::kChromeCanvas;
  } else if (app_id == web_app::kCursiveAppId) {
    return apps::DefaultAppName::kCursive;
  } else if (app_id == web_app::kGmailAppId) {
    return apps::DefaultAppName::kGmail;
  } else if (app_id == web_app::kGoogleMoviesAppId) {
    return apps::DefaultAppName::kPlayMovies;
  } else if (app_id == web_app::kGoogleCalendarAppId) {
    return apps::DefaultAppName::kGoogleCalendar;
  } else if (app_id == web_app::kGoogleChatAppId) {
    return apps::DefaultAppName::kGoogleChat;
  } else if (app_id == web_app::kGoogleDocsAppId) {
    return apps::DefaultAppName::kDocs;
  } else if (app_id == web_app::kGoogleDriveAppId) {
    return apps::DefaultAppName::kDrive;
  } else if (app_id == web_app::kGoogleMeetAppId) {
    return apps::DefaultAppName::kGoogleMeet;
  } else if (app_id == web_app::kGoogleSheetsAppId) {
    return apps::DefaultAppName::kSheets;
  } else if (app_id == web_app::kGoogleSlidesAppId) {
    return apps::DefaultAppName::kSlides;
  } else if (app_id == web_app::kGoogleKeepAppId) {
    return apps::DefaultAppName::kKeep;
  } else if (app_id == web_app::kGoogleMapsAppId) {
    return apps::DefaultAppName::kGoogleMaps;
  } else if (app_id == web_app::kMessagesAppId) {
    return apps::DefaultAppName::kGoogleMessages;
  } else if (app_id == web_app::kPlayBooksAppId) {
    return apps::DefaultAppName::kPlayBooks;
  } else if (app_id == web_app::kYoutubeAppId) {
    return apps::DefaultAppName::kYouTube;
  } else if (app_id == web_app::kYoutubeMusicAppId) {
    return apps::DefaultAppName::kYouTubeMusic;
  } else {
    return absl::nullopt;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
const absl::optional<apps::DefaultAppName> SystemWebAppIdToName(
    const std::string& app_id) {
  // These apps should all have chrome:// URLs.
  if (app_id == web_app::kCameraAppId) {
    return apps::DefaultAppName::kCamera;
  } else if (app_id == web_app::kDiagnosticsAppId) {
    return apps::DefaultAppName::kDiagnosticsApp;
  } else if (app_id == file_manager::kFileManagerSwaAppId) {
    return apps::DefaultAppName::kFiles;
  } else if (app_id == web_app::kFirmwareUpdateAppId) {
    return apps::DefaultAppName::kFirmwareUpdateApp;
  } else if (app_id == web_app::kHelpAppId) {
    return apps::DefaultAppName::kHelpApp;
  } else if (app_id == web_app::kMediaAppId) {
    return apps::DefaultAppName::kMediaApp;
    // `MockSystemApp` is for tests only.
  } else if (app_id == web_app::kMockSystemAppId) {
    return apps::DefaultAppName::kMockSystemApp;
  } else if (app_id == web_app::kOsFeedbackAppId) {
    return apps::DefaultAppName::kOsFeedbackApp;
  } else if (app_id == web_app::kOsSettingsAppId) {
    return apps::DefaultAppName::kSettings;
  } else if (app_id == web_app::kPrintManagementAppId) {
    return apps::DefaultAppName::kPrintManagementApp;
  } else if (app_id == ash::kChromeUITrustedProjectorSwaAppId) {
    return apps::DefaultAppName::kProjector;
  } else if (app_id == web_app::kScanningAppId) {
    return apps::DefaultAppName::kScanningApp;
  } else if (app_id == web_app::kShimlessRMAAppId) {
    return apps::DefaultAppName::kShimlessRMAApp;
  } else if (app_id == web_app::kShortcutCustomizationAppId) {
    return apps::DefaultAppName::kShortcutCustomizationApp;
  } else {
    return absl::nullopt;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
