// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/unguessable_token.h"
#include "services/network/public/mojom/connection_allowlist.mojom-shared.h"
#include "url/gurl.h"

namespace network {

// This implements a data structure holding information from a parsed
// `Connection-Allowlist` or `Connection-Allowlist-Report-Only` header.
//
// This struct is needed as we can't use the mojom generated struct directly
// from the blink public API, given that we cannot include .mojo.h there due to
// DEPS rules.
struct COMPONENT_EXPORT(NETWORK_CPP_CONNECTION_ALLOWLIST) ConnectionAllowlist {
  enum class RedirectBehavior {
    kAllow,
    kBlock,
  };

  enum class WebRtcBehavior {
    kAllow,
    kBlock,
  };

  ConnectionAllowlist();
  ~ConnectionAllowlist();

  ConnectionAllowlist(ConnectionAllowlist&&);
  ConnectionAllowlist& operator=(ConnectionAllowlist&&);

  ConnectionAllowlist(const ConnectionAllowlist&);
  ConnectionAllowlist& operator=(const ConnectionAllowlist&);

  bool operator==(const ConnectionAllowlist&) const;

  std::vector<std::string> allowlist;
  std::optional<std::string> reporting_endpoint;
  std::vector<mojom::ConnectionAllowlistIssue> issues;
  RedirectBehavior redirect_behavior = RedirectBehavior::kBlock;
  WebRtcBehavior webrtc_behavior = WebRtcBehavior::kBlock;

  // True when the `response-origin` token was present but its resolution was
  // deferred (the allowlist was parsed without knowing the origin to resolve
  // against, e.g. in the renderer for an iframe attribute). The browser
  // resolves it against the actual response origin before enforcing. When the
  // origin is known at parse time (response headers), `response-origin` is
  // resolved eagerly into `allowlist` and this stays false.
  bool match_response_origin = false;

  // The original serialized value this allowlist was parsed from (e.g. the
  // iframe `connectionallowlist` attribute string). Retained so the browser can
  // re-emit it verbatim in the `Sec-Required-Connection-Allowlist` request
  // header that advertises an embedded-enforcement requirement to the framed
  // document, mirroring CSP embedded enforcement's `Sec-Required-CSP`. Empty
  // unless populated by the requirement's producer (the iframe attribute
  // parser); response-header parsing leaves it empty.
  std::string serialized_value;
};

COMPONENT_EXPORT(NETWORK_CPP_CONNECTION_ALLOWLIST)
bool ConnectionAllowlistMatchesUrl(
    const ConnectionAllowlist& connection_allowlist,
    const GURL& url);

// Returns true if `candidate` is "at least as strict as" `required`: every
// endpoint permitted by `candidate` is also permitted by `required`, and
// `candidate` is at least as strict as `required` with respect to redirects and
// WebRTC. An empty `candidate` allowlist permits no endpoints and is therefore
// trivially at least as strict as any `required` (it is maximally strict).
//
// Allowlist entries are compared as already-resolved strings (a conservative,
// syntactic set-membership test), so the `response-origin` token must be
// resolved against the same response URL in both allowlists before calling
// this. Used by Connection-Allowlist embedded enforcement; see
// https://github.com/WICG/connection-allowlists/issues/1.
COMPONENT_EXPORT(NETWORK_CPP_CONNECTION_ALLOWLIST)
bool ConnectionAllowlistSubsumes(const ConnectionAllowlist& required,
                                 const ConnectionAllowlist& candidate);

// The set of allowlists associated with a given response, typemapped for the
// same reason.
struct COMPONENT_EXPORT(NETWORK_CPP_CONNECTION_ALLOWLIST) ConnectionAllowlists {
  ConnectionAllowlists();
  ~ConnectionAllowlists();

  ConnectionAllowlists(ConnectionAllowlists&&);
  ConnectionAllowlists& operator=(ConnectionAllowlists&&);

  ConnectionAllowlists(const ConnectionAllowlists&);
  ConnectionAllowlists& operator=(const ConnectionAllowlists&);

  bool operator==(const ConnectionAllowlists&) const;

  GURL response_url;
  std::optional<ConnectionAllowlist> enforced;
  std::optional<ConnectionAllowlist> report_only;
  std::optional<base::UnguessableToken> reporting_source;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_H_
