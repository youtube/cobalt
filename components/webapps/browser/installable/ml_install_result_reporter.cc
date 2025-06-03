// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/ml_install_result_reporter.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webapps {

const base::FeatureParam<double> kGuardrailResultReportProb(
    &webapps::features::kWebAppsEnableMLModelForPromotion,
    "guardrail_report_prob",
    0);

const base::FeatureParam<double> kModelDeclineUserDeclineReportProb(
    &webapps::features::kWebAppsEnableMLModelForPromotion,
    "model_and_user_decline_report_prob",
    0);

MlInstallResultReporter::MlInstallResultReporter(
    base::WeakPtr<AppBannerManager> app_banner_manager,
    segmentation_platform::TrainingRequestId training_request,
    std::string ml_output_label,
    const GURL& manifest_id,
    bool ml_promotion_blocked_by_guardrail)
    : app_banner_manager_(app_banner_manager),
      training_request_(training_request),
      ml_output_label_(ml_output_label),
      manifest_id_(manifest_id),
      ml_promotion_blocked_by_guardrail_(ml_promotion_blocked_by_guardrail) {
  CHECK(app_banner_manager_);
  CHECK(manifest_id_.is_valid());
}

MlInstallResultReporter::~MlInstallResultReporter() {
  if (ml_promotion_blocked_by_guardrail_) {
    ReportResultInternal(install_source_attached_,
                         MlInstallResponse::kBlockedGuardrails);
  } else {
    ReportResultInternal(install_source_attached_,
                         MlInstallResponse::kReporterDestroyed);
  }
}

void MlInstallResultReporter::OnInstallTrackerAttached(
    WebappInstallSource install_source) {
  // If attached to an install tracker, destruction of this tracker means that
  // the user ignored the install UX, so we will not be reporting the guardrail
  // result.
  ml_promotion_blocked_by_guardrail_ = false;
  install_source_attached_ = install_source;
}

const std::string& MlInstallResultReporter::output_label() const {
  return ml_output_label_;
}
bool MlInstallResultReporter::ml_promotion_blocked_by_guardrail() const {
  return ml_promotion_blocked_by_guardrail_;
}

void MlInstallResultReporter::ReportResult(
    WebappInstallSource source,
    MlInstallUserResponse user_response) {
  MlInstallResponse response;
  switch (user_response) {
    case MlInstallUserResponse::kAccepted:
      response = MlInstallResponse::kAccepted;
      break;
    case MlInstallUserResponse::kCancelled:
      response = MlInstallResponse::kCancelled;
      break;
    case MlInstallUserResponse::kIgnored:
      response = MlInstallResponse::kIgnored;
      break;
  }
  ReportResultInternal(source, response);
}

void MlInstallResultReporter::ReportResultInternal(
    absl::optional<WebappInstallSource> source,
    MlInstallResponse response) {
  if (reported_ || !app_banner_manager_ ||
      !app_banner_manager_->GetSegmentationPlatformService()) {
    return;
  }
  // This training request can only be reported once.
  reported_ = true;
  if (source) {
    base::UmaHistogramEnumeration("WebApp.MlInstall.InstallSource",
                                  source.value(), WebappInstallSource::COUNT);
  }
  base::UmaHistogramEnumeration("WebApp.MlInstall.DialogResponse", response);

  bool ml_promoted =
      ml_output_label_ == MLInstallabilityPromoter::kShowInstallPromptLabel;
  if (ml_promoted) {
    switch (response) {
      case MlInstallResponse::kAccepted:
        app_banner_manager_->SaveInstallationAcceptedForMl(manifest_id_);
        break;
      case MlInstallResponse::kReporterDestroyed:
      case MlInstallResponse::kIgnored:
        app_banner_manager_->SaveInstallationIgnoredForMl(manifest_id_);
        break;
      case MlInstallResponse::kCancelled:
        app_banner_manager_->SaveInstallationDismissedForMl(manifest_id_);
        break;
      case MlInstallResponse::kBlockedGuardrails:
        break;
    }
  }

  // If promition occurred but we were destroyed, then consider that an
  // 'ignore'.
  if (ml_promoted && response == MlInstallResponse::kReporterDestroyed) {
    response = MlInstallResponse::kIgnored;
  }

  // Exit early to avoid over-sampling in a few cases.
  if (!ml_promoted && response == MlInstallResponse::kReporterDestroyed &&
      base::RandDouble() > kModelDeclineUserDeclineReportProb.Get()) {
    // If the UX is never shown, then this reporter will simply be destroyed. Do
    // not bother reporting this result.
    return;
  }
  if (response == MlInstallResponse::kBlockedGuardrails &&
      base::RandDouble() > kGuardrailResultReportProb.Get()) {
    return;
  }

  segmentation_platform::SegmentationPlatformService* segmentation =
      app_banner_manager_->GetSegmentationPlatformService();
  segmentation_platform::TrainingLabels training_labels;
  training_labels.output_metric =
      std::make_pair("WebApps.MlInstall.DialogResponse",
                     static_cast<base::HistogramBase::Sample>(response));
  segmentation->CollectTrainingData(
      segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_WEB_APP_INSTALLATION_PROMO,
      training_request_, training_labels, base::DoNothing());
}

}  // namespace webapps
