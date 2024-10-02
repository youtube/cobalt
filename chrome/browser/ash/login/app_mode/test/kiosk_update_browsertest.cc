// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/test_kiosk_extension_builder.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/test_app_data_load_waiter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/result_catcher.h"

namespace ash {

namespace {

// An offline enable test app. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/iiigpodgfihagabpagjehoocpakbnclp
// An app profile with version 1.0.0 installed is in
//   chrome/test/data/chromeos/app_mode/offline_enabled_app_profile
// The version 2.0.0 crx is in
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
const char kTestOfflineEnabledKioskApp[] = "iiigpodgfihagabpagjehoocpakbnclp";

// An app to test local fs data persistence across app update. V1 app writes
// data into local fs. V2 app reads and verifies the data.
// Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/abbjjkefakmllanciinhgjgjamdmlbdg
const char kTestLocalFsKioskApp[] = "abbjjkefakmllanciinhgjgjamdmlbdg";

// Testing apps for testing kiosk multi-app feature. All the crx files are in
//    chrome/test/data/chromeos/app_mode/webstore/downloads.

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/primary_app
const char kTestPrimaryKioskApp[] = "fclmjfpgiaifbnbnlpmdjhicolkapihc";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/secondary_app_1
const char kTestSecondaryApp1[] = "elbhpkeieolijdlflcplbbabceggjknh";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/secondary_app_2
const char kTestSecondaryApp2[] = "coamgmmgmjeeaodkbpdajekljacgfhkc";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/secondary_app_3
const char kTestSecondaryApp3[] = "miccbahcahimnejpdoaafjeolookhoem";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/
//         secondary_extensions_1
const char kTestSecondaryExtension[] = "pegeblegnlhnpgghhjblhchdllfijodp";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/
//         shared_module_primary_app
const char kTestSharedModulePrimaryApp[] = "kidkeddeanfhailinhfokehpolmfdppa";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/secondary_app
const char kTestSecondaryApp[] = "ffceghmcpipkneddgikbgoagnheejdbf";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/shared_module
const char kTestSharedModuleId[] = "hpanhkopkhnkpcmnedlnjmkfafmlamak";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/
//         secondary_extension
const char kTestSecondaryExt[] = "meaknlbicgahoejcchpnkenkmbekcddf";

// Fake usb stick mount path.
const char kFakeUsbMountPathUpdatePass[] =
    "chromeos/app_mode/external_update/update_pass";
const char kFakeUsbMountPathNoManifest[] =
    "chromeos/app_mode/external_update/no_manifest";
const char kFakeUsbMountPathBadManifest[] =
    "chromeos/app_mode/external_update/bad_manifest";
const char kFakeUsbMountPathLowerAppVersion[] =
    "chromeos/app_mode/external_update/lower_app_version";
const char kFakeUsbMountPathLowerCrxVersion[] =
    "chromeos/app_mode/external_update/lower_crx_version";
const char kFakeUsbMountPathBadCrx[] =
    "chromeos/app_mode/external_update/bad_crx";

// Placeholder for an icon, as the icon is required by the kiosk app manager.
const char kFakeIconURL[] = "/chromeos/app_mode/red16x16.png";

bool IsAppInstalled(const std::string& app_id, const std::string& version) {
  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(app_profile);
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(app_profile)
          ->GetInstalledExtension(app_id);
  return app != nullptr && version == app->version().GetString();
}

extensions::Manifest::Type GetAppType(const std::string& app_id) {
  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(app_profile);
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(app_profile)
          ->GetInstalledExtension(app_id);
  DCHECK(app);
  return app->GetType();
}

class KioskFakeDiskMountManager : public file_manager::FakeDiskMountManager {
 public:
  KioskFakeDiskMountManager() = default;

  KioskFakeDiskMountManager(const KioskFakeDiskMountManager&) = delete;
  KioskFakeDiskMountManager& operator=(const KioskFakeDiskMountManager&) =
      delete;

  ~KioskFakeDiskMountManager() override = default;

  void set_usb_mount_path(const std::string& usb_mount_path) {
    usb_mount_path_ = usb_mount_path;
  }

  void MountUsbStick() {
    DCHECK(!usb_mount_path_.empty());
    MountPath(usb_mount_path_, "", "", {}, MountType::kDevice,
              MountAccessMode::kReadOnly, base::DoNothing());
  }

  void UnMountUsbStick() {
    DCHECK(!usb_mount_path_.empty());
    UnmountPath(usb_mount_path_,
                disks::DiskMountManager::UnmountPathCallback());
  }

 private:
  std::string usb_mount_path_;
};

}  // namespace

class KioskUpdateTest : public KioskBaseTest {
 public:
  KioskUpdateTest() = default;

  KioskUpdateTest(const KioskUpdateTest&) = delete;
  KioskUpdateTest& operator=(const KioskUpdateTest&) = delete;

  ~KioskUpdateTest() override = default;

  struct TestAppInfo {
    std::string id;
    std::string version;
    std::string crx_filename;
    extensions::Manifest::Type type;
    TestAppInfo() = default;
    TestAppInfo(const std::string& id,
                const std::string& version,
                const std::string& crx_filename,
                extensions::Manifest::Type type)
        : id(id), version(version), crx_filename(crx_filename), type(type) {}
    ~TestAppInfo() = default;
  };

 protected:
  void SetUp() override {
    fake_disk_mount_manager_ = new KioskFakeDiskMountManager();
    disks::DiskMountManager::InitializeForTesting(fake_disk_mount_manager_);

    KioskBaseTest::SetUp();
  }

  void TearDown() override {
    disks::DiskMountManager::Shutdown();
    KioskBaseTest::TearDown();
  }

  void SetUpOnMainThread() override {
    // For update tests, we cache the app in the PRE part, and then we load it
    // in the test, so we need to both store the apps list on teardown (so that
    // the app manager would accept existing files in its extension cache on the
    // next startup) and copy the list to our stub settings provider as well.
    settings_helper_.CopyStoredValue(kAccountsPrefDeviceLocalAccounts);

    KioskBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    settings_helper_.StoreCachedDeviceSetting(kAccountsPrefDeviceLocalAccounts);
    KioskBaseTest::TearDownOnMainThread();
  }

  void PreCacheApp(const std::string& app_id,
                   const std::string& version,
                   const std::string& crx_file,
                   bool wait_for_app_data) {
    set_test_app_id(app_id);
    set_test_app_version(version);
    set_test_crx_file(crx_file);

    KioskAppManager* manager = KioskAppManager::Get();
    TestAppDataLoadWaiter waiter(manager, app_id, version);
    ReloadKioskApps();
    if (wait_for_app_data) {
      waiter.WaitForAppData();
    } else {
      waiter.Wait();
    }
    EXPECT_TRUE(waiter.loaded());
    std::string cached_version;
    base::FilePath file_path;
    EXPECT_TRUE(manager->GetCachedCrx(app_id, &file_path, &cached_version));
    EXPECT_EQ(version, cached_version);
  }

  void UpdateExternalCache(const std::string& version,
                           const std::string& crx_file) {
    set_test_app_version(version);
    set_test_crx_file(crx_file);
    SetupTestAppUpdateCheck();

    KioskAppManager* manager = KioskAppManager::Get();
    TestAppDataLoadWaiter waiter(manager, test_app_id(), version);
    KioskAppManager::Get()->UpdateExternalCache();
    waiter.Wait();
    EXPECT_TRUE(waiter.loaded());
    std::string cached_version;
    base::FilePath file_path;
    EXPECT_TRUE(
        manager->GetCachedCrx(test_app_id(), &file_path, &cached_version));
    EXPECT_EQ(version, cached_version);
  }

  void SetupFakeDiskMountManagerMountPath(const std::string& mount_path) {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII(mount_path);
    fake_disk_mount_manager_->set_usb_mount_path(test_data_dir.value());
  }

  void SimulateUpdateAppFromUsbStick(const std::string& usb_mount_path,
                                     bool* app_update_notified,
                                     bool* update_success) {
    SetupFakeDiskMountManagerMountPath(usb_mount_path);
    KioskAppExternalUpdateWaiter waiter(KioskAppManager::Get(), test_app_id());
    fake_disk_mount_manager_->MountUsbStick();
    waiter.Wait();
    fake_disk_mount_manager_->UnMountUsbStick();
    *update_success = waiter.update_success();
    *app_update_notified = waiter.app_update_notified();
  }

  void PreCacheAndLaunchApp(const std::string& app_id,
                            const std::string& version,
                            const std::string& crx_file) {
    set_test_app_id(app_id);
    set_test_app_version(version);
    set_test_crx_file(crx_file);
    PrepareAppLaunch();
    SimulateNetworkOnline();
    EXPECT_TRUE(LaunchApp(test_app_id()));
    WaitForAppLaunchSuccess();
    EXPECT_EQ(version, GetInstalledAppVersion().GetString());
  }

  void LaunchKioskWithSecondaryApps(
      const TestAppInfo& primary_app,
      const std::vector<TestAppInfo>& secondary_apps) {
    // Pre-cache the primary app.
    PreCacheApp(primary_app.id, primary_app.version, primary_app.crx_filename,
                /*wait_for_app_data=*/false);

    set_test_app_id(primary_app.id);
    fake_cws()->SetNoUpdate(primary_app.id);
    for (const auto& app : secondary_apps) {
      fake_cws()->SetUpdateCrx(app.id, app.crx_filename, app.version);
    }

    // Launch the primary app.
    SimulateNetworkOnline();
    ASSERT_TRUE(LaunchApp(test_app_id()));
    WaitForAppLaunchWithOptions(false, true);

    // Verify the primary app and the secondary apps are all installed.
    EXPECT_TRUE(IsAppInstalled(primary_app.id, primary_app.version));
    for (const auto& app : secondary_apps) {
      EXPECT_TRUE(IsAppInstalled(app.id, app.version));
      EXPECT_EQ(GetAppType(app.id), app.type);
    }
  }

  void LaunchTestKioskAppWithTwoSecondaryApps() {
    TestAppInfo primary_app(kTestPrimaryKioskApp, "1.0.0",
                            std::string(kTestPrimaryKioskApp) + "-1.0.0.crx",
                            extensions::Manifest::TYPE_PLATFORM_APP);
    SetupAppDetailInFakeCws(primary_app);

    std::vector<TestAppInfo> secondary_apps;
    TestAppInfo secondary_app_1(kTestSecondaryApp1, "1.0.0",
                                std::string(kTestSecondaryApp1) + "-1.0.0.crx",
                                extensions::Manifest::TYPE_PLATFORM_APP);
    SetupAppDetailInFakeCws(secondary_app_1);
    secondary_apps.push_back(secondary_app_1);
    TestAppInfo secondary_app_2(kTestSecondaryApp2, "1.0.0",
                                std::string(kTestSecondaryApp2) + "-1.0.0.crx",
                                extensions::Manifest::TYPE_PLATFORM_APP);
    SetupAppDetailInFakeCws(secondary_app_2);
    secondary_apps.push_back(secondary_app_2);

    LaunchKioskWithSecondaryApps(primary_app, secondary_apps);
  }

  void LaunchTestKioskAppWithSecondaryExtension() {
    TestAppInfo primary_app(kTestPrimaryKioskApp, "24.0.0",
                            std::string(kTestPrimaryKioskApp) + "-24.0.0.crx",
                            extensions::Manifest::TYPE_PLATFORM_APP);
    SetupAppDetailInFakeCws(primary_app);

    std::vector<TestAppInfo> secondary_apps;
    TestAppInfo secondary_extension(
        kTestSecondaryExtension, "1.0.0",
        std::string(kTestSecondaryExtension) + "-1.0.0.crx",
        extensions::Manifest::TYPE_EXTENSION);
    secondary_apps.push_back(secondary_extension);

    LaunchKioskWithSecondaryApps(primary_app, secondary_apps);
  }

  void LaunchAppWithSharedModuleAndSecondaryApp() {
    TestAppInfo primary_app(
        kTestSharedModulePrimaryApp, "1.0.0",
        std::string(kTestSharedModulePrimaryApp) + "-1.0.0.crx",
        extensions::Manifest::TYPE_PLATFORM_APP);
    SetupAppDetailInFakeCws(primary_app);

    std::vector<TestAppInfo> secondary_apps;
    TestAppInfo secondary_app(kTestSecondaryApp, "1.0.0",
                              std::string(kTestSecondaryApp) + "-1.0.0.crx",
                              extensions::Manifest::TYPE_PLATFORM_APP);
    secondary_apps.push_back(secondary_app);
    // Setting up FakeCWS for shared module is the same for shared module as
    // for kiosk secondary apps.
    TestAppInfo shared_module(kTestSharedModuleId, "1.0.0",
                              std::string(kTestSharedModuleId) + "-1.0.0.crx",
                              extensions::Manifest::TYPE_SHARED_MODULE);
    secondary_apps.push_back(shared_module);

    LaunchKioskWithSecondaryApps(primary_app, secondary_apps);
    EXPECT_TRUE(IsAppInstalled(shared_module.id, shared_module.version));
  }

  void LaunchAppWithSharedModule() {
    TestAppInfo primary_app(
        kTestSharedModulePrimaryApp, "2.0.0",
        std::string(kTestSharedModulePrimaryApp) + "-2.0.0.crx",
        extensions::Manifest::TYPE_PLATFORM_APP);
    SetupAppDetailInFakeCws(primary_app);

    std::vector<TestAppInfo> secondary_apps;
    // Setting up FakeCWS for shared module is the same for shared module as
    // for kiosk secondary apps.
    TestAppInfo shared_module(kTestSharedModuleId, "1.0.0",
                              std::string(kTestSharedModuleId) + "-1.0.0.crx",
                              extensions::Manifest::TYPE_SHARED_MODULE);
    secondary_apps.push_back(shared_module);

    LaunchKioskWithSecondaryApps(primary_app, secondary_apps);
  }

  bool PrimaryAppUpdateIsPending() const {
    Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
    return !!extensions::ExtensionSystem::Get(app_profile)
                 ->extension_service()
                 ->GetPendingExtensionUpdate(test_app_id());
  }

  void SetupAppDetailInFakeCws(const TestAppInfo& app) {
    TestKioskExtensionBuilder app_builder(
        extensions::Manifest::TYPE_PLATFORM_APP, app.id);
    app_builder.set_version(app.version);
    std::string manifest_json;
    base::JSONWriter::Write(*app_builder.Build()->manifest()->value(),
                            &manifest_json);
    // In these tests we need to provide basic app detail, not necessary correct
    // one, just to prevent KioskAppData to remove the app.
    fake_cws()->SetAppDetails(app.id, /*localized_name=*/"Test App",
                              /*icon_url=*/kFakeIconURL, manifest_json);
  }

 private:
  class KioskAppExternalUpdateWaiter : public KioskAppManagerObserver {
   public:
    KioskAppExternalUpdateWaiter(KioskAppManager* manager,
                                 const std::string& app_id)
        : runner_(nullptr), manager_(manager), app_id_(app_id) {
      manager_->AddObserver(this);
    }

    KioskAppExternalUpdateWaiter(const KioskAppExternalUpdateWaiter&) = delete;
    KioskAppExternalUpdateWaiter& operator=(
        const KioskAppExternalUpdateWaiter&) = delete;

    ~KioskAppExternalUpdateWaiter() override { manager_->RemoveObserver(this); }

    void Wait() {
      if (quit_) {
        return;
      }
      runner_ = std::make_unique<base::RunLoop>();
      runner_->Run();
    }

    bool update_success() const { return update_success_; }

    bool app_update_notified() const { return app_update_notified_; }

   private:
    // KioskAppManagerObserver overrides:
    void OnKioskAppCacheUpdated(const std::string& app_id) override {
      if (app_id_ != app_id) {
        return;
      }
      app_update_notified_ = true;
    }

    void OnKioskAppExternalUpdateComplete(bool success) override {
      quit_ = true;
      update_success_ = success;
      if (runner_.get()) {
        runner_->Quit();
      }
    }

    std::unique_ptr<base::RunLoop> runner_;
    raw_ptr<KioskAppManager, ExperimentalAsh> manager_;
    const std::string app_id_;
    bool quit_ = false;
    bool update_success_ = false;
    bool app_update_notified_ = false;
  };

  // Owned by DiskMountManager.
  raw_ptr<KioskFakeDiskMountManager, ExperimentalAsh> fake_disk_mount_manager_;
};

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_LaunchOfflineEnabledAppNoNetwork) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchOfflineEnabledAppNoNetwork) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  SimulateNetworkOffline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();

  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
  EXPECT_EQ(ManifestLocation::kExternalPref, GetInstalledAppLocation());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       PRE_LaunchCachedOfflineEnabledAppNoNetwork) {
  PreCacheApp(kTestOfflineEnabledKioskApp, "1.0.0",
              std::string(kTestOfflineEnabledKioskApp) + "_v1.crx",
              /*wait_for_app_data=*/true);
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       LaunchCachedOfflineEnabledAppNoNetwork) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  EXPECT_TRUE(
      KioskAppManager::Get()->HasCachedCrx(kTestOfflineEnabledKioskApp));
  SimulateNetworkOffline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();

  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
  EXPECT_EQ(ManifestLocation::kExternalPref, GetInstalledAppLocation());
}

// Network offline, app v1.0 has run before, has cached v2.0 crx and v2.0 should
// be installed and launched during next launch.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       PRE_LaunchCachedNewVersionOfflineEnabledAppNoNetwork) {
  // Install and launch v1 app.
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
  // Update cache for v2 app.
  UpdateExternalCache("2.0.0",
                      std::string(kTestOfflineEnabledKioskApp) + ".crx");
  // The installed app is still in v1.
  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       LaunchCachedNewVersionOfflineEnabledAppNoNetwork) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  EXPECT_TRUE(KioskAppManager::Get()->HasCachedCrx(test_app_id()));

  SimulateNetworkOffline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();

  // v2 app should have been installed.
  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
  EXPECT_EQ(ManifestLocation::kExternalPref, GetInstalledAppLocation());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_LaunchOfflineEnabledAppNoUpdate) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchOfflineEnabledAppNoUpdate) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  fake_cws()->SetNoUpdate(test_app_id());

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();

  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
  EXPECT_EQ(ManifestLocation::kExternalPref, GetInstalledAppLocation());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_LaunchOfflineEnabledAppHasUpdate) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchOfflineEnabledAppHasUpdate) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  fake_cws()->SetUpdateCrx(test_app_id(),
                           "iiigpodgfihagabpagjehoocpakbnclp.crx", "2.0.0");

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();

  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
  EXPECT_EQ(ManifestLocation::kExternalPref, GetInstalledAppLocation());
}

// Pre-cache v1 kiosk app, then launch the app without network,
// plug in usb stick with a v2 app for offline updating.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_UsbStickUpdateAppNoNetwork) {
  PreCacheApp(kTestOfflineEnabledKioskApp, "1.0.0",
              std::string(kTestOfflineEnabledKioskApp) + "_v1.crx",
              /*wait_for_app_data=*/true);

  set_test_app_id(kTestOfflineEnabledKioskApp);
  SimulateNetworkOffline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();
  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());

  // Simulate mounting of usb stick with v2 app on the stick.
  bool update_success;
  bool app_update_notified;
  SimulateUpdateAppFromUsbStick(kFakeUsbMountPathUpdatePass,
                                &app_update_notified, &update_success);
  EXPECT_TRUE(update_success);
  EXPECT_TRUE(app_update_notified);

  // The v2 kiosk app is loaded into external cache, but won't be installed
  // until next time the device is started.
  base::FilePath crx_path;
  std::string cached_version;
  EXPECT_TRUE(KioskAppManager::Get()->GetCachedCrx(test_app_id(), &crx_path,
                                                   &cached_version));
  EXPECT_EQ("2.0.0", cached_version);
  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
}

// Restart the device, verify the app has been updated to v2.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, UsbStickUpdateAppNoNetwork) {
  // Verify the kiosk app has been updated to v2.
  set_test_app_id(kTestOfflineEnabledKioskApp);
  SimulateNetworkOffline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();
  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
}

// Usb stick is plugged in without a manifest file on it.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, UsbStickUpdateAppNoManifest) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());

  // Simulate mounting of usb stick with v2 app on the stick.
  bool update_success;
  bool app_update_notified;
  SimulateUpdateAppFromUsbStick(kFakeUsbMountPathNoManifest,
                                &app_update_notified, &update_success);
  EXPECT_FALSE(update_success);

  // Kiosk app is not updated.
  base::FilePath crx_path;
  std::string cached_version;
  EXPECT_TRUE(KioskAppManager::Get()->GetCachedCrx(test_app_id(), &crx_path,
                                                   &cached_version));
  EXPECT_EQ("1.0.0", cached_version);
}

// Usb stick is plugged in with a bad manifest file on it.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, UsbStickUpdateAppBadManifest) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());

  // Simulate mounting of usb stick with v2 app on the stick.
  bool update_success;
  bool app_update_notified;
  SimulateUpdateAppFromUsbStick(kFakeUsbMountPathBadManifest,
                                &app_update_notified, &update_success);
  EXPECT_FALSE(update_success);

  // Kiosk app is not updated.
  base::FilePath crx_path;
  std::string cached_version;
  EXPECT_TRUE(KioskAppManager::Get()->GetCachedCrx(test_app_id(), &crx_path,
                                                   &cached_version));
  EXPECT_EQ("1.0.0", cached_version);
}

// Usb stick is plugged in with a lower version of crx file specified in
// manifest.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, UsbStickUpdateAppLowerAppVersion) {
  // Precache v2 version of app.
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "2.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + ".crx");
  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());

  // Simulate mounting of usb stick with v1 app on the stick.
  bool update_success;
  bool app_update_notified;
  SimulateUpdateAppFromUsbStick(kFakeUsbMountPathLowerAppVersion,
                                &app_update_notified, &update_success);
  EXPECT_FALSE(update_success);

  // Kiosk app is NOT updated to the lower version.
  base::FilePath crx_path;
  std::string cached_version;
  EXPECT_TRUE(KioskAppManager::Get()->GetCachedCrx(test_app_id(), &crx_path,
                                                   &cached_version));
  EXPECT_EQ("2.0.0", cached_version);
}

// Usb stick is plugged in with a v1 crx file, although the manifest says
// this is a v3 version.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, UsbStickUpdateAppLowerCrxVersion) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "2.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + ".crx");
  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());

  // Simulate mounting of usb stick with v1 crx file on the stick, although
  // the manifest says it is v3 app.
  bool update_success;
  bool app_update_notified;
  SimulateUpdateAppFromUsbStick(kFakeUsbMountPathLowerCrxVersion,
                                &app_update_notified, &update_success);
  EXPECT_FALSE(update_success);

  // Kiosk app is NOT updated to the lower version.
  base::FilePath crx_path;
  std::string cached_version;
  EXPECT_TRUE(KioskAppManager::Get()->GetCachedCrx(test_app_id(), &crx_path,
                                                   &cached_version));
  EXPECT_EQ("2.0.0", cached_version);
}

// Usb stick is plugged in with a bad crx file.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, UsbStickUpdateAppBadCrx) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());

  // Simulate mounting of usb stick with v1 crx file on the stick, although
  // the manifest says it is v3 app.
  bool update_success;
  bool app_update_notified;
  SimulateUpdateAppFromUsbStick(kFakeUsbMountPathBadCrx, &app_update_notified,
                                &update_success);
  EXPECT_FALSE(update_success);

  // Kiosk app is NOT updated.
  base::FilePath crx_path;
  std::string cached_version;
  EXPECT_TRUE(KioskAppManager::Get()->GetCachedCrx(test_app_id(), &crx_path,
                                                   &cached_version));
  EXPECT_EQ("1.0.0", cached_version);
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_PermissionChange) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "2.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + ".crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PermissionChange) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  set_test_app_version("2.0.0");
  set_test_crx_file(test_app_id() + "_v2_permission_change.crx");

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();

  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_PreserveLocalData) {
  // Installs v1 app and writes some local data.
  set_test_app_id(kTestLocalFsKioskApp);
  set_test_app_version("1.0.0");
  set_test_crx_file(test_app_id() + ".crx");

  extensions::ResultCatcher catcher;
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(true /* check_launch_data */,
                              false /* terminate_app */);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PreserveLocalData) {
  // Update existing v1 app installed in PRE_PreserveLocalData to v2
  // that reads and verifies the local data.
  set_test_app_id(kTestLocalFsKioskApp);
  set_test_app_version("2.0.0");
  set_test_crx_file(test_app_id() + "_v2_read_and_verify_data.crx");
  extensions::ResultCatcher catcher;
  StartExistingAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(true /* check_launch_data */,
                              false /* terminate_app */);

  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests the primary app install with required platform version. The test
// has three runs:
//   1. Install an app.
//   2. App update is delayed because the required platform version is not
//      compliant.
//   3. Platform version changed and the new app is installed because it is
//      compliant now.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       PRE_PRE_IncompliantPlatformDelayInstall) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp, "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_IncompliantPlatformDelayInstall) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_VERSION=1233.0.0", base::Time::Now());

  set_test_app_id(kTestOfflineEnabledKioskApp);
  set_test_app_version("2.0.0");
  set_test_crx_file(test_app_id() + "_v2_required_platform_version_added.crx");

  // Fake auto launch.
  ReloadAutolaunchKioskApps();
  KioskAppManager::Get()->SetEnableAutoLaunch(true);
  KioskAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(
      kTestOfflineEnabledKioskApp);

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();

  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
  EXPECT_TRUE(PrimaryAppUpdateIsPending());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, IncompliantPlatformDelayInstall) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_VERSION=1234.0.0", base::Time::Now());

  set_test_app_id(kTestOfflineEnabledKioskApp);
  set_test_app_version("2.0.0");
  set_test_crx_file(test_app_id() + "_v2_required_platform_version_added.crx");

  // Fake auto launch.
  ReloadAutolaunchKioskApps();
  KioskAppManager::Get()->SetEnableAutoLaunch(true);
  KioskAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(
      kTestOfflineEnabledKioskApp);

  SimulateNetworkOnline();

  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();

  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
  EXPECT_FALSE(PrimaryAppUpdateIsPending());
}

// Tests that app is installed for the first time even on an incompliant
// platform.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, IncompliantPlatformFirstInstall) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_VERSION=1234.0.0", base::Time::Now());

  set_test_app_id(kTestOfflineEnabledKioskApp);
  set_test_app_version("2.0.0");
  set_test_crx_file(test_app_id() + "_v2_required_platform_version_added.crx");

  // Fake auto launch.
  ReloadAutolaunchKioskApps();
  KioskAppManager::Get()->SetEnableAutoLaunch(true);
  KioskAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(
      kTestOfflineEnabledKioskApp);

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchSuccess();

  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
  EXPECT_FALSE(PrimaryAppUpdateIsPending());
}

/* ***** Test Kiosk multi-app feature ***** */

// Launch a primary kiosk app which has two secondary apps.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       LaunchTestKioskAppWithTwoSecondaryApps) {
  LaunchTestKioskAppWithTwoSecondaryApps();
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_UpdateMultiAppKioskRemoveOneApp) {
  LaunchTestKioskAppWithTwoSecondaryApps();
}

// Update the primary app to version 2 which removes one of the secondary app
// from its manifest.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, UpdateMultiAppKioskRemoveOneApp) {
  TestAppInfo primary_app(kTestPrimaryKioskApp, "2.0.0",
                          std::string(kTestPrimaryKioskApp) + "-2.0.0.crx",
                          extensions::Manifest::TYPE_PLATFORM_APP);
  set_test_app_id(primary_app.id);
  fake_cws()->SetUpdateCrx(primary_app.id, primary_app.crx_filename,
                           primary_app.version);
  SetupAppDetailInFakeCws(primary_app);
  fake_cws()->SetNoUpdate(kTestSecondaryApp1);
  fake_cws()->SetNoUpdate(kTestSecondaryApp2);

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchWithOptions(false, true);

  // Verify the secondary app kTestSecondaryApp1 is removed.
  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
  EXPECT_FALSE(IsAppInstalled(kTestSecondaryApp1, "1.0.0"));
  EXPECT_TRUE(IsAppInstalled(kTestSecondaryApp2, "1.0.0"));
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_UpdateMultiAppKioskAddOneApp) {
  LaunchTestKioskAppWithTwoSecondaryApps();
}

// Update the primary app to version 3 which adds a new secondary app in its
// manifest.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, UpdateMultiAppKioskAddOneApp) {
  TestAppInfo primary_app(kTestPrimaryKioskApp, "3.0.0",
                          std::string(kTestPrimaryKioskApp) + "-3.0.0.crx",
                          extensions::Manifest::TYPE_PLATFORM_APP);
  set_test_app_id(primary_app.id);
  fake_cws()->SetUpdateCrx(primary_app.id, primary_app.crx_filename,
                           primary_app.version);
  SetupAppDetailInFakeCws(primary_app);
  fake_cws()->SetNoUpdate(kTestSecondaryApp1);
  fake_cws()->SetNoUpdate(kTestSecondaryApp2);
  TestAppInfo secondary_app(kTestSecondaryApp3, "1.0.0",
                            std::string(kTestSecondaryApp3) + "-1.0.0.crx",
                            extensions::Manifest::TYPE_PLATFORM_APP);
  fake_cws()->SetUpdateCrx(secondary_app.id, secondary_app.crx_filename,
                           secondary_app.version);
  SetupAppDetailInFakeCws(secondary_app);

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchWithOptions(false, true);

  // Verify the secondary app kTestSecondaryApp3 is installed.
  EXPECT_EQ("3.0.0", GetInstalledAppVersion().GetString());
  EXPECT_TRUE(IsAppInstalled(kTestSecondaryApp1, "1.0.0"));
  EXPECT_TRUE(IsAppInstalled(kTestSecondaryApp2, "1.0.0"));
  EXPECT_TRUE(IsAppInstalled(kTestSecondaryApp3, "1.0.0"));
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchKioskAppWithSecondaryExtension) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-22a4b826-851a-4065-a32b-273a0e261bf3");

  LaunchTestKioskAppWithSecondaryExtension();
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       LaunchAppWithSharedModuleAndSecondaryApp) {
  LaunchAppWithSharedModuleAndSecondaryApp();
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       PRE_UpdateAppWithSharedModuleRemoveAllSecondaryApps) {
  LaunchAppWithSharedModuleAndSecondaryApp();
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       UpdateAppWithSharedModuleRemoveAllSecondaryApps) {
  set_test_app_id(kTestSharedModulePrimaryApp);
  fake_cws()->SetUpdateCrx(
      kTestSharedModulePrimaryApp,
      std::string(kTestSharedModulePrimaryApp) + "-2.0.0.crx", "2.0.0");
  fake_cws()->SetNoUpdate(kTestSecondaryApp1);
  fake_cws()->SetNoUpdate(kTestSharedModuleId);

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchWithOptions(false, true);

  // Verify the secondary app is removed.
  EXPECT_TRUE(IsAppInstalled(kTestSharedModuleId, "1.0.0"));
  EXPECT_FALSE(IsAppInstalled(kTestSecondaryApp1, "1.0.0"));
}

// This simulates the stand-alone ARC kiosk app case. The primary app has a
// shared ARC runtime but no secondary apps.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchAppWithSharedModuleNoSecondary) {
  LaunchAppWithSharedModule();
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_LaunchAppWithUpdatedModule) {
  LaunchAppWithSharedModule();
  // Verify the shared module is installed with version 1.0.0.
  EXPECT_TRUE(IsAppInstalled(kTestSharedModuleId, "1.0.0"));
}

// This simulates the case the shared module is updated to a newer version.
// See crbug.com/555083.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchAppWithUpdatedModule) {
  // No update for primary app, while the shared module is set up to a new
  // version on cws.
  set_test_app_id(kTestSharedModulePrimaryApp);
  fake_cws()->SetNoUpdate(kTestSharedModulePrimaryApp);
  fake_cws()->SetUpdateCrx(kTestSharedModuleId,
                           std::string(kTestSharedModuleId) + "-2.0.0.crx",
                           "2.0.0");

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchWithOptions(false, true);

  // Verify the shared module is updated to the new version after primary app
  // is launched.
  EXPECT_TRUE(IsAppInstalled(kTestSharedModuleId, "2.0.0"));
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       LaunchAppWithSecondaryArcLikeAppAndExtension) {
  TestAppInfo primary_app(
      kTestSharedModulePrimaryApp, "3.0.0",
      std::string(kTestSharedModulePrimaryApp) + "-3.0.0.crx",
      extensions::Manifest::TYPE_PLATFORM_APP);
  SetupAppDetailInFakeCws(primary_app);

  std::vector<TestAppInfo> secondary_apps;
  // Setting up FakeCWS for shared module is the same for shared module as
  // for kiosk secondary apps.
  TestAppInfo shared_module(kTestSharedModuleId, "1.0.0",
                            std::string(kTestSharedModuleId) + "-1.0.0.crx",
                            extensions::Manifest::TYPE_SHARED_MODULE);
  secondary_apps.push_back(shared_module);
  // The secondary app has a shared module, which is similar to an ARC app.
  TestAppInfo secondary_app(kTestSecondaryApp, "2.0.0",
                            std::string(kTestSecondaryApp) + "-2.0.0.crx",
                            extensions::Manifest::TYPE_PLATFORM_APP);
  secondary_apps.push_back(secondary_app);
  TestAppInfo secondary_ext(kTestSecondaryExt, "1.0.0",
                            std::string(kTestSecondaryExt) + "-1.0.0.crx",
                            extensions::Manifest::TYPE_EXTENSION);
  secondary_apps.push_back(secondary_ext);

  LaunchKioskWithSecondaryApps(primary_app, secondary_apps);
}

}  // namespace ash
