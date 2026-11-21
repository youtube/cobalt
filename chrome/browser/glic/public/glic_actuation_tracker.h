// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_ACTUATION_TRACKER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_ACTUATION_TRACKER_H_

#include "base/callback_list.h"

namespace content {
class WebContents;
}

namespace performance_manager {
enum class GlicActuationState;
}

namespace glic {

using GlicActuationState = performance_manager::GlicActuationState;

// Tracks the Glic actuation state across all WebContents.
// Used by Performance Manager's PageLiveStateDecoratorHelper to adjust Priority
// Voting.
class GlicActuationTracker {
 public:
  using Callback =
      base::RepeatingCallback<void(content::WebContents*, GlicActuationState)>;

  static GlicActuationTracker* GetInstance();

  GlicActuationTracker();
  ~GlicActuationTracker();
  GlicActuationTracker(const GlicActuationTracker&) = delete;
  GlicActuationTracker& operator=(const GlicActuationTracker&) = delete;

  base::CallbackListSubscription AddActuatingChangedCallback(Callback observer);

  void NotifyActuatingChanged(content::WebContents* web_contents,
                              GlicActuationState state);

 private:
  base::RepeatingCallbackList<void(content::WebContents*, GlicActuationState)>

      actuating_changed_callbacks_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_ACTUATION_TRACKER_H_
