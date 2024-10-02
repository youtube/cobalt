// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_PERMISSION_MANAGER_H_
#define CONTENT_PUBLIC_TEST_MOCK_PERMISSION_MANAGER_H_

#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
enum class PermissionType;
}

namespace content {

// Mock of the permission manager for unit tests.
class MockPermissionManager : public PermissionControllerDelegate {
 public:
  MockPermissionManager();

  MockPermissionManager(const MockPermissionManager&) = delete;
  MockPermissionManager& operator=(const MockPermissionManager&) = delete;

  ~MockPermissionManager() override;

  // PermissionManager:
  MOCK_METHOD3(GetPermissionStatus,
               blink::mojom::PermissionStatus(blink::PermissionType permission,
                                              const GURL& requesting_origin,
                                              const GURL& embedding_origin));
  MOCK_METHOD2(GetPermissionResultForOriginWithoutContext,
               content::PermissionResult(blink::PermissionType permission,
                                         const url::Origin& origin));
  MOCK_METHOD2(GetPermissionStatusForCurrentDocument,
               blink::mojom::PermissionStatus(
                   blink::PermissionType permission,
                   content::RenderFrameHost* render_frame_host));
  MOCK_METHOD3(GetPermissionStatusForWorker,
               blink::mojom::PermissionStatus(
                   blink::PermissionType permission,
                   content::RenderProcessHost* render_process_host,
                   const GURL& worker_origin));
  MOCK_METHOD3(
      GetPermissionStatusForEmbeddedRequester,
      blink::mojom::PermissionStatus(blink::PermissionType permission,
                                     RenderFrameHost* render_frame_host,
                                     const url::Origin& overridden_origin));
  void RequestPermission(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void RequestPermissions(
      const std::vector<blink::PermissionType>& permission,
      RenderFrameHost* render_frame_host,
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
  MOCK_METHOD5(SubscribePermissionStatusChange,
               SubscriptionId(
                   blink::PermissionType permission,
                   RenderProcessHost* render_process_host,
                   RenderFrameHost* render_frame_host,
                   const GURL& requesting_origin,
                   base::RepeatingCallback<void(blink::mojom::PermissionStatus)>
                       callback));
  MOCK_METHOD1(UnsubscribePermissionStatusChange,
               void(SubscriptionId subscription_id));
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_PERMISSION_MANAGER_H_
