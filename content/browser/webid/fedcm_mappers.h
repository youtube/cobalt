// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FEDCM_MAPPERS_H_
#define CONTENT_BROWSER_FEDCM_MAPPERS_H_

#include <string>
#include <vector>

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"

namespace content {

enum class FedCmLifecycleStateFailureReason;

// This header file defines functions which convert between FedCM types. It also
// defines some constants used in some of these conversions.

inline constexpr char kFedCmDefaultFieldName[] = "name";
inline constexpr char kFedCmDefaultFieldEmail[] = "email";
inline constexpr char kFedCmDefaultFieldPicture[] = "picture";
inline constexpr char kFedCmFieldPhoneNumber[] = "tel";
inline constexpr char kFedCmFieldUsername[] = "username";

// Error codes sent to the metrics endpoint.
// Enum is part of public FedCM API. Do not renumber error codes.
// The error codes are not consecutive to make adding error codes easier in
// the future.
enum class MetricsEndpointErrorCode {
  kNone = 0,  // Success
  kOther = 1,
  // Errors triggered by how RP calls FedCM API.
  kRpFailure = 100,
  // User Failures.
  kUserFailure = 200,
  // Generic IDP Failures.
  kIdpServerInvalidResponse = 300,
  kIdpServerUnavailable = 301,
  kManifestError = 302,
  // Specific IDP Failures.
  kAccountsEndpointInvalidResponse = 401,
  kTokenEndpointInvalidResponse = 402,
};

// Converts a list of IdentityRequestDialogDisclosureField to a list of strings.
// May return an empty list.
std::vector<std::string> DisclosureFieldsToStringList(
    const std::vector<IdentityRequestDialogDisclosureField>& fields);

// Converts a FederatedAuthRequestResult, which is a browser type for the
// result, to a RequestTokenStatus, which is a renderer type, e.g. the one to be
// exposed to web developers.
blink::mojom::RequestTokenStatus FederatedAuthRequestResultToRequestTokenStatus(
    blink::mojom::FederatedAuthRequestResult result);

// Converts a FederatedAuthRequestResult, which is a browser type for the
// result, to a MetricsEndpointErrorCode, which is a type used in the metrics
// endpoint error code.
MetricsEndpointErrorCode FederatedAuthRequestResultToMetricsEndpointErrorCode(
    blink::mojom::FederatedAuthRequestResult result);

// Converts an error ParseStatus from the accounts response to a pair. The first
// member of the pair is a FederatedAuthRequestResult, which is a browser type
// for the result. The second member of the pair is a FedCmRequestIdTokenStatus,
// which is a type used in metrics recording. Should not be invoked with
// ParseStatus::kSuccess.
std::pair<blink::mojom::FederatedAuthRequestResult, FedCmRequestIdTokenStatus>
AccountParseStatusToRequestResultAndTokenStatus(
    IdpNetworkRequestManager::ParseStatus status);

FedCmLifecycleStateFailureReason
LifecycleStateImplLifecycleStateImplToFedCmLifecycleStateFailureReason(
    RenderFrameHostImpl::LifecycleStateImpl lifecycle_state);

// Converts a FederatedApiPermissionStatus to a (FederatedAuthRequestResult,
// FedCmRequestIdTokenStatus) pair. Should not be invoked with
// FederatedApiPermissionStatus::GRANTED.
std::pair<blink::mojom::FederatedAuthRequestResult, FedCmRequestIdTokenStatus>
PermissionStatusToRequestResultAndTokenStatus(
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
        permission_status);

FedCmErrorDialogResult DismissReasonToErrorDialogResult(
    IdentityRequestDialogController::DismissReason dismiss_reason,
    bool has_url);

// Converts a FetchStatus from the ID assertion endpoint to a
// (FederatedAuthRequestResult, FedCmRequestIdTokenStatus) pair. Should not be
// invoked when the parse_status is ParseStatus::kSuccess.
std::pair<blink::mojom::FederatedAuthRequestResult, FedCmRequestIdTokenStatus>
IdAssertionFetchStatusToRequestResultAndTokenStatus(
    IdpNetworkRequestManager::FetchStatus status);

// Returns a list of fields that we should mediate authorization for. If
// empty, we should not show a permission request dialog.
CONTENT_EXPORT std::vector<IdentityRequestDialogDisclosureField>
GetDisclosureFields(const std::optional<std::vector<std::string>>& fields);

}  // namespace content

#endif  // CONTENT_BROWSER_FEDCM_MAPPERS_H_
