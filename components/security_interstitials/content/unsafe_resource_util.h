// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UNSAFE_RESOURCE_UTIL_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UNSAFE_RESOURCE_UTIL_H_

#include "components/security_interstitials/core/unsafe_resource.h"

namespace content {
class NavigationEntry;
class WebContents;
}  // namespace content

namespace security_interstitials {

// Returns the NavigationEntry for |resource| (for a main frame hit) or
// for the page which contains this resource (for a subresource hit).
// This method must only be called while the UnsafeResource is still
// "valid".
// I.e,
//   For MainPageLoadBlocked resources, it must not be called if the load
//   was aborted (going back or replaced with a different navigation),
//   or resumed (proceeded through warning or matched whitelist).
//   For non-MainPageLoadBlocked resources, it must not be called if any
//   other navigation has committed (whether by going back or unrelated
//   navigations), though a pending navigation is okay.
content::NavigationEntry* GetNavigationEntryForResource(
    const UnsafeResource& resource);

// Returns the WebContents associated with the given |resource| based on the
// frame or document for which it was created. If that frame/document no longer
// exists, this returns nullptr.
content::WebContents* GetWebContentsForResource(const UnsafeResource& resource);

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UNSAFE_RESOURCE_UTIL_H_
