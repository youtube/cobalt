// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_utils.h"

#include <array>
#include <iterator>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_map.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/policy/pre_redirection_url_observer.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/http/http_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#endif

namespace web_app {

class WebAppRegistrar;

namespace {

// We restrict the number of icons to limit disk usage per installed PWA. This
// value can change overtime as new features are added.
constexpr int kMaxIcons = 20;
constexpr SquareSizePx kMaxIconSize =
    webapps::InstallableEvaluator::kMaximumIconSizeInPx;

// Returns whether the home tab icons exist.
bool HomeTabIconsExistInTabStrip(const WebAppInstallInfo* web_app_info) {
  if (!web_app_info->tab_strip.has_value()) {
    return false;
  }

  if (!absl::holds_alternative<blink::Manifest::HomeTabParams>(
          web_app_info->tab_strip.value().home_tab)) {
    return false;
  }

  const auto& home_tab = absl::get<blink::Manifest::HomeTabParams>(
      web_app_info->tab_strip.value().home_tab);

  if (home_tab.icons.empty()) {
    return false;
  }

  return true;
}

// Append non-empty square icons from |icons_map| onto the |square_icons| list.
void AddSquareIconsFromMap(std::vector<SkBitmap>* square_icons,
                           const IconsMap& icons_map) {
  for (const auto& url_icon : icons_map) {
    for (const SkBitmap& icon : url_icon.second) {
      if (!icon.empty() && icon.width() == icon.height())
        square_icons->push_back(icon);
    }
  }
}

// Append non-empty square icons from |icons_map| onto the |square_icons| list,
// if they are also in |manifest_icons|.
void AddSquareIconsFromMapMatchingIconInfos(
    std::vector<SkBitmap>* square_icons,
    const std::vector<apps::IconInfo>& manifest_icons,
    const IconsMap& icons_map) {
  for (const auto& url_icon : icons_map) {
    for (const SkBitmap& icon : url_icon.second) {
      if (!icon.empty() && icon.width() == icon.height()) {
        for (const auto& info : manifest_icons) {
          if (info.url == url_icon.first) {
            square_icons->push_back(icon);
          }
        }
      }
    }
  }
}

// Append non-empty square icons from |bitmaps| onto the |square_icons| list.
void AddSquareIconsFromBitmaps(
    std::vector<SkBitmap>* square_icons,
    const std::map<SquareSizePx, SkBitmap>& bitmaps) {
  for (const std::pair<const SquareSizePx, SkBitmap>& icon : bitmaps) {
    DCHECK_EQ(icon.first, icon.second.width());
    DCHECK_EQ(icon.first, icon.second.height());
    if (!icon.second.empty())
      square_icons->push_back(icon.second);
  }
}

// Populate |web_app_info|'s shortcuts_menu_item_infos vector using the
// blink::Manifest's shortcuts vector.
std::vector<WebAppShortcutsMenuItemInfo> ToWebAppShortcutsMenuItemInfos(
    const std::vector<blink::Manifest::ShortcutItem>& shortcuts) {
  std::vector<WebAppShortcutsMenuItemInfo> web_app_shortcut_infos;
  web_app_shortcut_infos.reserve(shortcuts.size());
  int num_shortcut_icons = 0;
  for (const auto& shortcut : shortcuts) {
    if (web_app_shortcut_infos.size() >= kMaxApplicationDockMenuItems) {
      DLOG(ERROR) << "Too many shortcuts";
      break;
    }

    WebAppShortcutsMenuItemInfo shortcut_info;
    shortcut_info.name = shortcut.name;
    shortcut_info.url = shortcut.url;

    for (IconPurpose purpose : kIconPurposes) {
      std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_icons;
      for (const auto& icon : shortcut.icons) {
        DCHECK(!icon.purpose.empty());
        if (!base::Contains(icon.purpose, purpose))
          continue;

        WebAppShortcutsMenuItemInfo::Icon info;

        // Filter out non-square or too large icons.
        auto valid_size_it =
            base::ranges::find_if(icon.sizes, [](const gfx::Size& size) {
              return size.width() == size.height() &&
                     size.width() <= kMaxIconSize;
            });
        if (valid_size_it == icon.sizes.end())
          continue;
        // TODO(https://crbug.com/1071308): Take the declared icon density and
        // sizes into account.
        info.square_size_px = valid_size_it->width();

        DCHECK_LE(num_shortcut_icons, kMaxIcons);
        if (num_shortcut_icons < kMaxIcons) {
          info.url = icon.src;
          shortcut_icons.push_back(std::move(info));
          ++num_shortcut_icons;
        }
        if (num_shortcut_icons == kMaxIcons)
          break;
      }

      // If any icons are specified in the manifest, they take precedence over
      // any we picked up from web_app_info.
      if (!shortcut_icons.empty())
        shortcut_info.SetShortcutIconInfosForPurpose(purpose,
                                                     std::move(shortcut_icons));
    }
    web_app_shortcut_infos.push_back(std::move(shortcut_info));
  }

  return web_app_shortcut_infos;
}

std::vector<SquareSizePx> GetSquareSizePxs(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps) {
  std::vector<SquareSizePx> sizes;
  sizes.reserve(icon_bitmaps.size());
  for (const std::pair<const SquareSizePx, SkBitmap>& item : icon_bitmaps)
    sizes.push_back(item.first);
  return sizes;
}

std::vector<SquareSizePx> GetSquareSizePxs(const IconBitmaps& icon_bitmaps,
                                           IconPurpose purpose) {
  switch (purpose) {
    case IconPurpose::ANY:
      return GetSquareSizePxs(icon_bitmaps.any);
    case IconPurpose::MASKABLE:
      return GetSquareSizePxs(icon_bitmaps.maskable);
    case IconPurpose::MONOCHROME:
      return GetSquareSizePxs(icon_bitmaps.monochrome);
  }
}

// Returns a new vector of item infos with `downloaded_icon_sizes` set from
// `shortcuts_menu_icon_bitmaps` and other info copied from
// `shortcuts_menu_items`.
std::vector<WebAppShortcutsMenuItemInfo> GetShortcutsMenuInfoWithIconSizes(
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_items,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps) {
  // Due to the bitmaps possibly being not populated (see
  // https://crbug.com/1427444), we create empty bitmaps in that case. We
  // continue to check to make sure that there aren't MORE bitmaps than
  // items.
  CHECK_LE(shortcuts_menu_icon_bitmaps.size(), shortcuts_menu_items.size());
  std::vector<WebAppShortcutsMenuItemInfo> items_with_sizes;
  items_with_sizes.reserve(shortcuts_menu_items.size());
  IconBitmaps empty_icon_bitmaps;
  for (size_t i = 0; i < shortcuts_menu_items.size(); ++i) {
    const IconBitmaps* shortcut_icon_bitmaps;
    if (i < shortcuts_menu_icon_bitmaps.size()) {
      shortcut_icon_bitmaps = &shortcuts_menu_icon_bitmaps[i];
    } else {
      shortcut_icon_bitmaps = &empty_icon_bitmaps;
    }

    WebAppShortcutsMenuItemInfo item_info = shortcuts_menu_items[i];
    for (IconPurpose purpose : kIconPurposes) {
      item_info.downloaded_icon_sizes.SetSizesForPurpose(
          purpose, GetSquareSizePxs(*shortcut_icon_bitmaps, purpose));
    }

    items_with_sizes.push_back(std::move(item_info));
  }
  return items_with_sizes;
}

apps::ShareTarget::Method ToAppsShareTargetMethod(
    blink::mojom::ManifestShareTarget_Method method) {
  switch (method) {
    case blink::mojom::ManifestShareTarget_Method::kGet:
      return apps::ShareTarget::Method::kGet;
    case blink::mojom::ManifestShareTarget_Method::kPost:
      return apps::ShareTarget::Method::kPost;
  }
  NOTREACHED();
}

apps::ShareTarget::Enctype ToAppsShareTargetEnctype(
    blink::mojom::ManifestShareTarget_Enctype enctype) {
  switch (enctype) {
    case blink::mojom::ManifestShareTarget_Enctype::kFormUrlEncoded:
      return apps::ShareTarget::Enctype::kFormUrlEncoded;
    case blink::mojom::ManifestShareTarget_Enctype::kMultipartFormData:
      return apps::ShareTarget::Enctype::kMultipartFormData;
  }
  NOTREACHED();
}

absl::optional<apps::ShareTarget> ToWebAppShareTarget(
    const absl::optional<blink::Manifest::ShareTarget>& share_target) {
  if (!share_target) {
    return absl::nullopt;
  }
  apps::ShareTarget apps_share_target;
  apps_share_target.action = share_target->action;
  apps_share_target.method = ToAppsShareTargetMethod(share_target->method);
  apps_share_target.enctype = ToAppsShareTargetEnctype(share_target->enctype);

  if (share_target->params.title.has_value()) {
    apps_share_target.params.title =
        base::UTF16ToUTF8(*share_target->params.title);
  }
  if (share_target->params.text.has_value()) {
    apps_share_target.params.text =
        base::UTF16ToUTF8(*share_target->params.text);
  }
  if (share_target->params.url.has_value()) {
    apps_share_target.params.url = base::UTF16ToUTF8(*share_target->params.url);
  }

  for (const auto& file_filter : share_target->params.files) {
    apps::ShareTarget::Files apps_share_target_files;
    apps_share_target_files.name = base::UTF16ToUTF8(file_filter.name);

    for (const auto& file_type : file_filter.accept) {
      apps_share_target_files.accept.push_back(base::UTF16ToUTF8(file_type));
    }

    apps_share_target.params.files.push_back(
        std::move(apps_share_target_files));
  }

  return std::move(apps_share_target);
}

apps::UrlHandlers ToWebAppUrlHandlers(
    const std::vector<blink::mojom::ManifestUrlHandlerPtr>& url_handlers) {
  apps::UrlHandlers apps_url_handlers;
  for (const auto& url_handler : url_handlers) {
    DCHECK(url_handler);
    apps_url_handlers.emplace_back(url_handler->origin,
                                   url_handler->has_origin_wildcard);
  }
  return apps_url_handlers;
}

ScopeExtensions ToWebAppScopeExtensions(
    const std::vector<blink::mojom::ManifestScopeExtensionPtr>&
        scope_extensions) {
  ScopeExtensions apps_scope_extensions;
  for (const auto& scope_extension : scope_extensions) {
    DCHECK(scope_extension);
    ScopeExtensionInfo new_scope_extension;
    new_scope_extension.origin = scope_extension->origin;
    new_scope_extension.has_origin_wildcard =
        scope_extension->has_origin_wildcard;
    apps_scope_extensions.insert(std::move(new_scope_extension));
  }
  return apps_scope_extensions;
}

std::vector<apps::ProtocolHandlerInfo> ToWebAppProtocolHandlers(
    const std::vector<blink::mojom::ManifestProtocolHandlerPtr>&
        manifest_protocol_handlers) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handlers;
  for (const auto& manifest_protocol_handler : manifest_protocol_handlers) {
    apps::ProtocolHandlerInfo protocol_handler;
    protocol_handler.protocol =
        base::UTF16ToUTF8(manifest_protocol_handler->protocol);
    protocol_handler.url = manifest_protocol_handler->url;
    protocol_handlers.push_back(std::move(protocol_handler));
  }
  return protocol_handlers;
}

void PopulateShortcutItemIcons(WebAppInstallInfo* web_app_info,
                               const IconsMap& icons_map) {
  web_app_info->shortcuts_menu_icon_bitmaps.clear();
  for (auto& shortcut : web_app_info->shortcuts_menu_item_infos) {
    IconBitmaps shortcut_icon_bitmaps;

    for (IconPurpose purpose : kIconPurposes) {
      std::map<SquareSizePx, SkBitmap> bitmaps;
      for (const auto& icon :
           shortcut.GetShortcutIconInfosForPurpose(purpose)) {
        auto it = icons_map.find(icon.url);
        if (it != icons_map.end()) {
          std::set<SquareSizePx> sizes_to_generate;
          sizes_to_generate.emplace(icon.square_size_px);
          SizeToBitmap resized_bitmaps(
              ConstrainBitmapsToSizes(it->second, sizes_to_generate));

          // Don't overwrite as a shortcut item could have multiple icon urls.
          bitmaps.insert(resized_bitmaps.begin(), resized_bitmaps.end());
        }
      }
      shortcut_icon_bitmaps.SetBitmapsForPurpose(purpose, std::move(bitmaps));
    }

    web_app_info->shortcuts_menu_icon_bitmaps.emplace_back(
        std::move(shortcut_icon_bitmaps));
  }
  CHECK_EQ(web_app_info->shortcuts_menu_icon_bitmaps.size(),
           web_app_info->shortcuts_menu_item_infos.size());
}

// Reconcile the file handling icons that were specified in the manifest with
// the icons we were successfully able to download. Store the actual bitmaps and
// update the icon metadata in `web_app_info`.
void PopulateFileHandlingIcons(WebAppInstallInfo* web_app_info,
                               const IconsMap& icons_map,
                               IconsMap& other_icon_bitmaps) {
  // Before starting, each `apps::IconInfo` in `web_app_info` has a source URL
  // and a purpose, but no size. Replace with structs that copy the URL and
  // purpose and set the size based on what is found in `icons_map`.
  for (auto& file_handler : web_app_info->file_handlers) {
    if (!WebAppFileHandlerManager::IconsEnabled()) {
      DCHECK(file_handler.downloaded_icons.empty());
      continue;
    }

    std::vector<apps::IconInfo> manifest_icons;

    for (const auto& icon_info_without_size : file_handler.downloaded_icons) {
      const GURL& src = icon_info_without_size.url;
      // Only store bitmaps for this URL if it's the first time we've seen it.
      bool bitmaps_already_saved_for_url =
          other_icon_bitmaps.find(src) != other_icon_bitmaps.end();
      const auto& downloaded_bitmaps_for_url = icons_map.find(src);
      if (downloaded_bitmaps_for_url == icons_map.end())
        continue;

      for (const SkBitmap& bitmap : downloaded_bitmaps_for_url->second) {
        // Filter out non-square or too large icons.
        if (bitmap.width() != bitmap.height() || bitmap.width() > kMaxIconSize)
          continue;

        // Add the size to the FileHandler icon metadata.
        apps::IconInfo icon_info_with_size(icon_info_without_size);
        icon_info_with_size.square_size_px = bitmap.width();
        manifest_icons.push_back(std::move(icon_info_with_size));

        // Add the bitmap to `other_icon_bitmaps`.
        if (!bitmaps_already_saved_for_url)
          other_icon_bitmaps[src].push_back(bitmap);
      }
    }
    file_handler.downloaded_icons = std::move(manifest_icons);
  }
}

// Reconcile the home tab icons that were specified in the manifest with
// the icons we were successfully able to download. Store the actual bitmaps and
// update the icon metadata in `web_app_info`.
void PopulateHomeTabIcons(WebAppInstallInfo* web_app_info,
                          const IconsMap& icons_map,
                          IconsMap& other_icon_bitmaps) {
  if (!HomeTabIconsExistInTabStrip(web_app_info)) {
    return;
  }

  const auto& home_tab = absl::get<blink::Manifest::HomeTabParams>(
      web_app_info->tab_strip.value().home_tab);

  for (const auto& icon : home_tab.icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    DCHECK(!icon.purpose.empty());

    // Only store bitmaps for this URL if this is the first time we've seen it.
    bool bitmaps_already_saved_for_url =
        other_icon_bitmaps.find(icon.src) != other_icon_bitmaps.end();
    const auto& downloaded_bitmaps_for_url = icons_map.find(icon.src);

    if (downloaded_bitmaps_for_url == icons_map.end()) {
      continue;
    }

    for (const SkBitmap& bitmap : downloaded_bitmaps_for_url->second) {
      // Filter out non-square or too large icons
      if (bitmap.width() != bitmap.height() || bitmap.width() > kMaxIconSize) {
        continue;
      }

      // Add the bitmap to 'other_icon_bitmaps'.
      if (!bitmaps_already_saved_for_url) {
        other_icon_bitmaps[icon.src].emplace_back(bitmap);
      }
    }

    DCHECK(other_icon_bitmaps.size() <= kMaxIcons);
  }
}

apps::FileHandler::LaunchType ToFileHandlerLaunchType(
    blink::mojom::ManifestFileHandler::LaunchType launch_type) {
  switch (launch_type) {
    case blink::mojom::ManifestFileHandler::LaunchType::kSingleClient:
      return apps::FileHandler::LaunchType::kSingleClient;
    case blink::mojom::ManifestFileHandler::LaunchType::kMultipleClients:
      return apps::FileHandler::LaunchType::kMultipleClients;
  }
}

base::flat_map<std::string, blink::Manifest::TranslationItem>
ToWebAppTranslations(
    const base::flat_map<std::u16string, blink::Manifest::TranslationItem>&
        manifest_translations) {
  std::vector<std::pair<std::string, blink::Manifest::TranslationItem>>
      translations_vector;
  translations_vector.reserve(manifest_translations.size());
  for (const auto& it : manifest_translations) {
    translations_vector.emplace_back(base::UTF16ToUTF8(it.first), it.second);
  }
  return base::flat_map<std::string, blink::Manifest::TranslationItem>(
      std::move(translations_vector));
}

}  // namespace

apps::FileHandlers CreateFileHandlersFromManifest(
    const std::vector<blink::mojom::ManifestFileHandlerPtr>&
        manifest_file_handlers,
    const GURL& app_scope) {
  apps::FileHandlers web_app_file_handlers;

  for (const auto& manifest_file_handler : manifest_file_handlers) {
    DCHECK(manifest_file_handler);
    apps::FileHandler web_app_file_handler;
    web_app_file_handler.action = manifest_file_handler->action;
    web_app_file_handler.display_name = manifest_file_handler->name;
    web_app_file_handler.launch_type =
        ToFileHandlerLaunchType(manifest_file_handler->launch_type);

    for (const auto& it : manifest_file_handler->accept) {
      apps::FileHandler::AcceptEntry web_app_accept_entry;
      web_app_accept_entry.mime_type = base::UTF16ToUTF8(it.first);
      for (const auto& manifest_file_extension : it.second) {
        web_app_accept_entry.file_extensions.insert(
            base::UTF16ToUTF8(manifest_file_extension));
      }
      web_app_file_handler.accept.push_back(std::move(web_app_accept_entry));
    }

    if (WebAppFileHandlerManager::IconsEnabled()) {
      for (const auto& image_resource : manifest_file_handler->icons) {
        for (const auto manifest_purpose : image_resource.purpose) {
          apps::IconInfo icon_info;
          icon_info.url = image_resource.src;
          icon_info.purpose =
              ManifestPurposeToIconInfoPurpose(manifest_purpose);
          web_app_file_handler.downloaded_icons.push_back(std::move(icon_info));
          // The list will be pruned and the sizes will be filled in when images
          // are actually downloaded.
        }
      }
    }

    web_app_file_handlers.push_back(std::move(web_app_file_handler));
  }

  return web_app_file_handlers;
}

// Create the WebAppInstallInfo icons list *outside* of |web_app_info|, so
// that we can decide later whether or not to replace the existing icons.
std::vector<apps::IconInfo> CreateWebAppInstallInfoIcons(
    const std::vector<blink::Manifest::ImageResource> icons) {
  std::vector<apps::IconInfo> web_app_icons;
  for (const auto& icon : icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    DCHECK(!icon.purpose.empty());

    for (IconPurpose purpose : icon.purpose) {
      apps::IconInfo info;

      if (!icon.sizes.empty()) {
        // Filter out non-square or too large icons.
        auto valid_size =
            base::ranges::find_if(icon.sizes, [](const gfx::Size& size) {
              return size.width() == size.height() &&
                     size.width() <= kMaxIconSize;
            });
        if (valid_size == icon.sizes.end()) {
          continue;
        }
        // TODO(https://crbug.com/1071308): Take the declared icon density and
        // sizes into account.
        info.square_size_px = valid_size->width();
      }

      info.url = icon.src;
      info.purpose = ManifestPurposeToIconInfoPurpose(purpose);
      web_app_icons.push_back(std::move(info));

      // Limit the number of icons we store on the user's machine.
      if (web_app_icons.size() == kMaxIcons) {
        return web_app_icons;
      }
    }
  }
  return web_app_icons;
}

// Create the WebAppInstallInfo icons list *outside* of |web_app_info|, so
// that we can decide later whether or not to replace the existing
// home tab icons.
// Icons are replaced if we filter out icons that are too large or non-square
// which limits the number of icons.
std::vector<blink::Manifest::ImageResource> FilterWebAppHomeTabIcons(
    const std::vector<blink::Manifest::ImageResource>& icons) {
  std::vector<blink::Manifest::ImageResource> home_tab_icons;
  for (const auto& icon : icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    DCHECK(!icon.purpose.empty());

    if (!icon.sizes.empty()) {
      // Filter out non-square or too large icons.
      auto valid_size =
          base::ranges::find_if(icon.sizes, [](const gfx::Size& size) {
            return size.width() == size.height() &&
                   size.width() <= kMaxIconSize;
          });
      if (valid_size == icon.sizes.end()) {
        continue;
      }
    }

    home_tab_icons.push_back(std::move(icon));

    // Limit the number of icons we store on the user's machine.
    if (home_tab_icons.size() == kMaxIcons) {
      break;
    }
  }
  return home_tab_icons;
}

void UpdateWebAppInfoFromManifest(const blink::mojom::Manifest& manifest,
                                  const GURL& manifest_url,
                                  WebAppInstallInfo* web_app_info) {
  // Give the full length name priority if it's not empty.
  std::u16string name = manifest.name.value_or(std::u16string());
  if (!name.empty())
    web_app_info->title = name;
  else if (manifest.short_name)
    web_app_info->title = *manifest.short_name;

  if (manifest.id.is_valid()) {
    web_app_info->manifest_id = manifest.id;
  }

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    web_app_info->start_url = manifest.start_url;

  if (manifest.scope.is_valid())
    web_app_info->scope = manifest.scope;

  if (manifest.has_theme_color) {
    web_app_info->theme_color =
        SkColorSetA(static_cast<SkColor>(manifest.theme_color), SK_AlphaOPAQUE);
  }

  if (manifest.has_background_color) {
    web_app_info->background_color = SkColorSetA(
        static_cast<SkColor>(manifest.background_color), SK_AlphaOPAQUE);
  }

  if (manifest.has_dark_theme_color) {
    web_app_info->dark_mode_theme_color = SkColorSetA(
        static_cast<SkColor>(manifest.dark_theme_color), SK_AlphaOPAQUE);
  } else if (manifest.user_preferences &&
             manifest.user_preferences->color_scheme_dark &&
             manifest.user_preferences->color_scheme_dark->has_theme_color) {
    web_app_info->dark_mode_theme_color = SkColorSetA(
        static_cast<SkColor>(
            manifest.user_preferences->color_scheme_dark->theme_color),
        SK_AlphaOPAQUE);
  }

  if (manifest.has_dark_background_color) {
    web_app_info->dark_mode_background_color = SkColorSetA(
        static_cast<SkColor>(manifest.dark_background_color), SK_AlphaOPAQUE);
  } else if (manifest.user_preferences &&
             manifest.user_preferences->color_scheme_dark &&
             manifest.user_preferences->color_scheme_dark
                 ->has_background_color) {
    web_app_info->dark_mode_background_color = SkColorSetA(
        static_cast<SkColor>(
            manifest.user_preferences->color_scheme_dark->background_color),
        SK_AlphaOPAQUE);
  }

  if (manifest.display != DisplayMode::kUndefined)
    web_app_info->display_mode = manifest.display;

  if (!manifest.display_override.empty())
    web_app_info->display_override = manifest.display_override;

  // Create the WebAppInstallInfo icons list *outside* of |web_app_info|, so
  // that we can decide later whether or not to replace the existing icons.
  std::vector<apps::IconInfo> web_app_icons =
      CreateWebAppInstallInfoIcons(manifest.icons);

  // If any icons are correctly specified in the manifest, they take precedence
  // over any we picked up from web page metadata.
  if (!web_app_icons.empty())
    web_app_info->manifest_icons = std::move(web_app_icons);

  // TODO(crbug.com/1218210): Confirm incoming icons to write to web_app_info.
  web_app_info->file_handlers = CreateFileHandlersFromManifest(
      manifest.file_handlers, web_app_info->scope);

  web_app_info->share_target = ToWebAppShareTarget(manifest.share_target);

  web_app_info->protocol_handlers =
      ToWebAppProtocolHandlers(manifest.protocol_handlers);

  web_app_info->url_handlers = ToWebAppUrlHandlers(manifest.url_handlers);

  web_app_info->scope_extensions =
      ToWebAppScopeExtensions(manifest.scope_extensions);

  GURL inferred_scope = web_app_info->scope.is_valid() ? web_app_info->scope
                        : web_app_info->start_url.is_valid()
                            ? web_app_info->start_url.GetWithoutFilename()
                            : GURL();
  if (base::FeatureList::IsEnabled(
          blink::features::kWebAppManifestLockScreen) &&
      manifest.lock_screen && manifest.lock_screen->start_url.is_valid() &&
      IsInScope(manifest.lock_screen->start_url, inferred_scope)) {
    web_app_info->lock_screen_start_url = manifest.lock_screen->start_url;
  }

  if (manifest.note_taking && manifest.note_taking->new_note_url.is_valid() &&
      IsInScope(manifest.note_taking->new_note_url, inferred_scope)) {
    web_app_info->note_taking_new_note_url = manifest.note_taking->new_note_url;
  }

  DCHECK(web_app_info->shortcuts_menu_item_infos.empty());
  web_app_info->shortcuts_menu_item_infos =
      ToWebAppShortcutsMenuItemInfos(manifest.shortcuts);

  web_app_info->capture_links = manifest.capture_links;

  if (manifest_url.is_valid())
    web_app_info->manifest_url = manifest_url;

  web_app_info->launch_handler = manifest.launch_handler;
  if (manifest.description.has_value()) {
    web_app_info->description = manifest.description.value();
  }

  web_app_info->translations = ToWebAppTranslations(manifest.translations);

  web_app_info->permissions_policy.clear();
  for (const auto& decl : manifest.permissions_policy) {
    blink::ParsedPermissionsPolicyDeclaration copy;
    copy.feature = decl.feature;
    copy.self_if_matches = decl.self_if_matches;
    for (const auto& origin : decl.allowed_origins)
      copy.allowed_origins.push_back(origin);
    copy.matches_all_origins = decl.matches_all_origins;
    copy.matches_opaque_src = decl.matches_opaque_src;
    web_app_info->permissions_policy.push_back(std::move(copy));
  }

  web_app_info->tab_strip = manifest.tab_strip;

  if (HomeTabIconsExistInTabStrip(web_app_info)) {
    auto& home_tab = absl::get<blink::Manifest::HomeTabParams>(
        web_app_info->tab_strip->home_tab);

    home_tab.icons = FilterWebAppHomeTabIcons(home_tab.icons);
    web_app_info->tab_strip->home_tab = home_tab;
  }
}

WebAppInstallInfo CreateWebAppInfoFromManifest(
    const blink::mojom::Manifest& manifest,
    const GURL& manifest_url) {
  WebAppInstallInfo info(manifest.id);
  UpdateWebAppInfoFromManifest(manifest, manifest_url, &info);
  return info;
}

namespace {

std::vector<GURL> GetAppIconUrls(const WebAppInstallInfo& web_app_info) {
  std::vector<GURL> urls;

  for (const apps::IconInfo& info : web_app_info.manifest_icons) {
    urls.push_back(info.url);
  }

  return urls;
}

std::vector<GURL> GetShortcutIcons(const WebAppInstallInfo& web_app_info) {
  std::vector<GURL> urls;
  for (const WebAppShortcutsMenuItemInfo& shortcut :
       web_app_info.shortcuts_menu_item_infos) {
    for (IconPurpose purpose : kIconPurposes) {
      for (const WebAppShortcutsMenuItemInfo::Icon& icon :
           shortcut.GetShortcutIconInfosForPurpose(purpose)) {
        urls.push_back(icon.url);
      }
    }
  }

  return urls;
}

std::vector<GURL> GetFileHandlingIcons(const WebAppInstallInfo& web_app_info) {
  std::vector<GURL> urls;

  for (const apps::FileHandler& file_handler : web_app_info.file_handlers) {
    for (const apps::IconInfo& icon : file_handler.downloaded_icons) {
      urls.push_back(icon.url);
    }
  }

  return urls;
}

std::vector<GURL> GetHomeTabIcons(const WebAppInstallInfo& web_app_info) {
  std::vector<GURL> urls;

  if (!HomeTabIconsExistInTabStrip(&web_app_info)) {
    return urls;
  }

  const auto& home_tab = absl::get<blink::Manifest::HomeTabParams>(
      web_app_info.tab_strip.value().home_tab);

  for (const auto& icon : home_tab.icons) {
    urls.push_back(icon.src);
  }

  return urls;
}

base::flat_set<GURL> RemoveDuplicates(std::vector<GURL> from_urls) {
  return base::flat_set<GURL>{from_urls};
}

void RemoveInvalidUrls(std::vector<GURL>& urls) {
  base::EraseIf(urls, [](const GURL& url) { return !url.is_valid(); });
}

}  // namespace

base::flat_set<GURL> GetValidIconUrlsToDownload(
    const WebAppInstallInfo& web_app_info) {
  std::vector<GURL> icon_urls;

  base::Extend(icon_urls, GetAppIconUrls(web_app_info));
  base::Extend(icon_urls, GetShortcutIcons(web_app_info));
  base::Extend(icon_urls, GetFileHandlingIcons(web_app_info));
  base::Extend(icon_urls, GetHomeTabIcons(web_app_info));

  RemoveInvalidUrls(std::ref(icon_urls));

  return RemoveDuplicates(std::move(icon_urls));
}

void PopulateOtherIcons(WebAppInstallInfo* web_app_info,
                        const IconsMap& icons_map) {
  IconsMap& other_icon_bitmaps = web_app_info->other_icon_bitmaps;
  other_icon_bitmaps.clear();
  PopulateShortcutItemIcons(web_app_info, icons_map);
  PopulateFileHandlingIcons(web_app_info, icons_map, other_icon_bitmaps);
  PopulateHomeTabIcons(web_app_info, icons_map, other_icon_bitmaps);
}

void PopulateProductIcons(WebAppInstallInfo* web_app_info,
                          const IconsMap* icons_map) {
  std::vector<apps::IconInfo> manifest_icons_any;
  std::vector<apps::IconInfo> manifest_icons_maskable;
  std::vector<apps::IconInfo> manifest_icons_monochrome;
  for (apps::IconInfo& icon_info : web_app_info->manifest_icons) {
    switch (icon_info.purpose) {
      case apps::IconInfo::Purpose::kAny:
        manifest_icons_any.push_back(icon_info);
        break;
      case apps::IconInfo::Purpose::kMaskable:
        manifest_icons_maskable.push_back(icon_info);
        break;
      case apps::IconInfo::Purpose::kMonochrome:
        manifest_icons_monochrome.push_back(icon_info);
        break;
    }
  }
  std::vector<SkBitmap> square_icons_any;
  std::vector<SkBitmap> square_icons_maskable;
  std::vector<SkBitmap> square_icons_monochrome;
  if (icons_map) {
    AddSquareIconsFromMapMatchingIconInfos(&square_icons_any,
                                           manifest_icons_any, *icons_map);
    AddSquareIconsFromMapMatchingIconInfos(&square_icons_maskable,
                                           manifest_icons_maskable, *icons_map);
    AddSquareIconsFromMapMatchingIconInfos(
        &square_icons_monochrome, manifest_icons_monochrome, *icons_map);
    // Fall back to using all icons from |icons_map| if none match
    // manifest_icons.
    if (square_icons_any.empty())
      AddSquareIconsFromMap(&square_icons_any, *icons_map);
  }
  AddSquareIconsFromBitmaps(&square_icons_any, web_app_info->icon_bitmaps.any);

  for (SkBitmap& bitmap : square_icons_maskable) {
    // Retain any bitmaps provided as input to the installation.
    if (web_app_info->icon_bitmaps.maskable.count(bitmap.width()) == 0)
      web_app_info->icon_bitmaps.maskable[bitmap.width()] = std::move(bitmap);
  }

  for (SkBitmap& bitmap : square_icons_monochrome) {
    // Retain any bitmaps provided as input to the installation.
    if (web_app_info->icon_bitmaps.monochrome.count(bitmap.width()) == 0)
      web_app_info->icon_bitmaps.monochrome[bitmap.width()] = std::move(bitmap);
  }

  char32_t icon_letter =
      web_app_info->title.empty()
          ? GenerateIconLetterFromUrl(web_app_info->start_url)
          : GenerateIconLetterFromAppName(web_app_info->title);

  // Ensure that all top-level icons that are in web_app_info with  Purpose::ANY
  // are present, by generating icons for any sizes that have failed to
  // download. This ensures that the created manifest for the web app does not
  // contain links to icons that are not actually created and linked on disk.
  // TODO(https://crbug.com/1029223): Don't resize before writing to disk, it's
  // not necessary and would simplify this code path to remove.
  SizeToBitmap size_to_icons = ResizeIconsAndGenerateMissing(
      square_icons_any, SizesToGenerate(), icon_letter,
      &web_app_info->is_generated_icon);

  for (auto& item : size_to_icons) {
    // Retain any bitmaps provided as input to the installation.
    if (web_app_info->icon_bitmaps.any.count(item.first) == 0)
      web_app_info->icon_bitmaps.any[item.first] = std::move(item.second);
  }
}

void RecordDownloadedIconsResultAndHttpStatusCodes(
    IconsDownloadedResult result,
    const DownloadedIconsHttpResults& icons_http_results) {
  RecordDownloadedIconsHttpResultsCodeClass(
      "WebApp.Icon.HttpStatusCodeClassOnCreate", result, icons_http_results);

  base::UmaHistogramEnumeration("WebApp.Icon.DownloadedResultOnCreate", result);
  RecordDownloadedIconHttpStatusCodes(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate", icons_http_results);
}

void RecordDownloadedIconsHttpResultsCodeClass(
    base::StringPiece histogram_name,
    IconsDownloadedResult result,
    const DownloadedIconsHttpResults& icons_http_results) {
  if (result != IconsDownloadedResult::kCompleted)
    return;

  for (const auto& url_and_http_status_code : icons_http_results) {
    int http_status_code = url_and_http_status_code.second;
    if (http_status_code != 0) {
      DCHECK_LE(100, http_status_code);
      DCHECK_GT(600, http_status_code);
      base::UmaHistogramExactLinear(histogram_name.data(),
                                    http_status_code / 100, 5);
    }
  }
}

void RecordDownloadedIconHttpStatusCodes(
    base::StringPiece histogram_name,
    const DownloadedIconsHttpResults& icons_http_results) {
  if (icons_http_results.empty())
    return;

  // Do not use UMA_HISTOGRAM_... macros here, as it caches the Histogram
  // instance and thus only works if |histogram_name| is constant.
  base::HistogramBase* counter = base::CustomHistogram::FactoryGet(
      histogram_name.data(), net::HttpUtil::GetStatusCodesForHistogram(),
      base::HistogramBase::kUmaTargetedHistogramFlag);

  // A web app may contain arbitrary number of icons. The histogram assumes that
  // most of them fail with same http status codes and counts each http status
  // code only once.
  std::vector<int> http_status_codes;
  http_status_codes.reserve(icons_http_results.size());
  for (const auto& url_and_http_status_code : icons_http_results)
    http_status_codes.push_back(url_and_http_status_code.second);

  base::flat_set<int> unique_http_status_codes{std::move(http_status_codes)};

  for (int http_status_code : unique_http_status_codes)
    counter->Add(net::HttpUtil::MapStatusCodeForHistogram(http_status_code));
}

WebAppManagement::Type ConvertExternalInstallSourceToSource(
    ExternalInstallSource external_install_source) {
  return ConvertInstallSurfaceToWebAppSource(
      ConvertExternalInstallSourceToInstallSource(external_install_source));
}

webapps::WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source) {
  switch (external_install_source) {
    case ExternalInstallSource::kInternalDefault:
      return webapps::WebappInstallSource::INTERNAL_DEFAULT;
    case ExternalInstallSource::kExternalDefault:
      return webapps::WebappInstallSource::EXTERNAL_DEFAULT;
    case ExternalInstallSource::kExternalPolicy:
      return webapps::WebappInstallSource::EXTERNAL_POLICY;
    case ExternalInstallSource::kSystemInstalled:
      return webapps::WebappInstallSource::SYSTEM_DEFAULT;
    case ExternalInstallSource::kKiosk:
      return webapps::WebappInstallSource::KIOSK;
    case ExternalInstallSource::kExternalLockScreen:
      return webapps::WebappInstallSource::EXTERNAL_LOCK_SCREEN;
    case ExternalInstallSource::kInternalMicrosoft365Setup:
      return webapps::WebappInstallSource::MICROSOFT_365_SETUP;
  }
}

webapps::WebappUninstallSource ConvertExternalInstallSourceToUninstallSource(
    ExternalInstallSource external_install_source) {
  switch (external_install_source) {
    case ExternalInstallSource::kInternalDefault:
      return webapps::WebappUninstallSource::kInternalPreinstalled;
    case ExternalInstallSource::kExternalDefault:
      return webapps::WebappUninstallSource::kExternalPreinstalled;
    case ExternalInstallSource::kExternalPolicy:
      return webapps::WebappUninstallSource::kExternalPolicy;
    case ExternalInstallSource::kSystemInstalled:
      return webapps::WebappUninstallSource::kSystemPreinstalled;
    case ExternalInstallSource::kKiosk:
      NOTREACHED() << "Kiosk apps should not be uninstalled";
      return webapps::WebappUninstallSource::kUnknown;
    case ExternalInstallSource::kExternalLockScreen:
      return webapps::WebappUninstallSource::kExternalLockScreen;
    case ExternalInstallSource::kInternalMicrosoft365Setup:
      NOTREACHED() << "Microsoft 365 apps should not be uninstalled externally";
      return webapps::WebappUninstallSource::kUnknown;
  }
}

WebAppManagement::Type ConvertInstallSurfaceToWebAppSource(
    webapps::WebappInstallSource install_source) {
  switch (install_source) {
    case webapps::WebappInstallSource::MENU_BROWSER_TAB:
    case webapps::WebappInstallSource::MENU_CUSTOM_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case webapps::WebappInstallSource::API_BROWSER_TAB:
    case webapps::WebappInstallSource::API_CUSTOM_TAB:
    case webapps::WebappInstallSource::DEVTOOLS:
    case webapps::WebappInstallSource::MANAGEMENT_API:
    case webapps::WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case webapps::WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case webapps::WebappInstallSource::RICH_INSTALL_UI_WEBLAYER:
    case webapps::WebappInstallSource::ML_PROMOTION:
    case webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case webapps::WebappInstallSource::SYNC:
    case webapps::WebappInstallSource::MENU_CREATE_SHORTCUT:
    case webapps::WebappInstallSource::CHROME_SERVICE:
    case webapps::WebappInstallSource::PROFILE_MENU:
      return WebAppManagement::kSync;

    case webapps::WebappInstallSource::ISOLATED_APP_DEV_INSTALL:
      return WebAppManagement::kCommandLine;

    case webapps::WebappInstallSource::INTERNAL_DEFAULT:
    case webapps::WebappInstallSource::EXTERNAL_DEFAULT:
      return WebAppManagement::kDefault;

    case webapps::WebappInstallSource::PRELOADED_DEFAULT:
      return WebAppManagement::kApsDefault;

    case webapps::WebappInstallSource::PRELOADED_OEM:
      return WebAppManagement::kOem;

    case webapps::WebappInstallSource::EXTERNAL_POLICY:
      return WebAppManagement::kPolicy;

    case webapps::WebappInstallSource::KIOSK:
      return WebAppManagement::kKiosk;

    case webapps::WebappInstallSource::EXTERNAL_LOCK_SCREEN:
    case webapps::WebappInstallSource::SYSTEM_DEFAULT:
      return WebAppManagement::kSystem;

    case webapps::WebappInstallSource::ARC:
      return WebAppManagement::kWebAppStore;

    case webapps::WebappInstallSource::SUB_APP:
      return WebAppManagement::kSubApp;

    case webapps::WebappInstallSource::MICROSOFT_365_SETUP:
      return WebAppManagement::kOneDriveIntegration;

    case webapps::WebappInstallSource::COUNT:
      NOTREACHED();
      return WebAppManagement::kSync;
  }
}

void CreateWebAppInstallTabHelpers(content::WebContents* web_contents) {
  webapps::InstallableManager::CreateForWebContents(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  webapps::PreRedirectionURLObserver::CreateForWebContents(web_contents);
}

void MaybeRegisterOsUninstall(const WebApp* web_app,
                              WebAppManagement::Type source_uninstalling,
                              OsIntegrationManager& os_integration_manager,
                              InstallOsHooksCallback callback) {
#if BUILDFLAG(IS_WIN)
  // |web_app| object will remove target |source_uninstalling| type.
  // If the remaining source types and they happen to be user
  // uninstallable, then it should register OsSettings.
  WebAppManagementTypes sources = web_app->GetSources();
  DCHECK(sources.Has(source_uninstalling));
  bool user_installable_before_uninstall = CanUserUninstallWebApp(sources);
  sources.Remove(source_uninstalling);
  bool user_installable_after_uninstall = CanUserUninstallWebApp(sources);

  if (!user_installable_before_uninstall && user_installable_after_uninstall) {
    InstallOsHooksOptions options;
    options.os_hooks[OsHookType::kUninstallationViaOsSettings] = true;
    auto os_hooks_barrier =
        OsIntegrationManager::GetBarrierForSynchronize(std::move(callback));
    // TODO(crbug.com/1401125): Remove InstallOsHooks() once OS integration
    // sub managers have been implemented.
    os_integration_manager.InstallOsHooks(web_app->app_id(), os_hooks_barrier,
                                          nullptr, options);
    os_integration_manager.Synchronize(
        web_app->app_id(), base::BindOnce(os_hooks_barrier, OsHooksErrors()));
    return;
  }
#endif
  std::move(callback).Run(OsHooksErrors());
}

void MaybeUnregisterOsUninstall(const WebApp* web_app,
                                WebAppManagement::Type source_installing,
                                OsIntegrationManager& os_integration_manager) {
#if BUILDFLAG(IS_WIN)
  // |web_app| object will add target |source_installing| type.
  // If the old source types are user installable, but new type is not, then
  // it should unregister OsSettings.
  WebAppManagementTypes sources = web_app->GetSources();
  bool user_installable_before_install = CanUserUninstallWebApp(sources);
  sources.Put(source_installing);
  bool user_installable_after_install = CanUserUninstallWebApp(sources);

  if (user_installable_before_install && !user_installable_after_install) {
    OsHooksOptions options;
    options[OsHookType::kUninstallationViaOsSettings] = true;
    // TODO(crbug.com/1401125): Remove UninstallOsHooks() once OS integration
    // sub managers have been implemented.
    os_integration_manager.UninstallOsHooks(web_app->app_id(), options,
                                            base::DoNothing());
    os_integration_manager.Synchronize(web_app->app_id(), base::DoNothing());
  }
#endif
}

void SetWebAppManifestFields(const WebAppInstallInfo& web_app_info,
                             WebApp& web_app,
                             bool skip_icons_on_download_failure) {
  DCHECK(!web_app_info.title.empty());
  web_app.SetName(base::UTF16ToUTF8(web_app_info.title));

  web_app.SetStartUrl(web_app_info.start_url);

  // TODO(b/280862254): CHECK that the manifest_id isn't empty after the empty
  // constructor is removed. Currently, `SetStartUrl` sets a default manifest_id
  // based on the start_url.
  if (web_app_info.manifest_id.is_valid()) {
    web_app.SetManifestId(web_app_info.manifest_id);
  }

  web_app.SetDisplayMode(web_app_info.display_mode);
  web_app.SetDisplayModeOverride(web_app_info.display_override);

  web_app.SetDescription(base::UTF16ToUTF8(web_app_info.description));
  web_app.SetLaunchQueryParams(web_app_info.launch_query_params);
  web_app.SetScope(web_app_info.scope);
  DCHECK(!web_app_info.theme_color.has_value() ||
         SkColorGetA(*web_app_info.theme_color) == SK_AlphaOPAQUE);
  web_app.SetThemeColor(web_app_info.theme_color);

  DCHECK(!web_app_info.dark_mode_theme_color.has_value() ||
         SkColorGetA(*web_app_info.dark_mode_theme_color) == SK_AlphaOPAQUE);
  web_app.SetDarkModeThemeColor(web_app_info.dark_mode_theme_color);

  DCHECK(!web_app_info.background_color.has_value() ||
         SkColorGetA(*web_app_info.background_color) == SK_AlphaOPAQUE);
  web_app.SetBackgroundColor(web_app_info.background_color);

  DCHECK(!web_app_info.dark_mode_background_color.has_value() ||
         SkColorGetA(*web_app_info.dark_mode_background_color) ==
             SK_AlphaOPAQUE);
  web_app.SetDarkModeBackgroundColor(web_app_info.dark_mode_background_color);

  WebApp::SyncFallbackData sync_fallback_data;
  sync_fallback_data.name = base::UTF16ToUTF8(web_app_info.title);
  sync_fallback_data.theme_color = web_app_info.theme_color;
  sync_fallback_data.scope = web_app_info.scope;
  sync_fallback_data.icon_infos = web_app_info.manifest_icons;
  web_app.SetSyncFallbackData(std::move(sync_fallback_data));

  if (!skip_icons_on_download_failure) {
    SetWebAppProductIconFields(web_app_info, web_app);
    web_app.SetShortcutsMenuInfo(GetShortcutsMenuInfoWithIconSizes(
        web_app_info.shortcuts_menu_item_infos,
        web_app_info.shortcuts_menu_icon_bitmaps));
  }

  web_app.SetPermissionsPolicy(web_app_info.permissions_policy);

  if (web_app.file_handler_approval_state() == ApiApprovalState::kAllowed &&
      !AreNewFileHandlersASubsetOfOld(web_app.file_handlers(),
                                      web_app_info.file_handlers)) {
    web_app.SetFileHandlerApprovalState(ApiApprovalState::kRequiresPrompt);
  }
  web_app.SetFileHandlers(web_app_info.file_handlers);
  web_app.SetShareTarget(web_app_info.share_target);
  web_app.SetProtocolHandlers(web_app_info.protocol_handlers);
  web_app.SetUrlHandlers(web_app_info.url_handlers);
  web_app.SetScopeExtensions(web_app_info.scope_extensions);

  if (base::FeatureList::IsEnabled(features::kWebLockScreenApi))
    web_app.SetLockScreenStartUrl(web_app_info.lock_screen_start_url);

  web_app.SetNoteTakingNewNoteUrl(web_app_info.note_taking_new_note_url);

  web_app.SetCaptureLinks(web_app_info.capture_links);

  web_app.SetManifestUrl(web_app_info.manifest_url);

  web_app.SetLaunchHandler(web_app_info.launch_handler);

  web_app.SetTabStrip(web_app_info.tab_strip);

  if (web_app_info.validated_scope_extensions.has_value()) {
    web_app.SetValidatedScopeExtensions(
        web_app_info.validated_scope_extensions.value());
  }
}

void SetWebAppProductIconFields(const WebAppInstallInfo& web_app_info,
                                WebApp& web_app) {
  web_app.SetManifestIcons(web_app_info.manifest_icons);
  for (IconPurpose purpose : kIconPurposes) {
    web_app.SetDownloadedIconSizes(
        purpose, GetSquareSizePxs(web_app_info.icon_bitmaps, purpose));
  }
  web_app.SetIsGeneratedIcon(web_app_info.is_generated_icon);
}

void MaybeDisableOsIntegration(const WebAppRegistrar* app_registrar,
                               const webapps::AppId& app_id,
                               InstallOsHooksOptions* options) {
#if !BUILDFLAG(IS_CHROMEOS)  // Deeper OS integration is expected on ChromeOS.
  DCHECK(app_registrar);

  // Disable OS integration if the app was installed by default only, and not
  // through any other means like an enterprise policy or store.
  if (app_registrar->WasInstalledByDefaultOnly(app_id)) {
    options->add_to_desktop = false;
    options->add_to_quick_launch_bar = false;
    options->os_hooks[OsHookType::kShortcuts] = false;
    options->os_hooks[OsHookType::kRunOnOsLogin] = false;
    options->os_hooks[OsHookType::kShortcutsMenu] = false;
    options->os_hooks[OsHookType::kUninstallationViaOsSettings] = false;
    options->os_hooks[OsHookType::kFileHandlers] = false;
    options->os_hooks[OsHookType::kProtocolHandlers] = false;
    options->os_hooks[OsHookType::kUrlHandlers] = false;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

bool CanWebAppUpdateIdentity(const WebApp* web_app) {
  if (web_app->IsPolicyInstalledApp() &&
      base::FeatureList::IsEnabled(
          features::kWebAppManifestPolicyAppIdentityUpdate)) {
    return true;
  }

  // WebAppChromeOsData::oem_installed is not included in this statement as
  // we would like to keep WebAppManagement::kOem and
  // WebAppChromeOsData::oem_installed separate.
  // WebAppChromeOsData::oem_installed will be migrated to
  // WebAppManagement::kOem eventually.
  return web_app->IsPreinstalledApp() || web_app->IsKioskInstalledApp() ||
         web_app->GetSources().HasAny(
             {WebAppManagement::kOem, WebAppManagement::kApsDefault});
}

void ApplyParamsToWebAppInstallInfo(const WebAppInstallParams& install_params,
                                    WebAppInstallInfo& web_app_info) {
  if (install_params.user_display_mode.has_value())
    web_app_info.user_display_mode = install_params.user_display_mode;

  // If `additional_search_terms` was a manifest property, it would be
  // sanitized while parsing the manifest. Since it's not, we sanitize it
  // here.
  for (const std::string& search_term :
       install_params.additional_search_terms) {
    if (!search_term.empty())
      web_app_info.additional_search_terms.push_back(search_term);
  }

  if (install_params.launch_query_params)
    web_app_info.launch_query_params = install_params.launch_query_params;

  if (install_params.install_url.is_valid())
    web_app_info.install_url = install_params.install_url;
}

void ApplyParamsToFinalizeOptions(
    const WebAppInstallParams& install_params,
    WebAppInstallFinalizer::FinalizeOptions& options) {
  if (IsChromeOsDataMandatory()) {
    options.chromeos_data.emplace();
    options.chromeos_data->show_in_launcher =
        install_params.add_to_applications_menu;
    options.chromeos_data->show_in_search = install_params.add_to_search;
    options.chromeos_data->show_in_management =
        install_params.add_to_management;
    options.chromeos_data->is_disabled = install_params.is_disabled;
    options.chromeos_data->oem_installed = install_params.oem_installed;
    options.chromeos_data->handles_file_open_intents =
        install_params.handles_file_open_intents;
  }
  options.bypass_os_hooks = install_params.bypass_os_hooks;
  options.add_to_applications_menu = install_params.add_to_applications_menu;
  options.add_to_desktop = install_params.add_to_desktop;
  options.add_to_quick_launch_bar = install_params.add_to_quick_launch_bar;
  options.skip_origin_association_validation =
      install_params.skip_origin_association_validation;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (install_params.system_app_type.has_value()) {
    options.system_web_app_data.emplace();
    options.system_web_app_data->system_app_type =
        install_params.system_app_type.value();
  }
#endif
}

}  // namespace web_app
