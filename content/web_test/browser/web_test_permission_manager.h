// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_PERMISSION_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_PERMISSION_MANAGER_H_

#include <stddef.h>

#include "base/containers/id_map.h"
#include "base/functional/callback_forward.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_automation.mojom.h"
#include "url/gurl.h"

namespace blink {
enum class PermissionType;
}

namespace content {

class WebTestPermissionManager
    : public PermissionControllerDelegate,
      public blink::test::mojom::PermissionAutomation {
 public:
  // `browser_context` must outlive `this`.
  explicit WebTestPermissionManager(BrowserContext& browser_context);

  WebTestPermissionManager(const WebTestPermissionManager&) = delete;
  WebTestPermissionManager& operator=(const WebTestPermissionManager&) = delete;

  ~WebTestPermissionManager() override;

  // PermissionManager overrides.
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
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForEmbeddedRequester(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const url::Origin& overridden_origin) override;
  SubscriptionId SubscribePermissionStatusChange(
      blink::PermissionType permission,
      RenderProcessHost* render_process_host,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;

  void SetPermission(
      blink::PermissionType permission,
      blink::mojom::PermissionStatus status,
      const GURL& url,
      const GURL& embedding_url,
      blink::test::mojom::PermissionAutomation::SetPermissionCallback callback);
  void ResetPermissions();

  // blink::test::mojom::PermissionAutomation
  void SetPermission(
      blink::mojom::PermissionDescriptorPtr descriptor,
      blink::mojom::PermissionStatus status,
      const GURL& url,
      const GURL& embedding_url,
      blink::test::mojom::PermissionAutomation::SetPermissionCallback) override;

  void Bind(
      mojo::PendingReceiver<blink::test::mojom::PermissionAutomation> receiver);

 private:
  // Representation of a permission for the WebTestPermissionManager.
  struct PermissionDescription {
    PermissionDescription() = default;
    PermissionDescription(blink::PermissionType type,
                          const GURL& origin,
                          const GURL& embedding_origin);
    // Note that the comparison operator does not always require strict
    // origin equality for the requesting and embedding origin. For permission
    // types such as STORAGE_ACCESS_GRANT, which are scoped to (requesting
    // site, embedding site), it will apply a same-site check instead.
    bool operator==(const PermissionDescription& other) const;
    bool operator!=(const PermissionDescription& other) const;

    // Hash operator for hash maps.
    struct Hash {
      size_t operator()(const PermissionDescription& description) const;
    };

    blink::PermissionType type;
    GURL origin;
    GURL embedding_origin;
  };

  struct Subscription;
  using SubscriptionsMap =
      base::IDMap<std::unique_ptr<Subscription>, SubscriptionId>;
  using PermissionsMap = std::unordered_map<PermissionDescription,
                                            blink::mojom::PermissionStatus,
                                            PermissionDescription::Hash>;
  using DefaultPermissionStatusMap =
      std::unordered_map<blink::PermissionType, blink::mojom::PermissionStatus>;

  void OnPermissionChanged(
      const PermissionDescription& permission,
      blink::mojom::PermissionStatus status,
      blink::test::mojom::PermissionAutomation::SetPermissionCallback callback);

  raw_ref<BrowserContext> browser_context_;

  // Mutex for permissions access. Unfortunately, the permissions can be
  // accessed from the IO thread because of Notifications' synchronous IPC.
  base::Lock permissions_lock_;

  // List of permissions currently known by the WebTestPermissionManager and
  // their associated |PermissionStatus|.
  PermissionsMap permissions_;

  // A map of permission types to their default statuses, for those that require
  // specific statuses to be returned in the absence of another value.
  DefaultPermissionStatusMap default_permission_status_ = {
      {blink::PermissionType::STORAGE_ACCESS_GRANT,
       blink::mojom::PermissionStatus::ASK},
      {blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS,
       blink::mojom::PermissionStatus::ASK},
  };

  // List of subscribers currently listening to permission changes.
  SubscriptionsMap subscriptions_;
  SubscriptionId::Generator subscription_id_generator_;

  mojo::ReceiverSet<blink::test::mojom::PermissionAutomation> receivers_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_PERMISSION_MANAGER_H_
