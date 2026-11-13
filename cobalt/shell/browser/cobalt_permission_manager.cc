// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/shell/browser/cobalt_permission_manager.h"

#include <utility>

#include "base/functional/callback.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::PermissionType;
using blink::mojom::PermissionStatus;

namespace content {

CobaltPermissionManager::CobaltPermissionManager() = default;

CobaltPermissionManager::~CobaltPermissionManager() = default;

// static
PermissionStatus CobaltPermissionManager::GetPermissionStatusForOrigins(
    PermissionType permission,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
    return PermissionStatus::DENIED;
  }
  // Only the top-level application's own origin may use the EME distinctive
  // identifier. Cross-origin and opaque-origin (e.g. sandboxed) frames are
  // denied.
  if (requesting_origin.opaque() || requesting_origin != embedding_origin ||
      !network::IsOriginPotentiallyTrustworthy(requesting_origin)) {
    return PermissionStatus::DENIED;
  }
  return PermissionStatus::GRANTED;
}

std::vector<PermissionStatus> CobaltPermissionManager::GetStatusesForFrame(
    RenderFrameHost* render_frame_host,
    const PermissionRequestDescription& request_description) {
  std::vector<PermissionStatus> result;
  result.reserve(request_description.permissions.size());
  for (const auto& permission : request_description.permissions) {
    result.push_back(GetPermissionStatusForCurrentDocument(
        permission, render_frame_host,
        /*should_include_device_status=*/false));
  }
  return result;
}

void CobaltPermissionManager::RequestPermissions(
    RenderFrameHost* render_frame_host,
    const PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  std::move(callback).Run(
      GetStatusesForFrame(render_frame_host, request_description));
}

void CobaltPermissionManager::RequestPermissionsFromCurrentDocument(
    RenderFrameHost* render_frame_host,
    const PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  std::move(callback).Run(
      GetStatusesForFrame(render_frame_host, request_description));
}

PermissionStatus CobaltPermissionManager::GetPermissionStatus(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // Without a frame context the frame ancestry cannot be verified, so nothing
  // is granted.
  return PermissionStatus::DENIED;
}

PermissionResult
CobaltPermissionManager::GetPermissionResultForOriginWithoutContext(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return PermissionResult(PermissionStatus::DENIED,
                          PermissionStatusSource::UNSPECIFIED);
}

PermissionStatus CobaltPermissionManager::GetPermissionStatusForCurrentDocument(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  if (!render_frame_host || render_frame_host->IsNestedWithinFencedFrame()) {
    return PermissionStatus::DENIED;
  }
  const url::Origin& requesting_origin =
      render_frame_host->GetLastCommittedOrigin();
  // Every ancestor up to the outermost main frame must be same-origin with
  // the requesting frame, so e.g. A > B > A is denied.
  for (RenderFrameHost* ancestor =
           render_frame_host->GetParentOrOuterDocument();
       ancestor; ancestor = ancestor->GetParentOrOuterDocument()) {
    if (ancestor->GetLastCommittedOrigin() != requesting_origin) {
      return PermissionStatus::DENIED;
    }
  }
  return GetPermissionStatusForOrigins(
      blink::PermissionDescriptorToPermissionType(permission_descriptor),
      requesting_origin,
      render_frame_host->GetOutermostMainFrame()->GetLastCommittedOrigin());
}

PermissionResult CobaltPermissionManager::GetPermissionResultForCurrentDocument(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  return PermissionResult(GetPermissionStatusForCurrentDocument(
                              permission_descriptor, render_frame_host,
                              should_include_device_status),
                          PermissionStatusSource::UNSPECIFIED);
}

PermissionStatus CobaltPermissionManager::GetPermissionStatusForWorker(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  // EME is not exposed to workers; nothing is granted to them.
  return PermissionStatus::DENIED;
}

PermissionStatus
CobaltPermissionManager::GetPermissionStatusForEmbeddedRequester(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    RenderFrameHost* render_frame_host,
    const url::Origin& requesting_origin) {
  // Only used for TOP_LEVEL_STORAGE_ACCESS, which Cobalt does not grant.
  return PermissionStatus::DENIED;
}

void CobaltPermissionManager::ResetPermission(PermissionType permission,
                                              const GURL& requesting_origin,
                                              const GURL& embedding_origin) {}

}  // namespace content
