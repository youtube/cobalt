// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/cached_result_writer.h"

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform {

CachedResultWriter::CachedResultWriter(std::unique_ptr<ClientResultPrefs> prefs,
                                       base::Clock* clock)
    : result_prefs_(std::move(prefs)), clock_(clock) {}

CachedResultWriter::~CachedResultWriter() = default;

void CachedResultWriter::UpdatePrefsIfExpired(
    Config* config,
    proto::ClientResult client_result,
    const PlatformOptions& platform_options) {
  if (!IsPrefUpdateRequiredForClient(config, platform_options) ||
      config->on_demand_execution) {
    return;
  }
  UpdateNewClientResultToPrefs(config, client_result);
}

bool CachedResultWriter::IsPrefUpdateRequiredForClient(
    Config* config,
    const PlatformOptions& platform_options) {
  absl::optional<proto::ClientResult> client_result =
      result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  if (!client_result.has_value()) {
    return true;
  }
  PostProcessor post_processor;
  proto::PredictionResult pred_result = client_result->client_result();
  base::TimeDelta ttl_to_use =
      post_processor.GetTTLForPredictedResult(pred_result);
  base::Time expiration_time =
      base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(client_result->timestamp_us())) +
      ttl_to_use;

  bool force_refresh_results = platform_options.force_refresh_results;
  bool has_expired_results = expiration_time <= clock_->Now();

  if (!force_refresh_results && !has_expired_results) {
    stats::RecordSegmentSelectionFailure(
        *config, stats::SegmentationSelectionFailureReason::
                     kProtoPrefsUpdateNotRequired);
    VLOG(1) << __func__ << ": previous client_result"
            << " has not yet expired. Expiration: " << expiration_time;
    return false;
  }
  return true;
}

void CachedResultWriter::UpdateNewClientResultToPrefs(
    Config* config,
    const proto::ClientResult& client_result) {
  auto prev_client_result =
      result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  absl::optional<proto::PredictionResult> prev_prediction_result =
      prev_client_result.has_value()
          ? absl::make_optional(prev_client_result->client_result())
          : absl::nullopt;
  stats::RecordClassificationResultUpdated(*config, prev_prediction_result,
                                           client_result.client_result());
  stats::RecordSegmentSelectionFailure(
      *config, stats::SegmentationSelectionFailureReason::kProtoPrefsUpdated);
  result_prefs_->SaveClientResultToPrefs(config->segmentation_key,
                                         client_result);
}

}  // namespace segmentation_platform
