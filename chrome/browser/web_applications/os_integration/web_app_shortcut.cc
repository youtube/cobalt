// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/os_integration/file_handling_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "skia/ext/image_operations.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep_default.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/icon_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

using content::BrowserThread;

namespace web_app {

namespace {

#if BUILDFLAG(IS_MAC)
const int kDesiredIconSizesForShortcut[] = {16, 32, 128, 256, 512};
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Linux supports icons of any size. FreeDesktop Icon Theme Specification states
// that "Minimally you should install a 48x48 icon in the hicolor theme."
const int kDesiredIconSizesForShortcut[] = {16, 32, 48, 128, 256, 512};
#elif BUILDFLAG(IS_WIN)
const int* kDesiredIconSizesForShortcut = IconUtil::kIconDimensions;
#else
const int kDesiredIconSizesForShortcut[] = {32};
#endif

#if BUILDFLAG(IS_WIN)
base::LazyThreadPoolCOMSTATaskRunner g_shortcuts_task_runner =
    LAZY_COM_STA_TASK_RUNNER_INITIALIZER(
        base::TaskTraits({base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                          base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
        base::SingleThreadTaskRunnerThreadMode::SHARED);
#else
base::LazyThreadPoolSequencedTaskRunner g_shortcuts_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits({base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                          base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
#endif

size_t GetNumDesiredIconSizesForShortcut() {
#if BUILDFLAG(IS_WIN)
  return IconUtil::kNumIconDimensions;
#else
  return std::size(kDesiredIconSizesForShortcut);
#endif
}

void DeleteShortcutInfoOnUIThread(std::unique_ptr<ShortcutInfo> shortcut_info,
                                  ResultCallback callback,
                                  Result result) {
  shortcut_info.reset();
  if (callback)
    std::move(callback).Run(result);
}

void CreatePlatformShortcutsAndPostCallback(
    const base::FilePath& shortcut_data_path,
    const ShortcutLocations& creation_locations,
    ShortcutCreationReason creation_reason,
    CreateShortcutsCallback callback,
    const ShortcutInfo& shortcut_info) {
  bool shortcut_created = internals::CreatePlatformShortcuts(
      shortcut_data_path, creation_locations, creation_reason, shortcut_info);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), shortcut_created));
}

void DeletePlatformShortcutsAndPostCallback(
    const base::FilePath& shortcut_data_path,
    DeleteShortcutsCallback callback,
    const ShortcutInfo& shortcut_info) {
  internals::DeletePlatformShortcuts(shortcut_data_path, shortcut_info,
                                     content::GetUIThreadTaskRunner({}),
                                     std::move(callback));
}

void DeleteMultiProfileShortcutsForAppAndPostCallback(const std::string& app_id,
                                                      ResultCallback callback) {
  internals::DeleteMultiProfileShortcutsForApp(app_id);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Result::kOk));
}

std::vector<WebAppShortcutsMenuItemInfo::Icon>
ConvertIconProtoDataToShortcutsMenuIcon(
    const ::google::protobuf::RepeatedPtrField<proto::ShortcutIconData>&
        shortcut_icon_data) {
  std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_menu_item_icons;
  for (const auto& icon_data : shortcut_icon_data) {
    WebAppShortcutsMenuItemInfo::Icon icon;
    icon.square_size_px = icon_data.icon_size();
    // The icon url is set to an empty GURL() because we need this data
    // structure to pass in for OS integration but the url is not used for
    // setting OS integration.
    icon.url = GURL();
    shortcut_menu_item_icons.push_back(std::move(icon));
  }
  return shortcut_menu_item_icons;
}

gfx::ImageFamily PackageIconsIntoImageFamily(
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  gfx::ImageFamily image_family;
  for (auto& size_and_bitmap : icon_bitmaps) {
    image_family.Add(gfx::ImageSkia(
        gfx::ImageSkiaRep(size_and_bitmap.second, /*scale=*/0.0f)));
  }

  // If the image failed to load, use the standard application icon.
  if (image_family.empty()) {
    SquareSizePx icon_size_in_px = GetDesiredIconSizesForShortcut().back();
    gfx::ImageSkia image_skia = CreateDefaultApplicationIcon(icon_size_in_px);
    image_family.Add(gfx::Image(image_skia));
  }

  return image_family;
}

std::unique_ptr<ShortcutInfo> SetFavicon(
    std::unique_ptr<ShortcutInfo> shortcut_info,
    gfx::ImageFamily image_family) {
  shortcut_info->favicon = std::move(image_family);
  return shortcut_info;
}

}  // namespace

ShortcutInfo::ShortcutInfo() = default;

ShortcutInfo::~ShortcutInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<ShortcutInfo> BuildShortcutInfoWithoutFavicon(
    const AppId& app_id,
    const GURL& start_url,
    const base::FilePath& profile_path,
    const std::string& profile_name,
    const proto::WebAppOsIntegrationState& state) {
  auto shortcut_info = std::make_unique<ShortcutInfo>();

  shortcut_info->app_id = app_id;
  shortcut_info->url = start_url;
  DCHECK(state.has_shortcut());
  const proto::ShortcutDescription& shortcut_state = state.shortcut();
  DCHECK(shortcut_state.has_title());
  shortcut_info->title = base::UTF8ToUTF16(shortcut_state.title());
  DCHECK(shortcut_state.has_description());
  shortcut_info->description = base::UTF8ToUTF16(shortcut_state.description());
  shortcut_info->profile_path = profile_path;
  shortcut_info->profile_name = profile_name;
  shortcut_info->is_multi_profile = true;

  if (state.has_file_handling()) {
    shortcut_info->file_handler_extensions =
        GetFileExtensionsFromFileHandlingProto(state.file_handling());
    shortcut_info->file_handler_mime_types =
        GetMimeTypesFromFileHandlingProto(state.file_handling());
  }

  if (state.has_protocols_handled()) {
    for (const auto& protocol_handler : state.protocols_handled().protocols()) {
      DCHECK(protocol_handler.has_protocol());
      if (protocol_handler.has_protocol() &&
          !protocol_handler.protocol().empty()) {
        shortcut_info->protocol_handlers.emplace(protocol_handler.protocol());
      }
    }
  }

// TODO(crbug.com/1416965): Implement tests on Linux for using shortcuts_menu
// actions.
#if BUILDFLAG(IS_LINUX)
  const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos =
      CreateShortcutsMenuItemInfos(state.shortcut_menus());
  DCHECK_LE(shortcuts_menu_item_infos.size(), kMaxApplicationDockMenuItems);
  for (const auto& shortcuts_menu_item_info : shortcuts_menu_item_infos) {
    if (!shortcuts_menu_item_info.name.empty() &&
        !shortcuts_menu_item_info.url.is_empty()) {
      // Generates ID from the name by replacing all characters that are not
      // numbers, letters, or '-' with '-'.
      std::string id = base::UTF16ToUTF8(shortcuts_menu_item_info.name);
      RE2::GlobalReplace(&id, "[^a-zA-Z0-9\\-]", "-");
      shortcut_info->actions.emplace(
          id, base::UTF16ToUTF8(shortcuts_menu_item_info.name),
          shortcuts_menu_item_info.url);
    }
  }
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC)
  shortcut_info->handlers_per_profile =
      AppShimRegistry::Get()->GetHandlersForApp(app_id);
#endif

  return shortcut_info;
}

void PopulateFaviconForShortcutInfo(
    const WebApp* app,
    WebAppIconManager& icon_manager,
    std::unique_ptr<ShortcutInfo> shortcut_info_to_populate,
    base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)> callback) {
  DCHECK(app);

  // Build a common intersection between desired and downloaded icons.
  auto icon_sizes_in_px = base::STLSetIntersection<std::vector<SquareSizePx>>(
      app->downloaded_icon_sizes(IconPurpose::ANY),
      GetDesiredIconSizesForShortcut());

  auto populate_and_return_shortcut_info =
      base::BindOnce(&SetFavicon, std::move(shortcut_info_to_populate))
          .Then(std::move(callback));

  if (!icon_sizes_in_px.empty()) {
    icon_manager.ReadIcons(
        app->app_id(), IconPurpose::ANY, icon_sizes_in_px,
        base::BindOnce(&PackageIconsIntoImageFamily)
            .Then(std::move(populate_and_return_shortcut_info)));
    return;
  }

  // If there is no single icon at the desired sizes, we will resize what we can
  // get.
  SquareSizePx desired_icon_size = GetDesiredIconSizesForShortcut().back();
  icon_manager.ReadIconAndResize(
      app->app_id(), IconPurpose::ANY, desired_icon_size,
      base::BindOnce(&PackageIconsIntoImageFamily)
          .Then(std::move(populate_and_return_shortcut_info)));
}

std::vector<WebAppShortcutsMenuItemInfo> CreateShortcutsMenuItemInfos(
    const proto::ShortcutMenus& shortcut_menus) {
  std::vector<WebAppShortcutsMenuItemInfo> shortcut_menu_item_infos;
  for (const auto& shortcut_menu_info : shortcut_menus.shortcut_menu_info()) {
    WebAppShortcutsMenuItemInfo item_info;
    item_info.name = base::UTF8ToUTF16(shortcut_menu_info.shortcut_name());
    item_info.url = GURL(shortcut_menu_info.shortcut_launch_url());
    item_info.any = ConvertIconProtoDataToShortcutsMenuIcon(
        shortcut_menu_info.icon_data_any());
    item_info.maskable = ConvertIconProtoDataToShortcutsMenuIcon(
        shortcut_menu_info.icon_data_maskable());
    item_info.monochrome = ConvertIconProtoDataToShortcutsMenuIcon(
        shortcut_menu_info.icon_data_monochrome());
    shortcut_menu_item_infos.push_back(std::move(item_info));
  }
  return shortcut_menu_item_infos;
}

std::string GenerateApplicationNameFromInfo(const ShortcutInfo& shortcut_info) {
  if (shortcut_info.app_id.empty()) {
    return GenerateApplicationNameFromURL(shortcut_info.url);
  }

  return GenerateApplicationNameFromAppId(shortcut_info.app_id);
}

base::FilePath GetOsIntegrationResourcesDirectoryForApp(
    const base::FilePath& profile_path,
    const std::string& app_id,
    const GURL& url) {
  DCHECK(!profile_path.empty());
  base::FilePath app_data_dir(profile_path.Append(chrome::kWebAppDirname));

  if (!app_id.empty()) {
    return app_data_dir.AppendASCII(GenerateApplicationNameFromAppId(app_id));
  }

  std::string host(url.host());
  std::string scheme(url.has_scheme() ? url.scheme() : "http");
  std::string port(url.has_port() ? url.port() : "80");
  std::string scheme_port(scheme + "_" + port);

#if BUILDFLAG(IS_WIN)
  base::FilePath::StringType host_path(base::UTF8ToWide(host));
  base::FilePath::StringType scheme_port_path(base::UTF8ToWide(scheme_port));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  base::FilePath::StringType host_path(host);
  base::FilePath::StringType scheme_port_path(scheme_port);
#else
#error "Unknown platform"
#endif

  return app_data_dir.Append(host_path).Append(scheme_port_path);
}

base::span<const int> GetDesiredIconSizesForShortcut() {
  return base::span<const int>(kDesiredIconSizesForShortcut,
                               GetNumDesiredIconSizesForShortcut());
}

gfx::ImageSkia CreateDefaultApplicationIcon(int size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/860581): Create web_app_browser_resources.grd with the
  // default app icon. Remove dependency on extensions_browser_resources.h and
  // use IDR_WEB_APP_DEFAULT_ICON here.
  gfx::Image default_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_APP_DEFAULT_ICON);
  SkBitmap bmp = skia::ImageOperations::Resize(
      *default_icon.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST, size,
      size);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bmp);
  // We are on the UI thread, and this image can be used from the FILE thread,
  // for creating shortcut icon files.
  image_skia.MakeThreadSafe();

  return image_skia;
}

namespace internals {

void PostShortcutIOTask(base::OnceCallback<void(const ShortcutInfo&)> task,
                        std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ownership of |shortcut_info| moves to the Reply, which is guaranteed to
  // outlive the const reference.
  const ShortcutInfo& shortcut_info_ref = *shortcut_info;
  GetShortcutIOTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(std::move(task), std::cref(shortcut_info_ref)),
      base::BindOnce(
          [](std::unique_ptr<ShortcutInfo> shortcut_info) {
            // This lambda is to own and delete the shortcut info.
            shortcut_info.reset();
          },
          std::move(shortcut_info)));
}

void ScheduleCreatePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    const ShortcutLocations& creation_locations,
    ShortcutCreationReason reason,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    CreateShortcutsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PostShortcutIOTask(base::BindOnce(&CreatePlatformShortcutsAndPostCallback,
                                    shortcut_data_path, creation_locations,
                                    reason, std::move(callback)),
                     std::move(shortcut_info));
}

void ScheduleDeletePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    DeleteShortcutsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PostShortcutIOTask(base::BindOnce(&DeletePlatformShortcutsAndPostCallback,
                                    shortcut_data_path, std::move(callback)),
                     std::move(shortcut_info));
}

void ScheduleDeleteMultiProfileShortcutsForApp(const std::string& app_id,
                                               ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetShortcutIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteMultiProfileShortcutsForAppAndPostCallback, app_id,
                     std::move(callback)));
}

void PostShortcutIOTaskAndReplyWithResult(
    base::OnceCallback<Result(const ShortcutInfo&)> task,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    ResultCallback reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ownership of |shortcut_info| moves to the Reply, which is guaranteed to
  // outlive the const reference.
  const ShortcutInfo& shortcut_info_ref = *shortcut_info;
  GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(std::move(task), std::cref(shortcut_info_ref)),
      base::BindOnce(&DeleteShortcutInfoOnUIThread, std::move(shortcut_info),
                     std::move(reply)));
}

scoped_refptr<base::SequencedTaskRunner> GetShortcutIOTaskRunner() {
  return g_shortcuts_task_runner.Get();
}

base::FilePath GetShortcutDataDir(const ShortcutInfo& shortcut_info) {
  return GetOsIntegrationResourcesDirectoryForApp(
      shortcut_info.profile_path, shortcut_info.app_id, shortcut_info.url);
}

#if !BUILDFLAG(IS_MAC)
void DeleteMultiProfileShortcutsForApp(const std::string& app_id) {
  // Multi-profile shortcuts exist only on macOS.
  NOTREACHED();
}
#endif

}  // namespace internals
}  // namespace web_app
