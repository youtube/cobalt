// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_page_data.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_enabling.h"
#endif

namespace contextual_cueing {

class ScopedNudgeDecisionRecorder {
 public:
  ScopedNudgeDecisionRecorder(
      optimization_guide::proto::OptimizationType optimization_type,
      ukm::SourceId source_id)
      : optimization_type_(optimization_type), source_id_(source_id) {}
  ~ScopedNudgeDecisionRecorder() {
    CHECK_NE(nudge_decision_, NudgeDecision::kUnknown);
    base::UmaHistogramEnumeration(
        "ContextualCueing.NudgeDecision." +
            optimization_guide::GetStringNameForOptimizationType(
                optimization_type_),
        nudge_decision_);

    auto* ukm_recorder = ukm::UkmRecorder::Get();
    ukm::builders::ContextualCueing_NudgeDecision(source_id_)
        .SetNudgeDecision(static_cast<int64_t>(nudge_decision_))
        .SetOptimizationType(optimization_type_)
        .Record(ukm_recorder->Get());
  }

  void set_nudge_decision(NudgeDecision nudge_decision) {
    nudge_decision_ = nudge_decision;
  }

  NudgeDecision nudge_decision() const { return nudge_decision_; }

 private:
  optimization_guide::proto::OptimizationType optimization_type_;
  ukm::SourceId source_id_;
  NudgeDecision nudge_decision_ = NudgeDecision::kUnknown;
};

ContextualCueingHelper::ContextualCueingHelper(
    content::WebContents* web_contents,
    OptimizationGuideKeyedService* ogks,
    ContextualCueingService* ccs)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualCueingHelper>(*web_contents),
      optimization_guide_keyed_service_(ogks),
      contextual_cueing_service_(ccs) {
  // LINT.IfChange(OptType)
  optimization_guide_keyed_service_->RegisterOptimizationTypes(
      {optimization_guide::proto::GLIC_CONTEXTUAL_CUEING});
  // LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_cueing/histograms.xml:OptType)
}

ContextualCueingHelper::~ContextualCueingHelper() = default;

tabs::GlicNudgeController* ContextualCueingHelper::GetGlicNudgeController() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser) {
    return nullptr;
  }
  return browser->browser_window_features()->glic_nudge_controller();
}

void ContextualCueingHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || navigation_handle->IsErrorPage() ||
      !navigation_handle->HasCommitted() ||
      !navigation_handle->ShouldUpdateHistory()) {
    return;
  }
  if (PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                               ui::PAGE_TRANSITION_RELOAD)) {
    return;
  }
  contextual_cueing_service_->ReportPageLoad();
  auto* glic_nudge_controller = GetGlicNudgeController();
  if (glic_nudge_controller) {
    glic_nudge_controller->UpdateNudgeLabel(web_contents(), std::string(),
                                            base::DoNothing());
  }
}

void ContextualCueingHelper::PrimaryMainDocumentElementAvailable() {
  auto* glic_nudge_controller = GetGlicNudgeController();
  if (!glic_nudge_controller) {
    return;
  }
  // Determine if server data indicates a nudge should be shown.
  optimization_guide_keyed_service_->CanApplyOptimization(
      web_contents()->GetLastCommittedURL(),
      optimization_guide::proto::GLIC_CONTEXTUAL_CUEING,
      base::BindOnce(&ContextualCueingHelper::OnOptimizationGuideCueingMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContextualCueingHelper::OnOptimizationGuideCueingMetadata(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  std::unique_ptr<ScopedNudgeDecisionRecorder> recorder =
      std::make_unique<ScopedNudgeDecisionRecorder>(
          optimization_guide::proto::GLIC_CONTEXTUAL_CUEING,
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue ||
      metadata.empty()) {
    recorder->set_nudge_decision(NudgeDecision::kServerDataUnavailable);
    return;
  }
  auto parsed = metadata.ParsedMetadata<
      optimization_guide::proto::GlicContextualCueingMetadata>();
  if (!parsed) {
    recorder->set_nudge_decision(NudgeDecision::kServerDataMalformed);
    return;
  }

  ContextualCueingPageData::CreateForPage(
      web_contents()->GetPrimaryPage(), std::move(*parsed),
      base::BindOnce(&ContextualCueingHelper::OnCueingDecision,
                     weak_ptr_factory_.GetWeakPtr(), std::move(recorder)));
}

bool ContextualCueingHelper::IsBrowserBlockingNudges(
    ScopedNudgeDecisionRecorder* recorder) {
  // Determine if the Browser is available for nudging.
  if (!web_contents()) {
    return false;
  }

  auto* tab_interface = tabs::TabInterface::GetFromContents(web_contents());
  if (!tab_interface) {
    return false;
  }

  auto* browser_window_interface = tab_interface->GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return false;
  }

  auto* user_education_interface =
      browser_window_interface->GetUserEducationInterface();
  if (!user_education_interface) {
    return false;
  }

  if (user_education_interface->IsFeaturePromoActive(
          feature_engagement::kIPHGlicPromoFeature)) {
    recorder->set_nudge_decision(NudgeDecision::kNudgeNotShownIPH);
    return true;
  }

  return false;
}

void ContextualCueingHelper::OnCueingDecision(
    std::unique_ptr<ScopedNudgeDecisionRecorder> decision_recorder,
    std::string cue_label) {
  CHECK_EQ(NudgeDecision::kUnknown, decision_recorder->nudge_decision());
  if (ContextualCueingPageData::GetForPage(web_contents()->GetPrimaryPage())) {
    ContextualCueingPageData::DeleteForPage(web_contents()->GetPrimaryPage());
  }

  if (cue_label.empty()) {
    decision_recorder->set_nudge_decision(
        NudgeDecision::kClientConditionsUnmet);
    return;
  }

  if (IsBrowserBlockingNudges(decision_recorder.get())) {
    return;
  }

  const GURL& url = web_contents()->GetLastCommittedURL();
  auto can_show_decision = contextual_cueing_service_->CanShowNudge(url);
  decision_recorder->set_nudge_decision(can_show_decision);
  if (can_show_decision != NudgeDecision::kSuccess) {
    return;
  }

  GetGlicNudgeController()->UpdateNudgeLabel(
      web_contents(), cue_label,
      base::BindRepeating(
          &ContextualCueingService::OnNudgeActivity,
          contextual_cueing_service_->GetWeakPtr(), url,
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId()));
}

// static
void ContextualCueingHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(contextual_cueing::kContextualCueing)) {
    return;
  }

#if BUILDFLAG(ENABLE_GLIC)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!glic::GlicEnabling::IsProfileEligible(profile)) {
    return;
  }

  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_keyed_service ||
      !optimization_guide_keyed_service->GetModelExecutionFeaturesController()
           ->ShouldModelExecutionBeAllowedForUser()) {
    return;
  }

  auto* contextual_cueing_service =
      ContextualCueingServiceFactory::GetForProfile(profile);
  if (!contextual_cueing_service) {
    return;
  }

  ContextualCueingHelper::CreateForWebContents(web_contents,
                                               optimization_guide_keyed_service,
                                               contextual_cueing_service);
#endif  // BUILDFLAG(ENABLE_GLIC)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualCueingHelper);

}  // namespace contextual_cueing
