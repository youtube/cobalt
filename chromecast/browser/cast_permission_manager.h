// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_PERMISSION_MANAGER_H_
#define CHROMECAST_BROWSER_CAST_PERMISSION_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

namespace blink {
enum class PermissionType;
}

namespace chromecast {
namespace shell {

class CastPermissionManager : public content::PermissionControllerDelegate {
 public:
  CastPermissionManager();

  CastPermissionManager(const CastPermissionManager&) = delete;
  CastPermissionManager& operator=(const CastPermissionManager&) = delete;

  ~CastPermissionManager() override;

  // content::PermissionControllerDelegate implementation:
  void RequestPermission(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void RequestPermissions(
      const std::vector<blink::PermissionType>& permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  void RequestPermissionsFromCurrentDocument(
      const std::vector<blink::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  content::PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForEmbeddedRequester(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const url::Origin& requesting_origin) override;
  SubscriptionId SubscribePermissionStatusChange(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;
};

}  // namespace shell
}  // namespace chromecast

#endif // CHROMECAST_BROWSER_CAST_PERMISSION_MANAGER_H_
