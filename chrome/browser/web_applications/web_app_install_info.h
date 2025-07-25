// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_INFO_H_

#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
              BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA));

namespace web_app {

// A map of icon urls to the bitmaps provided by that url.
using IconsMap = std::map<GURL, std::vector<SkBitmap>>;

// A map of icon urls to http status results. `http_status_code` is never 0.
using DownloadedIconsHttpResults = std::map<GURL, int /*http_status_code*/>;

using SquareSizePx = int;
// Iterates in ascending order (checked in SortedSizesPxIsAscending test).
using SortedSizesPx = base::flat_set<SquareSizePx, std::less<>>;
using IconPurpose = blink::mojom::ManifestImageResource_Purpose;
constexpr std::array<IconPurpose,
                     static_cast<int>(IconPurpose::kMaxValue) -
                         static_cast<int>(IconPurpose::kMinValue) + 1>
    kIconPurposes{IconPurpose::ANY, IconPurpose::MONOCHROME,
                  IconPurpose::MASKABLE};

apps::IconInfo::Purpose ManifestPurposeToIconInfoPurpose(
    IconPurpose manifest_purpose);

// Icon bitmaps for each IconPurpose.
struct IconBitmaps {
  IconBitmaps();
  ~IconBitmaps();
  IconBitmaps(const IconBitmaps&);
  IconBitmaps(IconBitmaps&&) noexcept;
  IconBitmaps& operator=(const IconBitmaps&);
  IconBitmaps& operator=(IconBitmaps&&) noexcept;

  bool operator==(const IconBitmaps&) const;

  const std::map<SquareSizePx, SkBitmap>& GetBitmapsForPurpose(
      IconPurpose purpose) const;
  void SetBitmapsForPurpose(IconPurpose purpose,
                            std::map<SquareSizePx, SkBitmap> bitmaps);

  bool empty() const;

  // TODO(crbug.com/1152661): Consider using base::flat_map.

  // Icon bitmaps suitable for any context, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#dfn-any-purpose
  std::map<SquareSizePx, SkBitmap> any;

  // Icon bitmaps designed for masking, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#dfn-maskable-purpose
  std::map<SquareSizePx, SkBitmap> maskable;

  // Monochrome bitmaps designed for any context, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#purpose-member
  std::map<SquareSizePx, SkBitmap> monochrome;
};

// Icon sizes for each IconPurpose.
struct IconSizes {
  IconSizes();
  ~IconSizes();
  IconSizes(const IconSizes&);
  IconSizes(IconSizes&&) noexcept;
  IconSizes& operator=(const IconSizes&);
  IconSizes& operator=(IconSizes&&) noexcept;
  base::Value AsDebugValue() const;

  const std::vector<SquareSizePx>& GetSizesForPurpose(
      IconPurpose purpose) const;
  void SetSizesForPurpose(IconPurpose purpose, std::vector<SquareSizePx> sizes);

  bool empty() const;

  // Sizes of icon bitmaps suitable for any context.
  // See https://www.w3.org/TR/appmanifest/#dfn-any-purpose
  std::vector<SquareSizePx> any;

  // Sizes of icon bitmaps designed for masking.
  // See https://www.w3.org/TR/appmanifest/#dfn-maskable-purpose
  std::vector<SquareSizePx> maskable;

  // Sizes of monochrome bitmaps, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#purpose-member
  std::vector<SquareSizePx> monochrome;
};

using ShortcutsMenuIconBitmaps = std::vector<IconBitmaps>;

// Structure used when creating app icon shortcuts menu and for downloading
// associated shortcut icons when supported by OS platform (eg. Windows).
struct WebAppShortcutsMenuItemInfo {
  struct Icon {
    Icon();
    Icon(const Icon&);
    Icon(Icon&&) noexcept;
    ~Icon();
    Icon& operator=(const Icon&);
    Icon& operator=(Icon&&);
    base::Value AsDebugValue() const;

    GURL url;
    SquareSizePx square_size_px = 0;
  };

  WebAppShortcutsMenuItemInfo();
  WebAppShortcutsMenuItemInfo(const WebAppShortcutsMenuItemInfo&);
  WebAppShortcutsMenuItemInfo(WebAppShortcutsMenuItemInfo&&) noexcept;
  ~WebAppShortcutsMenuItemInfo();
  WebAppShortcutsMenuItemInfo& operator=(const WebAppShortcutsMenuItemInfo&);
  WebAppShortcutsMenuItemInfo& operator=(
      WebAppShortcutsMenuItemInfo&&) noexcept;

  const std::vector<Icon>& GetShortcutIconInfosForPurpose(
      IconPurpose purpose) const;
  void SetShortcutIconInfosForPurpose(
      IconPurpose purpose,
      std::vector<Icon> shortcut_manifest_icons);

  base::Value AsDebugValue() const;

  // Title of shortcut item in App Icon Shortcut Menu.
  std::u16string name;

  // URL launched when shortcut item is selected.
  GURL url;

  // List of shortcut icon URLs with associated square size,
  // suitable for any context.
  // See https://www.w3.org/TR/appmanifest/#dfn-any-purpose
  std::vector<Icon> any;

  // List of shortcut icon URLs with associated square size,
  // designed for masking.
  // See https://www.w3.org/TR/appmanifest/#dfn-maskable-purpose
  std::vector<Icon> maskable;

  // List of shortcut icon URLs with associated square size,
  // designed for monochrome contexts.
  // See https://www.w3.org/TR/appmanifest/#purpose-member
  std::vector<Icon> monochrome;

  // Sizes of successfully downloaded icons for this shortcut menu item.
  IconSizes downloaded_icon_sizes{};
};

// Structure used when installing a web page as an app.
struct WebAppInstallInfo {
  enum MobileCapable {
    MOBILE_CAPABLE_UNSPECIFIED,
    MOBILE_CAPABLE,
    MOBILE_CAPABLE_APPLE
  };

  // Returns a copy of `other` retaining only the fields that are needed for
  // a shortcut (e.g icons), and using the document title and URL instead of
  // manifest properties. This will strip out app-like fields (e.g. file
  // handlers).
  static WebAppInstallInfo CreateInstallInfoForCreateShortcut(
      const GURL& document_url,
      const std::u16string& document_title,
      const WebAppInstallInfo& other);

  // This creates a WebAppInstallInfo where the `manifest_id` is derived from
  // the `start_url` using `GenerateManifestIdFromStartUrlOnly`.
  static std::unique_ptr<WebAppInstallInfo> CreateWithStartUrlForTesting(
      const GURL& start_url);

  // TODO(b/280862254): Remove this constructor to force users to use specify
  // the manifest_id and start_url (or call `CreateWithStartUrlForTesting`).
  WebAppInstallInfo();

  // TODO(b/280862254): Remove this constructor to force users to use specify
  // both the manifest_id and start_url (or call
  // `CreateWithStartUrlForTesting`).
  explicit WebAppInstallInfo(const webapps::ManifestId& manifest_id);

  // The `manifest_id` and the `start_url` MUST be valid. The `manifest_id` MUST
  // be created properly, and cannot contain refs (e.g. '#refs').
  WebAppInstallInfo(const webapps::ManifestId& manifest_id,
                    const GURL& start_url);

  // Deleted to prevent accidental copying. Use Clone() to deep copy explicitly.
  WebAppInstallInfo& operator=(const WebAppInstallInfo&) = delete;

  WebAppInstallInfo(WebAppInstallInfo&&);
  WebAppInstallInfo& operator=(WebAppInstallInfo&&);
  ~WebAppInstallInfo();

  // Creates a deep copy of this struct.
  WebAppInstallInfo Clone() const;

  // Id specified in the manifest.
  // TODO(b/280862254): After the manifest id constructor is required, this can
  // be guaranteed to be valid & non-empty.
  // https://www.w3.org/TR/appmanifest/#id-member
  webapps::ManifestId manifest_id;

  // Title of the application.
  std::u16string title;

  // Description of the application.
  std::u16string description;

  // The URL the site would prefer the user agent load when launching the app.
  // https://www.w3.org/TR/appmanifest/#start_url-member
  GURL start_url;

  // The URL of the manifest.
  // https://www.w3.org/TR/appmanifest/#web-application-manifest
  GURL manifest_url;

  // Optional query parameters to add to the start_url when launching the app.
  absl::optional<std::string> launch_query_params;

  // Scope for the app. Dictates what URLs will be opened in the app.
  // https://www.w3.org/TR/appmanifest/#scope-member
  GURL scope;

  // List of icon URLs with associated square size and purpose. The size
  // corresponds to what was specified in the manifest rather than any actual
  // size of a downloaded icon.
  std::vector<apps::IconInfo> manifest_icons;

  // Icon bitmaps, keyed by their square size.
  IconBitmaps icon_bitmaps;

  // A collection of unprocessed icons keyed by their download URL. The usage
  // and purpose of these icons is tracked elsewhere, such as in
  // `file_handlers`. Currently, this is only used for file handling icons, but
  // other icons may be added here in the future.
  IconsMap other_icon_bitmaps;

  // Represents whether the icons for the web app are generated by Chrome due to
  // no suitable icons being available.
  bool is_generated_icon = false;

  // Whether the page is marked as mobile-capable, including apple specific meta
  // tag.
  MobileCapable mobile_capable = MOBILE_CAPABLE_UNSPECIFIED;

  // The color to use for the web app frame.
  absl::optional<SkColor> theme_color;

  // The color to use for the web app frame when
  // launched in dark mode. This doesn't yet have manifest support.
  absl::optional<SkColor> dark_mode_theme_color;

  // The expected page background color of the web app.
  // https://www.w3.org/TR/appmanifest/#background_color-member
  absl::optional<SkColor> background_color;

  // The color to use for the background when
  // launched in dark mode. This doesn't yet have manifest support.
  absl::optional<SkColor> dark_mode_background_color;

  // App preference regarding whether the app should be opened in a tab,
  // in a window (with or without minimal-ui buttons), or full screen. Defaults
  // to browser display mode as specified in
  // https://w3c.github.io/manifest/#display-modes
  blink::mojom::DisplayMode display_mode = blink::mojom::DisplayMode::kBrowser;

  // App preference to control display fallback ordering
  std::vector<blink::mojom::DisplayMode> display_override;

  // User preference for whether the app should be opened as a tab or in an app
  // window. Must be either kBrowser or kStandalone, this will be checked by
  // WebApp::SetUserDisplayMode().
  absl::optional<web_app::mojom::UserDisplayMode> user_display_mode =
      web_app::mojom::UserDisplayMode::kBrowser;

  // The extensions and mime types the app can handle.
  apps::FileHandlers file_handlers;

  // File types the app accepts as a Web Share Target.
  absl::optional<apps::ShareTarget> share_target;

  // Additional search terms that users can use to find the app.
  std::vector<std::string> additional_search_terms;

  // Set of shortcuts menu item infos populated using shortcuts specified in the
  // manifest.
  std::vector<WebAppShortcutsMenuItemInfo> shortcuts_menu_item_infos;

  // Vector of shortcut icon bitmaps keyed by their square size. The index of a
  // given |IconBitmaps| matches that of the shortcut in
  // |shortcuts_menu_item_infos| whose bitmaps it contains.
  // Notes: It is not guaranteed that these are populated if the menu items are.
  // See https://crbug.com/1427444.
  ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps;

  // The URL protocols/schemes that the app can handle.
  std::vector<apps::ProtocolHandlerInfo> protocol_handlers;

  // The app intends to act as a URL handler for URLs described by this
  // information.
  apps::UrlHandlers url_handlers;

  // The app intends to have an extended scope containing URLs described by this
  // information.
  base::flat_set<web_app::ScopeExtensionInfo> scope_extensions;

  // `scope_extensions` after going through validation with associated origins.
  // Only entries that have been validated by the corresponding origins remain.
  // See
  // https://github.com/WICG/manifest-incubations/blob/gh-pages/scope_extensions-explainer.md
  // for association requirements.
  absl::optional<base::flat_set<web_app::ScopeExtensionInfo>>
      validated_scope_extensions;

  // URL within scope to launch on the lock screen for a "show on lock screen"
  // action. Valid iff this is considered a lock-screen-capable app.
  GURL lock_screen_start_url;

  // URL within scope to launch for a "new note" action. Valid iff this is
  // considered a note-taking app.
  GURL note_taking_new_note_url;

  // The link capturing behaviour to use for navigations into in the app's
  // scope.
  blink::mojom::CaptureLinks capture_links =
      blink::mojom::CaptureLinks::kUndefined;

  // The window selection behaviour of app launches.
  absl::optional<blink::Manifest::LaunchHandler> launch_handler;

  // A mapping from locales to translated fields.
  base::flat_map<std::string, blink::Manifest::TranslationItem> translations;

  // The declared permissions policy to apply as the baseline policy for all
  // documents belonging to the application.
  blink::ParsedPermissionsPolicy permissions_policy;

  // See ExternallyManagedAppManager for placeholder app documentation.
  // Intended to be a temporary app while we wait for the install_url to
  // successfully load.
  bool is_placeholder = false;

  // The install URL for the app. This does not always need to be
  // populated (especially for user installed or sync installed apps)
  // in which case the URL will not be written to the web_app DB.
  GURL install_url;

  // Customisations to the tab strip. This field is only used when the
  // display mode is set to 'tabbed'.
  absl::optional<blink::Manifest::TabStrip> tab_strip;

  // Id of the app that called the SUB_APP API to install this app. This field
  // is only used when the app is installed as a sub app through the SUB_APP
  // API.
  absl::optional<webapps::AppId> parent_app_id;

  // ManifestId of the app that called the SUB_APP API to install this app. This
  // field is only used when the app is installed as a sub app through the
  // SUB_APP API.
  absl::optional<webapps::ManifestId> parent_app_manifest_id;

  // A list of additional terms to use when matching this app against
  // identifiers in admin policies (for shelf pinning, default file handlers,
  // etc).
  // Note that list is not meant to be an exhaustive enumeration of all possible
  // policy_ids but rather just a supplement for tricky cases.
  std::vector<std::string> additional_policy_ids;

  // Used to specify the version of an Isolated Web App that is being installed.
  base::Version isolated_web_app_version;

  // Bookkeeping details about attempts to fix broken icons from sync installed
  // web apps.
  absl::optional<GeneratedIconFix> generated_icon_fix;

 private:
  // Used this method in Clone() method. Use Clone() to deep copy explicitly.
  WebAppInstallInfo(const WebAppInstallInfo& other);
};

bool operator==(const IconSizes& icon_sizes1, const IconSizes& icon_sizes2);

bool operator==(const WebAppShortcutsMenuItemInfo::Icon& icon1,
                const WebAppShortcutsMenuItemInfo::Icon& icon2);

bool operator==(const WebAppShortcutsMenuItemInfo& shortcut_info1,
                const WebAppShortcutsMenuItemInfo& shortcut_info2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_INFO_H_
