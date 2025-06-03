// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_attempt_impl.h"

#include "base/metrics/crc32.h"
#include "base/metrics/histogram_functions.h"
#include "base/state_transitions.h"
#include "base/strings/strcat.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_config.h"
#include "content/public/browser/preloading.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {

namespace {

void DCHECKTriggeringOutcomeTransitions(PreloadingTriggeringOutcome old_state,
                                        PreloadingTriggeringOutcome new_state) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<
      base::StateTransitions<PreloadingTriggeringOutcome>>
      allowed_transitions(base::StateTransitions<PreloadingTriggeringOutcome>({
          {PreloadingTriggeringOutcome::kUnspecified,
           {PreloadingTriggeringOutcome::kDuplicate,
            PreloadingTriggeringOutcome::kRunning,
            PreloadingTriggeringOutcome::kReady,
            PreloadingTriggeringOutcome::kSuccess,
            PreloadingTriggeringOutcome::kFailure,
            PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown,
            PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender,
            PreloadingTriggeringOutcome::kTriggeredButPending,
            PreloadingTriggeringOutcome::kNoOp}},

          {PreloadingTriggeringOutcome::kDuplicate, {}},

          {PreloadingTriggeringOutcome::kRunning,
           {PreloadingTriggeringOutcome::kReady,
            PreloadingTriggeringOutcome::kFailure,
            PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender}},

          // It can be possible that the preloading attempt may end up failing
          // after being ready to use, for cases where we have to cancel the
          // attempt for performance and security reasons.
          // The transition of kReady to kReady occurs when the main frame
          // navigation is completed in a preloaded page.
          {PreloadingTriggeringOutcome::kReady,
           {PreloadingTriggeringOutcome::kReady,
            PreloadingTriggeringOutcome::kSuccess,
            PreloadingTriggeringOutcome::kFailure,
            PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender}},

          {PreloadingTriggeringOutcome::kSuccess, {}},

          {PreloadingTriggeringOutcome::kFailure, {}},

          {PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown, {}},

          {PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender,
           {PreloadingTriggeringOutcome::kFailure}},

          {PreloadingTriggeringOutcome::kTriggeredButPending,
           {PreloadingTriggeringOutcome::kRunning,
            PreloadingTriggeringOutcome::kFailure}},

          {PreloadingTriggeringOutcome::kNoOp, {}},
      }));
  DCHECK_STATE_TRANSITION(allowed_transitions,
                          /*old_state=*/old_state,
                          /*new_state=*/new_state);
#endif  // DCHECK_IS_ON()
}

}  // namespace

void PreloadingAttemptImpl::SetEligibility(PreloadingEligibility eligibility) {
  // Ensure that eligiblity is only set once and that it's set before the
  // holdback status and the triggering outcome.
  CHECK_EQ(eligibility_, PreloadingEligibility::kUnspecified);
  CHECK_EQ(holdback_status_, PreloadingHoldbackStatus::kUnspecified);
  CHECK_EQ(triggering_outcome_, PreloadingTriggeringOutcome::kUnspecified);
  CHECK_NE(eligibility, PreloadingEligibility::kUnspecified);
  eligibility_ = eligibility;
}

// TODO(crbug.com/1464836): most call sites of this should be removed, as
// PreloadingConfig should subsume most feature-specific holdbacks that exist
// today. Some cases can remain as specific overrides of the PreloadingConfig
// logic, e.g. if DevTools is open, or for features that are still launching and
// thus have their own separate holdback feature while they ramp up.
void PreloadingAttemptImpl::SetHoldbackStatus(
    PreloadingHoldbackStatus holdback_status) {
  // Ensure that the holdback status is only set once and that it's set for
  // eligible attempts and before the triggering outcome.
  CHECK_EQ(eligibility_, PreloadingEligibility::kEligible);
  CHECK_EQ(holdback_status_, PreloadingHoldbackStatus::kUnspecified);
  CHECK_EQ(triggering_outcome_, PreloadingTriggeringOutcome::kUnspecified);
  CHECK_NE(holdback_status, PreloadingHoldbackStatus::kUnspecified);
  holdback_status_ = holdback_status;
}

bool PreloadingAttemptImpl::ShouldHoldback() {
  CHECK_EQ(eligibility_, PreloadingEligibility::kEligible);
  if (holdback_status_ != PreloadingHoldbackStatus::kUnspecified) {
    // The holdback status has already been determined, just use that value.
    return holdback_status_ == PreloadingHoldbackStatus::kHoldback;
  }

  bool should_holdback = PreloadingConfig::GetInstance().ShouldHoldback(
      preloading_type_, predictor_type_);
  if (should_holdback) {
    holdback_status_ = PreloadingHoldbackStatus::kHoldback;
  } else {
    holdback_status_ = PreloadingHoldbackStatus::kAllowed;
  }
  return should_holdback;
}

void PreloadingAttemptImpl::SetTriggeringOutcome(
    PreloadingTriggeringOutcome triggering_outcome) {
  // Ensure that the triggering outcome is only set for eligible and
  // non-holdback attempts.
  CHECK_EQ(eligibility_, PreloadingEligibility::kEligible);
  CHECK_EQ(holdback_status_, PreloadingHoldbackStatus::kAllowed);
  // Check that we do the correct transition before setting
  // `triggering_outcome_`.
  DCHECKTriggeringOutcomeTransitions(/*old_state=*/triggering_outcome_,
                                     /*new_state=*/triggering_outcome);
  triggering_outcome_ = triggering_outcome;

  // Set the ready time, if this attempt was not already ready.
  switch (triggering_outcome_) {
    // Currently only Prefetch, Prerender and NoStatePrefetch have a ready
    // state. Other preloading features do not track the entire progress of the
    // preloading attempt, where
    // `PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown` is set for
    // those other features.
    case PreloadingTriggeringOutcome::kReady:
      CHECK(preloading_type_ == PreloadingType::kPrefetch ||
            preloading_type_ == PreloadingType::kPrerender ||
            preloading_type_ == PreloadingType::kNoStatePrefetch ||
            preloading_type_ == PreloadingType::kLinkPreview);
      if (!ready_time_) {
        ready_time_ = elapsed_timer_.Elapsed();
      }
      break;
    default:
      break;
  }
}

void PreloadingAttemptImpl::SetFailureReason(PreloadingFailureReason reason) {
  // Ensure that the failure reason is only set once and is only set for
  // eligible and non-holdback attempts.
  CHECK_EQ(eligibility_, PreloadingEligibility::kEligible);
  CHECK_EQ(holdback_status_, PreloadingHoldbackStatus::kAllowed);
  CHECK_EQ(failure_reason_, PreloadingFailureReason::kUnspecified);
  CHECK_NE(reason, PreloadingFailureReason::kUnspecified);

  // It could be possible that the TriggeringOutcome is already kFailure, when
  // we try to set FailureReason after setting TriggeringOutcome to kFailure.
  if (triggering_outcome_ != PreloadingTriggeringOutcome::kFailure)
    SetTriggeringOutcome(PreloadingTriggeringOutcome::kFailure);
  failure_reason_ = reason;
}

base::WeakPtr<PreloadingAttempt> PreloadingAttemptImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

PreloadingAttemptImpl::PreloadingAttemptImpl(
    PreloadingPredictor predictor,
    PreloadingType preloading_type,
    ukm::SourceId triggered_primary_page_source_id,
    base::RepeatingCallback<bool(const GURL&)> url_match_predicate,
    uint32_t sampling_seed)
    : predictor_type_(predictor),
      preloading_type_(preloading_type),
      triggered_primary_page_source_id_(triggered_primary_page_source_id),
      url_match_predicate_(std::move(url_match_predicate)),
      sampling_seed_(sampling_seed) {}

PreloadingAttemptImpl::~PreloadingAttemptImpl() = default;

void PreloadingAttemptImpl::RecordPreloadingAttemptMetrics(
    ukm::SourceId navigated_page_source_id) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();

  // Ensure that when the `triggering_outcome_` is kSuccess, then the
  // accurate_triggering should be true.
  if (triggering_outcome_ == PreloadingTriggeringOutcome::kSuccess) {
    // TODO(https://crbug.com/1431055): Fix PreloadingAttempt for Prefetching in
    // a different WebContents. It is allowed to activate a prefetched result in
    // another WebContents instance, and the WebContents that stores `this`
    // instance does not have the opportunity to set the
    // `is_accurate_triggering_` flag to true in this case.
    if (preloading_type_ != PreloadingType::kPrefetch) {
      CHECK(is_accurate_triggering_)
          << "TriggeringOutcome set to kSuccess without correct prediction\n";
    }
  }

  // Always record UMA, regardless of sampling.
  RecordPreloadingAttemptUMA();

  // Check if the preloading attempt is sampled in.
  // We prefer to use the UKM source ID of the triggering page for determining
  // sampling, so that all preloading attempts from a given (preloading_type,
  // predictor) for the same page are included (or not) together. If there is
  // no source for the triggering page, fallback to the navigated-to page.
  ukm::SourceId sampling_source = triggered_primary_page_source_id_;
  if (sampling_source == ukm::kInvalidSourceId) {
    sampling_source = navigated_page_source_id;
  }
  if (sampling_source == ukm::kInvalidSourceId) {
    // There is no valid UKM source, so there is nothing to log.
    return;
  }

  PreloadingConfig& config = PreloadingConfig::GetInstance();
  uint32_t sampled_num = sampling_seed_;
  sampled_num =
      base::Crc32(sampled_num, &sampling_source, sizeof(sampling_source));

  double sampling_likelihood =
      config.SamplingLikelihood(preloading_type_, predictor_type_);
  if (sampled_num >
      sampling_likelihood * std::numeric_limits<uint32_t>::max()) {
    // PreloadingAttempt is sampled out.
    return;
  }

  // Turn sampling_likelihood into an int64_t for UKM logging. Multiply by one
  // million to preserve accuracy.
  int64_t sampling_likelihood_per_million =
      static_cast<int64_t>(1'000'000 * sampling_likelihood);

  if (navigated_page_source_id != ukm::kInvalidSourceId) {
    ukm::builders::Preloading_Attempt builder(navigated_page_source_id);
    builder.SetPreloadingType(static_cast<int64_t>(preloading_type_))
        .SetPreloadingPredictor(predictor_type_.ukm_value())
        .SetEligibility(static_cast<int64_t>(eligibility_))
        .SetHoldbackStatus(static_cast<int64_t>(holdback_status_))
        .SetTriggeringOutcome(static_cast<int64_t>(triggering_outcome_))
        .SetFailureReason(static_cast<int64_t>(failure_reason_))
        .SetAccurateTriggering(is_accurate_triggering_)
        .SetSamplingLikelihood(sampling_likelihood_per_million);
    if (time_to_next_navigation_) {
      builder.SetTimeToNextNavigation(ukm::GetExponentialBucketMinForCounts1000(
          time_to_next_navigation_->InMilliseconds()));
    }
    if (ready_time_) {
      builder.SetReadyTime(ukm::GetExponentialBucketMinForCounts1000(
          ready_time_->InMilliseconds()));
    }
    if (eagerness_) {
      builder.SetSpeculationEagerness(static_cast<int64_t>(eagerness_.value()));
    }
    if (service_worker_registered_check_) {
      builder.SetPrefetchServiceWorkerRegisteredCheck(
          static_cast<int64_t>(service_worker_registered_check_.value()));
    }
    if (service_worker_registered_check_duration_) {
      builder.SetPrefetchServiceWorkerRegisteredForURLCheckDuration(
          ukm::GetExponentialBucketMin(
              service_worker_registered_check_duration_.value()
                  .InMicroseconds(),
              kServiceWorkerRegisteredCheckDurationBucketSpacing));
    }
    builder.Record(ukm_recorder);
  }

  if (triggered_primary_page_source_id_ != ukm::kInvalidSourceId) {
    ukm::builders::Preloading_Attempt_PreviousPrimaryPage builder(
        triggered_primary_page_source_id_);
    builder.SetPreloadingType(static_cast<int64_t>(preloading_type_))
        .SetPreloadingPredictor(predictor_type_.ukm_value())
        .SetEligibility(static_cast<int64_t>(eligibility_))
        .SetHoldbackStatus(static_cast<int64_t>(holdback_status_))
        .SetTriggeringOutcome(static_cast<int64_t>(triggering_outcome_))
        .SetFailureReason(static_cast<int64_t>(failure_reason_))
        .SetAccurateTriggering(is_accurate_triggering_)
        .SetSamplingLikelihood(sampling_likelihood_per_million);
    if (time_to_next_navigation_) {
      builder.SetTimeToNextNavigation(ukm::GetExponentialBucketMinForCounts1000(
          time_to_next_navigation_->InMilliseconds()));
    }
    if (ready_time_) {
      builder.SetReadyTime(ukm::GetExponentialBucketMinForCounts1000(
          ready_time_->InMilliseconds()));
    }
    if (eagerness_) {
      builder.SetSpeculationEagerness(static_cast<int64_t>(eagerness_.value()));
    }
    if (service_worker_registered_check_) {
      builder.SetPrefetchServiceWorkerRegisteredCheck(
          static_cast<int64_t>(service_worker_registered_check_.value()));
    }
    if (service_worker_registered_check_duration_) {
      builder.SetPrefetchServiceWorkerRegisteredForURLCheckDuration(
          ukm::GetExponentialBucketMin(
              service_worker_registered_check_duration_.value()
                  .InMicroseconds(),
              kServiceWorkerRegisteredCheckDurationBucketSpacing));
    }
    builder.Record(ukm_recorder);
  }
}

void PreloadingAttemptImpl::RecordPreloadingAttemptUMA() {
  // Records the triggering outcome enum. This can be used to:
  // 1. Track the number of attempts;
  // 2. Track the attempts' rates of various terminal status (i.e. success
  // rate).
  const auto uma_triggering_outcome_histogram =
      base::StrCat({"Preloading.", PreloadingTypeToString(preloading_type_),
                    ".Attempt.", predictor_type_.name(), ".TriggeringOutcome"});
  base::UmaHistogramEnumeration(std::move(uma_triggering_outcome_histogram),
                                triggering_outcome_);
}

void PreloadingAttemptImpl::SetIsAccurateTriggering(const GURL& navigated_url) {
  CHECK(url_match_predicate_);

  // `PreloadingAttemptImpl::SetIsAccurateTriggering` is called during
  // `WCO::DidStartNavigation`.
  if (!time_to_next_navigation_) {
    time_to_next_navigation_ = elapsed_timer_.Elapsed();
  }

  // Use the predicate to match the URLs as the matching logic varies for each
  // predictor.
  is_accurate_triggering_ |= url_match_predicate_.Run(navigated_url);
}

void PreloadingAttemptImpl::SetSpeculationEagerness(
    blink::mojom::SpeculationEagerness eagerness) {
  CHECK(predictor_type_.ukm_value() ==
            content_preloading_predictor::kSpeculationRules.ukm_value() ||
        predictor_type_.ukm_value() ==
            content_preloading_predictor::kSpeculationRulesFromIsolatedWorld
                .ukm_value())
      << "predictor_type_: " << predictor_type_.name()
      << " (ukm_value = " << predictor_type_.ukm_value() << ")";
  eagerness_ = eagerness;
}

void PreloadingAttemptImpl::SetServiceWorkerRegisteredCheck(
    ServiceWorkerRegisteredCheck check) {
  service_worker_registered_check_ = check;
}

void PreloadingAttemptImpl::SetServiceWorkerRegisteredCheckDuration(
    base::TimeDelta duration) {
  service_worker_registered_check_duration_ = duration;
}

// Used for StateTransitions matching.
std::ostream& operator<<(std::ostream& os,
                         const PreloadingTriggeringOutcome& outcome) {
  switch (outcome) {
    case PreloadingTriggeringOutcome::kUnspecified:
      os << "Unspecified";
      break;
    case PreloadingTriggeringOutcome::kDuplicate:
      os << "Duplicate";
      break;
    case PreloadingTriggeringOutcome::kRunning:
      os << "Running";
      break;
    case PreloadingTriggeringOutcome::kReady:
      os << "Ready";
      break;
    case PreloadingTriggeringOutcome::kSuccess:
      os << "Success";
      break;
    case PreloadingTriggeringOutcome::kFailure:
      os << "Failure";
      break;
    case PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown:
      os << "TriggeredButOutcomeUnknown";
      break;
    case PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender:
      os << "TriggeredButUpgradedToPrerender";
      break;
    case PreloadingTriggeringOutcome::kTriggeredButPending:
      os << "TriggeredButPending";
      break;
    case PreloadingTriggeringOutcome::kNoOp:
      os << "NoOp";
      break;
  }
  return os;
}

}  // namespace content
