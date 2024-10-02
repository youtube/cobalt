// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/profile_policy_connector.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/chrome_paths_lacros.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#endif

namespace web_app {

namespace {

constexpr char kUserTypesTestDir[] = "user_types";

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kGoodJsonTestDir[] = "good_json";

constexpr char kAppAllUrl[] = "https://www.google.com/all";
constexpr char kAppGuestUrl[] = "https://www.google.com/guest";
constexpr char kAppManagedUrl[] = "https://www.google.com/managed";
constexpr char kAppUnmanagedUrl[] = "https://www.google.com/unmanaged";
constexpr char kAppChildUrl[] = "https://www.google.com/child";
#endif

}  // namespace

class PreinstalledWebAppManagerTest : public testing::Test,
                                      public testing::WithParamInterface<bool> {
 public:
  PreinstalledWebAppManagerTest() { BuildAndInitFeatureList(); }

  PreinstalledWebAppManagerTest(const PreinstalledWebAppManagerTest&) = delete;
  PreinstalledWebAppManagerTest& operator=(
      const PreinstalledWebAppManagerTest&) = delete;
  ~PreinstalledWebAppManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_enabler_.reset();
#endif
    testing::Test::TearDown();
  }

 protected:
  void BuildAndInitFeatureList() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam())
      enabled_features.push_back(features::kUseWebAppDBInsteadOfExternalPrefs);
    else
      disabled_features.push_back(features::kUseWebAppDBInsteadOfExternalPrefs);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    disabled_features.push_back(features::kWebAppsCrosapi);
    disabled_features.push_back(ash::features::kLacrosPrimary);
#endif
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  std::vector<ExternalInstallOptions> LoadApps(
      base::StringPiece test_dir,
      Profile* profile = nullptr,
      bool disable_default_apps = false) {
    std::unique_ptr<TestingProfile> testing_profile;
    if (!profile) {
#if BUILDFLAG(IS_CHROMEOS)
      testing_profile = CreateProfileAndLogin();
      profile = testing_profile.get();
#else
      NOTREACHED();
#endif
    }

    base::FilePath config_dir = GetConfigDir(test_dir);
    SetPreinstalledWebAppConfigDirForTesting(&config_dir);

    if (profile_ != profile) {
      provider_ = FakeWebAppProvider::Get(profile);
      test::AwaitStartWebAppProviderAndSubsystems(profile);
      profile_ = profile;
    }

    if (!disable_default_apps) {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          switches::kDisableDefaultApps);
    }

    std::vector<ExternalInstallOptions> result;
    base::RunLoop run_loop;
    provider_->preinstalled_web_app_manager().LoadForTesting(
        base::BindLambdaForTesting(
            [&](std::vector<ExternalInstallOptions> install_options_list) {
              result = std::move(install_options_list);
              run_loop.Quit();
            }));
    run_loop.Run();

    SetPreinstalledWebAppConfigDirForTesting(nullptr);

    return result;
  }

  // Helper that creates simple test profile.
  std::unique_ptr<TestingProfile> CreateProfile(bool is_guest = false) {
    TestingProfile::Builder profile_builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    if (is_guest) {
      profile_builder.SetGuestSession();
    }

    return profile_builder.Build();
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Helper that creates simple test guest profile.
  std::unique_ptr<TestingProfile> CreateGuestProfile() {
    return CreateProfile(/*is_guest=*/true);
  }

  // Helper that creates simple test profile and logs it into user manager.
  // This makes profile appears as a primary profile in ChromeOS.
  std::unique_ptr<TestingProfile> CreateProfileAndLogin() {
    std::unique_ptr<TestingProfile> profile = CreateProfile();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile->GetProfileUserName(), "1234567890"));
    user_manager()->AddUser(account_id);
    user_manager()->LoginUser(account_id);
#endif
    return profile;
  }

  // Helper that creates simple test guest profile and logs it into user
  // manager. This makes profile appears as a primary profile in ChromeOS.
  std::unique_ptr<TestingProfile> CreateGuestProfileAndLogin() {
    std::unique_ptr<TestingProfile> profile = CreateGuestProfile();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager()->AddGuestUser();
    user_manager()->LoginUser(user_manager()->GetGuestAccountId());
#endif
    return profile;
  }

  void SetExtraWebAppsDir(base::StringPiece test_dir,
                          base::StringPiece extra_web_apps_dir) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kExtraWebAppsDir, extra_web_apps_dir);
#else
    base::FilePath config_dir = GetConfigDir(test_dir);
    auto default_paths = crosapi::mojom::DefaultPaths::New();
    default_paths->documents =
        base::PathService::CheckedGet(chrome::DIR_USER_DOCUMENTS);
    default_paths->downloads =
        base::PathService::CheckedGet(chrome::DIR_DEFAULT_DOWNLOADS);
    default_paths->preinstalled_web_app_config = config_dir;
    default_paths->preinstalled_web_app_extra_config =
        config_dir.AppendASCII(extra_web_apps_dir);
    chrome::SetLacrosDefaultPathsFromInitParams(default_paths.get());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void VerifySetOfApps(Profile* profile, const std::set<GURL>& expectations) {
    const auto install_options_list = LoadApps(kUserTypesTestDir, profile);
    ASSERT_EQ(expectations.size(), install_options_list.size());
    for (const auto& install_options : install_options_list)
      ASSERT_EQ(1u, expectations.count(install_options.install_url));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  void ExpectHistograms(int enabled, int disabled, int errors) {
    histograms_.ExpectUniqueSample(
        PreinstalledWebAppManager::kHistogramEnabledCount, enabled, 1);
    histograms_.ExpectUniqueSample(
        PreinstalledWebAppManager::kHistogramDisabledCount, disabled, 1);
    histograms_.ExpectUniqueSample(
        PreinstalledWebAppManager::kHistogramConfigErrorCount, errors, 1);
  }

  base::HistogramTester histograms_;

  ScopedTestingPreinstalledAppData preinstalled_web_app_override_;

 private:
  base::FilePath GetConfigDir(base::StringPiece test_dir) {
    // Uses the chrome/test/data/web_app_default_apps/test_dir directory
    // that holds the *.json data files from which tests should parse as app
    // configs.
    base::FilePath config_dir;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &config_dir)) {
      ADD_FAILURE()
          << "base::PathService::Get could not resolve chrome::DIR_TEST_DATA";
    }
    return config_dir.AppendASCII("web_app_default_apps").AppendASCII(test_dir);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FakeChromeUserManager* user_manager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  // To support primary/non-primary users.
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  base::test::ScopedCommandLine command_line_;
#endif
  raw_ptr<FakeWebAppProvider> provider_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
  // To support context of browser threads.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(PreinstalledWebAppManagerTest, ReplacementExtensionBlockedByPolicy) {
  using PolicyUpdater = extensions::ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>;
  auto test_profile = CreateProfile();
  sync_preferences::TestingPrefServiceSyncable* prefs =
      test_profile->GetTestingPrefService();

  GURL install_url("https://test.app");
  constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
  ExternalInstallOptions options(install_url, mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalDefault);
  options.user_type_allowlist = {"unmanaged"};
  options.uninstall_and_replace = {kExtensionId};
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating(
      []() { return std::make_unique<WebAppInstallInfo>(); });
  preinstalled_web_app_override_.apps.push_back(std::move(options));

  auto expect_present = [&]() {
    std::vector<ExternalInstallOptions> options_list =
        LoadApps(/*test_dir=*/"", test_profile.get());
    ASSERT_EQ(options_list.size(), 1u);
    EXPECT_EQ(options_list[0].install_url, install_url);
  };

  auto expect_not_present = [&]() {
    std::vector<ExternalInstallOptions> options_list =
        LoadApps(/*test_dir=*/"", test_profile.get());
    ASSERT_EQ(options_list.size(), 0u);
  };

  expect_present();

  PolicyUpdater(prefs).SetBlocklistedByDefault(false);
  expect_present();

  PolicyUpdater(prefs).SetBlocklistedByDefault(true);
  expect_not_present();

  PolicyUpdater(prefs).SetIndividualExtensionInstallationAllowed(kExtensionId,
                                                                 true);
  expect_present();

  PolicyUpdater(prefs).SetBlocklistedByDefault(false);
  PolicyUpdater(prefs).SetIndividualExtensionInstallationAllowed(kExtensionId,
                                                                 false);
  expect_not_present();

  // Force installing the replaced extension also blocks the replacement.
  PolicyUpdater(prefs).SetIndividualExtensionAutoInstalled(
      kExtensionId, /*update_url=*/{}, /*forced=*/true);

  expect_present();
}

// Only Chrome OS parses config files.
#if BUILDFLAG(IS_CHROMEOS)
TEST_P(PreinstalledWebAppManagerTest, GoodJson) {
  const auto install_options_list = LoadApps(kGoodJsonTestDir);

  // The good_json directory contains two good JSON files:
  // chrome_platform_status.json and google_io_2016.json.
  // google_io_2016.json is missing a "create_shortcuts" field, so the default
  // value of false should be used.
  std::vector<ExternalInstallOptions> test_install_options_list;
  {
    ExternalInstallOptions install_options(
        GURL("https://www.chromestatus.com/features"),
        mojom::UserDisplayMode::kBrowser,
        ExternalInstallSource::kExternalDefault);
    install_options.user_type_allowlist = {"unmanaged"};
    install_options.add_to_applications_menu = true;
    install_options.add_to_search = true;
    install_options.add_to_management = true;
    install_options.add_to_desktop = true;
    install_options.add_to_quick_launch_bar = false;
    install_options.require_manifest = true;
    install_options.disable_if_touchscreen_with_stylus_not_supported = false;
    test_install_options_list.push_back(std::move(install_options));
  }
  {
    ExternalInstallOptions install_options(
        GURL("https://events.google.com/io2016/?utm_source=web_app_manifest"),
        mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    install_options.user_type_allowlist = {"unmanaged"};
    install_options.add_to_applications_menu = true;
    install_options.add_to_search = true;
    install_options.add_to_management = true;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.require_manifest = true;
    install_options.disable_if_touchscreen_with_stylus_not_supported = false;
    install_options.uninstall_and_replace.push_back("migrationsourceappid");
    test_install_options_list.push_back(std::move(install_options));
  }

  EXPECT_EQ(test_install_options_list.size(), install_options_list.size());
  for (const auto& install_option : test_install_options_list) {
    EXPECT_TRUE(base::Contains(install_options_list, install_option));
  }
  ExpectHistograms(/*enabled=*/2, /*disabled=*/0, /*errors=*/0);
}

TEST_P(PreinstalledWebAppManagerTest, BadJson) {
  const auto app_infos = LoadApps("bad_json");

  // The bad_json directory contains one (malformed) JSON file.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_P(PreinstalledWebAppManagerTest, TxtButNoJson) {
  const auto app_infos = LoadApps("txt_but_no_json");

  // The txt_but_no_json directory contains one file, and the contents of that
  // file is valid JSON, but that file's name does not end with ".json".
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/0);
}

TEST_P(PreinstalledWebAppManagerTest, MixedJson) {
  const auto app_infos = LoadApps("mixed_json");

  // The mixed_json directory contains one empty JSON file, one malformed JSON
  // file and one good JSON file. ScanDirForExternalWebAppsForTesting should
  // still pick up that one good JSON file: polytimer.json.
  EXPECT_EQ(1u, app_infos.size());
  if (app_infos.size() == 1) {
    EXPECT_EQ(app_infos[0].install_url.spec(),
              std::string("https://polytimer.rocks/?homescreen=1"));
  }
  ExpectHistograms(/*enabled=*/1, /*disabled=*/0, /*errors=*/2);
}

TEST_P(PreinstalledWebAppManagerTest, MissingAppUrl) {
  const auto app_infos = LoadApps("missing_app_url");

  // The missing_app_url directory contains one JSON file which is correct
  // except for a missing "app_url" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_P(PreinstalledWebAppManagerTest, EmptyAppUrl) {
  const auto app_infos = LoadApps("empty_app_url");

  // The empty_app_url directory contains one JSON file which is correct
  // except for an empty "app_url" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_P(PreinstalledWebAppManagerTest, InvalidAppUrl) {
  const auto app_infos = LoadApps("invalid_app_url");

  // The invalid_app_url directory contains one JSON file which is correct
  // except for an invalid "app_url" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_P(PreinstalledWebAppManagerTest, TrueHideFromUser) {
  const auto app_infos = LoadApps("true_hide_from_user");

  EXPECT_EQ(1u, app_infos.size());
  const auto& app = app_infos[0];
  EXPECT_FALSE(app.add_to_applications_menu);
  EXPECT_FALSE(app.add_to_search);
  EXPECT_FALSE(app.add_to_management);
  ExpectHistograms(/*enabled=*/1, /*disabled=*/0, /*errors=*/0);
}

TEST_P(PreinstalledWebAppManagerTest, InvalidHideFromUser) {
  const auto app_infos = LoadApps("invalid_hide_from_user");

  // The invalid_hide_from_user directory contains on JSON file which is correct
  // except for an invalid "hide_from_user" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_P(PreinstalledWebAppManagerTest, InvalidCreateShortcuts) {
  const auto app_infos = LoadApps("invalid_create_shortcuts");

  // The invalid_create_shortcuts directory contains one JSON file which is
  // correct except for an invalid "create_shortcuts" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_P(PreinstalledWebAppManagerTest, MissingLaunchContainer) {
  const auto app_infos = LoadApps("missing_launch_container");

  // The missing_launch_container directory contains one JSON file which is
  // correct except for a missing "launch_container" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_P(PreinstalledWebAppManagerTest, InvalidLaunchContainer) {
  const auto app_infos = LoadApps("invalid_launch_container");

  // The invalid_launch_container directory contains one JSON file which is
  // correct except for an invalid "launch_container" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_P(PreinstalledWebAppManagerTest, InvalidUninstallAndReplace) {
  const auto app_infos = LoadApps("invalid_uninstall_and_replace");

  // The invalid_uninstall_and_replace directory contains 2 JSON files which are
  // correct except for invalid "uninstall_and_replace" fields.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/2);
}

TEST_P(PreinstalledWebAppManagerTest, PreinstalledWebAppInstallDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kPreinstalledWebAppInstallation);
  const auto app_infos = LoadApps(kGoodJsonTestDir);

  EXPECT_EQ(0u, app_infos.size());
  histograms_.ExpectTotalCount(
      PreinstalledWebAppManager::kHistogramConfigErrorCount, 0);
  histograms_.ExpectTotalCount(
      PreinstalledWebAppManager::kHistogramEnabledCount, 0);
  histograms_.ExpectTotalCount(
      PreinstalledWebAppManager::kHistogramDisabledCount, 0);
}

TEST_P(PreinstalledWebAppManagerTest, EnabledByFinch) {
  base::AutoReset<bool> testing_scope =
      SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();

  const auto app_infos = LoadApps("enabled_by_finch");

  // The enabled_by_finch directory contains two JSON file containing apps
  // that have field trials. As the matching feature is enabled, they should be
  // in our list of apps to install.
  EXPECT_EQ(2u, app_infos.size());
  ExpectHistograms(/*enabled=*/2, /*disabled=*/0, /*errors=*/0);
}

TEST_P(PreinstalledWebAppManagerTest, NotEnabledByFinch) {
  const auto app_infos = LoadApps("enabled_by_finch");

  // The enabled_by_finch directory contains two JSON file containing apps
  // that have field trials. As the matching feature isn't enabled, they should
  // not be in our list of apps to install.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/2, /*errors=*/0);
}

TEST_P(PreinstalledWebAppManagerTest, GuestUser) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // App service is available for OTR profile in Guest mode.
  auto primary_profile = CreateGuestProfileAndLogin();
  auto* otr_profile =
      primary_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  VerifySetOfApps(otr_profile, {GURL(kAppAllUrl), GURL(kAppGuestUrl)});
#else
  VerifySetOfApps(CreateGuestProfileAndLogin().get(),
                  {GURL(kAppAllUrl), GURL(kAppGuestUrl)});
#endif
}

TEST_P(PreinstalledWebAppManagerTest, UnmanagedUser) {
  VerifySetOfApps(CreateProfileAndLogin().get(),
                  {GURL(kAppAllUrl), GURL(kAppUnmanagedUrl)});
}

TEST_P(PreinstalledWebAppManagerTest, ManagedUser) {
  const auto profile = CreateProfileAndLogin();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  VerifySetOfApps(profile.get(), {GURL(kAppAllUrl), GURL(kAppManagedUrl)});
}

TEST_P(PreinstalledWebAppManagerTest, ManagedGuestUser) {
  profiles::testing::ScopedTestManagedGuestSession test_managed_guest_session;
  const auto profile = CreateProfileAndLogin();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  VerifySetOfApps(profile.get(), {});
}

TEST_P(PreinstalledWebAppManagerTest, ChildUser) {
  const auto profile = CreateProfileAndLogin();
  profile->SetIsSupervisedProfile();
  EXPECT_TRUE(profile->IsChild());
  VerifySetOfApps(profile.get(), {GURL(kAppAllUrl), GURL(kAppChildUrl)});
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_P(PreinstalledWebAppManagerTest, NonPrimaryProfile) {
  VerifySetOfApps(CreateProfile().get(),
                  {GURL(kAppAllUrl), GURL(kAppUnmanagedUrl)});
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_P(PreinstalledWebAppManagerTest, ExtraWebApps) {
  // The extra_web_apps directory contains two JSON files in different named
  // subdirectories. The --extra-web-apps-dir switch should control which
  // directory apps are loaded from.
  SetExtraWebAppsDir("extra_web_apps", "model1");

  const auto app_infos = LoadApps("extra_web_apps");
  EXPECT_EQ(1u, app_infos.size());
  ExpectHistograms(/*enabled=*/1, /*disabled=*/0, /*errors=*/0);
}

TEST_P(PreinstalledWebAppManagerTest, ExtraWebAppsNoMatchingDirectory) {
  SetExtraWebAppsDir("extra_web_apps", "model3");

  const auto app_infos = LoadApps("extra_web_apps");
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/0);
}
#else
// No app is expected for non-ChromeOS builds.
TEST_P(PreinstalledWebAppManagerTest, NoApp) {
  EXPECT_TRUE(LoadApps(kUserTypesTestDir, CreateProfile().get()).empty());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
class DisabledPreinstalledWebAppManagerTest
    : public PreinstalledWebAppManagerTest {
 public:
  DisabledPreinstalledWebAppManagerTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableDefaultApps);
  }
};

TEST_P(DisabledPreinstalledWebAppManagerTest, LoadConfigsWhileDisabled) {
  EXPECT_EQ(LoadApps(kGoodJsonTestDir, /*profile=*/nullptr,
                     /*disable_default_apps=*/true)
                .size(),
            0u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         DisabledPreinstalledWebAppManagerTest,
                         ::testing::Bool());
#endif  // #if BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(All, PreinstalledWebAppManagerTest, ::testing::Bool());

}  // namespace web_app
