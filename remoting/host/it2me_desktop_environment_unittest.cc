// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me_desktop_environment.h"

#include <memory>

#include "ash/curtain/security_curtain_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/chromeos/scoped_fake_ash_proxy.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/client_session_events.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/capability_names.h"
#include "remoting/protocol/errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/scoped_fake_ash_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace remoting {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kTestEmail[] = "test@localhost";
#endif

}  // namespace
namespace {

using ::testing::Eq;
using ::testing::IsNull;

class ClientSessionControlMock : public ClientSessionControl {
 public:
  ClientSessionControlMock() = default;
  ClientSessionControlMock(const ClientSessionControlMock&) = delete;
  ClientSessionControlMock& operator=(const ClientSessionControlMock&) = delete;
  ~ClientSessionControlMock() override = default;

  // ClientSessionControl implementation:
  const std::string& client_jid() const override { return client_jid_; }
  MOCK_METHOD(void, DisconnectSession, (protocol::ErrorCode error));
  MOCK_METHOD(void,
              OnLocalPointerMoved,
              (const webrtc::DesktopVector& position, ui::EventType type));
  MOCK_METHOD(void, OnLocalKeyPressed, (uint32_t usb_keycode));
  MOCK_METHOD(void, SetDisableInputs, (bool disable_inputs));
  MOCK_METHOD(void,
              OnDesktopDisplayChanged,
              (std::unique_ptr<protocol::VideoLayout> layout));

  base::WeakPtr<ClientSessionControlMock> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::string client_jid_ = "<fake-client-jid>";
  base::WeakPtrFactory<ClientSessionControlMock> weak_ptr_factory_{this};
};

class FakeClientSessionEvents : public ClientSessionEvents {
 public:
  FakeClientSessionEvents() = default;
  FakeClientSessionEvents(const FakeClientSessionEvents&) = delete;
  FakeClientSessionEvents& operator=(const FakeClientSessionEvents&) = delete;
  ~FakeClientSessionEvents() override = default;

  // ClientSessionEvents implementation:
  void OnDesktopAttached(uint32_t session_id) override {}
  void OnDesktopDetached() override {}

  base::WeakPtr<FakeClientSessionEvents> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeClientSessionEvents> weak_ptr_factory_{this};
};

class FakeSecurityCurtainController
    : public ash::curtain::SecurityCurtainController {
 public:
  FakeSecurityCurtainController() = default;
  FakeSecurityCurtainController(const FakeSecurityCurtainController&) = delete;
  FakeSecurityCurtainController& operator=(
      const FakeSecurityCurtainController&) = delete;
  ~FakeSecurityCurtainController() override = default;

  // ash::curtain::SecurityCurtainController implementation:
  void Enable(InitParams params) override {
    DCHECK(!is_enabled_);
    is_enabled_ = true;
    init_params_ = params;
  }
  void Disable() override {
    DCHECK(is_enabled_);
    is_enabled_ = false;
  }
  bool IsEnabled() const override { return is_enabled_; }

  // InitParams passed to the last |Enable()| call.
  InitParams last_init_params() const { return init_params_; }

 private:
  bool is_enabled_ = false;
  InitParams init_params_;
};

class It2MeDesktopEnvironmentTest : public ::testing::Test {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  It2MeDesktopEnvironmentTest()
      : scoped_user_manager_(std::make_unique<user_manager::FakeUserManager>()),
        fake_user_manager_(*static_cast<user_manager::FakeUserManager*>(
            user_manager::UserManager::Get())) {}
#endif

  ~It2MeDesktopEnvironmentTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    feature_list_.InitAndEnableFeature(features::kEnableCrdAdminRemoteAccess);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

#if BUILDFLAG(IS_CHROMEOS)
  void TearDown() override {
    // Wait until DeleteSoon is finished.
    environment_.RunUntilIdle();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  DesktopEnvironmentOptions default_options() {
    DesktopEnvironmentOptions options;
    // These options must be false or we run into crashes in HostWindowProxy.
    options.set_enable_user_interface(false);
    options.set_enable_notifications(false);
    return options;
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return environment_.GetMainThreadTaskRunner();
  }

  std::unique_ptr<It2MeDesktopEnvironment> Create(
      DesktopEnvironmentOptions options) {
    auto base_ptr = It2MeDesktopEnvironmentFactory(task_runner(), task_runner(),
                                                   task_runner(), task_runner())
                        .Create(session_control_.GetWeakPtr(),
                                session_events_.GetWeakPtr(), options);
    // Cast to It2MeDesktopEnvironment
    auto desktop_environment = std::unique_ptr<It2MeDesktopEnvironment>(
        static_cast<It2MeDesktopEnvironment*>(base_ptr.release()));

    // Give the code time to instantiate the curtain mode on the UI sequence (if
    // needed).
    FlushUiSequence();
    return desktop_environment;
  }

  std::unique_ptr<It2MeDesktopEnvironment> CreateCurtainedSession() {
    DesktopEnvironmentOptions options(default_options());
    options.set_enable_curtaining(true);
    return Create(options);
  }

  void FlushUiSequence() {
    // In our test scenario all sequences are single threaded,
    // so to flush the UI sequence we can simply flush the main thread.
    environment_.RunUntilIdle();
  }

#if BUILDFLAG(IS_CHROMEOS)
  FakeSecurityCurtainController& security_curtain_controller() {
    return security_curtain_controller_;
  }

  test::ScopedFakeAshProxy& ash_proxy() { return ash_proxy_; }

  user_manager::FakeUserManager& user_manager() { return *fake_user_manager_; }

  void LogInUser() {
    auto account_id = AccountId::FromUserEmail(kTestEmail);
    user_manager().AddPublicAccountUser(account_id);
    user_manager().UserLoggedIn(account_id, account_id.GetUserEmail(), false,
                                false);
  }

  void LogOutUser() {
    user_manager().LogoutAllUsers();
    auto account_id = AccountId::FromUserEmail(kTestEmail);
    user_manager().RemoveUserFromList(account_id);
  }

  ClientSessionControlMock& session_control() { return session_control_; }

#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  base::test::SingleThreadTaskEnvironment environment_;

  base::test::ScopedFeatureList feature_list_;
  testing::StrictMock<ClientSessionControlMock> session_control_;
  FakeClientSessionEvents session_events_;

#if BUILDFLAG(IS_CHROMEOS)
  FakeSecurityCurtainController security_curtain_controller_;
  test::ScopedFakeAshProxy ash_proxy_{&security_curtain_controller_};

  user_manager::ScopedUserManager scoped_user_manager_;
  const raw_ref<user_manager::FakeUserManager> fake_user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

#if BUILDFLAG(IS_CHROMEOS)
ui::KeyEvent EventWithSource(int source_device_id) {
  ui::KeyEvent result{ui::EventType::ET_KEY_PRESSED, ui::KeyboardCode::VKEY_C,
                      /*flags=*/0};
  result.set_source_device_id(source_device_id);
  return result;
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldStartCurtainWhenEnableCurtainingIsTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEnableCrdAdminRemoteAccess);
  DesktopEnvironmentOptions options(default_options());

  options.set_enable_curtaining(true);

  auto desktop_environment = Create(options);
  FlushUiSequence();

  EXPECT_THAT(desktop_environment->is_curtained(), Eq(true));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldNotStartCurtainWhenEnableCurtainingIsFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEnableCrdAdminRemoteAccess);
  DesktopEnvironmentOptions options(default_options());

  options.set_enable_curtaining(false);

  auto desktop_environment = Create(options);
  FlushUiSequence();

  EXPECT_THAT(desktop_environment->is_curtained(), Eq(false));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldNotStartCurtainWhenCrdAdminRemoteAccessFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kEnableCrdAdminRemoteAccess);
  DesktopEnvironmentOptions options(default_options());

  options.set_enable_curtaining(true);

  auto desktop_environment = Create(options);
  FlushUiSequence();

  EXPECT_THAT(desktop_environment->is_curtained(), Eq(false));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ACurtainedSessionShouldEnableSecurityCurtain) {
  ASSERT_THAT(security_curtain_controller().IsEnabled(), Eq(false));

  auto desktop_environment = CreateCurtainedSession();
  FlushUiSequence();

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(true));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ACurtainedSessionShouldFilterNonRemoteEvents) {
  ASSERT_THAT(security_curtain_controller().IsEnabled(), Eq(false));

  auto desktop_environment = CreateCurtainedSession();
  FlushUiSequence();

  ash::curtain::EventFilter event_filter =
      security_curtain_controller().last_init_params().event_filter;

  EXPECT_THAT(event_filter.Run(EventWithSource(ui::ED_REMOTE_INPUT_DEVICE)),
              Eq(ash::curtain::FilterResult::kKeepEvent));

  EXPECT_THAT(event_filter.Run(EventWithSource(5)),
              Eq(ash::curtain::FilterResult::kSuppressEvent));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ClosingACurtainedSessionShouldDisableCurtainMode) {
  auto desktop_environment = CreateCurtainedSession();
  FlushUiSequence();

  // Closing the CRD session will destroy the desktop environment.
  desktop_environment.reset();
  FlushUiSequence();

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(false));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldLogoutUserInDestructorIfCurtainModeIsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEnableCrdAdminRemoteAccess);
  DesktopEnvironmentOptions options(default_options());

  options.set_enable_curtaining(true);

  auto desktop_environment = Create(options);

  FlushUiSequence();
  EXPECT_THAT(ash_proxy().request_sign_out_count(), Eq(0));

  desktop_environment = nullptr;

  FlushUiSequence();
  EXPECT_THAT(ash_proxy().request_sign_out_count(), Eq(1));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldNotLogoutUserInDestructorIfCurtainModeIsDisabled) {
  base::test::ScopedFeatureList feature_list{
      features::kEnableCrdAdminRemoteAccess};
  DesktopEnvironmentOptions options(default_options());

  options.set_enable_curtaining(false);

  auto desktop_environment = Create(options);
  desktop_environment = nullptr;

  FlushUiSequence();
  EXPECT_THAT(ash_proxy().request_sign_out_count(), Eq(0));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldRefuseCurtainSessionIfUserIsLoggedIn) {
  base::test::ScopedFeatureList feature_list{
      features::kEnableCrdAdminRemoteAccess};

  EXPECT_CALL(session_control(),
              DisconnectSession(protocol::ErrorCode::HOST_CONFIGURATION_ERROR));

  LogInUser();
  auto desktop_environment = CreateCurtainedSession();
  FlushUiSequence();
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldNotForceLogoutUserIfInitializeCurtainModeFails) {
  base::test::ScopedFeatureList feature_list{
      features::kEnableCrdAdminRemoteAccess};
  EXPECT_CALL(session_control(),
              DisconnectSession(protocol::ErrorCode::HOST_CONFIGURATION_ERROR));

  LogInUser();
  auto desktop_environment = CreateCurtainedSession();
  desktop_environment = nullptr;

  FlushUiSequence();
  EXPECT_THAT(ash_proxy().request_sign_out_count(), Eq(0));

  EXPECT_THAT(ash_proxy().request_sign_out_count(), Eq(0));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldHaveFileTransferCapabilitiesWhenEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEnableCrdFileTransferForKiosk);
  DesktopEnvironmentOptions options(default_options());
  std::string expected_capabilities(" ");
  expected_capabilities += protocol::kFileTransferCapability;

  options.set_enable_file_transfer(true);
  auto desktop_environment = Create(options);

  EXPECT_THAT(desktop_environment->GetCapabilities(),
              testing::HasSubstr(expected_capabilities));
}

TEST_F(It2MeDesktopEnvironmentTest,
       ShouldNotHaveFileTransferCapabilitiesWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEnableCrdFileTransferForKiosk);
  DesktopEnvironmentOptions options(default_options());

  options.set_enable_file_transfer(false);
  auto desktop_environment = Create(options);

  EXPECT_THAT(desktop_environment->GetCapabilities(), testing::HasSubstr(""));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace remoting
