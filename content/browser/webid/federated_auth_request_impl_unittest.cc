// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/containers/adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/test/delegated_idp_network_request_manager.h"
#include "content/browser/webid/test/federated_auth_request_request_token_callback_helper.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_auto_reauthn_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/browser/webid/webid_utils.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::RequestTokenStatus;
using AccountList = content::IdpNetworkRequestManager::AccountList;
using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using AuthRequestCallbackHelper =
    content::FederatedAuthRequestRequestTokenCallbackHelper;
using DismissReason = content::IdentityRequestDialogController::DismissReason;
using FedCmEntry = ukm::builders::Blink_FedCm;
using FedCmIdpEntry = ukm::builders::Blink_FedCmIdp;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

constexpr char kProviderUrlFull[] = "https://idp.example/fedcm.json";
constexpr char kRpUrl[] = "https://rp.example/";
constexpr char kRpOtherUrl[] = "https://rp.example/random/";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kCrossOriginAccountsEndpoint[] = "https://idp2.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kClientMetadataEndpoint[] =
    "https://idp.example/client_metadata";
constexpr char kMetricsEndpoint[] = "https://idp.example/metrics";
constexpr char kPrivacyPolicyUrl[] = "https://rp.example/pp";
constexpr char kTermsOfServiceUrl[] = "https://rp.example/tos";
constexpr char kClientId[] = "client_id_123";
constexpr char kNonce[] = "nonce123";
constexpr char kAccountEmailNicolas[] = "nicolas@email.com";
constexpr char kAccountEmailPeter[] = "peter@email.com";
constexpr char kAccountEmailZach[] = "zach@email.com";
constexpr char kAccountId[] = "1234";
constexpr char kAccountIdNicolas[] = "nico_id";
constexpr char kAccountIdPeter[] = "peter_id";
constexpr char kAccountIdZach[] = "zach_id";
constexpr char kEmail[] = "ken@idp.example";

// Values will be added here as token introspection is implemented.
constexpr char kToken[] = "[not a real token]";
constexpr char kEmptyToken[] = "";

static const std::initializer_list<IdentityRequestAccount> kSingleAccount{{
    kAccountId,                 // id
    kEmail,                     // email
    "Ken R. Example",           // name
    "Ken",                      // given_name
    GURL(),                     // picture
    std::vector<std::string>()  // hints
}};

static const std::initializer_list<IdentityRequestAccount>
    kSingleAccountWithHint{{
        kAccountId,           // id
        kEmail,               // email
        "Ken R. Example",     // name
        "Ken",                // given_name
        GURL(),               // picture
        {kAccountId, kEmail}  // hints
    }};

static const std::initializer_list<IdentityRequestAccount> kMultipleAccounts{
    {
        kAccountIdNicolas,           // id
        kAccountEmailNicolas,        // email
        "Nicolas P",                 // name
        "Nicolas",                   // given_name
        GURL(),                      // picture
        std::vector<std::string>(),  // hints
        LoginState::kSignUp          // login_state
    },
    {
        kAccountIdPeter,             // id
        kAccountEmailPeter,          // email
        "Peter K",                   // name
        "Peter",                     // given_name
        GURL(),                      // picture
        std::vector<std::string>(),  // hints
        LoginState::kSignIn          // login_state
    },
    {
        kAccountIdZach,              // id
        "zach@email.com",            // email
        "Zachary T",                 // name
        "Zach",                      // given_name
        GURL(),                      // picture
        std::vector<std::string>(),  // hints
        LoginState::kSignUp          // login_state
    }};

static const std::initializer_list<IdentityRequestAccount>
    kMultipleAccountsWithHints{
        {
            kAccountIdNicolas,                          // id
            kAccountEmailNicolas,                       // email
            "Nicolas P",                                // name
            "Nicolas",                                  // given_name
            GURL(),                                     // picture
            {kAccountIdNicolas, kAccountEmailNicolas},  // hints
            LoginState::kSignUp                         // login_state
        },
        {
            kAccountIdPeter,                        // id
            kAccountEmailPeter,                     // email
            "Peter K",                              // name
            "Peter",                                // given_name
            GURL(),                                 // picture
            {kAccountIdPeter, kAccountEmailPeter},  // hints
            LoginState::kSignIn                     // login_state
        },
        {
            kAccountIdZach,                       // id
            kAccountEmailZach,                    // email
            "Zachary T",                          // name
            "Zach",                               // given_name
            GURL(),                               // picture
            {kAccountIdZach, kAccountEmailZach},  // hints
            LoginState::kSignUp                   // login_state
        }};

static const std::set<std::string> kWellKnown{kProviderUrlFull};

struct IdentityProviderParameters {
  const char* provider;
  const char* client_id;
  const char* nonce;
  const char* login_hint;
};

// Parameters for a call to RequestToken.
struct RequestParameters {
  std::vector<IdentityProviderParameters> identity_providers;
  bool auto_reauthn;
  blink::mojom::RpContext rp_context;
};

// Expected return values from a call to RequestToken.
//
// DO NOT ADD NEW MEMBERS.
// Having a lot of members in RequestExpectations encourages bad test design.
// Specifically:
// - It encourages making the test harness more magic
// - It makes each test "test everything", making it really hard to determine
//   at a later date what the test was actually testing.

struct RequestExpectations {
  absl::optional<RequestTokenStatus> return_status;
  std::vector<FederatedAuthRequestResult> devtools_issue_statuses;
  absl::optional<std::string> selected_idp_config_url;
};

// Mock configuration values for test.
struct MockClientIdConfiguration {
  FetchStatus fetch_status;
  std::string privacy_policy_url;
  std::string terms_of_service_url;
};

struct MockWellKnown {
  std::set<std::string> provider_urls;
  FetchStatus fetch_status;
};

// Mock information returned from IdpNetworkRequestManager::FetchConfig().
struct MockConfig {
  FetchStatus fetch_status;
  std::string accounts_endpoint;
  std::string token_endpoint;
  std::string client_metadata_endpoint;
  std::string metrics_endpoint;
};

struct MockIdpInfo {
  MockWellKnown well_known;
  MockConfig config;
  MockClientIdConfiguration client_metadata;
  FetchStatus accounts_response;
  AccountList accounts;
};

// Action on accounts dialog taken by TestDialogController. Does not indicate a
// test expectation.
enum class AccountsDialogAction {
  kNone,
  kClose,
  kSelectFirstAccount,
};

// Action on IdP-sign-in-status-mismatch dialog taken by TestDialogController.
// Does not indicate a test expectation.
enum class IdpSigninStatusMismatchDialogAction {
  kNone,
  kClose,
};

struct MockConfiguration {
  const char* token;
  base::flat_map<std::string, MockIdpInfo> idp_info;
  FetchStatus token_response;
  bool delay_token_response;
  AccountsDialogAction accounts_dialog_action;
  IdpSigninStatusMismatchDialogAction idp_signin_status_mismatch_dialog_action;
  bool succeed_with_console_message = false;
};

static const MockClientIdConfiguration kDefaultClientMetadata{
    {ParseStatus::kSuccess, net::HTTP_OK},
    kPrivacyPolicyUrl,
    kTermsOfServiceUrl};

static const IdentityProviderParameters kDefaultIdentityProviderConfig{
    kProviderUrlFull, kClientId, kNonce, /*login_hint=*/""};

static const RequestParameters kDefaultRequestParameters{
    std::vector<IdentityProviderParameters>{kDefaultIdentityProviderConfig},
    /*auto_reauthn=*/false, blink::mojom::RpContext::kSignIn};

static const MockIdpInfo kDefaultIdentityProviderInfo{
    {kWellKnown, {ParseStatus::kSuccess, net::HTTP_OK}},
    {
        {ParseStatus::kSuccess, net::HTTP_OK},
        kAccountsEndpoint,
        kTokenEndpoint,
        kClientMetadataEndpoint,
        kMetricsEndpoint,
    },
    kDefaultClientMetadata,
    {ParseStatus::kSuccess, net::HTTP_OK},
    kSingleAccount,
};

static const base::flat_map<std::string, MockIdpInfo> kSingleProviderInfo{
    {kProviderUrlFull, kDefaultIdentityProviderInfo}};

constexpr char kProviderTwoUrlFull[] = "https://idp2.example/fedcm.json";
static const MockIdpInfo kProviderTwoInfo{
    {{kProviderTwoUrlFull}},
    {
        {ParseStatus::kSuccess, net::HTTP_OK},
        "https://idp2.example/accounts",
        "https://idp2.example/token",
        "https://idp2.example/client_metadata",
        "https://idp2.example/metrics",
    },
    kDefaultClientMetadata,
    {ParseStatus::kSuccess, net::HTTP_OK},
    kMultipleAccounts};

static const MockConfiguration kConfigurationValid{
    kToken,
    kSingleProviderInfo,
    {ParseStatus::kSuccess, net::HTTP_OK},
    /*delay_token_response=*/false,
    AccountsDialogAction::kSelectFirstAccount,
    IdpSigninStatusMismatchDialogAction::kNone};

static const RequestExpectations kExpectationSuccess{
    RequestTokenStatus::kSuccess,
    {FederatedAuthRequestResult::kSuccess},
    kProviderUrlFull};

static const RequestParameters kDefaultMultiIdpRequestParameters{
    std::vector<IdentityProviderParameters>{
        {kProviderUrlFull, kClientId, kNonce, /*login_hint=*/""},
        {kProviderTwoUrlFull, kClientId, kNonce, /*login_hint=*/""}},
    /*auto_reauthn=*/false,
    /*rp_context=*/blink::mojom::RpContext::kSignIn};

MockConfiguration kConfigurationMultiIdpValid{
    kToken,
    {{kProviderUrlFull, kDefaultIdentityProviderInfo},
     {kProviderTwoUrlFull, kProviderTwoInfo}},
    {ParseStatus::kSuccess, net::HTTP_OK},
    false /* delay_token_response */,
    AccountsDialogAction::kSelectFirstAccount,
    IdpSigninStatusMismatchDialogAction::kNone};

url::Origin OriginFromString(const std::string& url_string) {
  return url::Origin::Create(GURL(url_string));
}

enum class FetchedEndpoint {
  CONFIG,
  CLIENT_METADATA,
  ACCOUNTS,
  TOKEN,
  WELL_KNOWN,
};

class TestIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  void SetTestConfig(const MockConfiguration& configuration) {
    config_ = configuration;
  }

  void RunDelayedCallbacks() {
    for (base::OnceClosure& delayed_callback : delayed_callbacks_) {
      std::move(delayed_callback).Run();
    }
    delayed_callbacks_.clear();
  }

  void FetchWellKnown(const GURL& provider,
                      FetchWellKnownCallback callback) override {
    ++num_fetched_[FetchedEndpoint::WELL_KNOWN];

    std::string provider_key = provider.spec();
    std::set<GURL> url_set(
        config_.idp_info[provider_key].well_known.provider_urls.begin(),
        config_.idp_info[provider_key].well_known.provider_urls.end());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       config_.idp_info[provider_key].well_known.fetch_status,
                       url_set));
  }

  void FetchConfig(const GURL& provider,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    ++num_fetched_[FetchedEndpoint::CONFIG];

    std::string provider_key = provider.spec();
    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.token =
        GURL(config_.idp_info[provider_key].config.token_endpoint);
    endpoints.accounts =
        GURL(config_.idp_info[provider_key].config.accounts_endpoint);
    endpoints.client_metadata =
        GURL(config_.idp_info[provider_key].config.client_metadata_endpoint);
    endpoints.metrics =
        GURL(config_.idp_info[provider_key].config.metrics_endpoint);

    IdentityProviderMetadata idp_metadata;
    idp_metadata.config_url = provider;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       config_.idp_info[provider_key].config.fetch_status,
                       endpoints, idp_metadata));
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    ++num_fetched_[FetchedEndpoint::CLIENT_METADATA];

    // Find the info of the provider with the same client metadata endpoint.
    MockIdpInfo info;
    for (const auto& idp_info : config_.idp_info) {
      info = idp_info.second;
      if (GURL(info.config.client_metadata_endpoint) == endpoint)
        break;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), info.client_metadata.fetch_status,
                       IdpNetworkRequestManager::ClientMetadata{
                           GURL(info.client_metadata.privacy_policy_url),
                           GURL(info.client_metadata.terms_of_service_url)}));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    ++num_fetched_[FetchedEndpoint::ACCOUNTS];

    // Find the info of the provider with the same accounts endpoint.
    MockIdpInfo info;
    for (const auto& idp_info : config_.idp_info) {
      info = idp_info.second;
      if (GURL(info.config.accounts_endpoint) == accounts_url)
        break;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), info.accounts_response,
                                  info.accounts));
  }

  void SendTokenRequest(const GURL& token_url,
                        const std::string& account,
                        const std::string& url_encoded_post_data,
                        TokenRequestCallback callback,
                        ContinueOnCallback on_continue) override {
    ++num_fetched_[FetchedEndpoint::TOKEN];

    std::string delivered_token =
        config_.token_response.parse_status == ParseStatus::kSuccess
            ? config_.token
            : std::string();
    base::OnceCallback bound_callback = base::BindOnce(
        std::move(callback), config_.token_response, delivered_token);
    if (config_.delay_token_response) {
      delayed_callbacks_.push_back(std::move(bound_callback));
    } else {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(bound_callback));
    }
  }

  std::map<FetchedEndpoint, size_t> num_fetched_;

 protected:
  MockConfiguration config_{kConfigurationValid};
  std::vector<base::OnceClosure> delayed_callbacks_;
};

// TestIdpNetworkRequestManager subclass which checks the values of the method
// params when executing an endpoint request.
class IdpNetworkRequestManagerParamChecker
    : public TestIdpNetworkRequestManager {
 public:
  void SetExpectations(const std::string& expected_client_id,
                       const std::string& expected_selected_account_id) {
    expected_client_id_ = expected_client_id;
    expected_selected_account_id_ = expected_selected_account_id;
  }

  void SetExpectedTokenPostData(
      const std::string& expected_url_encoded_post_data) {
    expected_url_encoded_post_data_ = expected_url_encoded_post_data;
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    if (expected_client_id_)
      EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::FetchClientMetadata(endpoint, client_id,
                                                      std::move(callback));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    if (expected_client_id_)
      EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::SendAccountsRequest(accounts_url, client_id,
                                                      std::move(callback));
  }

  void SendTokenRequest(const GURL& token_url,
                        const std::string& account,
                        const std::string& url_encoded_post_data,
                        TokenRequestCallback callback,
                        ContinueOnCallback on_continue) override {
    if (expected_selected_account_id_)
      EXPECT_EQ(expected_selected_account_id_, account);
    if (expected_url_encoded_post_data_)
      EXPECT_EQ(expected_url_encoded_post_data_, url_encoded_post_data);
    TestIdpNetworkRequestManager::SendTokenRequest(
        token_url, account, url_encoded_post_data, std::move(callback),
        std::move(on_continue));
  }

 private:
  absl::optional<std::string> expected_client_id_;
  absl::optional<std::string> expected_selected_account_id_;
  absl::optional<std::string> expected_url_encoded_post_data_;
};

class TestDialogController
    : public NiceMock<MockIdentityRequestDialogController> {
 public:
  struct State {
    // State related to ShowAccountsDialog().
    AccountList displayed_accounts;
    absl::optional<IdentityRequestAccount::SignInMode> sign_in_mode;
    blink::mojom::RpContext rp_context;
    // State related to ShowFailureDialog().
    size_t num_show_idp_signin_status_mismatch_dialog_requests{0u};
    // State related to ShowIdpSigninFailureDialog().
    bool did_show_idp_signin_failure_dialog{false};
  };

  explicit TestDialogController(MockConfiguration config)
      : accounts_dialog_action_(config.accounts_dialog_action),
        idp_signin_status_mismatch_dialog_action_(
            config.idp_signin_status_mismatch_dialog_action) {}

  ~TestDialogController() override = default;
  TestDialogController(TestDialogController&) = delete;
  TestDialogController& operator=(TestDialogController&) = delete;

  void SetState(State* state) { state_ = state; }

  void ShowAccountsDialog(
      WebContents* rp_web_contents,
      const std::string& top_frame_for_display,
      const absl::optional<std::string>& iframe_for_display,
      const std::vector<IdentityProviderData>& identity_provider_data,
      IdentityRequestAccount::SignInMode sign_in_mode,
      bool show_auto_reauthn_checkbox,
      IdentityRequestDialogController::AccountSelectionCallback on_selected,
      IdentityRequestDialogController::DismissCallback dismiss_callback)
      override {
    if (!state_) {
      return;
    }

    state_->sign_in_mode = sign_in_mode;
    state_->rp_context = identity_provider_data[0].rp_context;

    base::span<const content::IdentityRequestAccount> accounts =
        identity_provider_data[0].accounts;
    state_->displayed_accounts = AccountList(accounts.begin(), accounts.end());

    switch (accounts_dialog_action_) {
      case AccountsDialogAction::kSelectFirstAccount: {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(on_selected),
                           identity_provider_data[0].idp_metadata.config_url,
                           accounts[0].id,
                           accounts[0].login_state == LoginState::kSignIn));
        break;
      }
      case AccountsDialogAction::kClose:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(dismiss_callback),
                                      DismissReason::kCloseButton));
        break;
      case AccountsDialogAction::kNone:
        break;
    }
  }

  void ShowFailureDialog(content::WebContents* rp_web_contents,
                         const std::string& top_frame_for_display,
                         const absl::optional<std::string>& iframe_for_display,
                         const std::string& idp_for_display,
                         const IdentityProviderMetadata& idp_metadata,
                         IdentityRequestDialogController::DismissCallback
                             dismiss_callback) override {
    if (!state_) {
      return;
    }

    ++state_->num_show_idp_signin_status_mismatch_dialog_requests;
    switch (idp_signin_status_mismatch_dialog_action_) {
      case IdpSigninStatusMismatchDialogAction::kClose:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(dismiss_callback),
                                      DismissReason::kCloseButton));
        break;
      case IdpSigninStatusMismatchDialogAction::kNone:
        break;
    }
  }

  void ShowIdpSigninFailureDialog(base::OnceClosure dismiss_callback) override {
    if (!state_) {
      return;
    }

    state_->did_show_idp_signin_failure_dialog = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(dismiss_callback));
  }

 private:
  AccountsDialogAction accounts_dialog_action_{AccountsDialogAction::kNone};
  IdpSigninStatusMismatchDialogAction idp_signin_status_mismatch_dialog_action_{
      IdpSigninStatusMismatchDialogAction::kNone};

  // Pointer so that the state can be queried after FederatedAuthRequestImpl
  // destroys TestDialogController.
  raw_ptr<State> state_;
};

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  using PermissionOverride = std::pair<url::Origin, ApiPermissionStatus>;
  PermissionOverride permission_override_ =
      std::make_pair(url::Origin(), ApiPermissionStatus::GRANTED);
  absl::optional<std::pair<size_t, PermissionOverride>>
      permission_override_for_nth_;
  std::set<url::Origin> embargoed_origins_;
  size_t api_invocation_counter{0};

  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    ++api_invocation_counter;

    if (embargoed_origins_.count(origin))
      return ApiPermissionStatus::BLOCKED_EMBARGO;

    if (permission_override_for_nth_ &&
        permission_override_for_nth_->first == api_invocation_counter) {
      return (origin == permission_override_for_nth_->second.first)
                 ? permission_override_for_nth_->second.second
                 : ApiPermissionStatus::GRANTED;
    }

    return (origin == permission_override_.first)
               ? permission_override_.second
               : ApiPermissionStatus::GRANTED;
  }

  void RecordDismissAndEmbargo(const url::Origin& origin) override {
    embargoed_origins_.insert(origin);
  }

  void RemoveEmbargoAndResetCounts(const url::Origin& origin) override {
    embargoed_origins_.erase(origin);
  }
};

class TestPermissionDelegate : public NiceMock<MockPermissionDelegate> {
 public:
  std::map<url::Origin, absl::optional<bool>> idp_signin_statuses_;

  TestPermissionDelegate() = default;
  ~TestPermissionDelegate() override = default;

  TestPermissionDelegate(TestPermissionDelegate&) = delete;
  TestPermissionDelegate& operator=(TestPermissionDelegate&) = delete;

  absl::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override {
    auto it = idp_signin_statuses_.find(idp_origin);
    return (it != idp_signin_statuses_.end()) ? it->second : absl::nullopt;
  }

  void SetIdpSigninStatus(const url::Origin& idp_origin,
                          bool idp_signin_status) override {
    idp_signin_statuses_[idp_origin] = idp_signin_status;
    // Call parent so that EXPECT_CALL() works.
    NiceMock<MockPermissionDelegate>::SetIdpSigninStatus(idp_origin,
                                                         idp_signin_status);
  }
};

class TestAutoReauthnPermissionDelegate
    : public MockAutoReauthnPermissionDelegate {
 public:
  std::set<url::Origin> embargoed_origins_;

  void RecordDisplayAndEmbargo(const url::Origin& origin) override {
    embargoed_origins_.insert(origin);
  }
};

}  // namespace

class FederatedAuthRequestImplTest : public RenderViewHostImplTestHarness {
 protected:
  FederatedAuthRequestImplTest() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  ~FederatedAuthRequestImplTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    test_api_permission_delegate_ =
        std::make_unique<TestApiPermissionDelegate>();
    test_permission_delegate_ = std::make_unique<TestPermissionDelegate>();
    test_auto_reauthn_permission_delegate_ =
        std::make_unique<TestAutoReauthnPermissionDelegate>();

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kRpUrl), ui::PAGE_TRANSITION_LINK);

    federated_auth_request_impl_ = &FederatedAuthRequestImpl::CreateForTesting(
        *main_test_rfh(), test_api_permission_delegate_.get(),
        test_auto_reauthn_permission_delegate_.get(),
        test_permission_delegate_.get(),
        request_remote_.BindNewPipeAndPassReceiver());

    std::unique_ptr<TestIdpNetworkRequestManager> network_request_manager =
        std::make_unique<TestIdpNetworkRequestManager>();
    SetNetworkRequestManager(std::move(network_request_manager));

    federated_auth_request_impl_->SetTokenRequestDelayForTests(
        base::TimeDelta());
  }

  void SetNetworkRequestManager(
      std::unique_ptr<TestIdpNetworkRequestManager> manager) {
    test_network_request_manager_ = std::move(manager);
    // DelegatedIdpNetworkRequestManager is owned by
    // |federated_auth_request_impl_|.
    federated_auth_request_impl_->SetNetworkManagerForTests(
        std::make_unique<DelegatedIdpNetworkRequestManager>(
            test_network_request_manager_.get()));
  }

  // Sets the TestDialogController to be used for the next call of
  // RunAuthTest().
  void SetDialogController(
      std::unique_ptr<TestDialogController> dialog_controller) {
    custom_dialog_controller_ = std::move(dialog_controller);
  }

  void RunAuthTest(const RequestParameters& request_parameters,
                   const RequestExpectations& expectations,
                   const MockConfiguration& configuration) {
    request_remote_.set_disconnect_handler(auth_helper_.quit_closure());

    RunAuthDontWaitForCallback(request_parameters, configuration);
    WaitForCurrentAuthRequest();
    CheckAuthExpectations(configuration, expectations);
  }

  void RunAuthDontWaitForCallback(const RequestParameters& request_parameters,
                                  const MockConfiguration& configuration) {
    if (!custom_dialog_controller_) {
      custom_dialog_controller_ =
          std::make_unique<TestDialogController>(configuration);
    }

    dialog_controller_state_ = TestDialogController::State();
    custom_dialog_controller_->SetState(&dialog_controller_state_);
    federated_auth_request_impl_->SetDialogControllerForTests(
        std::move(custom_dialog_controller_));

    test_network_request_manager_->SetTestConfig(configuration);

    std::vector<blink::mojom::IdentityProviderGetParametersPtr> idp_get_params;
    for (const auto& identity_provider :
         request_parameters.identity_providers) {
      std::vector<blink::mojom::IdentityProviderPtr> idp_ptrs;
      blink::mojom::IdentityProviderConfigPtr config =
          blink::mojom::IdentityProviderConfig::New();
      config->config_url = GURL(identity_provider.provider);
      config->client_id = identity_provider.client_id;
      config->nonce = identity_provider.nonce;
      config->login_hint = identity_provider.login_hint;
      blink::mojom::IdentityProviderPtr idp_ptr =
          blink::mojom::IdentityProvider::NewFederated(std::move(config));
      idp_ptrs.push_back(std::move(idp_ptr));
      blink::mojom::IdentityProviderGetParametersPtr get_params =
          blink::mojom::IdentityProviderGetParameters::New(
              std::move(idp_ptrs), request_parameters.auto_reauthn,
              request_parameters.rp_context);
      idp_get_params.push_back(std::move(get_params));
    }

    PerformAuthRequest(std::move(idp_get_params));
  }

  void CheckAuthExpectations(const MockConfiguration& configuration,
                             const RequestExpectations& expectation) {
    ASSERT_EQ(expectation.return_status, auth_helper_.status());
    if (expectation.return_status == RequestTokenStatus::kSuccess) {
      EXPECT_EQ(configuration.token, auth_helper_.token());
    } else {
      EXPECT_TRUE(auth_helper_.token() == absl::nullopt ||
                  auth_helper_.token() == kEmptyToken);
    }

    if (expectation.return_status == RequestTokenStatus::kSuccess) {
      EXPECT_TRUE(DidFetchWellKnownAndConfig());
      EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
      EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
      // FetchedEndpoint::CLIENT_METADATA is optional.

      EXPECT_TRUE(did_show_accounts_dialog());
    }

    EXPECT_EQ(expectation.selected_idp_config_url,
              auth_helper_.selected_idp_config_url());

    if (!expectation.devtools_issue_statuses.empty()) {
      std::map<FederatedAuthRequestResult, int> devtools_issue_counts;
      for (FederatedAuthRequestResult devtools_issue_status :
           expectation.devtools_issue_statuses) {
        if (devtools_issue_status == FederatedAuthRequestResult::kSuccess)
          continue;

        ++devtools_issue_counts[devtools_issue_status];
      }

      for (auto& [devtools_issue_status, expected_count] :
           devtools_issue_counts) {
        int issue_count = main_test_rfh()->GetFederatedAuthRequestIssueCount(
            devtools_issue_status);
        EXPECT_LE(expected_count, issue_count);
      }
      if (devtools_issue_counts.empty()) {
        int issue_count =
            main_test_rfh()->GetFederatedAuthRequestIssueCount(absl::nullopt);
        EXPECT_EQ(0, issue_count);
      }
      CheckConsoleMessages(expectation.devtools_issue_statuses,
                           configuration.succeed_with_console_message);
    }
  }

  void CheckConsoleMessages(
      const std::vector<FederatedAuthRequestResult>& devtools_issue_statuses,
      bool succeed_with_console_message) {
    std::vector<std::string> messages =
        RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();

    bool did_expect_any_messages = succeed_with_console_message;
    size_t expected_message_index = messages.size() - 1;
    for (const auto& expected_status :
         base::Reversed(devtools_issue_statuses)) {
      if (expected_status == FederatedAuthRequestResult::kSuccess) {
        continue;
      }

      std::string expected_message =
          webid::GetConsoleErrorMessageFromResult(expected_status);
      did_expect_any_messages = true;
      ASSERT_GE(expected_message_index, 0u);
      EXPECT_EQ(expected_message, messages[expected_message_index]);
      --expected_message_index;
    }

    if (!did_expect_any_messages)
      EXPECT_EQ(0u, messages.size());
  }

  void PerformAuthRequest(
      std::vector<blink::mojom::IdentityProviderGetParametersPtr>
          idp_get_params) {
    request_remote_->RequestToken(std::move(idp_get_params),
                                  auth_helper_.callback());

    // Ensure that the request makes its way to FederatedAuthRequestImpl.
    request_remote_.FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void WaitForCurrentAuthRequest() {
    request_remote_.set_disconnect_handler(auth_helper_.quit_closure());

    // Fast forward clock so that the pending
    // FederatedAuthRequestImpl::OnRejectRequest() task, if any, gets a
    // chance to run.
    task_environment()->FastForwardBy(base::Minutes(10));
    auth_helper_.WaitForCallback();

    request_remote_.set_disconnect_handler(base::OnceClosure());
  }

  base::span<const content::IdentityRequestAccount> displayed_accounts() const {
    return dialog_controller_state_.displayed_accounts;
  }

  bool did_show_accounts_dialog() const {
    return !displayed_accounts().empty();
  }
  bool did_show_idp_signin_status_mismatch_dialog() const {
    return dialog_controller_state_
        .num_show_idp_signin_status_mismatch_dialog_requests;
  }

  int CountNumLoginStateIsSignin() {
    int num_sign_in_login_state = 0;
    for (const auto& account : displayed_accounts()) {
      if (account.login_state == LoginState::kSignIn) {
        ++num_sign_in_login_state;
      }
    }
    return num_sign_in_login_state;
  }

  bool DidFetchAnyEndpoint() {
    for (auto& [endpoint, num] : test_network_request_manager_->num_fetched_) {
      if (num > 0) {
        return true;
      }
    }
    return false;
  }

  // Convenience method as WELL_KNOWN and CONFIG endpoints are fetched in
  // parallel.
  bool DidFetchWellKnownAndConfig() {
    return DidFetch(FetchedEndpoint::WELL_KNOWN) &&
           DidFetch(FetchedEndpoint::CONFIG);
  }

  bool DidFetch(FetchedEndpoint endpoint) { return NumFetched(endpoint) > 0u; }

  size_t NumFetched(FetchedEndpoint endpoint) {
    return test_network_request_manager_->num_fetched_[endpoint];
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  void ExpectRequestTokenStatusUKM(TokenStatus status) {
    ExpectRequestTokenStatusUKMInternal(status, FedCmEntry::kEntryName);
    ExpectRequestTokenStatusUKMInternal(status, FedCmIdpEntry::kEntryName);
  }

  void ExpectRequestTokenStatusUKMInternal(TokenStatus status,
                                           const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty())
        << "No " << entry_name << " entry was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    bool metric_found = false;
    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status.RequestIdToken");
      if (!metric) {
        continue;
      }
      EXPECT_FALSE(metric_found)
          << "Found more than one entry with Status.RequestIdToken in "
          << entry_name;
      metric_found = true;
      EXPECT_EQ(static_cast<int>(status), *metric)
          << "Unexpected status recorded in " << entry_name;
    }
    EXPECT_TRUE(metric_found)
        << "No Status.RequestIdToken entry was found in " << entry_name;
  }

  void ExpectTimingUKM(const std::string& metric_name) {
    ExpectTimingUKMInternal(metric_name, FedCmEntry::kEntryName);
    ExpectTimingUKMInternal(metric_name, FedCmIdpEntry::kEntryName);
  }

  void ExpectTimingUKMInternal(const std::string& metric_name,
                               const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty())
        << "No " << entry_name << " entry was recorded";

    for (const auto* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected UKM was not recorded in " << entry_name;
  }

  void ExpectNoTimingUKM(const std::string& metric_name) {
    ExpectNoTimingUKMInternal(metric_name, FedCmEntry::kEntryName);
    ExpectNoTimingUKMInternal(metric_name, FedCmIdpEntry::kEntryName);
  }

  void ExpectNoTimingUKMInternal(const std::string& metric_name,
                                 const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty())
        << "No " << entry_name << " entry was recorded";

    for (const auto* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name))
        FAIL() << "Unexpected UKM was recorded in " << entry_name;
    }
    SUCCEED();
  }

  void ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus status) {
    auto entries = ukm_recorder()->GetEntriesByName(FedCmIdpEntry::kEntryName);

    ASSERT_FALSE(entries.empty()) << "No FedCm entry was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    bool metric_found = false;
    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status.SignInStateMatch");
      if (!metric) {
        continue;
      }
      EXPECT_FALSE(metric_found)
          << "Found more than one Status.SignInStateMatch";
      metric_found = true;
      EXPECT_EQ(static_cast<int>(status), *metric);
    }
    EXPECT_TRUE(metric_found) << "No Status.SignInStateMatch was found";
  }

  void ExpectAutoReauthnMetrics(
      FedCmMetrics::NumReturningAccounts expected_returning_accounts,
      bool expected_succeeded,
      bool expected_auto_reauthn_setting_blocked,
      bool expected_auto_reauthn_embargoed) {
    // UMA checks
    histogram_tester_.ExpectUniqueSample("Blink.FedCm.AutoReauthn.Succeeded",
                                         expected_succeeded, 1);
    histogram_tester_.ExpectUniqueSample(
        "Blink.FedCm.AutoReauthn.ReturningAccounts",
        static_cast<int>(expected_returning_accounts), 1);
    histogram_tester_.ExpectUniqueSample(
        "Blink.FedCm.AutoReauthn.BlockedByContentSettings",
        expected_auto_reauthn_setting_blocked, 1);
    histogram_tester_.ExpectUniqueSample(
        "Blink.FedCm.AutoReauthn.BlockedByEmbargo",
        expected_auto_reauthn_embargoed, 1);
    histogram_tester_.ExpectTotalCount(
        "Blink.FedCm.AutoReauthn.TimeFromEmbargoWhenBlocked",
        expected_auto_reauthn_embargoed ? 1 : 0);

    // UKM checks
    auto entries = ukm_recorder()->GetEntriesByName(FedCmEntry::kEntryName);
    ASSERT_FALSE(entries.empty()) << "No FedCM UKM entry was found!";

    bool metric_found = false;
    for (const auto* entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "AutoReauthn.Succeeded");
      if (!metric) {
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(
            entry, "AutoReauthn.ReturningAccounts"));
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(
            entry, "AutoReauthn.BlockedByContentSettings"));
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(
            entry, "AutoReauthn.BlockedByEmbargo"));
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(
            entry, "AutoReauthn.TimeFromEmbargoWhenBlocked"));
        continue;
      }
      EXPECT_FALSE(metric_found) << "Found more than one AutoReauthn entry";
      metric_found = true;
      EXPECT_EQ(expected_succeeded, *metric);

      metric = ukm_recorder()->GetEntryMetric(entry,
                                              "AutoReauthn.ReturningAccounts");
      ASSERT_TRUE(metric) << "AutoReauthn.ReturningAccounts was not found";
      EXPECT_EQ(static_cast<int>(expected_returning_accounts), *metric);

      metric = ukm_recorder()->GetEntryMetric(
          entry, "AutoReauthn.BlockedByContentSettings");
      ASSERT_TRUE(metric)
          << "AutoReauthn.BlockedByContentSettings was not found";
      EXPECT_EQ(expected_auto_reauthn_setting_blocked, *metric);

      metric =
          ukm_recorder()->GetEntryMetric(entry, "AutoReauthn.BlockedByEmbargo");
      ASSERT_TRUE(metric) << "AutoReauthn.BlockedByEmbargo was not found";
      EXPECT_EQ(expected_auto_reauthn_embargoed, *metric);

      metric = ukm_recorder()->GetEntryMetric(
          entry, "AutoReauthn.TimeFromEmbargoWhenBlocked");
      EXPECT_EQ(expected_auto_reauthn_embargoed, !!metric);
    }
    EXPECT_TRUE(metric_found) << "Did not find AutoReauthn metrics";
    CheckAllFedCmSessionIDs();
  }

  void CheckAllFedCmSessionIDs() {
    absl::optional<int> session_id;
    auto CheckUKMSessionID = [&](const auto& ukm_entries) {
      ASSERT_FALSE(ukm_entries.empty());
      for (const auto* const entry : ukm_entries) {
        const auto* const metric =
            ukm_recorder()->GetEntryMetric(entry, "FedCmSessionID");
        ASSERT_TRUE(metric)
            << "All UKM events should have the SessionID metric";
        if (!session_id.has_value()) {
          session_id = *metric;
        } else {
          EXPECT_EQ(*metric, *session_id)
              << "All UKM events should have the same SessionID";
        }
      }
    };
    CheckUKMSessionID(ukm_recorder()->GetEntriesByName(FedCmEntry::kEntryName));
    CheckUKMSessionID(
        ukm_recorder()->GetEntriesByName(FedCmIdpEntry::kEntryName));
  }

  void ComputeLoginStateAndReorderAccounts(
      const blink::mojom::IdentityProviderConfigPtr& identity_provider,
      AccountList& accounts) {
    federated_auth_request_impl_->ComputeLoginStateAndReorderAccounts(
        identity_provider, accounts);
  }

 protected:
  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  raw_ptr<FederatedAuthRequestImpl> federated_auth_request_impl_;

  std::unique_ptr<TestIdpNetworkRequestManager> test_network_request_manager_;

  std::unique_ptr<TestApiPermissionDelegate> test_api_permission_delegate_;
  std::unique_ptr<TestPermissionDelegate> test_permission_delegate_;
  std::unique_ptr<TestAutoReauthnPermissionDelegate>
      test_auto_reauthn_permission_delegate_;

  AuthRequestCallbackHelper auth_helper_;

  // Enables test to inspect TestDialogController state after
  // FederatedAuthRequestImpl destroys TestDialogController. Recreated during
  // each run of RunAuthTest().
  TestDialogController::State dialog_controller_state_;

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<TestDialogController> custom_dialog_controller_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Test successful FedCM request.
TEST_F(FederatedAuthRequestImplTest, SuccessfulRequest) {
  // Use IdpNetworkRequestManagerParamChecker to validate passed-in parameters
  // to IdpNetworkRequestManager methods.
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kClientId, kAccountId);
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  // Check that client metadata is fetched. Using `kExpectationSuccess`
  // expectation does not check that the client metadata was fetched because
  // client metadata is optional.
  EXPECT_TRUE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
}

// Test successful well-known fetching.
TEST_F(FederatedAuthRequestImplTest, WellKnownSuccess) {
  // Use IdpNetworkRequestManagerParamChecker to validate passed-in parameters
  // to IdpNetworkRequestManager methods.
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kClientId, kAccountId);
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test the provider url is not in the well-known.
TEST_F(FederatedAuthRequestImplTest, WellKnownNotInList) {
  RequestExpectations request_not_in_list = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/absl::nullopt};

  const char* idp_config_url =
      kDefaultRequestParameters.identity_providers[0].provider;
  const char* kWellKnownMismatchConfigUrl = "https://mismatch.example";
  EXPECT_NE(std::string(idp_config_url), kWellKnownMismatchConfigUrl);

  MockConfiguration config = kConfigurationValid;
  config.idp_info[idp_config_url].well_known = {
      {kWellKnownMismatchConfigUrl}, {ParseStatus::kSuccess, net::HTTP_OK}};
  RunAuthTest(kDefaultRequestParameters, request_not_in_list, config);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that not having the filename in the well-known fails.
TEST_F(FederatedAuthRequestImplTest, WellKnownHasNoFilename) {
  MockConfiguration config{kConfigurationValid};
  config.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{GURL(kProviderUrlFull).GetWithoutFilename().spec()};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, config);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that request fails if config is missing token endpoint.
TEST_F(FederatedAuthRequestImplTest, MissingTokenEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.token_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"id_assertion_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Test that request fails if config is missing accounts endpoint.
TEST_F(FederatedAuthRequestImplTest, MissingAccountsEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Test that client metadata endpoint is not required in config.
TEST_F(FederatedAuthRequestImplTest, MissingClientMetadataEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.client_metadata_endpoint = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
}

// Test that request fails if the accounts endpoint is in a different origin
// than identity provider.
TEST_F(FederatedAuthRequestImplTest, AccountEndpointDifferentOriginIdp) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint =
      kCrossOriginAccountsEndpoint;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that request fails if the idp is not https.
TEST_F(FederatedAuthRequestImplTest, ProviderNotTrustworthy) {
  IdentityProviderParameters identity_provider{
      "http://idp.example/fedcm.json", kClientId, kNonce, /*login_hint=*/""};
  RequestParameters request{
      std::vector<IdentityProviderParameters>{identity_provider},
      /*auto_reauthn=*/false,
      /*rp_context=*/blink::mojom::RpContext::kSignIn};
  MockConfiguration configuration = kConfigurationValid;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(request, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.RequestIdToken",
      TokenStatus::kIdpNotPotentiallyTrustworthy, 1);
}

// Test that request fails if accounts endpoint cannot be reached.
TEST_F(FederatedAuthRequestImplTest, AccountEndpointCannotBeReached) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kNoResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that request fails if account endpoint response cannot be parsed.
TEST_F(FederatedAuthRequestImplTest, AccountsCannotBeParsed) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that privacy policy URL or terms of service is not required in client
// metadata.
TEST_F(FederatedAuthRequestImplTest,
       ClientMetadataNoPrivacyPolicyOrTermsOfServiceUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull].client_metadata.privacy_policy_url =
      "";
  configuration.idp_info[kProviderUrlFull]
      .client_metadata.terms_of_service_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that privacy policy URL is not required in client metadata.
TEST_F(FederatedAuthRequestImplTest, ClientMetadataNoPrivacyPolicyUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull].client_metadata.privacy_policy_url =
      "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that terms of service URL is not required in client metadata.
TEST_F(FederatedAuthRequestImplTest, ClientMetadataNoTermsOfServiceUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull]
      .client_metadata.terms_of_service_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that request fails if all of the endpoints in the config are invalid.
TEST_F(FederatedAuthRequestImplTest, AllInvalidEndpoints) {
  // Both an empty url and cross origin urls are invalid endpoints.
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint =
      "https://cross-origin-1.com";
  configuration.idp_info[kProviderUrlFull].config.token_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"id_assertion_endpoint\"\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Tests for Login State
TEST_F(FederatedAuthRequestImplTest, LoginStateShouldBeSignUpForFirstTimeUser) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignUp, displayed_accounts()[0].login_state);
}

TEST_F(FederatedAuthRequestImplTest, LoginStateShouldBeSignInForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  // CLIENT_METADATA only needs to be fetched for obtaining links to display in
  // the disclosure text. The disclosure text is not displayed for returning
  // users, thus fetching the client metadata endpoint should be skipped.
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
}

TEST_F(FederatedAuthRequestImplTest,
       LoginStateSuccessfulSignUpGrantsSharingPermission) {
  EXPECT_CALL(*test_permission_delegate_, HasSharingPermission(_, _, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(
      *test_permission_delegate_,
      GrantSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                             OriginFromString(kProviderUrlFull), kAccountId))
      .Times(1);
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

TEST_F(FederatedAuthRequestImplTest,
       LoginStateFailedSignUpNotGrantSharingPermission) {
  EXPECT_CALL(*test_permission_delegate_, HasSharingPermission(_, _, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*test_permission_delegate_, GrantSharingPermission(_, _, _, _))
      .Times(0);

  MockConfiguration configuration = kConfigurationValid;
  configuration.token_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
}

// Test that auto re-authn permission is not embargoed upon explicit sign-in.
TEST_F(FederatedAuthRequestImplTest, ExplicitSigninEmbargo) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
  EXPECT_TRUE(
      test_auto_reauthn_permission_delegate_->embargoed_origins_.empty());
}

// Test that auto re-authn permission is embargoed upon successful auto
// re-authn.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnEmbargo) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .Times(2)
      .WillRepeatedly(Return(true));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              HasAutoReauthnContentSetting())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.auto_reauthn = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);
  EXPECT_TRUE(test_auto_reauthn_permission_delegate_->embargoed_origins_.count(
      OriginFromString(kRpUrl)));

  ExpectAutoReauthnMetrics(FedCmMetrics::NumReturningAccounts::kOne,
                           /*expected_succeeded=*/true,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false);
}

// Test that auto re-authn with a single account where the account is a
// returning user sets the sign-in mode to auto.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForSingleReturningUserSingleAccount) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .Times(2)
      .WillRepeatedly(Return(true));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              HasAutoReauthnContentSetting())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.auto_reauthn = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumReturningAccounts::kOne,
                           /*expected_succeeded=*/true,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false);
}

// Test that auto re-authn with multiple accounts and a single returning user
// sets the sign-in mode to auto.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForSingleReturningUserMultipleAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              HasSharingPermission(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderUrlFull), kAccountIdNicolas))
      .WillOnce(Return(false));

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdPeter))
      .Times(2)
      .WillRepeatedly(Return(true));

  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdZach))
      .WillOnce(Return(false));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              HasAutoReauthnContentSetting())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.auto_reauthn = true;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].id, kAccountIdPeter);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumReturningAccounts::kOne,
                           /*expected_succeeded=*/true,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false);
}

// Test that auto re-authn with multiple accounts and multiple returning users
// sets the sign-in mode to explicit.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForMultipleReturningUsersMultipleAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              HasSharingPermission(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderUrlFull), kAccountIdNicolas))
      .Times(2)
      .WillRepeatedly(Return(true));

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdPeter))
      .Times(2)
      .WillRepeatedly(Return(true));

  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdZach))
      .WillOnce(Return(false));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              HasAutoReauthnContentSetting())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.auto_reauthn = true;

  AccountList multiple_accounts = kMultipleAccounts;
  multiple_accounts[0].login_state = LoginState::kSignIn;
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = multiple_accounts;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  ASSERT_EQ(displayed_accounts().size(), 3u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 2);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumReturningAccounts::kMultiple,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false);
}

// Test that auto re-authn with single non-returning account sets the sign-in
// mode to explicit.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnForZeroReturningUsers) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(false));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              HasAutoReauthnContentSetting())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.auto_reauthn = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignUp);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumReturningAccounts::kZero,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false);
}

// Test that auto re-authn with multiple accounts and a single returning user
// sets the sign-in mode to kExplicit if `autoReauthn` is not specified.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForSingleReturningUserWithoutSettingAutoReauthn) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  RequestParameters request_parameters = kDefaultRequestParameters;
  // request_parameters.auto_reauthn is default to false;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
}

// Test that if browser has not observed sign-in in the past, the sign-in mode
// is set to explicit regardless the account's login state.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnBrowserNotObservedSigninBefore) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .Times(2)
      .WillRepeatedly(Return(false));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              HasAutoReauthnContentSetting())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.auto_reauthn = true;

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts[0].login_state =
      LoginState::kSignIn;

  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
}

// Test that auto re-authn for a first time user sets the sign-in mode to
// explicit.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnForFirstTimeUser) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              HasAutoReauthnContentSetting())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.auto_reauthn = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignUp);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
}

// Test that auto re-authn where the auto re-authn permission is blocked sets
// the sign-in mode to explicit.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnWithBlockedAutoReauthnPermissions) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(true));

  // Pretend the auto re-authn permission has been blocked for this account.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              HasAutoReauthnContentSetting())
      .WillOnce(Return(false));

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.auto_reauthn = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumReturningAccounts::kOne,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/true,
                           /*expected_auto_reauthn_embargoed=*/false);
}

// Test that auto re-authn where the auto re-authn cooldown is on sets
// the sign-in mode to explicit.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnWithCooldown) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoReauthn);

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(true));

  // Pretend the auto re-authn permission has been granted for this account.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              HasAutoReauthnContentSetting())
      .WillOnce(Return(true));
  // Pretend that auto re-authn is embargoed.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(true));

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.auto_reauthn = true;
  MockConfiguration configuration = kConfigurationValid;
  configuration.succeed_with_console_message = true;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumReturningAccounts::kOne,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/true);

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(1U, messages.size());
  EXPECT_EQ(
      "Auto re-authn was previously triggered less than 10 minutes ago. Only "
      "one auto re-authn request can be made every 10 minutes.",
      messages[0]);
}

TEST_F(FederatedAuthRequestImplTest, MetricsForSuccessfulSignInCase) {
  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              HasSharingPermission(_, _, OriginFromString(kProviderUrlFull),
                                   kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kSuccess, 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.IsSignInUser", 1, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.ContinueOnDialog");
  ExpectTimingUKM("Timing.IdTokenResponse");
  ExpectTimingUKM("Timing.TurnaroundTime");
  ExpectNoTimingUKM("Timing.CancelOnDialog");

  ExpectRequestTokenStatusUKM(TokenStatus::kSuccess);
  CheckAllFedCmSessionIDs();
}

// Test that request fails if account picker is explicitly dismissed.
TEST_F(FederatedAuthRequestImplTest, MetricsForUIExplicitlyDismissed) {
  base::HistogramTester histogram_tester_;
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kShouldEmbargo},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));

  ukm_loop.Run();

  ASSERT_TRUE(did_show_accounts_dialog());
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignUp);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kShouldEmbargo, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.CancelOnDialog");
  ExpectNoTimingUKM("Timing.ContinueOnDialog");
  ExpectNoTimingUKM("Timing.IdTokenResponse");
  ExpectNoTimingUKM("Timing.TurnaroundTime");

  ExpectRequestTokenStatusUKM(TokenStatus::kShouldEmbargo);
  CheckAllFedCmSessionIDs();
}

namespace {

// TestDialogController subclass which supports WeakPtr.
class WeakTestDialogController
    : public TestDialogController,
      public base::SupportsWeakPtr<WeakTestDialogController> {
 public:
  explicit WeakTestDialogController(MockConfiguration configuration)
      : TestDialogController(configuration) {}
  ~WeakTestDialogController() override = default;
  WeakTestDialogController(WeakTestDialogController&) = delete;
  WeakTestDialogController& operator=(WeakTestDialogController&) = delete;
};

}  // namespace

// Test that request is not completed if user ignores the UI.
TEST_F(FederatedAuthRequestImplTest, UIIsIgnored) {
  base::HistogramTester histogram_tester_;

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;

  auto dialog_controller =
      std::make_unique<WeakTestDialogController>(configuration);
  base::WeakPtr<WeakTestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  task_environment()->FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(auth_helper_.was_callback_called());

  // The dialog should have been shown. The dialog controller should not be
  // destroyed.
  ASSERT_TRUE(did_show_accounts_dialog());
  EXPECT_TRUE(weak_dialog_controller);

  // Only the time to show the account dialog gets recorded.
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Status.RequestIdToken", 0);
}

TEST_F(FederatedAuthRequestImplTest, MetricsForWebContentsVisible) {
  base::HistogramTester histogram_tester;
  // Sets RenderFrameHost to visible
  test_rvh()->SimulateWasShown();
  ASSERT_EQ(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              HasSharingPermission(_, _, OriginFromString(kProviderUrlFull),
                                   kAccountId))
      .WillOnce(Return(true));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 1, 1);
}

// Test that request fails if the web contents are hidden.
TEST_F(FederatedAuthRequestImplTest, MetricsForWebContentsInvisible) {
  base::HistogramTester histogram_tester;
  test_rvh()->SimulateWasShown();
  ASSERT_EQ(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  // Sets the RenderFrameHost to invisible
  test_rvh()->SimulateWasHidden();
  ASSERT_NE(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorRpPageNotVisible},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 0, 1);
}

TEST_F(FederatedAuthRequestImplTest, DisabledWhenThirdPartyCookiesBlocked) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED);

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kThirdPartyCookiesBlocked,
                                       1);
  ExpectRequestTokenStatusUKM(TokenStatus::kThirdPartyCookiesBlocked);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, MetricsForFeatureIsDisabled) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithUnhandledRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  // Delete the request before DelayTimer kicks in.
  federated_auth_request_impl_->ResetAndDeleteThis();

  // If double counted, these samples would not be unique so the following
  // checks will fail.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithAbortedRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  // Abort the request before DelayTimer kicks in.
  federated_auth_request_impl_->CancelTokenRequest();

  // If double counted, these samples would not be unique so the following
  // checks will fail.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states match if IDP claims that user is signed in and
// browser also observes that user is signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForSignedInOnBothIdpAndBrowser) {
  // Set browser observes user is signed in.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  AccountList displayed_accounts =
      AccountList(kSingleAccount.begin(), kSingleAccount.end());
  displayed_accounts[0].login_state = LoginState::kSignIn;
  configuration.idp_info[kProviderUrlFull].accounts = displayed_accounts;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.SignInStateMatch",
                                       SignInStateMatchStatus::kMatch, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kMatch);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states match if IDP claims that user is not signed in and
// browser also observes that user is not signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForNotSignedInOnBothIdpAndBrowser) {
  // Set browser observes user is not signed in.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(false));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // By default, IDP claims user is not signed in.
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.SignInStateMatch",
                                       SignInStateMatchStatus::kMatch, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kMatch);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states mismatch if IDP claims that user is signed in but
// browser observes that user is not signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForOnlyIdpClaimedSignIn) {
  // Set browser observes user is not signed in.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(false));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  AccountList displayed_accounts =
      AccountList(kSingleAccount.begin(), kSingleAccount.end());
  displayed_accounts[0].login_state = LoginState::kSignIn;
  configuration.idp_info[kProviderUrlFull].accounts = displayed_accounts;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.SignInStateMatch",
      SignInStateMatchStatus::kIdpClaimedSignIn, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kIdpClaimedSignIn);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states mismatch if IDP claims that user is not signed in
// but browser observes that user is signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForOnlyBrowserObservedSignIn) {
  // Set browser observes user is signed in.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.SignInStateMatch",
      SignInStateMatchStatus::kBrowserObservedSignIn, 1);
  ExpectSignInStateMatchStatusUKM(
      SignInStateMatchStatus::kBrowserObservedSignIn);
  CheckAllFedCmSessionIDs();
}

// Test that embargo is requested if the
// IdentityRequestDialogController::ShowAccountsDialog() callback requests it.
TEST_F(FederatedAuthRequestImplTest, RequestEmbargo) {
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kShouldEmbargo},
      /*selected_idp_config_url=*/absl::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.count(
      main_test_rfh()->GetLastCommittedOrigin()));
}

// Test that the embargo dismiss count is reset when the user grants consent via
// the FedCM dialog.
TEST_F(FederatedAuthRequestImplTest, RemoveEmbargoOnUserConsent) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.empty());
}

// Test that token request fails if FEDERATED_IDENTITY_API content setting is
// disabled for the RP origin.
TEST_F(FederatedAuthRequestImplTest, ApiBlockedForOrigin) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_SETTINGS);
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorDisabledInSettings},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
}

// Test that token request succeeds if FEDERATED_IDENTITY_API content setting is
// enabled for RP origin but disabled for an unrelated origin.
TEST_F(FederatedAuthRequestImplTest, ApiBlockedForUnrelatedOrigin) {
  const url::Origin kUnrelatedOrigin = OriginFromString("https://rp2.example/");

  test_api_permission_delegate_->permission_override_ =
      std::make_pair(kUnrelatedOrigin, ApiPermissionStatus::BLOCKED_SETTINGS);
  ASSERT_NE(main_test_rfh()->GetLastCommittedOrigin(), kUnrelatedOrigin);
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

class FederatedAuthRequestImplTestCancelConsistency
    : public FederatedAuthRequestImplTest,
      public ::testing::WithParamInterface<int> {};
INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         FederatedAuthRequestImplTestCancelConsistency,
                         ::testing::Values(false, true),
                         ::testing::PrintToStringParamName());

// Test that the RP cannot use CancelTokenRequest() to determine whether
// Option 1: FedCM dialog is shown but user has not interacted with it
// Option 2: FedCM API is disabled via variations
TEST_P(FederatedAuthRequestImplTestCancelConsistency, AccountNotSelected) {
  const bool fedcm_disabled = GetParam();

  if (fedcm_disabled) {
    test_api_permission_delegate_->permission_override_ =
        std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                       ApiPermissionStatus::BLOCKED_VARIATIONS);
  }

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;
  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  EXPECT_FALSE(auth_helper_.was_callback_called());

  request_remote_->CancelTokenRequest();

  WaitForCurrentAuthRequest();
  RequestExpectations expectations{RequestTokenStatus::kErrorCanceled,
                                   /*devtools_issue_statuses=*/{},
                                   /*selected_idp_config_url=*/absl::nullopt};
  CheckAuthExpectations(configuration, expectations);
}

namespace {

// TestDialogController which disables FedCM API when FedCM account selection
// dialog is shown.
class DisableApiWhenDialogShownDialogController : public TestDialogController {
 public:
  DisableApiWhenDialogShownDialogController(
      MockConfiguration configuration,
      TestApiPermissionDelegate* api_permission_delegate,
      const url::Origin rp_origin_to_disable)
      : TestDialogController(configuration),
        api_permission_delegate_(api_permission_delegate),
        rp_origin_to_disable_(rp_origin_to_disable) {}
  ~DisableApiWhenDialogShownDialogController() override = default;

  DisableApiWhenDialogShownDialogController(
      const DisableApiWhenDialogShownDialogController&) = delete;
  DisableApiWhenDialogShownDialogController& operator=(
      DisableApiWhenDialogShownDialogController&) = delete;

  void ShowAccountsDialog(
      content::WebContents* rp_web_contents,
      const std::string& top_frame_for_display,
      const absl::optional<std::string>& iframe_for_display,
      const std::vector<IdentityProviderData>& identity_provider_data,
      SignInMode sign_in_mode,
      bool show_auto_reauthn_checkbox,
      IdentityRequestDialogController::AccountSelectionCallback on_selected,
      IdentityRequestDialogController::DismissCallback dismiss_callback)
      override {
    // Disable FedCM API
    api_permission_delegate_->permission_override_ = std::make_pair(
        rp_origin_to_disable_, ApiPermissionStatus::BLOCKED_SETTINGS);

    // Call parent class method in order to store callback parameters.
    TestDialogController::ShowAccountsDialog(
        rp_web_contents, top_frame_for_display, iframe_for_display,
        std::move(identity_provider_data), sign_in_mode,
        show_auto_reauthn_checkbox, std::move(on_selected),
        std::move(dismiss_callback));
  }

 private:
  raw_ptr<TestApiPermissionDelegate> api_permission_delegate_;
  url::Origin rp_origin_to_disable_;
};

}  // namespace

// Test that the request fails if user proceeds with the sign in workflow after
// disabling the API while an existing accounts dialog is shown.
TEST_F(FederatedAuthRequestImplTest, ApiDisabledAfterAccountsDialogShown) {
  base::HistogramTester histogram_tester_;

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorDisabledInSettings},
      /*selected_idp_config_url=*/absl::nullopt};

  url::Origin rp_origin_to_disable = main_test_rfh()->GetLastCommittedOrigin();
  SetDialogController(
      std::make_unique<DisableApiWhenDialogShownDialogController>(
          kConfigurationValid, test_api_permission_delegate_.get(),
          rp_origin_to_disable));

  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInSettings, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectNoTimingUKM("Timing.ContinueOnDialog");
  ExpectNoTimingUKM("Timing.IdTokenResponse");
  ExpectNoTimingUKM("Timing.TurnaroundTime");

  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInSettings);
  CheckAllFedCmSessionIDs();
}

// Test the disclosure_text_shown value in the token post data for sign-up case.
TEST_F(FederatedAuthRequestImplTest, DisclosureTextShownForFirstTimeUser) {
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) + "&disclosure_text_shown=true");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test the disclosure_text_shown value in the token post data for returning
// user case.
TEST_F(FederatedAuthRequestImplTest, DisclosureTextNotShownForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData("client_id=" + std::string(kClientId) +
                                    "&nonce=" + std::string(kNonce) +
                                    "&account_id=" + std::string(kAccountId) +
                                    "&disclosure_text_shown=false");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that the values in the token post data are escaped according to the
// application/x-www-form-urlencoded spec.
TEST_F(FederatedAuthRequestImplTest, TokenEndpointPostDataEscaping) {
  const std::string kAccountIdWithSpace("account id");
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts[0].id = kAccountIdWithSpace;

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=account+id&disclosure_text_shown=true");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

namespace {

// TestIdpNetworkRequestManager subclass which runs the `account_list_task`
// passed-in to the constructor prior to the accounts endpoint returning.
class IdpNetworkRequestManagerClientMetadataTaskRunner
    : public TestIdpNetworkRequestManager {
 public:
  explicit IdpNetworkRequestManagerClientMetadataTaskRunner(
      base::OnceClosure client_metadata_task)
      : client_metadata_task_(std::move(client_metadata_task)) {}

  IdpNetworkRequestManagerClientMetadataTaskRunner(
      const IdpNetworkRequestManagerClientMetadataTaskRunner&) = delete;
  IdpNetworkRequestManagerClientMetadataTaskRunner& operator=(
      const IdpNetworkRequestManagerClientMetadataTaskRunner&) = delete;

  void FetchClientMetadata(const GURL& client_metadata_endpoint_url,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    // Make copies because running the task might destroy
    // FederatedAuthRequestImpl and invalidate the references.
    GURL client_metadata_endpoint_url_copy = client_metadata_endpoint_url;
    std::string client_id_copy = client_id;

    if (client_metadata_task_)
      std::move(client_metadata_task_).Run();
    TestIdpNetworkRequestManager::FetchClientMetadata(
        client_metadata_endpoint_url_copy, client_id_copy, std::move(callback));
  }

 private:
  base::OnceClosure client_metadata_task_;
};

void NavigateToUrl(content::WebContents* web_contents, const GURL& url) {
  static_cast<TestWebContents*>(web_contents)
      ->NavigateAndCommit(url, ui::PAGE_TRANSITION_LINK);
}

}  // namespace

// Test that the account chooser is not shown if the page navigates prior to the
// client metadata endpoint request completing and BFCache is enabled.
TEST_F(FederatedAuthRequestImplTest,
       NavigateDuringClientMetadataFetchBFCacheEnabled) {
  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      GetBasicBackForwardCacheFeatureForTesting(),
      GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  ASSERT_TRUE(IsBackForwardCacheEnabled());

  SetNetworkRequestManager(
      std::make_unique<IdpNetworkRequestManagerClientMetadataTaskRunner>(
          base::BindOnce(&NavigateToUrl, web_contents(), GURL(kRpOtherUrl))));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that the account chooser is not shown if the page navigates prior to the
// accounts endpoint request completing and BFCache is disabled.
TEST_F(FederatedAuthRequestImplTest,
       NavigateDuringClientMetadataFetchBFCacheDisabled) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kBackForwardCache);
  ASSERT_FALSE(IsBackForwardCacheEnabled());

  SetNetworkRequestManager(
      std::make_unique<IdpNetworkRequestManagerClientMetadataTaskRunner>(
          base::BindOnce(&NavigateToUrl, web_contents(), GURL(kRpOtherUrl))));

  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that the accounts are reordered so that accounts with a LoginState equal
// to kSignIn are listed before accounts with a LoginState equal to kSignUp.
TEST_F(FederatedAuthRequestImplTest, ReorderMultipleAccounts) {
  // Run an auth test to initialize variables.
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  AccountList multiple_accounts = kMultipleAccounts;
  blink::mojom::IdentityProviderConfigPtr identity_provider =
      blink::mojom::IdentityProviderConfig::New();
  identity_provider->config_url = GURL(kProviderUrlFull);
  identity_provider->client_id = kClientId;
  identity_provider->nonce = kNonce;

  ComputeLoginStateAndReorderAccounts(identity_provider, multiple_accounts);

  // Check the account order using the account ids.
  ASSERT_EQ(multiple_accounts.size(), 3u);
  EXPECT_EQ(multiple_accounts[0].id, kAccountIdPeter);
  EXPECT_EQ(multiple_accounts[1].id, kAccountIdNicolas);
  EXPECT_EQ(multiple_accounts[2].id, kAccountIdZach);
}

// Test that first API call with a given IDP is not affected by the
// IdpSigninStatus bit.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusTestFirstTimeFetchSuccess) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*test_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), true))
      .Times(1);

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kClientId, kAccountId);
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that first API call with a given IDP will not show a UI in case of
// failure during fetching accounts.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusTestFirstTimeFetchNoFailureUi) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*test_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), false))
      .Times(1);
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that a failure UI will be displayed if the accounts fetch is failed but
// the IdpSigninStatus claims that the user is signed in.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusTestShowFailureUi) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  configuration.idp_signin_status_mismatch_dialog_action =
      IdpSigninStatusMismatchDialogAction::kClose;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that API calls will fail before sending any network request if
// IdpSigninStatus shows that the user is not signed in with the IDP. No failure
// UI is displayed.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusTestApiFailedIfUserNotSignedInWithIdp) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = false;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

namespace {

// TestIdpNetworkRequestManager which enables specifying the ParseStatus for
// config and accounts endpoint fetch.
class ParseStatusOverrideIdpNetworkRequestManager
    : public TestIdpNetworkRequestManager {
 public:
  ParseStatus config_parse_status_{ParseStatus::kSuccess};
  ParseStatus accounts_parse_status_{ParseStatus::kSuccess};

  ParseStatusOverrideIdpNetworkRequestManager() = default;
  ~ParseStatusOverrideIdpNetworkRequestManager() override = default;

  ParseStatusOverrideIdpNetworkRequestManager(
      const ParseStatusOverrideIdpNetworkRequestManager&) = delete;
  ParseStatusOverrideIdpNetworkRequestManager& operator=(
      const ParseStatusOverrideIdpNetworkRequestManager&) = delete;

  void FetchConfig(const GURL& provider,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    if (config_parse_status_ != ParseStatus::kSuccess) {
      ++num_fetched_[FetchedEndpoint::CONFIG];

      FetchStatus fetch_status{config_parse_status_, net::HTTP_OK};
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), fetch_status,
                                    IdpNetworkRequestManager::Endpoints(),
                                    IdentityProviderMetadata()));
      return;
    }
    TestIdpNetworkRequestManager::FetchConfig(
        provider, idp_brand_icon_ideal_size, idp_brand_icon_minimum_size,
        std::move(callback));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    if (accounts_parse_status_ != ParseStatus::kSuccess) {
      ++num_fetched_[FetchedEndpoint::ACCOUNTS];

      FetchStatus fetch_status{accounts_parse_status_, net::HTTP_OK};
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), fetch_status, AccountList()));
      return;
    }

    TestIdpNetworkRequestManager::SendAccountsRequest(accounts_url, client_id,
                                                      std::move(callback));
  }
};

}  // namespace

// Test behavior for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) User signs-in
// 3) User selects "Continue" in account chooser dialog.
TEST_F(FederatedAuthRequestImplTest, FailureUiThenSuccessfulSignin) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  // Simulate user signing into IdP by updating the IdP sign-in status and
  // calling the observer.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  network_manager->accounts_parse_status_ = ParseStatus::kSuccess;
  federated_auth_request_impl_->OnIdpSigninStatusChanged(
      kIdpOrigin, /*idp_signin_status=*/true);

  WaitForCurrentAuthRequest();
  CheckAuthExpectations(kConfigurationValid, kExpectationSuccess);

  EXPECT_TRUE(did_show_accounts_dialog());

  // After the IdP sign-in status was updated, the endpoints should have been
  // fetched a 2nd time.
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 2u);
}

// Test behavior for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) User switches tabs
// 3) User signs into IdP in different tab
TEST_F(FederatedAuthRequestImplTest, FailureUiThenSuccessfulSigninButHidden) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  // Simulate the user switching to a different tab.
  test_rvh()->SimulateWasHidden();

  // Simulate user signing into IdP by updating the IdP signin status and
  // calling observer.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  network_manager->accounts_parse_status_ = ParseStatus::kSuccess;
  federated_auth_request_impl_->OnIdpSigninStatusChanged(
      kIdpOrigin, /*idp_signin_status=*/true);

  WaitForCurrentAuthRequest();
  CheckAuthExpectations(kConfigurationValid, kExpectationSuccess);

  // The FedCM dialog should switch to the account picker. The user should
  // see a new dialog when they switch back to the FedCM tab.
  EXPECT_TRUE(did_show_accounts_dialog());
}

// Test behavior for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) In a different tab, user signs into different IdP
TEST_F(FederatedAuthRequestImplTest, FailureUiSigninFromDifferentIdp) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);
  url::Origin kOtherOrigin = OriginFromString("https://idp.other");

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  size_t num_well_known_fetches = NumFetched(FetchedEndpoint::WELL_KNOWN);

  // Simulate user signing into different IdP by updating the IdP signin status
  // and calling observer.
  test_permission_delegate_->idp_signin_statuses_[kOtherOrigin] = true;
  federated_auth_request_impl_->OnIdpSigninStatusChanged(
      kOtherOrigin, /*idp_signin_status=*/true);
  base::RunLoop().RunUntilIdle();

  // No fetches should have been triggered.
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), num_well_known_fetches);
}

// Test that for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) IdP sign-in status is updated
// 3) Accounts endpoint still returns an empty list
// That ShowFailureDialog() is called a 2nd time after the IdP sign-in status
// update.
TEST_F(FederatedAuthRequestImplTest, FailureUiAccountEndpointKeepsFailing) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  MockConfiguration configuration = kConfigurationValid;

  // Setup IdP sign-in status mismatch.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  // Update IdP sign-in status. Keep accounts endpoint returning empty list.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  federated_auth_request_impl_->OnIdpSigninStatusChanged(
      kIdpOrigin, /*idp_signin_status=*/true);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, dialog_controller_state_
                    .num_show_idp_signin_status_mismatch_dialog_requests);
  EXPECT_FALSE(dialog_controller_state_.did_show_idp_signin_failure_dialog);

  // After the IdP sign-in status was updated, the endpoints should have been
  // fetched a 2nd time.
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 2u);
}

// Test that for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) IdP sign-in status is updated
// 3) A different endpoint fails during the fetch initiated by the IdP sign-in
// status update.
// That user is shown IdP-sign-in-failure dialog.
TEST_F(FederatedAuthRequestImplTest, FailureUiThenFailDifferentEndpoint) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);

  // Make the fetch triggered by the IdP sign-in status changing fail for a
  // different endpoint.
  network_manager->config_parse_status_ = ParseStatus::kInvalidResponseError;

  // Simulate user signing into IdP by updating the IdP signin status and
  // calling the observer.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  network_manager->accounts_parse_status_ = ParseStatus::kSuccess;
  federated_auth_request_impl_->OnIdpSigninStatusChanged(
      kIdpOrigin, /*idp_signin_status=*/true);

  WaitForCurrentAuthRequest();
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  CheckAuthExpectations(kConfigurationValid, expectations);

  // The user should be shown IdP-sign-in-failure dialog.
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_EQ(1u, dialog_controller_state_
                    .num_show_idp_signin_status_mismatch_dialog_requests);
  EXPECT_TRUE(dialog_controller_state_.did_show_idp_signin_failure_dialog);

  // After the IdP sign-in status was updated, the endpoints should have been
  // fetched a 2nd time.
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);
}

// Test that the IdP-sign-in-failure-dialog is not shown if there is an error
// after the user has selected an account.
TEST_F(FederatedAuthRequestImplTest,
       FailAfterAccountSelectionHideDialogDoesNotShowIdpSigninFailureDialog) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  // Setup dialog controller to fail FedCM request after the user has selected
  // an account.
  url::Origin rp_origin_to_disable = main_test_rfh()->GetLastCommittedOrigin();
  SetDialogController(
      std::make_unique<DisableApiWhenDialogShownDialogController>(
          kConfigurationValid, test_api_permission_delegate_.get(),
          rp_origin_to_disable));

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  // Simulate user signing into IdP by updating the IdP signin status and
  // calling the observer.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  network_manager->accounts_parse_status_ = ParseStatus::kSuccess;
  federated_auth_request_impl_->OnIdpSigninStatusChanged(
      kIdpOrigin, /*idp_signin_status=*/true);
  WaitForCurrentAuthRequest();

  // Check that the FedCM request failed after the account picker was shown.
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  CheckAuthExpectations(kConfigurationValid, expectations);
  EXPECT_TRUE(did_show_accounts_dialog());

  // Check that the IdP-sign-in-failure dialog is not shown.
  EXPECT_FALSE(dialog_controller_state_.did_show_idp_signin_failure_dialog);
}

// Test that the IdP-sign-in-failure dialog is not shown in the
// following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) FedCM call is aborted.
TEST_F(FederatedAuthRequestImplTest,
       FailureUiAbortDoesNotShowIdpSigninFailureDialog) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  // Abort the request before DelayTimer kicks in.
  federated_auth_request_impl_->CancelTokenRequest();

  RequestExpectations expectations{RequestTokenStatus::kErrorCanceled,
                                   {FederatedAuthRequestResult::kErrorCanceled},
                                   /*selected_idp_config_url=*/absl::nullopt};
  WaitForCurrentAuthRequest();
  CheckAuthExpectations(kConfigurationValid, expectations);

  // Abort should not trigger IdP-sign-in-failure dialog.
  EXPECT_FALSE(dialog_controller_state_.did_show_idp_signin_failure_dialog);
}

// Test that when IdpSigninStatus API is in the metrics-only mode, that an IDP
// signed-out status stays signed-out regardless of what is returned by the
// accounts endpoint.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusMetricsModeStaysSignedout) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "true"}});

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = false;
  EXPECT_CALL(*test_permission_delegate_, SetIdpSigninStatus(_, _)).Times(0);

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that when IdpSigninStatus API does not have any state for an IDP, that
// the state transitions to sign-in if the accounts endpoint returns a
// non-empty list of accounts.
TEST_F(
    FederatedAuthRequestImplTest,
    IdpSigninStatusMetricsModeUndefinedTransitionsToSignedinWhenHaveAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "true"}});

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] =
      absl::nullopt;
  EXPECT_CALL(*test_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), true));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that when IdpSigninStatus API is in metrics-only mode, that IDP sign-in
// status transitions to signed-out if the accounts endpoint returns no
// information.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusMetricsModeTransitionsToSignedoutWhenNoAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "true"}});

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;
  EXPECT_CALL(*test_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), false));

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError, {}, absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Tests that multiple IDPs provided results in an error if the
// `kFedCmMultipleIdentityProviders` flag is disabled.
TEST_F(FederatedAuthRequestImplTest, MultiIdpError) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kFedCmMultipleIdentityProviders);

  RequestExpectations expectations = {
      RequestTokenStatus::kError, {}, absl::nullopt};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations,
              kConfigurationMultiIdpValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
}

// Test successful multi IDP FedCM request.
TEST_F(FederatedAuthRequestImplTest, AllSuccessfulMultiIdpRequest) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  RunAuthTest(kDefaultMultiIdpRequestParameters, kExpectationSuccess,
              kConfigurationMultiIdpValid);
  EXPECT_EQ(2u, NumFetched(FetchedEndpoint::ACCOUNTS));
}

// Test fetching information for the 1st IdP failing, and succeeding for the
// second.
TEST_F(FederatedAuthRequestImplTest, FirstIdpWellKnownInvalid) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Intentionally fail the 1st provider's request by having an invalid
  // well-known file.
  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};

  RequestExpectations expectations = {
      RequestTokenStatus::kSuccess,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/kProviderTwoUrlFull};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::CONFIG), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::TOKEN), 1u);
}

// Test fetching information for the 1st IdP succeeding, and failing for the
// second.
TEST_F(FederatedAuthRequestImplTest, SecondIdpWellKnownInvalid) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Intentionally fail the 2nd provider's request by having an invalid
  // well-known file.
  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.idp_info[kProviderTwoUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};

  RequestExpectations expectations = {
      RequestTokenStatus::kSuccess,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/kProviderUrlFull};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::CONFIG), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::TOKEN), 1u);
}

// Test fetching information for all of the IdPs failing.
TEST_F(FederatedAuthRequestImplTest, AllWellKnownsInvalid) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Intentionally fail the requests for both IdPs by returning an invalid
  // well-known file.
  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};
  configuration.idp_info[kProviderTwoUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/absl::nullopt};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::CONFIG), 2u);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test multi IDP FedCM request with duplicate IDPs should throw an error.
TEST_F(FederatedAuthRequestImplTest, DuplicateIdpMultiIdpRequest) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  RequestParameters request_parameters = kDefaultMultiIdpRequestParameters;
  request_parameters.identity_providers =
      std::vector<IdentityProviderParameters>{
          request_parameters.identity_providers[0],
          request_parameters.identity_providers[0]};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};

  RunAuthTest(request_parameters, expectations, kConfigurationMultiIdpValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
  EXPECT_FALSE(did_show_accounts_dialog());
}

TEST_F(FederatedAuthRequestImplTest, TooManyRequests) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;
  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());

  // Reset the network request manager so we can check that we fetch no
  // endpoints in the subsequent call.
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  SetNetworkRequestManager(std::make_unique<TestIdpNetworkRequestManager>());
  // The next FedCM request should fail since the initial request has not yet
  // been finalized.
  RequestExpectations expectations = {
      RequestTokenStatus::kErrorTooManyRequests,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());
}

// TestIdpNetworkRequestManager subclass which records requests to metrics
// endpoint.
class IdpNetworkRequestMetricsRecorder : public TestIdpNetworkRequestManager {
 public:
  IdpNetworkRequestMetricsRecorder() = default;
  IdpNetworkRequestMetricsRecorder(const IdpNetworkRequestMetricsRecorder&) =
      delete;
  IdpNetworkRequestMetricsRecorder& operator=(
      const IdpNetworkRequestMetricsRecorder&) = delete;

  void SendSuccessfulTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      base::TimeDelta api_call_to_show_dialog_time,
      base::TimeDelta show_dialog_to_continue_clicked_time,
      base::TimeDelta account_selected_to_token_response_time,
      base::TimeDelta api_call_to_token_response_time) override {
    metrics_endpoints_notified_success_.push_back(metrics_endpoint_url);
  }

  void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      MetricsEndpointErrorCode error_code) override {
    metrics_endpoints_notified_failure_.push_back(metrics_endpoint_url);
  }

  const std::vector<GURL>& get_metrics_endpoints_notified_success() {
    return metrics_endpoints_notified_success_;
  }

  const std::vector<GURL>& get_metrics_endpoints_notified_failure() {
    return metrics_endpoints_notified_failure_;
  }

 private:
  std::vector<GURL> metrics_endpoints_notified_success_;
  std::vector<GURL> metrics_endpoints_notified_failure_;
};

// Test that the metrics endpoint is notified as a result of a successful
// multi-IDP FederatedAuthRequestImpl::RequestToken() call.
TEST_F(FederatedAuthRequestImplTest, MetricsEndpointMultiIdp) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures(
      /*enabled_features=*/{features::kFedCmMetricsEndpoint,
                            features::kFedCmMultipleIdentityProviders},
      /*disabled_features=*/{});

  std::unique_ptr<IdpNetworkRequestMetricsRecorder> unique_metrics_recorder =
      std::make_unique<IdpNetworkRequestMetricsRecorder>();
  IdpNetworkRequestMetricsRecorder* metrics_recorder =
      unique_metrics_recorder.get();
  SetNetworkRequestManager(std::move(unique_metrics_recorder));

  RunAuthTest(kDefaultMultiIdpRequestParameters, kExpectationSuccess,
              kConfigurationMultiIdpValid);
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_success(),
              ElementsAre(kMetricsEndpoint));
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_failure(),
              ElementsAre("https://idp2.example/metrics"));
}

// Test that the metrics endpoint is notified when
// FederatedAuthRequestImpl::RequestToken() call fails.
TEST_F(FederatedAuthRequestImplTest, MetricsEndpointMultiIdpFail) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures(
      /*enabled_features=*/{features::kFedCmMetricsEndpoint,
                            features::kFedCmMultipleIdentityProviders},
      /*disabled_features=*/{});

  std::unique_ptr<IdpNetworkRequestMetricsRecorder> unique_metrics_recorder =
      std::make_unique<IdpNetworkRequestMetricsRecorder>();
  IdpNetworkRequestMetricsRecorder* metrics_recorder =
      unique_metrics_recorder.get();
  SetNetworkRequestManager(std::move(unique_metrics_recorder));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kShouldEmbargo},
      /* selected_idp_config_url=*/absl::nullopt};

  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());

  EXPECT_TRUE(
      metrics_recorder->get_metrics_endpoints_notified_success().empty());
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_failure(),
              ElementsAre(kMetricsEndpoint, "https://idp2.example/metrics"));
}

TEST_F(FederatedAuthRequestImplTest, LoginHintDisabled) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = "incorrect_login_hint";

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kSingleAccountWithHint;

  // The login hint should be disregarded when the flag is disabled. Therefore,
  // the account should be shown.
  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(displayed_accounts().size(), 1u);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountIdMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = kAccountId;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kSingleAccountWithHint;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].id, kAccountId);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountEmailMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = kEmail;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kSingleAccountWithHint;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].email, kEmail);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountNoMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = "incorrect_login_hint";
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty},
      /*selected_idp_config_url=*/absl::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kSingleAccountWithHint;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

TEST_F(FederatedAuthRequestImplTest, LoginHintFirstAccountMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = kAccountIdNicolas;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHints;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].id, kAccountIdNicolas);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintLastAccountMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = kAccountIdZach;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHints;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].id, kAccountIdZach);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintMultipleAccountsNoMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = "incorrect_login_hint";
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty},
      /*selected_idp_config_url=*/absl::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHints;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that when FedCmRpContext flag is enabled and rp_context is specified,
// the FedCM request succeeds with the specified rp_context.
TEST_F(FederatedAuthRequestImplTest, RpContextIsSetToNonDefaultValue) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmRpContext);

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.rp_context = blink::mojom::RpContext::kContinue;
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  EXPECT_EQ(dialog_controller_state_.rp_context,
            blink::mojom::RpContext::kContinue);
}

// Test that when FedCmRpContext flag is at its default setting and rp_context
// is specified, the FedCM request ignores the specified rp_context and defaults
// to sign in.
TEST_F(FederatedAuthRequestImplTest, RpContextIsDefaultToSignIn) {
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.rp_context = blink::mojom::RpContext::kContinue;
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  EXPECT_EQ(dialog_controller_state_.rp_context,
            blink::mojom::RpContext::kSignIn);
}

TEST_F(FederatedAuthRequestImplTest, WellKnownInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull]
      .well_known.fetch_status.parse_status =
      ParseStatus::kInvalidContentTypeError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidContentType},
      /*selected_idp_config_url=*/absl::nullopt};

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  ukm_loop.Run();

  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.RequestIdToken",
      TokenStatus::kWellKnownInvalidContentType, 1);

  ExpectRequestTokenStatusUKM(TokenStatus::kWellKnownInvalidContentType);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, ConfigInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.fetch_status.parse_status =
      ParseStatus::kInvalidContentTypeError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidContentType},
      /*selected_idp_config_url=*/absl::nullopt};

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  ukm_loop.Run();

  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kConfigInvalidContentType,
                                       1);

  ExpectRequestTokenStatusUKM(TokenStatus::kConfigInvalidContentType);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, ClientMetadataInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull]
      .client_metadata.fetch_status.parse_status =
      ParseStatus::kInvalidContentTypeError;

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  // The FedCM flow succeeds even if the client metadata fetch fails.
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  ukm_loop.Run();

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kSuccess, 1);

  ExpectRequestTokenStatusUKM(TokenStatus::kSuccess);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, AccountsInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidContentTypeError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidContentType},
      /*selected_idp_config_url=*/absl::nullopt};

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  ukm_loop.Run();

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kAccountsInvalidContentType,
                                       1);

  ExpectRequestTokenStatusUKM(TokenStatus::kAccountsInvalidContentType);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, IdTokenInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.token_response.parse_status =
      ParseStatus::kInvalidContentTypeError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidContentType},
      /*selected_idp_config_url=*/absl::nullopt};

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  ukm_loop.Run();

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kIdTokenInvalidContentType,
                                       1);

  ExpectRequestTokenStatusUKM(TokenStatus::kIdTokenInvalidContentType);
  CheckAllFedCmSessionIDs();
}

// Test that the account UI is not displayed if FedCM is disabled after accounts
// fetch.
TEST_F(FederatedAuthRequestImplTest,
       AccountUiNotDisplayedIfFedCmDisabledAfterAccountsFetch) {
  test_api_permission_delegate_->permission_override_for_nth_ = std::make_pair(
      /*override the nth invocation=*/2,
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_EMBARGO));
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorDisabledInSettings},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

}  // namespace content
