// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
#define CONTENT_BROWSER_WEBID_WEBID_UTILS_H_

#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink::mojom {
enum class FederatedAuthRequestResult;
enum class IdpSigninStatus;
}  // namespace blink::mojom

namespace content {
class BrowserContext;
enum class FedCmIdpSigninStatusMode;
class FedCmMetrics;
class FederatedIdentityPermissionContextDelegate;
enum class IdpSigninStatus;

namespace webid {

// Returns true if |origin| is same-origin with |render_frame_host| and
// all its ancestors. Also returns true if there are no ancestors or
// if |render_frame_host| is null.
bool IsSameOriginWithAncestors(const url::Origin& origin,
                               RenderFrameHost* render_frame_host);

void SetIdpSigninStatus(BrowserContext* context,
                        int frame_tree_node_id,
                        const url::Origin& origin,
                        blink::mojom::IdpSigninStatus status);

// Computes string to display in developer tools console for a FedCM endpoint
// request with the passed-in `endpoint_name` and which returns the passed-in
// `http_response_code`. Returns absl::nullopt if the `http_response_code` does
// not represent an error in the fetch.
absl::optional<std::string> ComputeConsoleMessageForHttpResponseCode(
    const char* endpoint_name,
    int http_response_code);

// Returns whether a FedCM endpoint URL is valid given the passed-in config
// endpoint URL.
bool IsEndpointSameOrigin(const GURL& identity_provider_config_url,
                          const GURL& endpoint_url);

// Returns whether FedCM should fail/skip the accounts endpoint request because
// the user is not signed-in to the IdP.
bool ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
    RenderFrameHost& host,
    const GURL& identity_provider_config_url,
    FederatedIdentityPermissionContextDelegate* permission_delegate);

// Updates the IdP sign-in status based on the accounts endpoint response. Also
// logs IdP sign-in status related UMA metrics.
//
// `does_idp_have_failing_idp_signin_status` indicates whether the accounts
// endpoint request would have been failed/skipped had the IdP signin-status
// been FedCmIdpSigninStatusMode::ENABLED.
void UpdateIdpSigninStatusForAccountsEndpointResponse(
    RenderFrameHost& host,
    const GURL& identity_provider_config_url,
    IdpNetworkRequestManager::FetchStatus account_endpoint_fetch_status,
    bool does_idp_have_failing_idp_signin_status,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    FedCmMetrics* metrics);

// Returns a string to be used as the console error message from a
// FederatedAuthRequestResult.
CONTENT_EXPORT std::string GetConsoleErrorMessageFromResult(
    blink::mojom::FederatedAuthRequestResult result);

FedCmIdpSigninStatusMode GetIdpSigninStatusMode(RenderFrameHost& host,
                                                const url::Origin& idp_origin);

// Returns the eTLD+1 for a given url. For localhost, returns the host.
std::string FormatUrlWithDomain(const GURL& url, bool for_display);

}  // namespace webid

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
