// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/form_submission_tracker_util.h"

#include "base/check.h"
#include "components/password_manager/core/browser/form_submission_observer.h"

namespace password_manager {

void NotifyDidNavigateMainFrame(bool is_renderer_initiated,
                                ui::PageTransition transition,
                                bool was_initiated_by_link_click,
                                FormSubmissionObserver* observer) {
  if (!observer)
    return;

  // Password manager is interested in
  // - form submission navigations,
  // - any JavaScript initiated navigations besides history navigations, because
  //   many form submissions are done with JavaScript.
  // Password manager is not interested in
  // - browser initiated navigations (e.g. reload, bookmark click),
  // - hyperlink navigations.
  // - session history navigations
  bool form_may_be_submitted =
      is_renderer_initiated &&
      !(transition & ui::PAGE_TRANSITION_FORWARD_BACK) &&
      (ui::PageTransitionCoreTypeIs(transition,
                                    ui::PAGE_TRANSITION_FORM_SUBMIT) ||
       !was_initiated_by_link_click);

  observer->DidNavigateMainFrame(form_may_be_submitted);
}

}  // namespace password_manager
