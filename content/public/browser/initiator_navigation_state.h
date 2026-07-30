// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INITIATOR_NAVIGATION_STATE_H_
#define CONTENT_PUBLIC_BROWSER_INITIATOR_NAVIGATION_STATE_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

// A handle to a record of the state of a navigation initiator. This is created
// by content/ and must be provided to LoadURLParams when starting a
// renderer-initiated navigation.
class CONTENT_EXPORT InitiatorNavigationState
    : public base::RefCounted<InitiatorNavigationState> {
 protected:
  friend class base::RefCounted<InitiatorNavigationState>;
  InitiatorNavigationState() = default;
  virtual ~InitiatorNavigationState() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INITIATOR_NAVIGATION_STATE_H_
