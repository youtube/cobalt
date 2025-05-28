// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"

#include "base/feature_list.h"
#include "nearby_share_features.h"

namespace features {

// Enables Quick Share branding.
BASE_FEATURE(kIsNameEnabled, "IsNameEnabled", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables UI features for Self Share to allow seamless sharing between a user's
// own devices.
BASE_FEATURE(kNearbySharingSelfShare,
             "NearbySharingSelfShare",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables contact restriction when not in high-visibility mode.
BASE_FEATURE(kNearbySharingRestrictToContacts,
             "NearbySharingRestrictToContacts",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNameEnabled() {
  return base::FeatureList::IsEnabled(kIsNameEnabled);
}

bool IsRestrictToContactsEnabled() {
  return base::FeatureList::IsEnabled(kNearbySharingRestrictToContacts);
}

}  // namespace features
