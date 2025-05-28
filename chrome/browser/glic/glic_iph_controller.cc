// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_iph_controller.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/browser/web_contents.h"

namespace glic {

namespace {

// Use the session start grace period as a guide for how often we could check to
// see if the promo should show. As a side effect, will not attempt to show the
// promo during a grace period that occurs at browser process start. Note that
// this is typically an anti-pattern - "wait until after grace period then show"
// immediately" is not a substitute for "I'd like to show this to everyone at
// startup but can't", however in this case since we will be checking
// periodically anyway until/unless we're told the promo can't be shown, it's as
// good as any of an interval to use.
base::TimeDelta GetPromoCheckInterval() {
  return user_education::features::GetSessionStartGracePeriod() +
         base::Minutes(1);
}

}  // namespace

GlicIphController::GlicIphController(BrowserWindowInterface* browser_window)
    : window_(*browser_window) {
  show_timer_.Start(FROM_HERE, GetPromoCheckInterval(),
                    base::BindRepeating(&GlicIphController::MaybeShowPromo,
                                        weak_ptr_factory_.GetWeakPtr()));
}

GlicIphController::~GlicIphController() = default;

void GlicIphController::MaybeShowPromo() {
  // Determine that there is a valid active tab we could show the promo for.
  auto* const tab = window_->GetActiveTabInterface();
  auto* const contents = tab->GetContents();
  if (!contents->GetURL().SchemeIsHTTPOrHTTPS() ||
      !contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    return;
  }

  // Attempt to show the promo. Results will be sent to `OnShowPromoResult()`.
  //
  // Note that under User Education Experience 2.0 this could interrupt the
  // user at an inopportune time. However in 2.5 there will be much less
  // chance of an interruption because promos are paused when the user is
  // interacting with the browser.
  user_education::FeaturePromoParams params(
      feature_engagement::kIPHGlicPromoFeature);
  params.show_promo_result_callback = base::BindOnce(
      &GlicIphController::OnShowPromoResult, weak_ptr_factory_.GetWeakPtr());
  window_->GetUserEducationInterface()->MaybeShowFeaturePromo(
      std::move(params));
}

void GlicIphController::OnShowPromoResult(
    user_education::FeaturePromoResult result) {
  // If there's no chance a promo could be shown in this browser window, stop
  // trying to check.
  if (result.is_blocked_this_instance()) {
    show_timer_.Stop();
  }
}

}  // namespace glic
