// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_platform_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/crx_file/crx_verifier.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sync/model/string_ordinal.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "extensions/common/api/web_accessible_resources.h"
#include "extensions/common/api/web_accessible_resources_mv2.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_background_page_waiter.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

using extensions::mojom::ManifestLocation;

namespace extensions {

using extensions::service_worker_test_utils::TestServiceWorkerContextObserver;

ExtensionBrowserTest::ExtensionBrowserTest(ContextType context_type)
    : ExtensionPlatformBrowserTest(context_type),
#if BUILDFLAG(IS_CHROMEOS)
      set_chromeos_user_(true),
#endif
      override_prompt_for_external_extensions_(
          FeatureSwitch::prompt_for_external_extensions(),
          false),
#if BUILDFLAG(IS_WIN)
      user_desktop_override_(base::DIR_USER_DESKTOP),
      common_desktop_override_(base::DIR_COMMON_DESKTOP),
      user_quick_launch_override_(base::DIR_USER_QUICK_LAUNCH),
      start_menu_override_(base::DIR_START_MENU),
      common_start_menu_override_(base::DIR_COMMON_START_MENU),
#endif
      profile_(nullptr),
      verifier_format_override_(crx_file::VerifierFormat::CRX3) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

ExtensionBrowserTest::~ExtensionBrowserTest() = default;

ExtensionService* ExtensionBrowserTest::extension_service() {
  return ExtensionSystem::Get(profile())->extension_service();
}

ExtensionRegistry* ExtensionBrowserTest::extension_registry() {
  return ExtensionRegistry::Get(profile());
}

void ExtensionBrowserTest::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  last_loaded_extension_id_ = extension->id();
  VLOG(1) << "Got EXTENSION_LOADED notification.";
}

void ExtensionBrowserTest::OnShutdown(ExtensionRegistry* registry) {
  registry_observation_.Reset();
}

Profile* ExtensionBrowserTest::profile() {
  if (!profile_) {
    if (browser()) {
      profile_ = browser()->profile();
    } else {
      profile_ = ProfileManager::GetLastUsedProfile();
    }
  }
  return profile_;
}

bool ExtensionBrowserTest::ShouldEnableContentVerification() {
  return false;
}

bool ExtensionBrowserTest::ShouldEnableInstallVerification() {
  return false;
}

bool ExtensionBrowserTest::ShouldAllowMV2Extensions() {
  return true;
}

// static
const Extension* ExtensionBrowserTest::GetExtensionByPath(
    const ExtensionSet& extensions,
    const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath extension_path = base::MakeAbsoluteFilePath(path);
  EXPECT_TRUE(!extension_path.empty());
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (extension->path() == extension_path) {
      return extension.get();
    }
  }
  return nullptr;
}

void ExtensionBrowserTest::SetUp() {
  test_extension_cache_ = std::make_unique<ExtensionCacheFake>();
  ExtensionPlatformBrowserTest::SetUp();
}

void ExtensionBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  ExtensionPlatformBrowserTest::SetUpCommandLine(command_line);

  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");

  if (!ShouldEnableContentVerification()) {
    ignore_content_verification_ =
        std::make_unique<ScopedIgnoreContentVerifierForTest>();
  }

  if (!ShouldEnableInstallVerification()) {
    ignore_install_verification_ =
        std::make_unique<ScopedInstallVerifierBypassForTest>();
  }

  if (ShouldAllowMV2Extensions()) {
    mv2_enabler_.emplace();
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (set_chromeos_user_) {
    // This makes sure that we create the Default profile first, with no
    // ExtensionService and then the real profile with one, as we do when
    // running on chromeos.
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    "testuser@gmail.com");
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
  }
#endif
}

void ExtensionBrowserTest::SetUpOnMainThread() {
  observer_ =
      std::make_unique<ChromeExtensionTestNotificationObserver>(browser());
  if (extension_service()->updater()) {
    extension_service()->updater()->SetExtensionCacheForTesting(
        test_extension_cache_.get());
  }

  SetUpTestProtocolHandler();
  content::URLDataSource::Add(profile(),
                              std::make_unique<ThemeSource>(profile()));
  registry_observation_.Observe(ExtensionRegistry::Get(profile()));
}

void ExtensionBrowserTest::TearDownOnMainThread() {
  TearDownTestProtocolHandler();
  registry_observation_.Reset();
}

const Extension* ExtensionBrowserTest::LoadExtension(
    const base::FilePath& path) {
  return LoadExtension(path, {});
}

const Extension* ExtensionBrowserTest::LoadExtension(
    const base::FilePath& path,
    const LoadOptions& options) {
  base::FilePath extension_path;
  if (!extensions::browser_test_util::ModifyExtensionIfNeeded(
          options, context_type_, GetTestPreCount(), temp_dir_.GetPath(), path,
          &extension_path)) {
    return nullptr;
  }

  if (options.load_as_component) {
    // TODO(crbug.com/40166157): Decide if other load options
    // can/should be supported when load_as_component is true.
    DCHECK(!options.allow_in_incognito);
    DCHECK(!options.allow_file_access);
    DCHECK(!options.ignore_manifest_warnings);
    DCHECK(options.wait_for_renderers);
    DCHECK(options.install_param == nullptr);
    DCHECK(!options.wait_for_registration_stored);
    return LoadExtensionAsComponent(extension_path);
  }
  ChromeTestExtensionLoader loader(profile());
  loader.set_allow_incognito_access(options.allow_in_incognito);
  loader.set_allow_file_access(options.allow_file_access);
  loader.set_ignore_manifest_warnings(options.ignore_manifest_warnings);
  loader.set_wait_for_renderers(options.wait_for_renderers);

  if (options.install_param != nullptr) {
    loader.set_install_param(options.install_param);
  }

  std::unique_ptr<TestServiceWorkerContextObserver> registration_observer;

  if (options.wait_for_registration_stored) {
    registration_observer =
        std::make_unique<TestServiceWorkerContextObserver>(profile_);
  }

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(extension_path);
  if (extension) {
    last_loaded_extension_id_ = extension->id();
  }

  if (options.wait_for_registration_stored &&
      BackgroundInfo::IsServiceWorkerBased(extension.get())) {
    registration_observer->WaitForRegistrationStored();
  }

  return extension.get();
}

void ExtensionBrowserTest::DisableExtension(
    const ExtensionId& extension_id,
    const DisableReasonSet& disable_reasons) {
  extension_service()->DisableExtension(extension_id, disable_reasons);
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponentWithManifest(
    const base::FilePath& path,
    const base::FilePath::CharType* manifest_relative_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string manifest;
  if (!base::ReadFileToString(path.Append(manifest_relative_path), &manifest)) {
    return nullptr;
  }

  extension_service()->component_loader()->set_ignore_allowlist_for_testing(
      true);
  extensions::ExtensionId extension_id =
      extension_service()->component_loader()->Add(manifest, path);
  const Extension* extension =
      extension_registry()->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    return nullptr;
  }
  last_loaded_extension_id_ = extension->id();
  return extension;
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponent(
    const base::FilePath& path) {
  return LoadExtensionAsComponentWithManifest(path, kManifestFilename);
}

const Extension* ExtensionBrowserTest::LoadAndLaunchApp(
    const base::FilePath& path,
    bool uses_guest_view) {
  const Extension* app = LoadExtension(path);
  CHECK(app);
  content::CreateAndLoadWebContentsObserver app_loaded_observer(
      /*num_expected_contents=*/uses_guest_view ? 2 : 1);
  apps::AppLaunchParams params(
      app->id(), apps::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
  params.command_line = *base::CommandLine::ForCurrentProcess();
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(std::move(params));
  app_loaded_observer.Wait();

  return app;
}

Browser* ExtensionBrowserTest::LaunchAppBrowser(const Extension* extension) {
  return browsertest_util::LaunchAppBrowser(profile(), extension);
}

base::FilePath ExtensionBrowserTest::PackExtension(
    const base::FilePath& dir_path,
    int extra_run_flags) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath crx_path = temp_dir_.GetPath().AppendASCII("temp.crx");
  if (!base::DeleteFile(crx_path)) {
    ADD_FAILURE() << "Failed to delete crx: " << crx_path.value();
    return base::FilePath();
  }

  // Look for PEM files with the same name as the directory.
  base::FilePath pem_path =
      dir_path.ReplaceExtension(FILE_PATH_LITERAL(".pem"));
  base::FilePath pem_path_out;

  if (!base::PathExists(pem_path)) {
    pem_path = base::FilePath();
    pem_path_out = crx_path.DirName().AppendASCII("temp.pem");
    if (!base::DeleteFile(pem_path_out)) {
      ADD_FAILURE() << "Failed to delete pem: " << pem_path_out.value();
      return base::FilePath();
    }
  }

  return PackExtensionWithOptions(dir_path, crx_path, pem_path, pem_path_out,
                                  extra_run_flags);
}

base::FilePath ExtensionBrowserTest::PackExtensionWithOptions(
    const base::FilePath& dir_path,
    const base::FilePath& crx_path,
    const base::FilePath& pem_path,
    const base::FilePath& pem_out_path,
    int extra_run_flags) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::PathExists(dir_path)) {
    ADD_FAILURE() << "Extension dir not found: " << dir_path.value();
    return base::FilePath();
  }

  if (!base::PathExists(pem_path) && pem_out_path.empty()) {
    ADD_FAILURE() << "Must specify a PEM file or PEM output path";
    return base::FilePath();
  }

  std::unique_ptr<ExtensionCreator> creator(new ExtensionCreator());
  if (!creator->Run(dir_path, crx_path, pem_path, pem_out_path,
                    extra_run_flags | ExtensionCreator::kOverwriteCRX)) {
    ADD_FAILURE() << "ExtensionCreator::Run() failed: "
                  << creator->error_message();
    return base::FilePath();
  }

  if (!base::PathExists(crx_path)) {
    ADD_FAILURE() << crx_path.value() << " was not created.";
    return base::FilePath();
  }
  return crx_path;
}

const Extension* ExtensionBrowserTest::UpdateExtensionWaitForIdle(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      id, path, InstallUIType::kNone, std::move(expected_change),
      ManifestLocation::kInternal, GetWindowController(), Extension::NO_FLAGS,
      false, false);
}

const Extension* ExtensionBrowserTest::InstallExtensionWithUIAutoConfirm(
    const base::FilePath& path,
    std::optional<int> expected_change,
    Browser* browser) {
  return InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kAutoConfirm,
      std::move(expected_change), browser->extension_window_controller(),
      Extension::NO_FLAGS);
}

const Extension* ExtensionBrowserTest::InstallExtensionFromWebstore(
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kAutoConfirm,
      std::move(expected_change), ManifestLocation::kInternal,
      GetWindowController(), Extension::FROM_WEBSTORE, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(id, path, ui_type, std::move(expected_change),
                                  ManifestLocation::kInternal,
                                  GetWindowController(), Extension::NO_FLAGS,
                                  true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    std::optional<int> expected_change,
    WindowController* window_controller,
    Extension::InitFromValueFlags creation_flags) {
  return InstallOrUpdateExtension(id, path, ui_type, std::move(expected_change),
                                  ManifestLocation::kInternal,
                                  window_controller, creation_flags, true,
                                  false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    std::optional<int> expected_change,
    ManifestLocation install_source) {
  return InstallOrUpdateExtension(id, path, ui_type, std::move(expected_change),
                                  install_source, GetWindowController(),
                                  Extension::NO_FLAGS, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    std::optional<int> expected_change,
    ManifestLocation install_source,
    WindowController* window_controller,
    Extension::InitFromValueFlags creation_flags,
    bool install_immediately,
    bool grant_permissions) {
  ExtensionRegistry* registry = extension_registry();
  size_t num_before = registry->enabled_extensions().size();

  scoped_refptr<CrxInstaller> installer;
  std::optional<CrxInstallError> install_error;
  {
    std::unique_ptr<ScopedTestDialogAutoConfirm> prompt_auto_confirm;
    if (ui_type == InstallUIType::kCancel) {
      prompt_auto_confirm = std::make_unique<ScopedTestDialogAutoConfirm>(
          ScopedTestDialogAutoConfirm::CANCEL);
    } else if (ui_type == InstallUIType::kNormal) {
      prompt_auto_confirm = std::make_unique<ScopedTestDialogAutoConfirm>(
          ScopedTestDialogAutoConfirm::NONE);
    } else if (ui_type == InstallUIType::kAutoConfirm) {
      prompt_auto_confirm = std::make_unique<ScopedTestDialogAutoConfirm>(
          ScopedTestDialogAutoConfirm::ACCEPT);
    }

    // TODO(tessamac): Update callers to always pass an unpacked extension
    //                 and then always pack the extension here.
    base::FilePath crx_path = path;
    if (crx_path.Extension() != FILE_PATH_LITERAL(".crx")) {
      crx_path = PackExtension(path, ExtensionCreator::kNoRunFlags);
    }
    if (crx_path.empty()) {
      return nullptr;
    }

    std::unique_ptr<ExtensionInstallPrompt> install_ui;
    if (prompt_auto_confirm) {
      install_ui = std::make_unique<ExtensionInstallPrompt>(
          window_controller->GetActiveTab());
    }
    installer =
        CrxInstaller::Create(extension_service(), std::move(install_ui));
    installer->set_expected_id(id);
    installer->set_creation_flags(creation_flags);
    installer->set_install_source(install_source);
    installer->set_install_immediately(install_immediately);
    installer->set_allow_silent_install(grant_permissions);
    if (!installer->is_gallery_install()) {
      installer->set_off_store_install_allow_reason(
          CrxInstaller::OffStoreInstallAllowedInTest);
    }

    base::test::TestFuture<std::optional<CrxInstallError>>
        installer_done_future;
    installer->AddInstallerCallback(
        installer_done_future
            .GetCallback<const std::optional<CrxInstallError>&>());

    installer->InstallCrx(crx_path);

    install_error = installer_done_future.Get();
  }

  if (expected_change.has_value()) {
    size_t num_after = registry->enabled_extensions().size();
    EXPECT_EQ(num_before + expected_change.value(), num_after);
    if (num_before + expected_change.value() != num_after) {
      VLOG(1) << "Num extensions before: " << base::NumberToString(num_before)
              << " num after: " << base::NumberToString(num_after)
              << " Installed extensions follow:";

      for (const scoped_refptr<const Extension>& extension :
           registry->enabled_extensions()) {
        VLOG(1) << "  " << extension->id();
      }

      VLOG(1) << "Errors follow:";
      const std::vector<std::u16string>* errors =
          LoadErrorReporter::GetInstance()->GetErrors();
      for (const auto& error : *errors) {
        VLOG(1) << error;
      }

      return nullptr;
    }
  }

  // If possible, wait for the extension's background context to be loaded.
  // `WaitForExtensionViewsToLoad()` by itself is insufficient for this, since
  // it only waits for existent views registered in the process manager, and
  // the background context may not be registered yet.
  std::string reason_unused;
  bool extension_enabled =
      !install_error &&
      registry->enabled_extensions().Contains(installer->extension()->id());
  if (extension_enabled && ExtensionBackgroundPageWaiter::CanWaitFor(
                               *installer->extension(), reason_unused)) {
    ExtensionBackgroundPageWaiter(profile(), *installer->extension())
        .WaitForBackgroundInitialized();
  }

  if (!observer_->WaitForExtensionViewsToLoad()) {
    return nullptr;
  }

  if (install_error) {
    return nullptr;
  }

  // Even though we can already get the Extension from the CrxInstaller,
  // ensure it's also in the list of enabled extensions.
  return registry->enabled_extensions().GetByID(installer->extension()->id());
}

void ExtensionBrowserTest::ReloadExtension(
    const extensions::ExtensionId& extension_id) {
  scoped_refptr<const Extension> extension =
      extension_registry()->GetInstalledExtension(extension_id);
  ASSERT_TRUE(extension);
  TestExtensionRegistryObserver observer(extension_registry(), extension_id);
  extension_service()->ReloadExtension(extension_id);
  // Re-grab the extension after the reload to get the updated copy.
  extension = observer.WaitForExtensionLoaded();
  // We need to let other ExtensionRegistryObservers handle the extension load
  // in order to finish initialization.
  base::RunLoop().RunUntilIdle();

  // Wait for the background context, if any, to start up.
  std::string reason_unused;
  if (extension_registry()->enabled_extensions().Contains(extension_id) &&
      ExtensionBackgroundPageWaiter::CanWaitFor(*extension, reason_unused)) {
    ExtensionBackgroundPageWaiter(profile(), *extension)
        .WaitForBackgroundInitialized();
  }

  // Wait for any additionally-registered extension views to load.
  observer_->WaitForExtensionViewsToLoad();
}

void ExtensionBrowserTest::UnloadExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->UnloadExtension(extension_id,
                                       UnloadedExtensionReason::DISABLE);
}

void ExtensionBrowserTest::UninstallExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->UninstallExtension(
      extension_id, UNINSTALL_REASON_FOR_TESTING, nullptr);
}

void ExtensionBrowserTest::DisableExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->DisableExtension(extension_id,
                                        disable_reason::DISABLE_USER_ACTION);
}

void ExtensionBrowserTest::EnableExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->EnableExtension(extension_id);
}

bool ExtensionBrowserTest::WaitForPageActionVisibilityChangeTo(int count) {
  return observer_->WaitForPageActionVisibilityChangeTo(count);
}

bool ExtensionBrowserTest::WaitForExtensionViewsToLoad() {
  return observer_->WaitForExtensionViewsToLoad();
}

bool ExtensionBrowserTest::WaitForExtensionIdle(
    const extensions::ExtensionId& extension_id) {
  return observer_->WaitForExtensionIdle(extension_id);
}

bool ExtensionBrowserTest::WaitForExtensionNotIdle(
    const extensions::ExtensionId& extension_id) {
  return observer_->WaitForExtensionNotIdle(extension_id);
}

void ExtensionBrowserTest::OpenWindow(content::WebContents* contents,
                                      const GURL& url,
                                      bool newtab_process_should_equal_opener,
                                      bool should_succeed,
                                      content::WebContents** newtab_result) {
  content::WebContentsAddedObserver tab_added_observer;
  ASSERT_TRUE(content::ExecJs(contents, "window.open('" + url.spec() + "');"));
  content::WebContents* newtab = tab_added_observer.GetWebContents();
  ASSERT_TRUE(newtab);
  WaitForLoadStop(newtab);

  if (should_succeed) {
    EXPECT_EQ(url, newtab->GetLastCommittedURL());
    EXPECT_EQ(content::PAGE_TYPE_NORMAL,
              newtab->GetController().GetLastCommittedEntry()->GetPageType());
  } else {
    // "Failure" comes in two forms: redirecting to about:blank or showing an
    // error page. At least one should be true.
    EXPECT_TRUE(
        newtab->GetLastCommittedURL() == GURL(url::kAboutBlankURL) ||
        newtab->GetController().GetLastCommittedEntry()->GetPageType() ==
            content::PAGE_TYPE_ERROR);
  }

  if (newtab_process_should_equal_opener) {
    EXPECT_EQ(contents->GetPrimaryMainFrame()->GetSiteInstance(),
              newtab->GetPrimaryMainFrame()->GetSiteInstance());
  } else {
    EXPECT_NE(contents->GetPrimaryMainFrame()->GetSiteInstance(),
              newtab->GetPrimaryMainFrame()->GetSiteInstance());
  }

  if (newtab_result) {
    *newtab_result = newtab;
  }
}

bool ExtensionBrowserTest::NavigateInRenderer(content::WebContents* contents,
                                              const GURL& url) {
  EXPECT_TRUE(
      content::ExecJs(contents, "window.location = '" + url.spec() + "';"));
  bool result = content::WaitForLoadStop(contents);
  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
  return result;
}

ExtensionHost* ExtensionBrowserTest::FindHostWithPath(ProcessManager* manager,
                                                      const std::string& path,
                                                      int expected_hosts) {
  ExtensionHost* result_host = nullptr;
  int num_hosts = 0;
  for (ExtensionHost* host : manager->background_hosts()) {
    if (host->GetLastCommittedURL().path() == path) {
      EXPECT_FALSE(result_host);
      result_host = host;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  return result_host;
}

content::ServiceWorkerContext* ExtensionBrowserTest::GetServiceWorkerContext() {
  return GetServiceWorkerContext(profile());
}

// static
content::ServiceWorkerContext* ExtensionBrowserTest::GetServiceWorkerContext(
    content::BrowserContext* browser_context) {
  return service_worker_test_utils::GetServiceWorkerContext(browser_context);
}

WindowController* ExtensionBrowserTest::GetWindowController() {
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  // TODO(b/361838438): Provide an implementation for the desktop android build.
  return nullptr;
#else
  return browser()->extension_window_controller();
#endif
}

}  // namespace extensions
