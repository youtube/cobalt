// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_extensions_external_loader.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_update_found_test_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::extensions::mojom::ManifestLocation;

// Information about found external extension file: {version, crx_path}.
using TestCrxInfo = std::tuple<std::string, std::string>;

constexpr char kTestExtensionId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

constexpr char kTestExtensionUpdateManifest[] =
    "extensions/good_v1_update_manifest.xml";

constexpr char kTestExtensionCRXVersion[] = "1.0.0.0";

class TestExternalProviderVisitor
    : public extensions::ExternalProviderInterface::VisitorInterface {
 public:
  TestExternalProviderVisitor() = default;

  TestExternalProviderVisitor(const TestExternalProviderVisitor&) = delete;
  TestExternalProviderVisitor& operator=(const TestExternalProviderVisitor&) =
      delete;

  ~TestExternalProviderVisitor() override = default;

  const std::map<std::string, TestCrxInfo>& loaded_crx_files() const {
    return loaded_crx_files_;
  }

  void WaitForReady() {
    if (ready_)
      return;
    ready_waiter_ = std::make_unique<base::RunLoop>();
    ready_waiter_->Run();
    ready_waiter_.reset();
  }

  void WaitForFileFound() {
    if (!loaded_crx_files_.empty())
      return;
    file_waiter_ = std::make_unique<base::RunLoop>();
    file_waiter_->Run();
    file_waiter_.reset();
  }

  void ClearLoadedFiles() { loaded_crx_files_.clear(); }

  // extensions::ExternalProviderInterface::VisitorInterface:
  bool OnExternalExtensionFileFound(
      const extensions::ExternalInstallInfoFile& info) override {
    EXPECT_EQ(0u, loaded_crx_files_.count(info.extension_id));
    EXPECT_EQ(ManifestLocation::kInternal, info.crx_location)
        << info.extension_id;

    loaded_crx_files_.emplace(
        info.extension_id,
        TestCrxInfo(info.version.GetString(), info.path.value()));
    if (file_waiter_)
      file_waiter_->Quit();
    return true;
  }

  bool OnExternalExtensionUpdateUrlFound(
      const extensions::ExternalInstallInfoUpdateUrl& info,
      bool force_update) override {
    return true;
  }

  void OnExternalProviderReady(
      const extensions::ExternalProviderInterface* provider) override {
    ready_ = true;
    if (ready_waiter_)
      ready_waiter_->Quit();
  }

  void OnExternalProviderUpdateComplete(
      const extensions::ExternalProviderInterface* provider,
      const std::vector<extensions::ExternalInstallInfoUpdateUrl>&
          update_url_extensions,
      const std::vector<extensions::ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    ADD_FAILURE() << "Found updated extensions.";
  }

 private:
  bool ready_ = false;

  std::map<std::string, TestCrxInfo> loaded_crx_files_;

  std::unique_ptr<base::RunLoop> ready_waiter_;

  std::unique_ptr<base::RunLoop> file_waiter_;
};

class DemoExtensionsExternalLoaderTest : public testing::Test {
 public:
  DemoExtensionsExternalLoaderTest()
      : fake_user_manager_(std::make_unique<FakeChromeUserManager>()),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  DemoExtensionsExternalLoaderTest(const DemoExtensionsExternalLoaderTest&) =
      delete;
  DemoExtensionsExternalLoaderTest& operator=(
      const DemoExtensionsExternalLoaderTest&) = delete;

  ~DemoExtensionsExternalLoaderTest() override = default;

  void SetUp() override {
    demo_mode_test_helper_ = std::make_unique<DemoModeTestHelper>();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_loader_factory_);
    profile_ = std::make_unique<TestingProfile>();
    profile_->ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetDemoMode();
  }

  void TearDown() override {
    profile_.reset();
    demo_mode_test_helper_.reset();
  }

 protected:
  std::string GetTestResourcePath(const std::string& rel_path) {
    return demo_mode_test_helper_->GetDemoResourcesPath()
        .Append(rel_path)
        .value();
  }

  bool SetExtensionsConfig(const base::Value::Dict& config) {
    std::string config_str;
    if (!base::JSONWriter::Write(config, &config_str))
      return false;

    base::FilePath config_path =
        demo_mode_test_helper_->GetDemoResourcesPath().Append(
            "demo_extensions.json");
    return base::WriteFile(config_path, config_str);
  }

  void AddExtensionToConfig(const std::string& id,
                            const absl::optional<std::string>& version,
                            const absl::optional<std::string>& path,
                            base::Value::Dict& config) {
    base::Value::Dict extension;
    if (version.has_value()) {
      extension.Set(extensions::ExternalProviderImpl::kExternalVersion,
                    version.value());
    }
    if (path.has_value()) {
      extension.Set(extensions::ExternalProviderImpl::kExternalCrx,
                    path.value());
    }
    config.Set(id, std::move(extension));
  }

  std::unique_ptr<extensions::ExternalProviderImpl> CreateExternalProvider(
      extensions::ExternalProviderInterface::VisitorInterface* visitor) {
    return std::make_unique<extensions::ExternalProviderImpl>(
        visitor,
        base::MakeRefCounted<DemoExtensionsExternalLoader>(
            base::FilePath() /*cache_dir*/),
        profile_.get(), ManifestLocation::kInternal,
        ManifestLocation::kInternal,
        extensions::Extension::FROM_WEBSTORE |
            extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  }

  TestExternalProviderVisitor external_provider_visitor_;

  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_;

  std::unique_ptr<TestingProfile> profile_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<DemoModeTestHelper> demo_mode_test_helper_;

  content::BrowserTaskEnvironment task_environment_;

 private:
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;
};

TEST_F(DemoExtensionsExternalLoaderTest, NoDemoExtensionsConfig) {
  demo_mode_test_helper_->InitializeSession();

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest, InvalidDemoExtensionsConfig) {
  demo_mode_test_helper_->InitializeSession();

  base::Value::Dict config;
  config.Set("invalid_config", "invalid_config");

  ASSERT_TRUE(SetExtensionsConfig(config));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest, SingleDemoExtension) {
  demo_mode_test_helper_->InitializeSession();

  base::Value::Dict config;
  AddExtensionToConfig(std::string(32, 'a'), absl::make_optional("1.0.0"),
                       absl::make_optional("extensions/a.crx"), config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("extensions/a.crx"))}};
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, MultipleDemoExtension) {
  demo_mode_test_helper_->InitializeSession();

  base::Value::Dict config;
  AddExtensionToConfig(std::string(32, 'a'), absl::make_optional("1.0.0"),
                       absl::make_optional("extensions/a.crx"), config);
  AddExtensionToConfig(std::string(32, 'b'), absl::make_optional("1.1.0"),
                       absl::make_optional("b.crx"), config);
  AddExtensionToConfig(std::string(32, 'c'), absl::make_optional("2.0.0"),
                       absl::make_optional("c.crx"), config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("extensions/a.crx"))},
      {std::string(32, 'b'),
       TestCrxInfo("1.1.0", GetTestResourcePath("b.crx"))},
      {std::string(32, 'c'),
       TestCrxInfo("2.0.0", GetTestResourcePath("c.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, CrxPathWithAbsolutePath) {
  demo_mode_test_helper_->InitializeSession();

  base::Value::Dict config;
  AddExtensionToConfig(std::string(32, 'a'), absl::make_optional("1.0.0"),
                       absl::make_optional("a.crx"), config);
  AddExtensionToConfig(std::string(32, 'b'), absl::make_optional("1.1.0"),
                       absl::make_optional(GetTestResourcePath("b.crx")),
                       config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, ExtensionWithPathMissing) {
  demo_mode_test_helper_->InitializeSession();

  base::Value::Dict config;
  AddExtensionToConfig(std::string(32, 'a'), absl::make_optional("1.0.0"),
                       absl::make_optional("a.crx"), config);
  AddExtensionToConfig(std::string(32, 'b'), absl::make_optional("1.1.0"),
                       absl::nullopt, config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, ExtensionWithVersionMissing) {
  demo_mode_test_helper_->InitializeSession();

  base::Value::Dict config;
  AddExtensionToConfig(std::string(32, 'a'), absl::make_optional("1.0.0"),
                       absl::make_optional("a.crx"), config);
  AddExtensionToConfig(std::string(32, 'b'), absl::nullopt,
                       absl::make_optional("b.crx"), config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, DemoResourcesNotLoaded) {
  demo_mode_test_helper_->InitializeSessionWithPendingComponent();
  demo_mode_test_helper_->FailLoadingComponent();

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest,
       StartLoaderBeforeOfflineResourcesLoaded) {
  demo_mode_test_helper_->InitializeSessionWithPendingComponent();

  base::Value::Dict config;
  AddExtensionToConfig(std::string(32, 'a'), absl::make_optional("1.0.0"),
                       absl::make_optional("a.crx"), config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();

  demo_mode_test_helper_->FinishLoadingComponent();

  external_provider_visitor_.WaitForReady();
  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
}

TEST_F(DemoExtensionsExternalLoaderTest,
       StartLoaderBeforeOfflineResourcesLoadFails) {
  demo_mode_test_helper_->InitializeSessionWithPendingComponent();

  base::Value::Dict config;
  AddExtensionToConfig(std::string(32, 'a'), absl::make_optional("1.0.0"),
                       absl::make_optional("a.crx"), config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();

  demo_mode_test_helper_->FailLoadingComponent();

  external_provider_visitor_.WaitForReady();
  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest, LoadApp) {
  extensions::ExtensionUpdateFoundTestObserver extension_update_found_observer;
  demo_mode_test_helper_->InitializeSession();

  // Create a temporary cache directory.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath cache_dir = temp_dir.GetPath().Append("cache");
  ASSERT_TRUE(base::CreateDirectoryAndGetError(cache_dir, nullptr /*error*/));

  auto loader = base::MakeRefCounted<DemoExtensionsExternalLoader>(cache_dir);
  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      std::make_unique<extensions::ExternalProviderImpl>(
          &external_provider_visitor_, loader, profile_.get(),
          ManifestLocation::kInternal, ManifestLocation::kExternalPrefDownload,
          extensions::Extension::FROM_WEBSTORE |
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();
  EXPECT_TRUE(external_provider->IsReady());

  loader->LoadApp(kTestExtensionId);
  // Verify that a downloader has started and is attempting to download an
  // update manifest.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  // Return a manifest to the downloader.
  std::string manifest;
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  EXPECT_TRUE(base::ReadFileToString(
      test_dir.Append(kTestExtensionUpdateManifest), &manifest));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.AddResponse(
      test_url_loader_factory_.pending_requests()->at(0).request.url.spec(),
      manifest);

  // Wait for the manifest to be parsed.
  extension_update_found_observer.Wait();

  // Verify that the downloader is attempting to download a CRX file.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  // Trigger downloading of the CRX file.
  test_url_loader_factory_.AddResponse(
      test_url_loader_factory_.pending_requests()->at(0).request.url.spec(),
      "Dummy content.");

  // Verify that the CRX file exists in the cache directory.
  external_provider_visitor_.WaitForFileFound();
  const base::FilePath cached_crx_path = cache_dir.Append(base::StringPrintf(
      "%s-%s.crx", kTestExtensionId, kTestExtensionCRXVersion));
  const std::map<std::string, TestCrxInfo> expected_info = {
      {kTestExtensionId,
       TestCrxInfo(kTestExtensionCRXVersion, cached_crx_path.value())}};
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());

  // Verify that loading the app again succeeds without downloading.
  test_url_loader_factory_.ClearResponses();
  external_provider_visitor_.ClearLoadedFiles();
  loader->LoadApp(kTestExtensionId);
  external_provider_visitor_.WaitForFileFound();
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

class ShouldCreateDemoExtensionsExternalLoaderTest : public testing::Test {
 public:
  ShouldCreateDemoExtensionsExternalLoaderTest() {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  ShouldCreateDemoExtensionsExternalLoaderTest(
      const ShouldCreateDemoExtensionsExternalLoaderTest&) = delete;
  ShouldCreateDemoExtensionsExternalLoaderTest& operator=(
      const ShouldCreateDemoExtensionsExternalLoaderTest&) = delete;

  ~ShouldCreateDemoExtensionsExternalLoaderTest() override = default;

  void SetUp() override {
    demo_mode_test_helper_ = std::make_unique<DemoModeTestHelper>();
  }

  void TearDown() override { demo_mode_test_helper_.reset(); }

 protected:
  std::unique_ptr<TestingProfile> AddTestUser(const AccountId& account_id) {
    auto profile = std::make_unique<TestingProfile>();
    profile->set_profile_name(account_id.GetUserEmail());
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    return profile;
  }

  void StartDemoSession(DemoSession::DemoModeConfig demo_config) {
    ASSERT_NE(DemoSession::DemoModeConfig::kNone, demo_config);
    demo_mode_test_helper_->InitializeSession();
  }

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<DemoModeTestHelper> demo_mode_test_helper_;
};

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, PrimaryDemoProfile) {
  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));
  profile->ScopedCrosSettingsTestHelper()->InstallAttributes()->SetDemoMode();
  StartDemoSession(DemoSession::DemoModeConfig::kOnline);

  EXPECT_TRUE(DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, ProfileWithNoUser) {
  TestingProfile profile;
  profile.ScopedCrosSettingsTestHelper()->InstallAttributes()->SetDemoMode();
  StartDemoSession(DemoSession::DemoModeConfig::kOnline);

  EXPECT_FALSE(DemoExtensionsExternalLoader::SupportedForProfile(&profile));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, MultiProfile) {
  std::unique_ptr<TestingProfile> primary_profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));
  primary_profile->ScopedCrosSettingsTestHelper()
      ->InstallAttributes()
      ->SetDemoMode();

  std::unique_ptr<TestingProfile> secondary_profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("secondary@test.com", "secondary_user"));

  StartDemoSession(DemoSession::DemoModeConfig::kOnline);

  EXPECT_TRUE(
      DemoExtensionsExternalLoader::SupportedForProfile(primary_profile.get()));
  EXPECT_FALSE(DemoExtensionsExternalLoader::SupportedForProfile(
      secondary_profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, NotDemoMode) {
  // This should be no-op, given that the default demo session enrollment state
  // is not-enrolled.
  DemoSession::StartIfInDemoMode();
  ASSERT_FALSE(DemoSession::Get());

  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_FALSE(
      DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, DemoSessionNotStarted) {
  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_FALSE(
      DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

}  // namespace
}  // namespace ash
