// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_UNSAFE_RESOURCE_CONTAINER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_UNSAFE_RESOURCE_CONTAINER_H_

#include "components/security_interstitials/core/unsafe_resource.h"
#import "ios/components/security_interstitials/safe_browsing/pending_unsafe_resource_storage.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

namespace web {
class NavigationItem;
}

// Helper object that holds pending UnsafeResources for a WebState.
// UnsafeResources are copied and added when a navigation are detected to be
// unsafe, then fetched to populate the error page shown to the user after a
// navigation fails.
class SafeBrowsingUnsafeResourceContainer
    : public web::WebStateUserData<SafeBrowsingUnsafeResourceContainer> {
 public:
  // SafeBrowsingUnsafeResourceContainer is move-only.
  SafeBrowsingUnsafeResourceContainer(
      SafeBrowsingUnsafeResourceContainer&& other);
  SafeBrowsingUnsafeResourceContainer& operator=(
      SafeBrowsingUnsafeResourceContainer&& other);
  ~SafeBrowsingUnsafeResourceContainer() override;

  // Stores a copy of `resource` in the container.  An allow list decision must
  // be pending for `resource` before it is added to the container.
  void StoreMainFrameUnsafeResource(
      const security_interstitials::UnsafeResource& resource);
  void StoreSubFrameUnsafeResource(
      const security_interstitials::UnsafeResource& resource,
      web::NavigationItem* main_frame_item);

  // Returns the pending main frame UnsafeResource, or null if one has not been
  // stored.
  const security_interstitials::UnsafeResource* GetMainFrameUnsafeResource()
      const;

  // Returns the pending sub frame UnsafeResource for `item`, or null if one has
  // not been stored.  `item` must be non-null.
  const security_interstitials::UnsafeResource* GetSubFrameUnsafeResource(
      web::NavigationItem* item) const;

 private:
  explicit SafeBrowsingUnsafeResourceContainer(web::WebState* web_state);
  friend class web::WebStateUserData<SafeBrowsingUnsafeResourceContainer>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // The WebState whose unsafe resources are managed by this container.
  web::WebState* web_state_ = nullptr;
  // The pending UnsafeResource for the main frame navigation.
  PendingUnsafeResourceStorage main_frame_unsafe_resource_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_UNSAFE_RESOURCE_CONTAINER_H_
