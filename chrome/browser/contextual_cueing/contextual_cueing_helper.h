// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class OptimizationGuideKeyedService;

namespace tabs {
class GlicNudgeController;
}  // namespace tabs

namespace contextual_cueing {

class ContextualCueingService;
class ScopedNudgeDecisionRecorder;

class ContextualCueingHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContextualCueingHelper> {
 public:
  // Creates ContextualCueingHelper and attaches it the `web_contents` if
  // contextual cueing is enabled.
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ContextualCueingHelper(const ContextualCueingHelper&) = delete;
  ContextualCueingHelper& operator=(const ContextualCueingHelper&) = delete;
  ~ContextualCueingHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;

  tabs::GlicNudgeController* GetGlicNudgeController();

 private:
  ContextualCueingHelper(content::WebContents* contents,
                         OptimizationGuideKeyedService* ogks,
                         ContextualCueingService* ccs);

  // Called when optimization guide metadata is received.
  void OnOptimizationGuideCueingMetadata(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  void OnCueingDecision(
      std::unique_ptr<ScopedNudgeDecisionRecorder> decision_recorder,
      std::string cue_label);

  bool IsBrowserBlockingNudges(ScopedNudgeDecisionRecorder* recorder);

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<ContextualCueingService> contextual_cueing_service_ = nullptr;

  base::WeakPtrFactory<ContextualCueingHelper> weak_ptr_factory_{this};

  friend WebContentsUserData<ContextualCueingHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_
