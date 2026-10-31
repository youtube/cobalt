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

#ifndef COBALT_SHELL_BROWSER_COBALT_PERMISSION_MANAGER_H_
#define COBALT_SHELL_BROWSER_COBALT_PERMISSION_MANAGER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

namespace blink {
enum class PermissionType;
}

namespace url {
class Origin;
}

namespace content {

// Cobalt has no permission UI, so every permission is denied, with a single
// exception: PROTECTED_MEDIA_IDENTIFIER (the EME "distinctive identifier") is
// granted to a document whose origin is the same as every ancestor frame up to
// and including its outermost main frame. Requests without a frame context
// (workers, origin-only queries) are denied. This keeps Widevine working for
// the top-level application while preventing cross-origin subframes (e.g. ad
// iframes granted allow="encrypted-media") from obtaining the device-unique
// identifier.
class CobaltPermissionManager : public PermissionControllerDelegate {
 public:
  CobaltPermissionManager();

  CobaltPermissionManager(const CobaltPermissionManager&) = delete;
  CobaltPermissionManager& operator=(const CobaltPermissionManager&) = delete;

  ~CobaltPermissionManager() override;

  // Returns the permission status for |permission| requested by
  // |requesting_origin| embedded in a top-level document of
  // |embedding_origin|. Callers must separately verify the frame ancestry.
  static blink::mojom::PermissionStatus GetPermissionStatusForOrigins(
      blink::PermissionType permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin);

  // PermissionControllerDelegate implementation.
  void RequestPermissions(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  PermissionResult GetPermissionResultForOriginWithoutContext(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      RenderFrameHost* render_frame_host,
      bool should_include_device_status) override;
  PermissionResult GetPermissionResultForCurrentDocument(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      RenderFrameHost* render_frame_host,
      bool should_include_device_status) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForEmbeddedRequester(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      RenderFrameHost* render_frame_host,
      const url::Origin& requesting_origin) override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;

 private:
  std::vector<blink::mojom::PermissionStatus> GetStatusesForFrame(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description);
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_COBALT_PERMISSION_MANAGER_H_
