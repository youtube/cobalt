// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_MINT_TOKEN_FLOW_H_
#define GOOGLE_APIS_GAIA_OAUTH2_MINT_TOKEN_FLOW_H_

#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

class GoogleServiceAuthError;
class OAuth2MintTokenFlowTest;

COMPONENT_EXPORT(GOOGLE_APIS)
extern const char kOAuth2MintTokenApiCallResultHistogram[];

// Values carrying the result of processing a successful API call.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OAuth2MintTokenApiCallResult {
  kMintTokenSuccess = 0,
  // DEPRECATED:
  // kIssueAdviceSuccess = 1,
  kRemoteConsentSuccess = 2,
  kApiCallFailure = 3,
  kParseJsonFailure = 4,
  kIssueAdviceKeyNotFoundFailure = 5,
  kParseMintTokenFailure = 6,
  // DEPRECATED:
  // kParseIssueAdviceFailure = 7,
  // DEPRECATED:
  // kRemoteConsentFallback = 8
  kParseRemoteConsentFailure = 9,
  // DEPRECATED:
  // kMintTokenSuccessWithFallbackScopes = 10,
  kChallengeResponseRequiredFailure = 11,
  kMaxValue = kChallengeResponseRequiredFailure
};

// Data for the remote consent resolution:
// - URL of the consent page to be displayed to the user.
// - Cookies that should be set before navigating to that URL.
struct COMPONENT_EXPORT(GOOGLE_APIS) RemoteConsentResolutionData {
  RemoteConsentResolutionData();
  ~RemoteConsentResolutionData();

  RemoteConsentResolutionData(const RemoteConsentResolutionData& other);
  RemoteConsentResolutionData& operator=(
      const RemoteConsentResolutionData& other);

  GURL url;
  net::CookieList cookies;

  bool operator==(const RemoteConsentResolutionData& rhs) const;
};

// This class implements the OAuth2 flow to Google to mint an OAuth2 access
// token for the given client and the given set of scopes from the OAuthLogin
// scoped "master" OAuth2 token for the user logged in to Chrome.
class COMPONENT_EXPORT(GOOGLE_APIS) OAuth2MintTokenFlow
    : public OAuth2ApiCallFlow {
 public:
  // There are four different modes when minting a token to grant
  // access to third-party app for a user.
  enum Mode {
    // Get the messages to display to the user without minting a token.
    MODE_ISSUE_ADVICE,
    // Record a grant but do not get a token back.
    MODE_RECORD_GRANT,
    // Mint a token for an existing grant.
    MODE_MINT_TOKEN_NO_FORCE,
    // Mint a token forcefully even if there is no existing grant.
    MODE_MINT_TOKEN_FORCE,
  };

  // Parameters needed to mint a token.
  struct COMPONENT_EXPORT(GOOGLE_APIS) Parameters {
   public:
    Parameters();

    static Parameters CreateForExtensionFlow(
        base::StringPiece extension_id,
        base::StringPiece client_id,
        base::span<const base::StringPiece> scopes,
        Mode mode,
        bool enable_granular_permissions,
        base::StringPiece version,
        base::StringPiece channel,
        base::StringPiece device_id = {},
        base::StringPiece selected_user_id = {},
        base::StringPiece consent_result = {});

    static Parameters CreateForClientFlow(
        base::StringPiece client_id,
        base::span<const base::StringPiece> scopes,
        base::StringPiece version,
        base::StringPiece channel,
        base::StringPiece device_id = {});

    Parameters(Parameters&& other) noexcept;
    Parameters& operator=(Parameters&& other) noexcept;

    ~Parameters();

    Parameters Clone();

    // Mandatory parameters:
    std::string client_id;
    std::vector<std::string> scopes;
    Mode mode = MODE_ISSUE_ADVICE;
    bool enable_granular_permissions = false;
    std::string version;
    std::string channel;

    // Optional parameters:
    std::string extension_id;  // Do not set if an access token should be issued
                               // for Chrome itself.
    std::string device_id;
    std::string selected_user_id;
    std::string consent_result;

   private:
    // Only an explicit copy with `Clone()` is allowed.
    Parameters(const Parameters&);
    Parameters& operator=(const Parameters&);
  };

  class COMPONENT_EXPORT(GOOGLE_APIS) Delegate {
   public:
    // TODO(alexilin): change `time_to_live` parameter to be `base::TimeDelta`
    // type.
    virtual void OnMintTokenSuccess(const std::string& access_token,
                                    const std::set<std::string>& granted_scopes,
                                    int time_to_live) {}
    virtual void OnMintTokenFailure(const GoogleServiceAuthError& error) {}
    virtual void OnRemoteConsentSuccess(
        const RemoteConsentResolutionData& resolution_data) {}

   protected:
    virtual ~Delegate() = default;
  };

  // This object stores `parameters` internally. Passing by value allows moving
  // them in.
  OAuth2MintTokenFlow(Delegate* delegate, Parameters parameters);

  OAuth2MintTokenFlow(const OAuth2MintTokenFlow&) = delete;
  OAuth2MintTokenFlow& operator=(const OAuth2MintTokenFlow&) = delete;

  ~OAuth2MintTokenFlow() override;

 protected:
  // Implementation of template methods in OAuth2ApiCallFlow.
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;

  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override;

 private:
  friend class OAuth2MintTokenFlowTest;
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest, ParseIssueAdviceResponse);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest,
                           ParseRemoteConsentResponse_EmptyCookies);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest,
                           ParseRemoteConsentResponse_NoResolutionData);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest,
                           ParseRemoteConsentResponse_NoUrl);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest,
                           ParseRemoteConsentResponse_BadUrl);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest,
                           ParseRemoteConsentResponse_NoApproach);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest,
                           ParseRemoteConsentResponse_BadApproach);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest,
                           ParseRemoteConsentResponse_NoCookies);
  FRIEND_TEST_ALL_PREFIXES(
      OAuth2MintTokenFlowTest,
      ParseRemoteConsentResponse_BadCookie_MissingRequiredField);
  FRIEND_TEST_ALL_PREFIXES(
      OAuth2MintTokenFlowTest,
      ParseRemoteConsentResponse_MissingCookieOptionalField);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest,
                           ParseRemoteConsentResponse_BadCookie_BadMaxAge);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest,
                           ParseRemoteConsentResponse_BadCookieList);
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFlowTest, ParseMintTokenResponse);

  void ReportSuccess(const std::string& access_token,
                     const std::set<std::string>& granted_scopes,
                     int time_to_live);
  void ReportRemoteConsentSuccess(
      const RemoteConsentResolutionData& resolution_data);
  void ReportFailure(const GoogleServiceAuthError& error);

  static bool ParseRemoteConsentResponse(
      const base::Value::Dict& dict,
      RemoteConsentResolutionData* resolution_data);

  static bool ParseMintTokenResponse(const base::Value::Dict& dict,
                                     std::string* access_token,
                                     std::set<std::string>* granted_scopes,
                                     int* time_to_live);

  raw_ptr<Delegate> delegate_;
  Parameters parameters_;
  base::WeakPtrFactory<OAuth2MintTokenFlow> weak_factory_{this};
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_MINT_TOKEN_FLOW_H_
