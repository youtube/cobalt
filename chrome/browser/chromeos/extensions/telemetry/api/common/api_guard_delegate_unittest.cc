// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/api_guard_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/fake_hardware_info_delegate.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/shell.h"
#include "ash/webui/shimless_rma/3p_diagnostics/external_app_dialog.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"  // nogncheck
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_contents_observer.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

struct ExtensionInfoTestParams {
  ExtensionInfoTestParams(const std::string& extension_id,
                          const std::string& app_ui_url,
                          const std::string& matches_origin,
                          const std::string& manufacturer)
      : extension_id(extension_id),
        app_ui_url(app_ui_url),
        matches_origin(matches_origin),
        manufacturer(manufacturer) {}
  ExtensionInfoTestParams(const ExtensionInfoTestParams& other) = default;
  ~ExtensionInfoTestParams() = default;

  const std::string extension_id;
  const std::string app_ui_url;
  const std::string matches_origin;
  const std::string manufacturer;
};

const std::vector<ExtensionInfoTestParams> kAllExtensionInfoTestParams{
    // Make sure the Google extension is allowed for every OEM.
    ExtensionInfoTestParams(
        /*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
        /*app_ui_url=*/"https://googlechromelabs.github.io/",
        /*matches_origin=*/"*://googlechromelabs.github.io/*",
        /*manufacturer=*/"HP"),
    ExtensionInfoTestParams(
        /*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
        /*app_ui_url=*/"https://googlechromelabs.github.io/",
        /*matches_origin=*/"*://googlechromelabs.github.io/*",
        /*manufacturer=*/"ASUS"),
    // Make sure the extensions of each OEM are allowed on their device.
    ExtensionInfoTestParams(
        /*extension_id=*/"alnedpmllcfpgldkagbfbjkloonjlfjb",
        /*app_ui_url=*/"https://hpcs-appschr.hpcloud.hp.com",
        /*matches_origin=*/"https://hpcs-appschr.hpcloud.hp.com/*",
        /*manufacturer=*/"HP"),
    ExtensionInfoTestParams(
        /*extension_id=*/"hdnhcpcfohaeangjpkcjkgmgmjanbmeo",
        /*app_ui_url=*/
        "https://dlcdnccls.asus.com/app/myasus_for_chromebook/ ",
        /*matches_origin=*/"https://dlcdnccls.asus.com/*",
        /*manufacturer=*/"ASUS"),
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kUserEmail[] = "user@example.com";
#endif  // IS_CHROMEOS_ASH

// Tests that Chrome OS System Extensions must fulfill the requirements to
// access Telemetry Extension APIs. All tests are parameterized with the
// following parameters:
// * |extension_id| - id of the extension under test.
// * |app_ui_url| - page URL of the app associated with the extension's id.
// * |matches_origin| - externally_connectable's matches entry of the
//                      extension's manifest.json.
// Note: All tests must be defined using the TEST_P macro and must use the
// INSTANTIATE_TEST_SUITE_P macro to instantiate the test suite.
class ApiGuardDelegateTest
    : public BrowserWithTestWindowTest,
      public testing::WithParamInterface<ExtensionInfoTestParams> {
 public:
  ApiGuardDelegateTest() = default;
  ~ApiGuardDelegateTest() override = default;

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    CreateExtension();
    // Make sure device manufacturer is allowlisted.
    SetDeviceManufacturer(manufacturer());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    AddUserAndLogIn();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->is_current_user_device_owner = true;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));

    profile()->SetIsMainProfile(true);
    ASSERT_TRUE(profile()->IsMainProfile());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Explicitly removing the user is required; otherwise ProfileHelper keeps a
    // dangling pointer to the User.
    // TODO(b/208629291): Consider removing all users from ProfileHelper in the
    // destructor of ash::FakeChromeUserManager.
    if (GetFakeUserManager().GetActiveUser()) {
      GetFakeUserManager().RemoveUserFromList(
          GetFakeUserManager().GetActiveUser()->GetAccountId());
    }
    scoped_user_manager_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  extensions::ExtensionId extension_id() const {
    return GetParam().extension_id;
  }

  std::string app_ui_url() const { return GetParam().app_ui_url; }

  std::string matches_origin() const { return GetParam().matches_origin; }

  std::string manufacturer() const { return GetParam().manufacturer; }

  const extensions::Extension* extension() { return extension_.get(); }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FakeChromeUserManager& GetFakeUserManager() {
    return CHECK_DEREF(static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get()));
  }

  virtual void AddUserAndLogIn() {
    auto& user_manager = GetFakeUserManager();
    // Make sure the current user is affiliated.
    const AccountId account_id = AccountId::FromUserEmail(kUserEmail);
    user_manager.AddUser(account_id);
    user_manager.LoginUser(account_id);
    user_manager.SwitchActiveUser(account_id);
  }

  void SetUserAsOwner() {
    auto& user_manager = GetFakeUserManager();
    // Make sure the current user is affiliated.
    const AccountId account_id = AccountId::FromUserEmail(kUserEmail);
    user_manager.SetOwnerId(account_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void SetDeviceManufacturer(std::string manufacturer) {
    hardware_info_delegate_factory_ =
        std::make_unique<FakeHardwareInfoDelegate::Factory>(manufacturer);
    HardwareInfoDelegate::Factory::SetForTesting(
        hardware_info_delegate_factory_.get());
  }

  void OpenAppUIUrlAndSetCertificateWithStatus(net::CertStatus cert_status) {
    const base::FilePath certs_dir = net::GetTestCertsDirectory();
    scoped_refptr<net::X509Certificate> test_cert(
        net::ImportCertFromFile(certs_dir, "ok_cert.pem"));
    ASSERT_TRUE(test_cert);

    // Open the app page url and set valid certificate to bypass the
    // IsAppUiOpenAndSecure() check.
    AddTab(browser(), GURL(app_ui_url()));

    // AddTab() adds a new tab at index 0.
    auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
    auto* entry = web_contents->GetController().GetVisibleEntry();
    content::SSLStatus& ssl = entry->GetSSL();
    ssl.certificate = test_cert;
    ssl.cert_status = cert_status;
  }

 private:
  void CreateExtension() {
    extension_ =
        extensions::ExtensionBuilder("Test ChromeOS System Extension")
            .SetManifestVersion(3)
            .SetManifestKey("chromeos_system_extension", base::Value::Dict())
            .SetManifestKey(
                "externally_connectable",
                base::Value::Dict().Set(
                    "matches", base::Value::List().Append(matches_origin())))
            .SetID(extension_id())
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .Build();
  }

  scoped_refptr<const extensions::Extension> extension_;
  std::unique_ptr<HardwareInfoDelegate::Factory>
      hardware_info_delegate_factory_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_P(ApiGuardDelegateTest, CurrentUserNotOwner) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto& user_manager = GetFakeUserManager();
  // Make sure the current user is not the device owner.
  const AccountId regular_user = AccountId::FromUserEmail("regular@gmail.com");
  user_manager.SetOwnerId(regular_user);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto params = crosapi::mojom::BrowserInitParams::New();
  params->is_current_user_device_owner = false;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not run by the device owner", error.value());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_P(ApiGuardDelegateTest, OwnershipDelayed) {
  OpenAppUIUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);
  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;

  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  // Trigger async ownership retrieval.
  SetUserAsOwner();

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_P(ApiGuardDelegateTest, CurrentUserOwnerButNotMainLacrosProfile) {
  // Don't set the current profile as the main profile.
  profile()->SetIsMainProfile(false);
  ASSERT_FALSE(profile()->IsMainProfile());

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not run by the device owner", error.value());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_P(ApiGuardDelegateTest, AppNotOpen) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  SetUserAsOwner();
#endif  // IS_CHROMEOS_ASH
  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion app UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateTest, AppIsOpenButNotSecure) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  SetUserAsOwner();
#endif  // IS_CHROMEOS_ASH
  OpenAppUIUrlAndSetCertificateWithStatus(
      /*cert_status=*/net::CERT_STATUS_INVALID);

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion app UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateTest, ManufacturerNotAllowed) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  SetUserAsOwner();
#endif  // IS_CHROMEOS_ASH
  OpenAppUIUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            error.value());
}

TEST_P(ApiGuardDelegateTest, SkipManufacturerCheck) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  SetUserAsOwner();
#endif  // IS_CHROMEOS_ASH
  OpenAppUIUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);
  // Append the switch to skip the manufacturer check.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kTelemetryExtensionSkipManufacturerCheckForTesting);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

TEST_P(ApiGuardDelegateTest, NoError) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  SetUserAsOwner();
#endif  // IS_CHROMEOS_ASH
  OpenAppUIUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ApiGuardDelegateTest,
                         testing::ValuesIn(kAllExtensionInfoTestParams));

class ApiGuardDelegateAffiliatedUserTest : public ApiGuardDelegateTest {
 public:
  ApiGuardDelegateAffiliatedUserTest() = default;
  ~ApiGuardDelegateAffiliatedUserTest() override = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetUp() override {
    ApiGuardDelegateTest::SetUp();

    // Make sure the main user is affiliated.
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->session_type = crosapi::mojom::SessionType::kPublicSession;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
    ASSERT_TRUE(policy::PolicyLoaderLacros::IsMainUserAffiliated());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void AddUserAndLogIn() override {
    auto& user_manager = GetFakeUserManager();
    // Make sure the current user is affiliated.
    const AccountId account_id = AccountId::FromUserEmail("user@example.com");
    user_manager.AddUserWithAffiliation(account_id, /*is_affiliated=*/true);
    user_manager.LoginUser(account_id);
    user_manager.SwitchActiveUser(account_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_P(ApiGuardDelegateAffiliatedUserTest, ExtensionNotForceInstalled) {
  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not installed by the admin", error.value());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, AppNotOpen) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion app UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, AppIsOpenButNotSecure) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  OpenAppUIUrlAndSetCertificateWithStatus(
      /*cert_status=*/net::CERT_STATUS_INVALID);

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion app UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, ManufacturerNotAllowed) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  OpenAppUIUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            error.value());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, NoError) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  OpenAppUIUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());
  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ApiGuardDelegateAffiliatedUserTest,
                         testing::ValuesIn(kAllExtensionInfoTestParams));

// TODO(b/292227137): Migrate Shimless RMA app to LaCrOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)

class WebContentsCloseWaiter : public content::WebContentsObserver {
 public:
  explicit WebContentsCloseWaiter(content::WebContents* contents);
  WebContentsCloseWaiter(const WebContentsCloseWaiter&) = delete;
  WebContentsCloseWaiter& operator=(const WebContentsCloseWaiter&) = delete;

  void Wait() {
    // TODO: handle return value.
    std::ignore = future_.Wait();
  }

 private:
  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override;

  base::test::TestFuture<void> future_;
};

WebContentsCloseWaiter::WebContentsCloseWaiter(content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

void WebContentsCloseWaiter::WebContentsDestroyed() {
  future_.SetValue();
}

class ApiGuardDelegateShimlessRMAAppTest : public ApiGuardDelegateTest {
 public:
  ApiGuardDelegateShimlessRMAAppTest() = default;
  ~ApiGuardDelegateShimlessRMAAppTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {
            ::ash::features::kShimlessRMA3pDiagnostics,
            ::chromeos::features::kIWAForTelemetryExtensionAPI,
        },
        {});

    chromeos_system_extension_info_ =
        ScopedChromeOSSystemExtensionInfo::CreateForTesting();
    // TODO(b/293560424): Remove this override after we add some valid IWA id to
    // the allowlist.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        chromeos::switches::kTelemetryExtensionIwaIdOverrideForTesting,
        "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic");
    chromeos_system_extension_info_->ApplyCommandLineSwitchesForTesting();

    // Above overrides need to be done before creating extensions.
    ApiGuardDelegateTest::SetUp();

    ash::Shell::Get()->session_controller()->SetSessionInfo(ash::SessionInfo{
        .can_lock_screen = true,
        .should_lock_screen_automatically = false,
        .add_user_session_policy = ash::AddUserSessionPolicy::ALLOWED,
        .state = session_manager::SessionState::RMA,
    });
  }

  void TearDown() override {
    if (ash::shimless_rma::ExternalAppDialog::GetWebContents()) {
      WebContentsCloseWaiter waiter(
          ash::shimless_rma::ExternalAppDialog::GetWebContents());
      ash::shimless_rma::ExternalAppDialog::CloseForTesting();
      waiter.Wait();
    }
    ApiGuardDelegateTest::TearDown();
  }

 protected:
  void OpenShimlessRmaAppDialog() {
    ash::shimless_rma::ExternalAppDialog::InitParams params;
    params.context = profile();
    params.app_name = "App Name";
    params.content_url = GURL(app_ui_url());
    ash::shimless_rma::ExternalAppDialog::Show(params);

    // Wait for WebContents being created.
    base::RunLoop().RunUntilIdle();
    auto* content = ash::shimless_rma::ExternalAppDialog::GetWebContents();
    CHECK(content);

    CommitPendingLoad(&content->GetController());
  }

  // BrowserWithTestWindowTest overrides.
  TestingProfile* CreateProfile() override {
    return profile_manager()->CreateTestingProfile(
        ash::kShimlessRmaAppBrowserContextBaseName);
  }

  // ApiGuardDelegateTest overrides.
  void AddUserAndLogIn() override {
    // No user is logged in during Shimless RMA.
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ScopedChromeOSSystemExtensionInfo>
      chromeos_system_extension_info_;
};

TEST_P(ApiGuardDelegateShimlessRMAAppTest, IwaNotOpen) {
  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion app UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateShimlessRMAAppTest, ManufacturerNotAllowed) {
  OpenShimlessRmaAppDialog();

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            error.value());
}

TEST_P(ApiGuardDelegateShimlessRMAAppTest, NoError) {
  OpenShimlessRmaAppDialog();

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());
  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

INSTANTIATE_TEST_SUITE_P(
    IWA,
    ApiGuardDelegateShimlessRMAAppTest,
    testing::Values(ExtensionInfoTestParams(
        /*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
        /*app_ui_url=*/
        "isolated-app://"
        "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic",
        /*matches_origin=*/
        "isolated-app://"
        "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/*",
        /*manufacturer=*/"HP")));

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace chromeos
