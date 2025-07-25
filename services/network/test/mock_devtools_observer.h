// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_MOCK_DEVTOOLS_OBSERVER_H_
#define SERVICES_NETWORK_TEST_MOCK_DEVTOOLS_OBSERVER_H_

#include <string>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/url_request/url_request.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

class MockDevToolsObserver : public mojom::DevToolsObserver {
 public:
  MockDevToolsObserver();
  ~MockDevToolsObserver() override;
  MockDevToolsObserver(const MockDevToolsObserver&) = delete;
  MockDevToolsObserver& operator=(const MockDevToolsObserver&) = delete;

  mojo::PendingRemote<mojom::DevToolsObserver> Bind();

  // mojom::DevToolsObserver:
  void OnRawRequest(
      const std::string& devtools_request_id,
      const net::CookieAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      const base::TimeTicks timestamp,
      network::mojom::ClientSecurityStatePtr client_security_state,
      network::mojom::OtherPartitionInfoPtr site_has_cookie_in_other_partition)
      override;

  void OnRawResponse(
      const std::string& devtools_request_id,
      const net::CookieAndLineAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      const absl::optional<std::string>& raw_response_headers,
      network::mojom::IPAddressSpace resource_address_space,
      int32_t http_status_code,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key)
      override;

  void OnPrivateNetworkRequest(
      const absl::optional<std::string>& devtools_request_id,
      const GURL& url,
      bool is_warning,
      network::mojom::IPAddressSpace resource_address_space,
      network::mojom::ClientSecurityStatePtr client_security_state) override;

  void OnCorsPreflightRequest(
      const base::UnguessableToken& devtool_request_id,
      const net::HttpRequestHeaders& request_headers,
      network::mojom::URLRequestDevToolsInfoPtr request_info,
      const GURL& initiator_url,
      const std::string& initiator_devtool_request_id) override;

  void OnCorsPreflightResponse(
      const base::UnguessableToken& devtool_request_id,
      const GURL& url,
      network::mojom::URLResponseHeadDevToolsInfoPtr head) override;

  void OnCorsPreflightRequestCompleted(
      const base::UnguessableToken& devtool_request_id,
      const network::URLLoaderCompletionStatus& status) override;

  void OnTrustTokenOperationDone(
      const std::string& devtool_request_id,
      network::mojom::TrustTokenOperationResultPtr result) override;

  MOCK_METHOD(void,
              OnSubresourceWebBundleMetadata,
              (const std::string& devtools_request_id,
               const std::vector<GURL>& urls),
              (override));

  MOCK_METHOD(void,
              OnSubresourceWebBundleMetadataError,
              (const std::string& devtools_request_id,
               const std::string& error_message),
              (override));

  MOCK_METHOD(void,
              OnSubresourceWebBundleInnerResponse,
              (const std::string& inner_request_devtools_id,
               const GURL& url,
               const absl::optional<std::string>& bundle_request_devtools_id),
              (override));

  MOCK_METHOD(void,
              OnSubresourceWebBundleInnerResponseError,
              (const std::string& inner_request_devtools_id,
               const GURL& url,
               const std::string& error_message,
               const absl::optional<std::string>& bundle_request_devtools_id),
              (override));

  void OnCorsError(const absl::optional<std::string>& devtool_request_id,
                   const absl::optional<::url::Origin>& initiator_origin,
                   mojom::ClientSecurityStatePtr client_security_state,
                   const GURL& url,
                   const network::CorsErrorStatus& status,
                   bool is_warning) override;

  MOCK_METHOD(void,
              OnCorbError,
              (const absl::optional<std::string>& devtools_request_id,
               const GURL& url),
              (override));

  void Clone(mojo::PendingReceiver<DevToolsObserver> observer) override;

  void WaitUntilRawResponse(size_t goal);
  void WaitUntilRawRequest(size_t goal);
  void WaitUntilPrivateNetworkRequest();
  void WaitUntilCorsError();

  const net::CookieAndLineAccessResultList& raw_response_cookies() const {
    return raw_response_cookies_;
  }

  const net::CookieAccessResultList& raw_request_cookies() const {
    return raw_request_cookies_;
  }

  const std::string devtools_request_id() { return devtools_request_id_; }

  const std::vector<network::mojom::HttpRawHeaderPairPtr>& response_headers()
      const {
    return response_headers_;
  }

  const absl::optional<std::string> raw_response_headers() const {
    return raw_response_headers_;
  }

  int32_t raw_response_http_status_code() const {
    return raw_response_http_status_code_;
  }

  const network::mojom::ClientSecurityStatePtr& client_security_state() const {
    return client_security_state_;
  }

  network::mojom::IPAddressSpace resource_address_space() const {
    return resource_address_space_;
  }

  struct OnPrivateNetworkRequestParams {
    OnPrivateNetworkRequestParams(
        const absl::optional<std::string>& devtools_request_id,
        const GURL& url,
        bool is_warning,
        network::mojom::IPAddressSpace resource_address_space,
        network::mojom::ClientSecurityStatePtr client_security_state);
    OnPrivateNetworkRequestParams(OnPrivateNetworkRequestParams&&);
    ~OnPrivateNetworkRequestParams();
    absl::optional<std::string> devtools_request_id;
    GURL url;
    bool is_warning;
    network::mojom::IPAddressSpace resource_address_space;
    network::mojom::ClientSecurityStatePtr client_security_state;
  };

  const absl::optional<OnPrivateNetworkRequestParams>&
  private_network_request_params() const {
    return params_of_private_network_request_;
  }

  struct OnCorsErrorParams {
    OnCorsErrorParams();
    OnCorsErrorParams(OnCorsErrorParams&&);
    OnCorsErrorParams& operator=(OnCorsErrorParams&&);
    ~OnCorsErrorParams();

    absl::optional<std::string> devtools_request_id;
    absl::optional<::url::Origin> initiator_origin;
    mojom::ClientSecurityStatePtr client_security_state;
    GURL url;
    absl::optional<network::CorsErrorStatus> status;
    bool is_warning = false;
  };

  const absl::optional<OnCorsErrorParams>& cors_error_params() const {
    return params_of_cors_error_;
  }

  const absl::optional<net::CookiePartitionKey>& response_cookie_partition_key()
      const {
    return response_cookie_partition_key_;
  }

  const absl::optional<network::URLLoaderCompletionStatus>& preflight_status()
      const {
    return preflight_status_;
  }

 private:
  net::CookieAndLineAccessResultList raw_response_cookies_;
  base::OnceClosure wait_for_raw_response_;
  size_t wait_for_raw_response_goal_ = 0u;
  bool got_raw_response_ = false;
  network::mojom::IPAddressSpace resource_address_space_;
  std::string devtools_request_id_;
  absl::optional<std::string> raw_response_headers_;
  std::vector<network::mojom::HttpRawHeaderPairPtr> response_headers_;
  int32_t raw_response_http_status_code_ = -1;
  absl::optional<network::URLLoaderCompletionStatus> preflight_status_;

  bool got_raw_request_ = false;
  net::CookieAccessResultList raw_request_cookies_;
  base::OnceClosure wait_for_raw_request_;
  size_t wait_for_raw_request_goal_ = 0u;
  network::mojom::ClientSecurityStatePtr client_security_state_;

  base::RunLoop wait_for_private_network_request_;
  absl::optional<OnPrivateNetworkRequestParams>
      params_of_private_network_request_;

  base::RunLoop wait_for_cors_error_;
  absl::optional<OnCorsErrorParams> params_of_cors_error_;

  mojo::ReceiverSet<mojom::DevToolsObserver> receivers_;

  absl::optional<net::CookiePartitionKey> response_cookie_partition_key_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_MOCK_DEVTOOLS_OBSERVER_H_
