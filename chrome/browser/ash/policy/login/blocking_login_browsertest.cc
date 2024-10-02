// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

namespace em = ::enterprise_management;

const char kDomain[] = "domain.com";
const char kUsername[] = "user@domain.com";
const char kUsernameOtherDomain[] = "user@other.com";

const char kOAuthCodeCookie[] = "oauth_code=1234; Secure; HttpOnly";

const char kOAuth2TokenPairData[] =
    "{"
    "  \"refresh_token\": \"1234\","
    "  \"access_token\": \"5678\","
    "  \"expires_in\": 3600"
    "}";

const char kOAuth2AccessTokenData[] =
    "{"
    "  \"access_token\": \"5678\","
    "  \"expires_in\": 3600"
    "}";

const char kDMRegisterRequest[] = "/device_management?request=register";
const char kDMPolicyRequest[] = "/device_management?request=policy";

void CopyLockResult(base::RunLoop* loop,
                    ash::InstallAttributes::LockResult* out,
                    ash::InstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

}  // namespace

struct BlockingLoginTestParam {
  const int steps;
  const char* username;
  const bool enroll_device;
};

// TODO(poromov): This test is completely broken - it originally was built
// when we made an entirely different set of network calls on startup. As a
// result it generates random failures in startup network requests, then waits
// to see if the profile finishes loading which is not at all what it is
// intended to test. We need to fix this test or remove it (crbug.com/580537).
class BlockingLoginTest
    : public ash::OobeBaseTest,
      public ProfileManagerObserver,
      public testing::WithParamInterface<BlockingLoginTestParam> {
 public:
  BlockingLoginTest() : profile_added_(nullptr) {}

  BlockingLoginTest(const BlockingLoginTest&) = delete;
  BlockingLoginTest& operator=(const BlockingLoginTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ash::OobeBaseTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kDeviceManagementUrl,
        embedded_test_server()->GetURL("/device_management").spec());
  }

  void SetUpOnMainThread() override {
    g_browser_process->profile_manager()->AddObserver(this);

    ash::OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    RunUntilIdle();
    EXPECT_TRUE(responses_.empty());
    g_browser_process->profile_manager()->RemoveObserver(this);
    ash::OobeBaseTest::TearDownOnMainThread();
  }

  void OnProfileAdded(Profile* profile) override {
    if (ash::ProfileHelper::IsLockScreenAppProfile(profile)) {
      return;
    }
    ASSERT_EQ(profile_added_, nullptr);
    profile_added_ = profile;
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void EnrollDevice(const std::string& domain) {
    base::RunLoop loop;
    ash::InstallAttributes::LockResult result;
    ash::InstallAttributes::Get()->LockDevice(
        DEVICE_MODE_ENTERPRISE, domain, std::string(), "100200300",
        base::BindOnce(&CopyLockResult, &loop, &result));
    loop.Run();
    EXPECT_EQ(ash::InstallAttributes::LOCK_SUCCESS, result);
    RunUntilIdle();
  }

  void Login(const std::string& username) {
    ash::LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<ash::GaiaScreenHandler>()
        ->ShowSigninScreenForTest(username, "password", "[]");

    // Wait for the session to start after submitting the credentials. This
    // will wait until all the background requests are done.
    ash::test::WaitForPrimaryUserSessionStart();
  }

  // Handles an HTTP request sent to the test server. This handler either
  // uses a canned response in |responses_| if the request path matches one
  // of the URLs that we mock, otherwise this handler delegates to |fake_gaia_|.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::HttpResponse> response;

    GaiaUrls* gaia = GaiaUrls::GetInstance();
    if (request.relative_url == gaia->oauth2_token_url().path() ||
        base::StartsWith(request.relative_url, kDMRegisterRequest,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(request.relative_url, kDMPolicyRequest,
                         base::CompareCase::SENSITIVE)) {
      if (!responses_.empty()) {
        response = std::move(responses_.back());
        responses_.pop_back();
      }
    }

    return response;
  }

  // Creates a new canned response that will respond with the given HTTP
  // status |code|. That response is appended to |responses_| and will be the
  // next response used.
  // Returns a reference to that response, so that it can be further customized.
  net::test_server::BasicHttpResponse& PushResponse(net::HttpStatusCode code) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    net::test_server::BasicHttpResponse* response_ptr = response.get();
    response->set_code(code);
    responses_.push_back(std::move(response));
    return *response_ptr;
  }

  // Returns the body of the register response from the policy server.
  std::string GetRegisterResponse() {
    em::DeviceManagementResponse response;
    em::DeviceRegisterResponse* register_response =
        response.mutable_register_response();
    register_response->set_device_management_token("1234");
    register_response->set_enrollment_type(
        em::DeviceRegisterResponse::ENTERPRISE);
    std::string data;
    EXPECT_TRUE(response.SerializeToString(&data));
    return data;
  }

  // Returns the body of the fetch response from the policy server.
  std::string GetPolicyResponse() {
    em::DeviceManagementResponse response;
    response.mutable_policy_response()->add_responses();
    std::string data;
    EXPECT_TRUE(response.SerializeToString(&data));
    return data;
  }

 protected:
  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &BlockingLoginTest::HandleRequest, base::Unretained(this)));
  }

  raw_ptr<Profile, ExperimentalAsh> profile_added_;

 private:
  std::vector<std::unique_ptr<net::test_server::HttpResponse>> responses_;
};

IN_PROC_BROWSER_TEST_P(BlockingLoginTest, LoginBlocksForUser) {
  // Verify that there isn't a logged in user when the test starts.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_FALSE(user_manager->IsUserLoggedIn());
  EXPECT_FALSE(ash::InstallAttributes::Get()->IsEnterpriseManaged());
  EXPECT_FALSE(profile_added_);

  // Enroll the device, if enrollment is enabled for this test instance.
  if (GetParam().enroll_device) {
    EnrollDevice(kDomain);

    EXPECT_FALSE(user_manager->IsUserLoggedIn());
    EXPECT_TRUE(ash::InstallAttributes::Get()->IsEnterpriseManaged());
    EXPECT_EQ(kDomain, ash::InstallAttributes::Get()->GetDomain());
    EXPECT_FALSE(profile_added_);
    RunUntilIdle();
    EXPECT_FALSE(
        user_manager->IsKnownUser(AccountId::FromUserEmail(kUsername)));
  }

  // Skip the OOBE, go to the sign-in screen, and wait for the login screen to
  // become visible.
  WaitForSigninScreen();
  EXPECT_FALSE(profile_added_);

  // Prepare the fake HTTP responses.
  if (GetParam().steps < 5) {
    // If this instance is not going to complete the entire flow successfully
    // then the last step will fail.

    // This response body is important to make the gaia fetcher skip its delayed
    // retry behavior, which makes testing harder. If this is sent to the policy
    // fetchers then it will make them fail too.
    PushResponse(net::HTTP_UNAUTHORIZED).set_content("Error=AccountDeleted");
  }

  // Push a response for each step that is going to succeed, in reverse order.
  switch (GetParam().steps) {
    default:
      ADD_FAILURE() << "Invalid step number: " << GetParam().steps;
      return;

    case 5:
      PushResponse(net::HTTP_OK).set_content(GetPolicyResponse());
      [[fallthrough]];

    case 4:
      PushResponse(net::HTTP_OK).set_content(GetRegisterResponse());
      [[fallthrough]];

    case 3:
      PushResponse(net::HTTP_OK).set_content(kOAuth2AccessTokenData);
      [[fallthrough]];

    case 2:
      PushResponse(net::HTTP_OK).set_content(kOAuth2TokenPairData);
      [[fallthrough]];

    case 1:
      PushResponse(net::HTTP_OK)
          .AddCustomHeader("Set-Cookie", kOAuthCodeCookie);
      break;

    case 0:
      break;
  }

  // Login now. This verifies that logging in with the canned responses (which
  // may include failures) won't be blocked due to the potential failures.
  EXPECT_FALSE(profile_added_);
  Login(GetParam().username);
  EXPECT_TRUE(profile_added_);
  ASSERT_TRUE(user_manager->IsUserLoggedIn());
  EXPECT_TRUE(user_manager->IsCurrentUserNew());
}

const BlockingLoginTestParam kBlockinLoginTestCases[] = {
    {0, kUsername, true},
    {1, kUsername, true},
    {2, kUsername, true},
    {3, kUsername, true},
    {4, kUsername, true},
    {5, kUsername, true},
    {0, kUsername, false},
    {1, kUsername, false},
    {2, kUsername, false},
    {3, kUsername, false},
    {4, kUsername, false},
    {5, kUsername, false},
    {0, kUsernameOtherDomain, true},
    {1, kUsernameOtherDomain, true},
    {2, kUsernameOtherDomain, true},
    {3, kUsernameOtherDomain, true},
    {4, kUsernameOtherDomain, true},
    {5, kUsernameOtherDomain, true},
};

// TODO(poromov): Disabled because it has become flaky due to incorrect mock
// network requests - re-enable this when https://crbug.com/580537 is fixed.
INSTANTIATE_TEST_SUITE_P(DISABLED_BlockingLoginTestInstance,
                         BlockingLoginTest,
                         testing::ValuesIn(kBlockinLoginTestCases));

}  // namespace policy
