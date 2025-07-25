// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_loader.h"
#include "chrome/browser/ash/crosapi/fake_device_ownership_waiter.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

using ::component_updater::FakeCrOSComponentManager;
using ::component_updater::MockComponentUpdateService;
using testing::_;
using update_client::UpdateClient;
using user_manager::User;

namespace crosapi {

namespace {

constexpr char kSampleLacrosPath[] =
    "/run/imageloader-lacros-dogfood-dev/97.0.4676/";

class MockBrowserService : public mojom::BrowserServiceInterceptorForTesting {
 public:
  BrowserService* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  MOCK_METHOD(void,
              NewWindow,
              (bool incognito,
               bool should_trigger_session_restore,
               int64_t target_display_id,
               NewWindowCallback callback),
              (override));
  MOCK_METHOD(void, OpenForFullRestore, (bool skip_crash_restore), (override));
  MOCK_METHOD(void, UpdateKeepAlive, (bool enabled), (override));
};

class BrowserManagerFake : public BrowserManager {
 public:
  BrowserManagerFake(std::unique_ptr<BrowserLoader> browser_loader,
                     component_updater::ComponentUpdateService* update_service)
      : BrowserManager(std::move(browser_loader), update_service) {}

  ~BrowserManagerFake() override = default;

  // BrowserManager:
  void Start(bool launching_at_login_screen = false) override {
    ++start_count_;
    BrowserManager::Start(launching_at_login_screen);
  }

  int start_count() const { return start_count_; }

  void PrelaunchAtLoginScreen() override { ++prelaunch_count_; }

  int prelaunch_count() const { return prelaunch_count_; }

  void TriggerLoginPromptVisible() { OnLoginPromptVisible(); }

  void SetStatePublic(State state) { SetState(state); }

  void SimulateLacrosTermination() {
    SetStatePublic(State::TERMINATING);
    if (browser_service_.has_value()) {
      OnBrowserServiceDisconnected(*crosapi_id_, browser_service_->mojo_id);
    }
    OnLacrosChromeTerminated();
  }

  void SimulateLacrosStart(mojom::BrowserService* browser_service) {
    crosapi_id_ = CrosapiId::FromUnsafeValue(42);  // Dummy value.
    SetStatePublic(State::STARTING);
    OnBrowserServiceConnected(*crosapi_id_,
                              mojo::RemoteSetElementId::FromUnsafeValue(42),
                              browser_service, 42);
  }

  // Make the State enum publicly available.
  using BrowserManager::State;

  int start_count_ = 0;
  int prelaunch_count_ = 0;
};

class MockVersionServiceDelegate : public BrowserVersionServiceAsh::Delegate {
 public:
  MockVersionServiceDelegate() = default;
  MockVersionServiceDelegate(const MockVersionServiceDelegate&) = delete;
  MockVersionServiceDelegate& operator=(const MockVersionServiceDelegate&) =
      delete;
  ~MockVersionServiceDelegate() override = default;

  // BrowserVersionServiceAsh::Delegate:
  base::Version GetLatestLaunchableBrowserVersion() const override {
    return latest_launchable_version_;
  }

  bool IsNewerBrowserAvailable() const override {
    return is_newer_browser_available_;
  }

  void set_latest_lauchable_version(base::Version version) {
    latest_launchable_version_ = std::move(version);
  }

  void set_is_newer_browser_available(bool is_newer_browser_available) {
    is_newer_browser_available_ = is_newer_browser_available;
  }

 private:
  base::Version latest_launchable_version_;
  bool is_newer_browser_available_ = false;
};

}  // namespace

class MockBrowserLoader : public BrowserLoader {
 public:
  explicit MockBrowserLoader(
      scoped_refptr<component_updater::CrOSComponentManager> manager)
      : BrowserLoader(manager) {}
  MockBrowserLoader(const MockBrowserLoader&) = delete;
  MockBrowserLoader& operator=(const MockBrowserLoader&) = delete;
  ~MockBrowserLoader() override = default;

  MOCK_METHOD1(Load, void(LoadCompletionCallback));
  MOCK_METHOD0(Unload, void());
};

class BrowserManagerTest : public testing::Test {
 public:
  BrowserManagerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~BrowserManagerTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(ash::standalone_browser::GetFeatureRefs(),
                                   {});

    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    auto* testing_profile = testing_profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);

    auto fake_cros_component_manager =
        base::MakeRefCounted<FakeCrOSComponentManager>();
    std::unique_ptr<MockBrowserLoader> browser_loader =
        std::make_unique<testing::StrictMock<MockBrowserLoader>>(
            fake_cros_component_manager);
    browser_loader_ = browser_loader.get();
    component_update_service_ =
        std::make_unique<testing::NiceMock<MockComponentUpdateService>>();
    fake_browser_manager_ = std::make_unique<BrowserManagerFake>(
        std::move(browser_loader), component_update_service_.get());
    auto version_service_delegate =
        std::make_unique<MockVersionServiceDelegate>();
    version_service_delegate_ = version_service_delegate.get();
    fake_browser_manager_->set_version_service_delegate_for_testing(
        std::move(version_service_delegate));
    fake_browser_manager_->set_device_ownership_waiter_for_testing(
        std::make_unique<FakeDeviceOwnershipWaiter>());

    shelf_model_ = std::make_unique<ash::ShelfModel>();
    shelf_controller_ = std::make_unique<ChromeShelfController>(
        testing_profile, shelf_model_.get());
    shelf_controller_->Init();

    // We need to avoid a DCHECK which happens when the policies have not yet
    // been loaded. As such we claim that the Lacros availability is allowed
    // to be set by the user.
    crosapi::browser_util::SetLacrosLaunchSwitchSourceForTest(
        ash::standalone_browser::LacrosAvailability::kUserChoice);

    EXPECT_CALL(mock_browser_service_, NewWindow(_, _, _, _)).Times(0);
    EXPECT_CALL(mock_browser_service_, OpenForFullRestore(_)).Times(0);
  }

  void TearDown() override {
    shelf_controller_.reset();
    version_service_delegate_ = nullptr;
    browser_loader_ = nullptr;
    fake_browser_manager_.reset();
    testing_profile_manager_.reset();
    fake_user_manager_.Reset();

    // Need to reverse the state back to non set.
    crosapi::browser_util::ClearLacrosAvailabilityCacheForTest();

    // Reset any CPU restrictions.
    ash::standalone_browser::BrowserSupport::SetCpuSupportedForTesting(
        absl::nullopt);

    // Reset the session manager state.
    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::UNKNOWN);
  }

  enum class UserType {
    kRegularUser = 0,
    kWebKiosk = 1,
    kChromeAppKiosk = 2,
    kMaxValue = kChromeAppKiosk,
  };

  void AddUser(UserType user_type) {
    AccountId account_id =
        AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName);

    User* user;
    switch (user_type) {
      case UserType::kRegularUser:
        user = fake_user_manager_->AddUser(account_id);
        break;
      case UserType::kWebKiosk:
        user = fake_user_manager_->AddWebKioskAppUser(account_id);
        break;
      case UserType::kChromeAppKiosk:
        user = fake_user_manager_->AddKioskAppUser(account_id);
        break;
    }

    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    fake_user_manager_->SimulateUserProfileLoad(account_id);

    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
        local_state_.Get(), user->username_hash(),
        ash::standalone_browser::migrator_util::MigrationMode::kCopy);

    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsLacrosAllowedToLaunch());
  }

  void ExpectCallingLoad(browser_util::LacrosSelection load_selection =
                             browser_util::LacrosSelection::kRootfs,
                         const std::string& lacros_path = "/run/lacros") {
    EXPECT_CALL(*browser_loader_, Load(_))
        .WillOnce([load_selection, lacros_path](
                      BrowserLoader::LoadCompletionCallback callback) {
          std::move(callback).Run(base::FilePath(lacros_path), load_selection,
                                  base::Version());
        });
  }

 protected:
  // The order of these members is relevant for both construction and
  // destruction timing.
  content::BrowserTaskEnvironment task_environment_;
  session_manager::SessionManager session_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<MockBrowserLoader, ExperimentalAsh> browser_loader_ = nullptr;
  std::unique_ptr<MockComponentUpdateService> component_update_service_;
  std::unique_ptr<BrowserManagerFake> fake_browser_manager_;
  raw_ptr<MockVersionServiceDelegate> version_service_delegate_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<ash::ShelfModel> shelf_model_;
  std::unique_ptr<ChromeShelfController> shelf_controller_;
  MockBrowserService mock_browser_service_;
  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BrowserManagerTest, LacrosKeepAlive) {
  // Disable the lacros launching on initialization and default keep-alive,
  // so that we can make sure the behavior controlled by the test scenario.
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisableLoginLacrosOpening);
  BrowserManager::ScopedUnsetAllKeepAliveForTesting unset_keep_alive(
      fake_browser_manager_.get());

  AddUser(UserType::kRegularUser);

  using State = BrowserManagerFake::State;
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Attempt to mount the Lacros image. Will not start as it does not meet the
  // automatic start criteria.
  ExpectCallingLoad();
  fake_browser_manager_->InitializeAndStartIfNeeded();
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  fake_browser_manager_->SetStatePublic(State::UNAVAILABLE);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Creating a ScopedKeepAlive does not start Lacros.
  std::unique_ptr<BrowserManager::ScopedKeepAlive> keep_alive =
      fake_browser_manager_->KeepAlive(BrowserManager::Feature::kTestOnly);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // On termination, KeepAlive should start Lacros.
  fake_browser_manager_->SimulateLacrosTermination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 1);

  // Repeating the process starts Lacros again.
  fake_browser_manager_->SimulateLacrosTermination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 2);

  // Once the ScopedKeepAlive is destroyed, this should no longer happen.
  keep_alive.reset();
  fake_browser_manager_->SimulateLacrosTermination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 2);
}

TEST_F(BrowserManagerTest, LacrosKeepAliveReloadsWhenUpdateAvailable) {
  // Disable the lacros launching on initialization and default keep-alive,
  // so that we can make sure the behavior controlled by the test scenario.
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisableLoginLacrosOpening);
  BrowserManager::ScopedUnsetAllKeepAliveForTesting unset_keep_alive(
      fake_browser_manager_.get());

  AddUser(UserType::kRegularUser);
  ExpectCallingLoad();
  fake_browser_manager_->InitializeAndStartIfNeeded();

  using State = BrowserManagerFake::State;
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  fake_browser_manager_->SetStatePublic(State::UNAVAILABLE);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  version_service_delegate_->set_is_newer_browser_available(true);
  version_service_delegate_->set_latest_lauchable_version(
      base::Version("1.0.0"));

  std::unique_ptr<BrowserManager::ScopedKeepAlive> keep_alive =
      fake_browser_manager_->KeepAlive(BrowserManager::Feature::kTestOnly);

  ExpectCallingLoad(browser_util::LacrosSelection::kStateful,
                    kSampleLacrosPath);

  // On simulated termination, KeepAlive restarts Lacros. Since there is an
  // update, it should first load the updated image.
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);
  fake_browser_manager_->SimulateLacrosTermination();
  EXPECT_GE(fake_browser_manager_->start_count(), 1);
}

TEST_F(BrowserManagerTest, NewWindowReloadsWhenUpdateAvailable) {
  // Disable the lacros launching on initialization and default keep-alive,
  // so that we can make sure the behavior controlled by the test scenario.
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisableLoginLacrosOpening);
  BrowserManager::ScopedUnsetAllKeepAliveForTesting unset_keep_alive(
      fake_browser_manager_.get());

  AddUser(UserType::kRegularUser);
  ExpectCallingLoad();
  fake_browser_manager_->InitializeAndStartIfNeeded();

  // Set the state of the browser manager as stopped, which would match the
  // state after the browser mounted an image, ran, and was terminated.
  using State = BrowserManagerFake::State;
  fake_browser_manager_->SetStatePublic(State::STOPPED);

  version_service_delegate_->set_is_newer_browser_available(true);
  version_service_delegate_->set_latest_lauchable_version(
      base::Version("1.0.0"));

  EXPECT_EQ(fake_browser_manager_->start_count(), 0);
  EXPECT_CALL(*browser_loader_, Load(_));
  EXPECT_CALL(mock_browser_service_, NewWindow(_, _, _, _))
      .Times(1)
      .RetiresOnSaturation();
  fake_browser_manager_->NewWindow(/*incognito=*/false,
                                   /*should_trigger_session_restore=*/false);
  EXPECT_EQ(fake_browser_manager_->start_count(), 1);
  fake_browser_manager_->SimulateLacrosStart(&mock_browser_service_);
}

TEST_F(BrowserManagerTest, LacrosKeepAliveDoesNotBlockRestart) {
  // Disable the lacros launching on initialization and default keep-alive,
  // so that we can make sure the behavior controlled by the test scenario.
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisableLoginLacrosOpening);
  BrowserManager::ScopedUnsetAllKeepAliveForTesting unset_keep_alive(
      fake_browser_manager_.get());

  EXPECT_CALL(mock_browser_service_, UpdateKeepAlive(_)).Times(0);
  AddUser(UserType::kRegularUser);

  using State = BrowserManagerFake::State;
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Attempt to mount the Lacros image. Will not start as it does not meet the
  // automatic start criteria.
  ExpectCallingLoad();
  fake_browser_manager_->InitializeAndStartIfNeeded();
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  fake_browser_manager_->SetStatePublic(State::UNAVAILABLE);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Creating a ScopedKeepAlive does not start Lacros.
  std::unique_ptr<BrowserManager::ScopedKeepAlive> keep_alive =
      fake_browser_manager_->KeepAlive(BrowserManager::Feature::kTestOnly);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Simulate a Lacros termination, keep alive should launch Lacros in a
  // windowless state.
  fake_browser_manager_->SimulateLacrosTermination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 1);
  EXPECT_CALL(mock_browser_service_, UpdateKeepAlive(_))
      .Times(1)
      .RetiresOnSaturation();
  fake_browser_manager_->SimulateLacrosStart(&mock_browser_service_);

  // Terminating again causes keep alive to again start Lacros in a windowless
  // state.
  fake_browser_manager_->SimulateLacrosTermination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 2);
  EXPECT_CALL(mock_browser_service_, UpdateKeepAlive(_))
      .Times(1)
      .RetiresOnSaturation();
  fake_browser_manager_->SimulateLacrosStart(&mock_browser_service_);

  // Request a relaunch. Keep alive should not start Lacros in a windowless
  // state but Lacros should instead start with the kRestoreLastSession action.
  fake_browser_manager_->set_relaunch_requested_for_testing(true);
  fake_browser_manager_->SimulateLacrosTermination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 3);
  EXPECT_CALL(mock_browser_service_, UpdateKeepAlive(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(mock_browser_service_, OpenForFullRestore(_))
      .Times(1)
      .RetiresOnSaturation();
  fake_browser_manager_->SimulateLacrosStart(&mock_browser_service_);

  // Resetting the relaunch requested bit should cause keep alive to start
  // Lacros in a windowless state.
  fake_browser_manager_->set_relaunch_requested_for_testing(false);
  fake_browser_manager_->SimulateLacrosTermination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 4);
}

// In the Kiosk session, the Lacros window is created during the kiosk launch,
// no need to create a new window in this case.
TEST_F(BrowserManagerTest, DoNotOpenNewLacrosWindowInChromeAppKiosk) {
  AddUser(UserType::kChromeAppKiosk);
  ExpectCallingLoad();

  fake_browser_manager_->InitializeAndStartIfNeeded();

  EXPECT_CALL(mock_browser_service_, NewWindow(_, _, _, _)).Times(0);

  fake_browser_manager_->SimulateLacrosStart(&mock_browser_service_);
}

TEST_F(BrowserManagerTest, DoNotOpenNewLacrosWindowInWebKiosk) {
  AddUser(UserType::kWebKiosk);
  ExpectCallingLoad();

  fake_browser_manager_->InitializeAndStartIfNeeded();

  EXPECT_CALL(mock_browser_service_, NewWindow(_, _, _, _)).Times(0);

  fake_browser_manager_->SimulateLacrosStart(&mock_browser_service_);
}

TEST_F(BrowserManagerTest, DisallowUseOfLacrosOnOldCPUs) {
  // As the CTOR needs to be tested, parts of the objects need to be destroyed
  // and re-created.
  shelf_controller_.reset();
  version_service_delegate_ = nullptr;
  browser_loader_ = nullptr;
  fake_browser_manager_.reset();

  // Set the used CPU type to really old.
  ash::standalone_browser::BrowserSupport::SetCpuSupportedForTesting(false);

  // Now re-create the required objects.
  auto fake_cros_component_manager =
      base::MakeRefCounted<FakeCrOSComponentManager>();
  std::unique_ptr<MockBrowserLoader> browser_loader =
      std::make_unique<testing::StrictMock<MockBrowserLoader>>(
          fake_cros_component_manager);
  browser_loader_ = browser_loader.get();

  fake_browser_manager_ = std::make_unique<BrowserManagerFake>(
      std::move(browser_loader), component_update_service_.get());

  // Simulate that we are ready and the log in screen is shown.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // Trigger the pre-launch logic as the log in screen is ready.
  fake_browser_manager_->TriggerLoginPromptVisible();

  // Expect the prelaunch logic was not called as the CPU is not sufficient.
  EXPECT_EQ(fake_browser_manager_->prelaunch_count(), 0);
}

TEST_F(BrowserManagerTest, AllowUseOfLacrosOnNormalCPUs) {
  // Simulate that we are ready and the log in screen is shown.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // Trigger the pre-launch logic as the log in screen is ready.
  fake_browser_manager_->TriggerLoginPromptVisible();

  // Expect that the prelaunch logic was called.
  EXPECT_EQ(fake_browser_manager_->prelaunch_count(), 1);
}

}  // namespace crosapi
