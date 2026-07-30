// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_actuation_tracker.h"

#include "base/no_destructor.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "content/public/browser/web_contents.h"

namespace glic {

// static
GlicActuationTracker* GlicActuationTracker::GetInstance() {
  static base::NoDestructor<GlicActuationTracker> instance;
  return instance.get();
}

GlicActuationTracker::GlicActuationTracker() = default;

GlicActuationTracker::~GlicActuationTracker() = default;

base::CallbackListSubscription
GlicActuationTracker::AddActuatingChangedCallback(Callback callback) {
  return actuating_changed_callbacks_.Add(std::move(callback));
}

void GlicActuationTracker::NotifyActuatingChanged(
    content::WebContents* web_contents,
    GlicActuationState state) {
  actuating_changed_callbacks_.Notify(web_contents, state);
}

}  // namespace glic
