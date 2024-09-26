// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_LOCAL_NETWORK_ACCESS_CHECKER_H_
#define SERVICES_NETWORK_LOCAL_NETWORK_ACCESS_CHECKER_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "net/base/ip_address.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/local_network_access_check_result.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

class GURL;

namespace net {

struct TransportInfo;

}  // namespace net

namespace network {

struct ResourceRequest;

// Applies Local Network Access checks to a single fetch / URL load.
//
// Manages state used for the "Local Network Access check" algorithm from
// the Local Network Access spec:
// https://wicg.github.io/local-network-access/#local-network-access-check
//
// Helper class for `URLLoader`. Should be instantiated once per `URLLoader`.
//
// Thread-compatible.
class COMPONENT_EXPORT(NETWORK_SERVICE) LocalNetworkAccessChecker {
 public:
  // `resource_request` and `url_load_options` correspond to `URLLoader`
  // constructor arguments.
  // `factory_client_security_state` should point to the client security
  // state object coming from the factory that built the owner `URLLoader`. It
  // can be nullptr when the factory doesn't use a client security state.
  LocalNetworkAccessChecker(
      const ResourceRequest& resource_request,
      const mojom::ClientSecurityState* factory_client_security_state,
      int32_t url_load_options);

  // Instances of this class are neither copyable nor movable.
  LocalNetworkAccessChecker(const LocalNetworkAccessChecker&) = delete;
  LocalNetworkAccessChecker& operator=(const LocalNetworkAccessChecker&) =
      delete;

  ~LocalNetworkAccessChecker();

  // Checks whether the client should be allowed to use the given transport.
  //
  // Implements the following "Local Network Access check" algorithm:
  // https://wicg.github.io/local-network-access/#local-network-access-check
  LocalNetworkAccessCheckResult Check(const net::TransportInfo& transport_info);

  // Returns the IP address space derived from the `transport_info` argument
  // passed to the last call to `Check()`, if any.
  //
  // Spec:
  // https://wicg.github.io/local-network-access/#response-ip-address-space
  absl::optional<mojom::IPAddressSpace> ResponseAddressSpace() const {
    return response_address_space_;
  }

  // The target IP address space applied to subsequent checks.
  //
  // Spec:
  // https://wicg.github.io/local-network-access/#request-target-ip-address-space
  mojom::IPAddressSpace TargetAddressSpace() const {
    return target_address_space_;
  }

  // Clears state from all checks this instance has performed, and sets the
  // request URL to `new_url`.
  //
  // This instance will behave as if newly constructed once more. In addition,
  // resets this instance's target IP address space to `kUnknown`.
  //
  // This should be called upon following a redirect or after a cache result
  // blocked without preflight because we'll try fetching from the network.
  void ResetForRedirect(const GURL& new_url);

  // Clears state from all checks this instance has performed.
  //
  // This instance will behave as if newly constructed once more. In addition,
  // resets this instance's target IP address space to `kUnknown`.
  //
  // This should be called after a cache result was blocked without preflight,
  // because we'll try fetching from the network again.
  void ResetForRetry();

  // Returns the client security state that applies to the current request.
  // May return nullptr.
  //
  // Contains relevant state derived from the fetch client's policy container.
  const mojom::ClientSecurityState* client_security_state() const {
    return client_security_state_.get();
  }

  // Returns an owned clone of `client_security_state()`.
  mojom::ClientSecurityStatePtr CloneClientSecurityState() const;

  // Returns the IP address space in `client_security_state()`.
  // Returns `kUnknown` if `client_security_state()` is nullptr.
  mojom::IPAddressSpace ClientAddressSpace() const;

 private:
  // Returns whether this instance has a client security state containing a
  // policy set to `kPreflightWarn`.
  bool IsPolicyPreflightWarn() const;

  // Helper for `Check()`.
  LocalNetworkAccessCheckResult CheckInternal(
      mojom::IPAddressSpace resource_address_space);

  // Sets the current request URL (it may change after redirects).
  void SetRequestUrl(const GURL& url);

  // The client security state copied from the request's trusted params.
  // May be nullptr.
  //
  // Should not be used directly. Use `client_security_state_` instead, which
  // points to the same struct iff this client security state should be used.
  const mojom::ClientSecurityStatePtr request_client_security_state_;

  // The security state of the client of the fetch. May be nullptr.
  const raw_ptr<const mojom::ClientSecurityState> client_security_state_;

  // Whether to block all requests to non-public IP address spaces, regardless
  // of other considerations. Set based on URL load options.
  const bool should_block_local_request_;

  // Whether the request URL's scheme is `http:`.
  bool is_request_url_scheme_http_ = false;

  // If the request URL's host is a local IP literal, then this stores the
  // IP. Nullopt otherwise.
  //
  // For example:
  //
  // - request url = `http://192.168.1.1`   -> `192.168.1.1`
  // - request url = `http://[fe80::]:1234` -> `fe80::`
  // - request url = `https://10.0.0.1`     -> `10.0.0.1`
  // - request url = `http://localhost`     -> nullptr
  //
  // Used to compute metrics for https://crbug.com/1381471.
  absl::optional<net::IPAddress> request_url_local_ip_;

  // The target IP address space set on the request. Ignored if `kUnknown`.
  //
  // Copied from `ResourceRequest::target_ip_address_space`.
  //
  // Invariant: always `kUnknown` if `client_security_state_` is nullptr, or
  // if `client_security_state_->local_network_request_policy` is `kAllow`.
  //
  // https://wicg.github.io/local-network-access/#request-target-ip-address-space
  mojom::IPAddressSpace target_address_space_;

  // The IP address space derived from the `transport_info` argument passed to
  // the last call to `Check()`.
  //
  // Set by `Check()`, reset by `ResetForRedirect()`.
  absl::optional<mojom::IPAddressSpace> response_address_space_;

  // The request initiator origin.
  absl::optional<url::Origin> request_initiator_;

  // The request is from/to a potentially trustworthy and same origin.
  bool is_potentially_trustworthy_same_origin_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_LOCAL_NETWORK_ACCESS_CHECKER_H_
