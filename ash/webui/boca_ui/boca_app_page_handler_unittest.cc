// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/webview_auth_delegate.h"
#include "ash/webui/boca_ui/webview_auth_handler.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/join_session_request.h"
#include "chromeos/ash/components/boca/session_api/remove_student_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/session_api/update_session_request.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_service.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/fake_browser_context_helper_delegate.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {
namespace {
constexpr GaiaId::Literal kGaiaId("123");
constexpr char kUserEmail[] = "cat@gmail.com";
constexpr char kWebviewHostName[] = "boca";
constexpr char kTestDefaultUrl[] = "https://test";
constexpr char kTestUrlBase[] = "https://test";

mojom::OnTaskConfigPtr GetCommonTestLockOnTaskConfig() {
  std::vector<mojom::ControlledTabPtr> tabs;
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New("google", ::GURL("http://google.com/"), "data/image"),
      /*=navigation_type*/ mojom::NavigationType::kOpen));
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New("youtube", ::GURL("http://youtube.com/"),
                          "data/image"),
      /*=navigation_type*/ mojom::NavigationType::kBlock));
  return mojom::OnTaskConfig::New(/*=is_locked*/ true, std::move(tabs));
}

mojom::OnTaskConfigPtr GetCommonTestUnLockedOnTaskConfig() {
  std::vector<mojom::ControlledTabPtr> tabs;
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New("google", ::GURL("http://google.com/"), "data/image"),
      /*=navigation_type*/ mojom::NavigationType::kOpen));
  return mojom::OnTaskConfig::New(/*=is_locked*/ false, std::move(tabs));
}

::boca::OnTaskConfig GetCommonTestLockOnTaskConfigProto() {
  ::boca::OnTaskConfig on_task_config;
  auto* active_bundle = on_task_config.mutable_active_bundle();
  active_bundle->set_locked(true);
  auto* content = active_bundle->mutable_content_configs()->Add();
  content->set_url("http://google.com/");
  content->set_favicon_url("data/image");
  content->set_title("google");
  content->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);
  auto* content_1 = active_bundle->mutable_content_configs()->Add();
  content_1->set_url("http://youtube.com/");
  content_1->set_favicon_url("data/image");
  content_1->set_title("youtube");
  content_1->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_BLOCK_NAVIGATION);
  return on_task_config;
}

::boca::OnTaskConfig GetCommonTestUnLockOnTaskConfigProto() {
  ::boca::OnTaskConfig on_task_config;
  auto* active_bundle = on_task_config.mutable_active_bundle();
  active_bundle->set_locked(false);
  auto* content = active_bundle->mutable_content_configs()->Add();
  content->set_url("http://google.com/");
  content->set_favicon_url("data/image");
  content->set_title("google");
  content->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);
  return on_task_config;
}

mojom::CaptionConfigPtr GetCommonCaptionConfig() {
  return mojom::CaptionConfig::New(true, true, true);
}

::boca::CaptionsConfig GetCommonCaptionConfigProto() {
  ::boca::CaptionsConfig config;
  config.set_captions_enabled(true);
  config.set_translations_enabled(true);
  return config;
}

::boca::Session GetCommonActiveSessionProto() {
  ::boca::Session session;
  session.mutable_duration()->set_seconds(120);
  session.set_session_state(::boca::Session::ACTIVE);
  auto* teacher = session.mutable_teacher();
  teacher->set_gaia_id("123");

  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(false);
  caption_config_1->set_translations_enabled(false);

  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(false);
  auto* content = active_bundle->mutable_content_configs()->Add();
  content->set_url("http://default.com/");
  content->set_favicon_url("data/image");
  content->set_title("default");
  content->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);

  (*session.mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);
  return session;
}

class MockSessionClientImpl : public SessionClientImpl {
 public:
  explicit MockSessionClientImpl(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SessionClientImpl(std::move(sender)) {}
  MOCK_METHOD(void,
              CreateSession,
              (std::unique_ptr<CreateSessionRequest>),
              (override));
  MOCK_METHOD(void,
              GetSession,
              (std::unique_ptr<GetSessionRequest>),
              (override));
  MOCK_METHOD(void,
              UpdateSession,
              (std::unique_ptr<UpdateSessionRequest>),
              (override));
  MOCK_METHOD(void,
              RemoveStudent,
              (std::unique_ptr<RemoveStudentRequest>),
              (override));
  MOCK_METHOD(void,
              JoinSession,
              (std::unique_ptr<JoinSessionRequest>),
              (override));
};

class MockBocaAppClient : public BocaAppClient {
 public:
  MOCK_METHOD(BocaSessionManager*, GetSessionManager, (), (override));
  MOCK_METHOD(void, AddSessionManager, (BocaSessionManager*), (override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(std::string, GetSchoolToolsServerBaseUrl, (), (override));
};

class MockSessionManager : public BocaSessionManager {
 public:
  explicit MockSessionManager(SessionClientImpl* session_client_impl)
      : BocaSessionManager(session_client_impl,
                           AccountId::FromUserEmail(kUserEmail),
                           /*=is_producer*/ false) {}
  MOCK_METHOD(void,
              NotifyLocalCaptionEvents,
              (::boca::CaptionsConfig config),
              (override));
  MOCK_METHOD(void,
              UpdateCurrentSession,
              (std::unique_ptr<::boca::Session>, bool),
              (override));
  MOCK_METHOD((::boca::Session*), GetCurrentSession, (), (override));
  MOCK_METHOD(void, ToggleAppStatus, (bool), (override));
  ~MockSessionManager() override = default;
};

class MockSpotlightService : public SpotlightService {
 public:
  explicit MockSpotlightService(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SpotlightService(std::move(sender)) {}
  MOCK_METHOD(void,
              ViewScreen,
              (std::string, std::string, ViewScreenRequestCallback),
              (override));
  MOCK_METHOD(void,
              UpdateViewScreenState,
              (std::string,
               ::boca::ViewScreenConfig::ViewScreenState,
               std::string,
               ViewScreenRequestCallback),
              (override));
};

class MockWebviewAuthHandler : public WebviewAuthHandler {
 public:
  MockWebviewAuthHandler(content::BrowserContext* context,
                         const std::string& webview_host_name)
      : WebviewAuthHandler(std::make_unique<WebviewAuthDelegate>(),
                           context,
                           webview_host_name) {}
  MockWebviewAuthHandler(const MockWebviewAuthHandler&) = delete;
  MockWebviewAuthHandler& operator=(const WebviewAuthHandler&) = delete;
  ~MockWebviewAuthHandler() override {}

  MOCK_METHOD1(AuthenticateWebview, void(AuthenticateWebviewCallback));
};

class BocaAppPageHandlerTest : public testing::Test {
 public:
  BocaAppPageHandlerTest() = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({ash::features::kBoca},
                                          /*disabled_features=*/{});
    // Set up UserManager related modules.
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    ash::boca_util::RegisterPrefs(local_state_.registry());
    fake_user_manager_.Reset(
        std::make_unique<user_manager::FakeUserManager>(&local_state_));

    auto account_id = AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaId);
    auto browser_context_helper_delegate =
        std::make_unique<ash::FakeBrowserContextHelperDelegate>();
    auto* browser_context_helper_delegate_ptr =
        browser_context_helper_delegate.get();
    browser_context_helper_ = std::make_unique<ash::BrowserContextHelper>(
        std::move(browser_context_helper_delegate));

    // Set up global BocaAppClient's mock.
    boca_app_client_ = std::make_unique<NiceMock<MockBocaAppClient>>();
    EXPECT_CALL(*boca_app_client_, AddSessionManager(_)).Times(1);
    ON_CALL(*boca_app_client_, GetIdentityManager())
        .WillByDefault(Return(nullptr));
    ON_CALL(*boca_app_client_, GetSchoolToolsServerBaseUrl())
        .WillByDefault(Return(kTestDefaultUrl));

    // Sign in a test user.
    fake_user_manager_->AddGaiaUser(account_id,
                                    user_manager::UserType::kRegular);
    fake_user_manager_->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
    auto* browser_context =
        browser_context_helper_delegate_ptr->CreateBrowserContext(
            browser_context_helper_delegate_ptr->GetUserDataDir()->AppendASCII(
                "test-browser-context"),
            /*is_off_the_record=*/false);
    ash::AnnotatedAccountId::Set(browser_context, account_id);

    // Create BocaSessionManager mock.
    EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(1);
    session_manager_ =
        std::make_unique<StrictMock<MockSessionManager>>(&session_client_impl_);

    // Register self as listener.
    ON_CALL(*boca_app_client(), GetSessionManager())
        .WillByDefault(Return(session_manager()));

    // Create the WebContents for the BrowserContext.
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser_context));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());

    EXPECT_CALL(*session_manager(), ToggleAppStatus(/*is_app_opened=*/true))
        .Times(1);
    boca_app_handler_ = std::make_unique<BocaAppHandler>(
        remote_.BindNewPipeAndPassReceiver(),
        // TODO(b/359929870):Setting nullptr for other dependencies for now.
        // Adding test case for classroom and tab info.
        pending_receiver_.InitWithNewPipeAndPassRemote(), web_ui_.get(),
        std::make_unique<MockWebviewAuthHandler>(browser_context,
                                                 kWebviewHostName),
        /*classroom_client_impl=*/nullptr,
        /*content_settings_handler=*/nullptr, &session_client_impl_,
        /*is_producer=*/true);
    boca_app_handler_->SetSpotlightService(&spotlight_service_);
    // Explicitly set pref
    boca_app_handler_->SetPrefForTesting(&local_state_);
  }

  void TearDown() override {
    EXPECT_CALL(*session_manager(), ToggleAppStatus(/*is_app_opened=*/false))
        .Times(1);

    boca_app_handler_.reset();
    web_ui_.reset();
    web_contents_.reset();
    session_manager_.reset();
    boca_app_client_.reset();
    browser_context_helper_.reset();
    fake_user_manager_.Reset();
  }

 protected:
  void TestUserPref(mojom::BocaValidPref pref, base::Value value) {
    base::test::TestFuture<void> set_pref_future;
    boca_app_handler_.get()->SetUserPref(pref, value.Clone(),
                                         set_pref_future.GetCallback());
    set_pref_future.Get();

    base::test::TestFuture<base::Value> get_pref_future;
    boca_app_handler_.get()->GetUserPref(pref, get_pref_future.GetCallback());

    EXPECT_EQ(get_pref_future.Get(), value);
  }

  MockSessionClientImpl* session_client_impl() { return &session_client_impl_; }
  MockBocaAppClient* boca_app_client() { return boca_app_client_.get(); }
  MockSessionManager* session_manager() { return session_manager_.get(); }
  BocaAppHandler* boca_app_handler() { return boca_app_handler_.get(); }
  MockSpotlightService* spotlight_service() { return &spotlight_service_; }
  MockWebviewAuthHandler* webview_auth_handler() {
    return static_cast<MockWebviewAuthHandler*>(
        boca_app_handler_.get()->GetWebviewAuthHandlerForTesting());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;

  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  std::unique_ptr<ash::BrowserContextHelper> browser_context_helper_;
  // Among all BocaAppHandler dependencies,BocaAppClient should construct early
  // and destruct last.
  std::unique_ptr<NiceMock<MockBocaAppClient>> boca_app_client_;

  StrictMock<MockSessionClientImpl> session_client_impl_{nullptr};
  std::unique_ptr<StrictMock<MockSessionManager>> session_manager_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  mojo::Remote<mojom::PageHandler> remote_;
  mojo::PendingReceiver<mojom::Page> pending_receiver_;
  std::unique_ptr<BocaAppHandler> boca_app_handler_;
  StrictMock<MockSpotlightService> spotlight_service_{nullptr};
};

TEST_F(BocaAppPageHandlerTest, CreateSessionWithFullInput) {
  auto session_duration = base::Minutes(2);

  std::vector<mojom::IdentityPtr> students;
  students.push_back(
      mojom::Identity::New("1", "a", "a@gmail.com", GURL("cdn://s1")));
  students.push_back(
      mojom::Identity::New("2", "b", "b@gmail.com", GURL("cdn://s2")));

  const auto config = mojom::Config::New(
      session_duration, std::nullopt, nullptr, std::move(students),
      std::vector<mojom::IdentityPtr>{}, GetCommonTestLockOnTaskConfig(),
      GetCommonCaptionConfig(), "");
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<bool> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  CreateSessionRequest request(
      nullptr, kTestUrlBase, teacher, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_client_impl(), CreateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
            ASSERT_EQ(session_duration, request->duration());
            ASSERT_EQ(
                ::boca::Session::SessionState::Session_SessionState_ACTIVE,
                request->session_state());

            // Optional attribute.
            ASSERT_EQ(1, request->roster()->student_groups().size());
            ASSERT_EQ(2,
                      request->roster()->student_groups()[0].students().size());
            EXPECT_EQ(
                "1",
                request->roster()->student_groups()[0].students()[0].gaia_id());
            EXPECT_EQ("a", request->roster()
                               ->student_groups()[0]
                               .students()[0]
                               .full_name());
            EXPECT_EQ(
                "a@gmail.com",
                request->roster()->student_groups()[0].students()[0].email());
            EXPECT_EQ("cdn://s1", request->roster()
                                      ->student_groups()[0]
                                      .students()[0]
                                      .photo_url());

            EXPECT_EQ(
                "2",
                request->roster()->student_groups()[0].students()[1].gaia_id());
            EXPECT_EQ("b", request->roster()
                               ->student_groups()[0]
                               .students()[1]
                               .full_name());
            EXPECT_EQ(
                "b@gmail.com",
                request->roster()->student_groups()[0].students()[1].email());
            EXPECT_EQ("cdn://s2", request->roster()
                                      ->student_groups()[0]
                                      .students()[1]
                                      .photo_url());
            ASSERT_TRUE(request->on_task_config());
            EXPECT_TRUE(request->on_task_config()->active_bundle().locked());
            ASSERT_EQ(2, request->on_task_config()
                             ->active_bundle()
                             .content_configs()
                             .size());
            EXPECT_EQ("google", request->on_task_config()
                                    ->active_bundle()
                                    .content_configs()[0]
                                    .title());
            EXPECT_EQ("http://google.com/", request->on_task_config()
                                                ->active_bundle()
                                                .content_configs()[0]
                                                .url());
            EXPECT_EQ("data/image", request->on_task_config()
                                        ->active_bundle()
                                        .content_configs()[0]
                                        .favicon_url());
            EXPECT_EQ(
                ::boca::LockedNavigationOptions::NavigationType::
                    LockedNavigationOptions_NavigationType_OPEN_NAVIGATION,
                request->on_task_config()
                    ->active_bundle()
                    .content_configs()[0]
                    .locked_navigation_options()
                    .navigation_type());

            EXPECT_EQ("youtube", request->on_task_config()
                                     ->active_bundle()
                                     .content_configs()[1]
                                     .title());
            EXPECT_EQ("http://youtube.com/", request->on_task_config()
                                                 ->active_bundle()
                                                 .content_configs()[1]
                                                 .url());
            EXPECT_EQ("data/image", request->on_task_config()
                                        ->active_bundle()
                                        .content_configs()[1]
                                        .favicon_url());
            EXPECT_EQ(
                ::boca::LockedNavigationOptions::NavigationType::
                    LockedNavigationOptions_NavigationType_BLOCK_NAVIGATION,
                request->on_task_config()
                    ->active_bundle()
                    .content_configs()[1]
                    .locked_navigation_options()
                    .navigation_type());

            ASSERT_TRUE(request->captions_config());
            EXPECT_TRUE(request->captions_config()->captions_enabled());
            EXPECT_TRUE(request->captions_config()->translations_enabled());
            request->callback().Run(std::make_unique<::boca::Session>());
          })));

  // Verify local events dispatched
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);

  boca_app_handler()->CreateSession(config->Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get());
}

TEST_F(BocaAppPageHandlerTest, CreateSessionWithCritialInputOnly) {
  auto session_duration = base::Minutes(2);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<bool> future_1;

  const auto config = mojom::Config::New(
      session_duration, std::nullopt, nullptr,
      std::vector<mojom::IdentityPtr>{}, std::vector<mojom::IdentityPtr>{},
      mojom::OnTaskConfigPtr(nullptr), mojom::CaptionConfigPtr(nullptr), "");

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  CreateSessionRequest request(
      nullptr, kTestUrlBase, teacher, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_client_impl(), CreateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
            ASSERT_EQ(session_duration, request->duration());
            ASSERT_EQ(
                ::boca::Session::SessionState::Session_SessionState_ACTIVE,
                request->session_state());

            ASSERT_FALSE(request->captions_config());
            ASSERT_FALSE(request->on_task_config());
            ASSERT_FALSE(request->roster());
            request->callback().Run(std::make_unique<::boca::Session>());
          })));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);

  // Verify local events not dispatched
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(0);

  boca_app_handler()->CreateSession(config.Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get());
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithFullInputTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        auto* start_time = session->mutable_start_time();
        start_time->set_seconds(1111111);
        start_time->set_nanos(22000000);

        session->mutable_duration()->set_seconds(120);
        session->set_session_state(::boca::Session::ACTIVE);
        auto* teacher = session->mutable_teacher();
        teacher->set_email("teacher@email.com");
        teacher->set_full_name("teacher");
        teacher->set_gaia_id("000");
        teacher->set_photo_url("cdn://s");

        auto* access_code = session->mutable_join_code();
        access_code->set_code("testCode");

        auto* student_groups_1 =
            session->mutable_roster()->mutable_student_groups()->Add();
        student_groups_1->set_title(kMainStudentGroupName);
        student_groups_1->set_group_source(::boca::StudentGroup::CLASSROOM);
        auto* student = student_groups_1->mutable_students()->Add();
        student->set_email("dog@email.com");
        student->set_full_name("dog");
        student->set_gaia_id("111");
        student->set_photo_url("cdn://s1");

        auto* student_groups_2 =
            session->mutable_roster()->mutable_student_groups()->Add();
        student_groups_2->set_title("accessCode");
        student_groups_2->set_group_source(::boca::StudentGroup::JOIN_CODE);
        auto* student_2 = student_groups_2->mutable_students()->Add();
        student_2->set_email("dog1@email.com");
        student_2->set_full_name("dog1");
        student_2->set_gaia_id("222");
        student_2->set_photo_url("cdn://s2");

        ::boca::SessionConfig session_config;
        auto* caption_config_1 = session_config.mutable_captions_config();

        caption_config_1->set_captions_enabled(true);
        caption_config_1->set_translations_enabled(true);

        auto* active_bundle =
            session_config.mutable_on_task_config()->mutable_active_bundle();
        active_bundle->set_locked(true);
        auto* content = active_bundle->mutable_content_configs()->Add();
        content->set_url("http://google.com/");
        content->set_favicon_url("data/image");
        content->set_title("google");
        content->mutable_locked_navigation_options()->set_navigation_type(
            ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);

        (*session->mutable_student_group_configs())[kMainStudentGroupName] =
            std::move(session_config);

        auto* student_statuses = session->mutable_student_statuses();
        std::map<std::string, ::boca::StudentStatus> activities;
        ::boca::StudentStatus status_1;
        status_1.set_state(::boca::StudentStatus::ACTIVE);
        ::boca::StudentDevice device_1;
        auto* activity_1 = device_1.mutable_activity();
        activity_1->mutable_active_tab()->set_title("google");
        ::boca::StudentDevice device_11;
        auto* activity_11 = device_11.mutable_activity();
        activity_11->mutable_active_tab()->set_title("google1");
        device_11.mutable_view_screen_config()
            ->mutable_connection_param()
            ->set_connection_code("abcd");
        (*status_1.mutable_devices())["device1"] = std::move(device_1);
        (*status_1.mutable_devices())["device11"] = std::move(device_11);
        (*student_statuses)["000"] = std::move(status_1);

        request->callback().Run(std::move(session));
      })));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(NotNull(), /*dispatch_event=*/true))
      .Times(1);

  boca_app_handler()->GetSession(future_1.GetCallback());

  auto result0 = std::move(future_1.Take()->get_session());
  auto result = std::move(result0->config);
  EXPECT_EQ(120, result->session_duration.InSeconds());
  EXPECT_EQ(1111111.022,
            result->session_start_time->InSecondsFSinceUnixEpoch());
  EXPECT_EQ("teacher", result->teacher->name);
  EXPECT_EQ("teacher@email.com", result->teacher->email);
  EXPECT_EQ("000", result->teacher->id);
  EXPECT_EQ("cdn://s", result->teacher->photo_url->spec());

  EXPECT_EQ("testCode", result->access_code);
  EXPECT_EQ(true, result->caption_config->session_caption_enabled);
  EXPECT_EQ(true, result->caption_config->session_translation_enabled);

  ASSERT_EQ(1u, result->students.size());

  EXPECT_EQ("dog", result->students[0]->name);
  EXPECT_EQ("111", result->students[0]->id);
  EXPECT_EQ("dog@email.com", result->students[0]->email);
  EXPECT_EQ("cdn://s1", result->students[0]->photo_url->spec());

  ASSERT_EQ(1u, result->students_join_via_code.size());

  EXPECT_EQ("dog1", result->students_join_via_code[0]->name);
  EXPECT_EQ("222", result->students_join_via_code[0]->id);
  EXPECT_EQ("dog1@email.com", result->students_join_via_code[0]->email);
  EXPECT_EQ("cdn://s2", result->students_join_via_code[0]->photo_url->spec());

  ASSERT_EQ(1u, result->on_task_config->tabs.size());
  ASSERT_TRUE(result->on_task_config->is_locked);
  EXPECT_EQ(mojom::NavigationType::kOpen,
            result->on_task_config->tabs[0]->navigation_type);
  EXPECT_EQ("http://google.com/",
            result->on_task_config->tabs[0]->tab->url.spec());
  EXPECT_EQ("google", result->on_task_config->tabs[0]->tab->title);
  EXPECT_EQ("data/image", result->on_task_config->tabs[0]->tab->favicon);

  auto activities = std::move(result0->activities);
  EXPECT_EQ(2u, activities.size());
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithPartialInputTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;
  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        session->mutable_duration()->set_seconds(120);
        session->set_session_state(::boca::Session::ACTIVE);
        request->callback().Run(std::move(session));
      })));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(NotNull(), /*dispatch_event=*/true))
      .Times(1);

  boca_app_handler()->GetSession(future_1.GetCallback());

  auto result = std::move(future_1.Take()->get_session()->config);
  EXPECT_EQ(120, result->session_duration.InSeconds());
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithHTTPError) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_BAD_REQUEST));
      })));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(0);
  boca_app_handler()->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GetSessionError::kHTTPError, result->get_error());
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithNullPtrInputTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke(
          [&](auto request) { request->callback().Run(base::ok(nullptr)); })));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(IsNull(), /*dispatch_event=*/true))
      .Times(1);

  boca_app_handler()->GetSession(future_1.GetCallback());

  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GetSessionError::kEmpty, result->get_error());
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithNonActiveSessionTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;
  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        request->callback().Run(std::make_unique<::boca::Session>());
      })));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(IsNull(), /*dispatch_event=*/true))
      .Times(1);

  boca_app_handler()->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GetSessionError::kEmpty, result->get_error());
}

TEST_F(BocaAppPageHandlerTest,
       GetSessionWithEmptySessionConfigShouldNotCrashTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        session->set_session_state(::boca::Session::ACTIVE);
        request->callback().Run(std::move(session));
      })));
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(NotNull(), /*dispatch_event=*/true))
      .Times(1);

  boca_app_handler()->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_FALSE(result->is_error());
}

TEST_F(BocaAppPageHandlerTest, EndSessionSucceed) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->mutable_duration()->set_seconds(120);
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  UpdateSessionRequest request(nullptr, kTestUrlBase, teacher, session_id,
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
            ASSERT_EQ(::boca::Session::PAST, *request->session_state());
            request->callback().Run(std::make_unique<::boca::Session>());
          })));

  boca_app_handler()->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, EndSessionWithHTTPFailure) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->mutable_duration()->set_seconds(120);
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  UpdateSessionRequest request(nullptr, kTestUrlBase, teacher, session_id,
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
            ASSERT_EQ(::boca::Session::PAST, *request->session_state());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          })));

  boca_app_handler()->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, EndSessionWithEmptyResponse) {
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, EndSessionWithNonActiveResponse) {
  ::boca::Session session;

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, ExtendSessionDurationSucceed) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->mutable_duration()->set_seconds(120);
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  UpdateSessionRequest request(nullptr, kTestUrlBase, teacher, session_id,
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
        ASSERT_EQ(base::Seconds(150), *request->duration());
        request->callback().Run(std::make_unique<::boca::Session>());
      })));

  boca_app_handler()->ExtendSessionDuration(base::Seconds(30),
                                            future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, UpdateOnTaskConfigSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            EXPECT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            ASSERT_EQ(GetCommonTestLockOnTaskConfigProto().SerializeAsString(),
                      request->on_task_config()->SerializeAsString());
            // Use latest caption cofig value from session.
            ASSERT_EQ(GetCommonActiveSessionProto()
                          .student_group_configs()
                          .at(kMainStudentGroupName)
                          .captions_config()
                          .SerializeAsString(),
                      request->captions_config()->SerializeAsString());
            request->callback().Run(std::make_unique<::boca::Session>(
                GetCommonActiveSessionProto()));
          })));
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, UpdateOnTaskConfigWithEmptySession) {
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, UpdateOnTaskConfigWithNonActiveSession) {
  ::boca::Session non_active_session;

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&non_active_session));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, UpdateOnTaskConfigWithHTTPFailure) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(2)
      .WillRepeatedly(Return(&session));

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          })));

  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kHTTPError, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, UpdateCaptionWithEmptySession) {
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, UpdateCaptionWithNonActiveSession) {
  ::boca::Session session;
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, UpdateCaptionConfigSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                      request->captions_config()->SerializeAsString());
            // Use latest on task cofig value from session.
            ASSERT_EQ(GetCommonActiveSessionProto()
                          .student_group_configs()
                          .at(kMainStudentGroupName)
                          .on_task_config()
                          .SerializeAsString(),
                      request->on_task_config()->SerializeAsString());
            request->callback().Run(std::make_unique<::boca::Session>(
                GetCommonActiveSessionProto()));
          })));

  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest,
       UpdateCaptionConfigWithLocalConfigOnlyShouldNotSendServerRequest) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_)).Times(0);

  boca_app_handler()->UpdateCaptionConfig(
      mojom::CaptionConfig::New(/*=session_caption_enabled*/ false,
                                /*local_caption_enabled*/ true,
                                /*=session_translation_enabled*/ false),
      future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, UpdateCaptionWithHTTPFailure) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(2)
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          })));

  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kHTTPError, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest,
       UpdateOnTaskConfigWithPendingCaptionConfigShouldNotOverride) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(2);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(2)
      .WillRepeatedly(Return(&session));
  // Failed remote caption update should still dispatch local events.
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_2;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        // Use latest on task cofig value from session.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .on_task_config()
                      .SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        request->callback().Run(std::unique_ptr<::boca::Session>());
      })))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        // Use pending on task config.
        ASSERT_EQ(GetCommonTestUnLockOnTaskConfigProto().SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        request->callback().Run(std::unique_ptr<::boca::Session>());
      })));
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                         future_2.GetCallback());

  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());

  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest,
       UpdateCaptionConfigWithPendingOnTaskConfigShouldNotOverride) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(2);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(2)
      .WillRepeatedly(Return(&session));
  // Failed remote caption update should still dispatch local events.
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_2;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonTestUnLockOnTaskConfigProto().SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        // Use latest caption cofig value from session.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .captions_config()
                      .SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        request->callback().Run(std::unique_ptr<::boca::Session>());
      })))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonTestUnLockOnTaskConfigProto().SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        // Use pending on task config.
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        request->callback().Run(std::unique_ptr<::boca::Session>());
      })));
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                         future_1.GetCallback());
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_2.GetCallback());

  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest,
       UpdateOnTaskConfigWithFailedCaptionConfigShouldUseSessionData) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(3)
      .WillRepeatedly(Return(&session));
  // Failed remote caption update should still dispatch local events.
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_2;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        // Use session on task config.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .on_task_config()
                      .SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
      })))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonTestUnLockOnTaskConfigProto().SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        // Use session caption config.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .captions_config()
                      .SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        request->callback().Run(
            std::make_unique<::boca::Session>(GetCommonActiveSessionProto()));
      })));
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                         future_2.GetCallback());

  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest,
       UpdateCaptionConfigWithFailedOnTaskConfigShouldUseSessionData) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(3)
      .WillRepeatedly(Return(&session));
  // Failed remote caption update should still dispatch local events.
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_2;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonTestUnLockOnTaskConfigProto().SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        // Use latest caption cofig value from session.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .captions_config()
                      .SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
      })))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        // Use session on task config.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .on_task_config()
                      .SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        request->callback().Run(
            std::make_unique<::boca::Session>(GetCommonActiveSessionProto()));
      })));
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                         future_1.GetCallback());
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_2.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, UpdateEmptyStudentActivitySucceed) {
  std::map<std::string, ::boca::StudentStatus> activities;
  base::test::TestFuture<std::vector<mojom::IdentifiedActivityPtr>> future;
  boca_app_handler()->SetActivityInterceptorCallbackForTesting(
      future.GetCallback());
  boca_app_handler()->OnConsumerActivityUpdated(activities);
  auto result = future.Take();
  ASSERT_TRUE(result.empty());
}

TEST_F(BocaAppPageHandlerTest, DISABLED_UpdateNonEmptyStudentActivitySucceed) {
  std::map<std::string, ::boca::StudentStatus> activities;
  ::boca::StudentStatus status_1;
  status_1.set_state(::boca::StudentStatus::ACTIVE);
  ::boca::StudentDevice device_1;
  auto* activity_1 = device_1.mutable_activity();
  activity_1->mutable_active_tab()->set_title("google");
  ::boca::StudentDevice device_11;
  auto* activity_11 = device_11.mutable_activity();
  activity_11->mutable_active_tab()->set_title("google1");
  device_11.mutable_view_screen_config()
      ->mutable_connection_param()
      ->set_connection_code("abcd");
  (*status_1.mutable_devices())["device1"] = std::move(device_1);
  (*status_1.mutable_devices())["device11"] = std::move(device_11);

  ::boca::StudentStatus status_2;
  status_2.set_state(::boca::StudentStatus::ADDED);
  ::boca::StudentDevice device_2;
  auto* activity_2 = device_2.mutable_activity();
  activity_2->mutable_active_tab()->set_title("youtube");
  (*status_2.mutable_devices())["device2"] = std::move(device_2);
  activities.emplace("1", std::move(status_1));
  activities.emplace("2", std::move(status_2));

  // EXPECT_CALL(mock_page(), OnStudentActivityUpdated(_)).Times(1);
  base::test::TestFuture<std::vector<mojom::IdentifiedActivityPtr>> future;
  boca_app_handler()->SetActivityInterceptorCallbackForTesting(
      future.GetCallback());
  boca_app_handler()->OnConsumerActivityUpdated(activities);
  auto result = future.Take();
  EXPECT_EQ(3u, result.size());
  // Verify multiple devices are both added.
  EXPECT_EQ("1", result[0]->id);
  EXPECT_TRUE(result[0]->activity->is_active);
  EXPECT_EQ("1", result[1]->id);
  EXPECT_TRUE(result[1]->activity->is_active);

  std::vector<std::string> tabs = {"google", "google1"};
  // The order shouldn't matter.
  EXPECT_TRUE(std::find(tabs.begin(), tabs.end(),
                        result[0]->activity->active_tab) != tabs.end());
  EXPECT_TRUE(std::find(tabs.begin(), tabs.end(),
                        result[1]->activity->active_tab) != tabs.end());

  EXPECT_EQ("2", result[2]->id);
  EXPECT_EQ("youtube", result[2]->activity->active_tab);
  EXPECT_FALSE(result[2]->activity->is_active);

  // Connection code should be set
  EXPECT_EQ("abcd", result[0]->activity->view_screen_session_code);
  EXPECT_EQ("", result[1]->activity->view_screen_session_code);
  EXPECT_EQ("", result[2]->activity->view_screen_session_code);
}

TEST_F(BocaAppPageHandlerTest, RemoveStudentSucceedAlsoRemoveFromLocalSession) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->set_session_state(::boca::Session::ACTIVE);
  auto* roster = session->mutable_roster();
  auto* student_groups = roster->mutable_student_groups()->Add();
  auto* student_1 = student_groups->mutable_students()->Add();
  student_1->set_gaia_id("2");

  auto* student_groups_2 = roster->mutable_student_groups()->Add();
  auto* student_3 = student_groups_2->mutable_students()->Add();
  student_3->set_gaia_id("4");
  auto* student_4 = student_groups_2->mutable_students()->Add();
  student_4->set_gaia_id("5");
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::RemoveStudentError>> future_1;

  RemoveStudentRequest request(nullptr, kTestUrlBase, kGaiaId, session_id,
                               future.GetCallback());

  const char student_id[] = "4";
  EXPECT_CALL(*session_client_impl(), RemoveStudent(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId, request->gaia_id());
            ASSERT_EQ(1u, request->student_ids().size());
            ASSERT_EQ(student_id, request->student_ids()[0]);
            request->callback().Run(true);
          })));

  boca_app_handler()->RemoveStudent(student_id, future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
  EXPECT_EQ(2, session->roster().student_groups().size());
  EXPECT_EQ(1, session->roster().student_groups()[1].students().size());
  EXPECT_EQ("5", session->roster().student_groups()[1].students()[0].gaia_id());
}

TEST_F(BocaAppPageHandlerTest, RemoveStudentWithHTTPFailure) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::RemoveStudentError>> future_1;

  RemoveStudentRequest request(nullptr, kTestUrlBase, kGaiaId, session_id,
                               future.GetCallback());

  const char student_id[] = "id";
  EXPECT_CALL(*session_client_impl(), RemoveStudent(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId, request->gaia_id());
            ASSERT_EQ(1u, request->student_ids().size());
            ASSERT_EQ(student_id, request->student_ids()[0]);
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          })));

  boca_app_handler()->RemoveStudent(student_id, future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, RemoveStudentWithEmptySession) {
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));

  // API callback.
  base::test::TestFuture<std::optional<mojom::RemoveStudentError>> future_1;

  boca_app_handler()->RemoveStudent("any", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::RemoveStudentError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, RemoveStudentWithNonActiveSession) {
  ::boca::Session session;
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));

  // API callback.
  base::test::TestFuture<std::optional<mojom::RemoveStudentError>> future_1;

  boca_app_handler()->RemoveStudent("any", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::RemoveStudentError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, OnSessionSessionStartedSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  boca_app_handler()->SetSessionConfigInterceptorCallbackForTesting(
      future.GetCallback());
  boca_app_handler()->OnSessionStarted(std::string(), ::boca::UserIdentity());
  auto result = future.Take();
  ASSERT_TRUE(result->is_config());
}

TEST_F(BocaAppPageHandlerTest, OnSessionEndedSucceed) {
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  boca_app_handler()->SetSessionConfigInterceptorCallbackForTesting(
      future.GetCallback());
  boca_app_handler()->OnSessionEnded("any");
  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
}

TEST_F(BocaAppPageHandlerTest, OnSessionCaptionUpdatedSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  boca_app_handler()->SetSessionConfigInterceptorCallbackForTesting(
      future.GetCallback());
  boca_app_handler()->OnSessionCaptionConfigUpdated(
      "any", ::boca::CaptionsConfig(), std::string());
  auto result = future.Take();
  ASSERT_TRUE(result->is_config());
}

TEST_F(BocaAppPageHandlerTest, OnSessionBundleUpdatedSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  boca_app_handler()->SetSessionConfigInterceptorCallbackForTesting(
      future.GetCallback());
  boca_app_handler()->OnBundleUpdated(::boca::Bundle());
  auto result = future.Take();
  ASSERT_TRUE(result->is_config());
}

TEST_F(BocaAppPageHandlerTest, OnSessionRosterUpdatedSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  boca_app_handler()->SetSessionConfigInterceptorCallbackForTesting(
      future.GetCallback());
  boca_app_handler()->OnSessionRosterUpdated({});
  auto result = future.Take();
  ASSERT_TRUE(result->is_config());
}

TEST_F(BocaAppPageHandlerTest, JoinSessionSucceeded) {
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::SubmitAccessCodeError>> future_1;

  JoinSessionRequest request(nullptr, kTestUrlBase, ::boca::UserIdentity(),
                             "device", "code", future.GetCallback());

  EXPECT_CALL(*session_client_impl(), JoinSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            request->callback().Run(std::make_unique<::boca::Session>());
          })));

  boca_app_handler()->SubmitAccessCode("code", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, JoinSessionFailed) {
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(0);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::SubmitAccessCodeError>> future_1;

  JoinSessionRequest request(nullptr, kTestUrlBase, ::boca::UserIdentity(),
                             "device", "code", future.GetCallback());

  EXPECT_CALL(*session_client_impl(), JoinSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          })));

  boca_app_handler()->SubmitAccessCode("code", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::SubmitAccessCodeError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, ViewScreenSucceeded) {
  const std::string student_id = "123";
  EXPECT_CALL(*spotlight_service(), ViewScreen(student_id, kTestUrlBase, _))
      .WillOnce(WithArg<2>(Invoke(
          [&](auto request) { std::move(request).Run(base::ok(true)); })));

  base::test::TestFuture<std::optional<mojom::ViewStudentScreenError>> future;

  boca_app_handler()->ViewStudentScreen(student_id, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, ViewScreenFailed) {
  const std::string student_id = "123";

  EXPECT_CALL(*spotlight_service(), ViewScreen(student_id, kTestUrlBase, _))
      .WillOnce(WithArg<2>(Invoke([&](auto request) {
        std::move(request).Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
      })));

  base::test::TestFuture<std::optional<mojom::ViewStudentScreenError>> future;

  boca_app_handler()->ViewStudentScreen(student_id, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(mojom::ViewStudentScreenError::kHTTPError, future.Get().value());
}

TEST_F(BocaAppPageHandlerTest, AuthenticateWebviewSuccess) {
  EXPECT_CALL(*webview_auth_handler(), AuthenticateWebview(testing::_))
      .WillOnce(
          testing::Invoke(base::test::RunOnceCallback<0>(/*is_success=*/true)));
  base::RunLoop run_loop;
  boca_app_handler()->AuthenticateWebview(
      base::BindLambdaForTesting([&](bool success) -> void {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BocaAppPageHandlerTest, AuthenticateWebviewFailure) {
  EXPECT_CALL(*webview_auth_handler(), AuthenticateWebview(testing::_))
      .WillOnce(testing::Invoke(
          base::test::RunOnceCallback<0>(/*is_success=*/false)));
  base::RunLoop run_loop;
  boca_app_handler()->AuthenticateWebview(
      base::BindLambdaForTesting([&](bool success) -> void {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BocaAppPageHandlerTest, TestPrefGetterAndSetter) {
  base::Value::Dict nav_map;
  base::Value::Dict nav_occurrence;
  nav_occurrence.Set("navRule", 0);
  nav_occurrence.Set("occurence", 1);
  nav_map.Set("google.com", std::move(nav_occurrence));
  TestUserPref(mojom::BocaValidPref::kNavigationSetting,
               /*value=*/base::Value(nav_map.Clone()));
}

TEST_F(BocaAppPageHandlerTest, EndViewScreenSessionSucceeded) {
  const std::string student_id = "123";
  EXPECT_CALL(
      *spotlight_service(),
      UpdateViewScreenState(student_id, ::boca::ViewScreenConfig::INACTIVE,
                            kTestUrlBase, _))
      .WillOnce(WithArg<3>(Invoke(
          [&](auto request) { std::move(request).Run(base::ok(true)); })));

  base::test::TestFuture<std::optional<mojom::EndViewScreenSessionError>>
      future;

  boca_app_handler()->EndViewScreenSession(student_id, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, EndViewScreenSessionFailed) {
  const std::string student_id = "123";

  EXPECT_CALL(
      *spotlight_service(),
      UpdateViewScreenState(student_id, ::boca::ViewScreenConfig::INACTIVE,
                            kTestUrlBase, _))
      .WillOnce(WithArg<3>(Invoke([&](auto request) {
        std::move(request).Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
      })));

  base::test::TestFuture<std::optional<mojom::EndViewScreenSessionError>>
      future;

  boca_app_handler()->EndViewScreenSession(student_id, future.GetCallback());
  EXPECT_EQ(mojom::EndViewScreenSessionError::kHTTPError, future.Get().value());
}

class BocaAppPageHandlerFloatModeTest : public AshTestBase {
 public:
  BocaAppPageHandlerFloatModeTest() = default;
  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
  }
};

TEST_F(BocaAppPageHandlerFloatModeTest, SetFloatModeTest) {
  UpdateDisplay("1366x768");
  std::unique_ptr<aura::Window> window = CreateToplevelTestWindow(
      gfx::Rect(800, 200, 500, 150), desks_util::GetActiveDeskContainerId());

  base::test::TestFuture<bool> future;
  BocaAppHandler::SetFloatModeAndBoundsForWindow(true, window.get(),
                                                 future.GetCallback());

  // TODO(crbug.com/374881187)We don't have a way to verify float state in unit
  // test, verify bounds for now. Move to browser test in the future.
  // WindowState* window_state = WindowState::Get(window.get());
  // EXPECT_TRUE(window_state->IsFloated());
  EXPECT_EQ(400, window->bounds().width());
  EXPECT_EQ(600, window->bounds().height());
  EXPECT_EQ(958, window->bounds().x());
  EXPECT_EQ(8, window->bounds().y());
  EXPECT_TRUE(future.Get());
}

TEST_F(BocaAppPageHandlerFloatModeTest, SetFloatModeTestWithFalse) {
  base::test::TestFuture<bool> future;
  BocaAppHandler::SetFloatModeAndBoundsForWindow(false, nullptr,
                                                 future.GetCallback());

  EXPECT_FALSE(future.Get());
}

}  // namespace
}  // namespace ash::boca
