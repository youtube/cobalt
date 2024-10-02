// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_MANAGER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_MANAGER_H_

#include <map>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/containers/id_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "url/origin.h"

namespace blink {
enum class PermissionType;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class RenderProcessHost;
}

class GeolocationPermissionContextDelegateTests;
class SubscriptionInterceptingPermissionManager;

namespace permissions {
class PermissionContextBase;
struct PermissionResult;
class PermissionManagerTest;

class PermissionManager : public KeyedService,
                          public content::PermissionControllerDelegate,
                          public permissions::Observer,
                          public PermissionDecisionAutoBlocker::Observer {
 public:
  using PermissionContextMap =
      std::unordered_map<ContentSettingsType,
                         std::unique_ptr<PermissionContextBase>,
                         ContentSettingsTypeHash>;
  PermissionManager(content::BrowserContext* browser_context,
                    PermissionContextMap permission_contexts);

  PermissionManager(const PermissionManager&) = delete;
  PermissionManager& operator=(const PermissionManager&) = delete;

  ~PermissionManager() override;

  // KeyedService implementation.
  void Shutdown() override;

  // PermissionDecisionAutoBlocker::Observer
  void OnEmbargoStarted(const GURL& origin,
                        ContentSettingsType content_setting) override;

  PermissionContextBase* GetPermissionContextForTesting(
      ContentSettingsType type);

  PermissionContextMap& PermissionContextsForTesting() {
    return permission_contexts_;
  }

 private:
  friend class PermissionManagerTest;
  friend class ::GeolocationPermissionContextDelegateTests;
  friend class ::SubscriptionInterceptingPermissionManager;

  // The `PendingRequestLocalId` will be unique within the `PermissionManager`
  // instance, thus within a `BrowserContext`, which overachieves the
  // requirement from `PermissionRequestID` that the `RequestLocalId` be unique
  // within each frame.
  class PendingRequest;
  using PendingRequestLocalId = PermissionRequestID::RequestLocalId;
  using PendingRequestsMap =
      base::IDMap<std::unique_ptr<PendingRequest>, PendingRequestLocalId>;

  class PermissionResponseCallback;

  struct Subscription;
  using SubscriptionsMap =
      base::IDMap<std::unique_ptr<Subscription>, SubscriptionId>;
  using SubscriptionTypeCounts = base::flat_map<ContentSettingsType, size_t>;

  PermissionContextBase* GetPermissionContext(ContentSettingsType type);

  // content::PermissionControllerDelegate implementation.
  void RequestPermission(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void RequestPermissions(
      const std::vector<blink::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void RequestPermissionsInternal(
      const std::vector<blink::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback);
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
  content::PermissionResult GetPermissionResultForCurrentDocument(
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
  bool IsPermissionOverridable(
      blink::PermissionType permission,
      const absl::optional<url::Origin>& origin) override;
  SubscriptionId SubscribePermissionStatusChange(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;

  // Called when a permission was decided for a given PendingRequest. The
  // PendingRequest is identified by its |request_local_id| and the permission
  // is identified by its |permission_id|. If the PendingRequest contains more
  // than one permission, it will wait for the remaining permissions to be
  // resolved. When all the permissions have been resolved, the PendingRequest's
  // callback is run.
  void OnPermissionsRequestResponseStatus(
      PendingRequestLocalId request_local_id,
      int permission_id,
      ContentSetting status);

  // permissions::Observer:
  void OnPermissionChanged(const ContentSettingsPattern& primary_pattern,
                           const ContentSettingsPattern& secondary_pattern,
                           ContentSettingsTypeSet content_type_set) override;

  // Only one of |render_process_host| and |render_frame_host| should be set,
  // or neither. RenderProcessHost will be inferred from |render_frame_host|.
  PermissionResult GetPermissionStatusInternal(
      ContentSettingsType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  raw_ptr<content::BrowserContext> browser_context_;

  PendingRequestsMap pending_requests_;
  PendingRequestLocalId::Generator request_local_id_generator_;

  SubscriptionsMap subscriptions_;
  SubscriptionId::Generator subscription_id_generator_;

  // Tracks the number of Subscriptions in |subscriptions_| which have a
  // certain ContentSettingsType. An entry for a given ContentSettingsType key
  // is added on first use and never removed. This is done to utilize the
  // flat_map's efficiency in accessing/editing items and minimize the use of
  // the unefficient addition/removal of items.
  SubscriptionTypeCounts subscription_type_counts_;

  PermissionContextMap permission_contexts_;

  bool is_shutting_down_ = false;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_MANAGER_H_
