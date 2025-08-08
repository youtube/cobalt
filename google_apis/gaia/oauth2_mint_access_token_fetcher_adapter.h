// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_MINT_ACCESS_TOKEN_FETCHER_ADAPTER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_MINT_ACCESS_TOKEN_FETCHER_ADAPTER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

class OAuth2AccessTokenConsumer;

namespace network {
class SharedURLLoaderFactory;
}

// Class adapting `OAuth2MintTokenFlow` to the `OAuth2AccessTokenFetcher`
// interface.
class COMPONENT_EXPORT(GOOGLE_APIS) OAuth2MintAccessTokenFetcherAdapter
    : public OAuth2AccessTokenFetcher,
      public OAuth2MintTokenFlow::Delegate {
 public:
  using OAuth2MintTokenFlowFactory =
      base::RepeatingCallback<std::unique_ptr<OAuth2MintTokenFlow>(
          OAuth2MintTokenFlow::Delegate*,
          OAuth2MintTokenFlow::Parameters)>;

  explicit OAuth2MintAccessTokenFetcherAdapter(
      OAuth2AccessTokenConsumer* consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& refresh_token,
      const std::string& device_id,
      const std::string& client_version,
      const std::string& client_channel);

  OAuth2MintAccessTokenFetcherAdapter(
      const OAuth2MintAccessTokenFetcherAdapter&) = delete;
  OAuth2MintAccessTokenFetcherAdapter& operator=(
      const OAuth2MintAccessTokenFetcherAdapter&) = delete;

  ~OAuth2MintAccessTokenFetcherAdapter() override;

  // OAuth2AccessTokenFetcher:
  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;
  void CancelRequest() override;

  void SetOAuth2MintTokenFlowFactoryForTesting(
      OAuth2MintTokenFlowFactory factory);

 private:
  // OAuth2MintTokenFlow::Delegate:
  void OnMintTokenSuccess(const std::string& access_token,
                          const std::set<std::string>& granted_scopes,
                          int time_to_live) override;
  void OnMintTokenFailure(const GoogleServiceAuthError& error) override;
  void OnRemoteConsentSuccess(
      const RemoteConsentResolutionData& resolution_data) override;

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string refresh_token_;
  const std::string device_id_;
  const std::string client_version_;
  const std::string client_channel_;

  OAuth2MintTokenFlowFactory mint_token_flow_factory_for_testing_;

  std::unique_ptr<OAuth2MintTokenFlow> mint_token_flow_;
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_MINT_ACCESS_TOKEN_FETCHER_ADAPTER_H_
