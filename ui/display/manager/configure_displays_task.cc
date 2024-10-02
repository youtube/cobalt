// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/configure_displays_task.h"

#include <cstddef>
#include <string>

#include "base/auto_reset.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/numerics/safe_conversions.h"
#include "ui/display/manager/display_manager_util.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

namespace {

// The epsilon by which a refresh rate value may drift. For example:
// 239.76Hz --> 240Hz. This value was chosen with the consideration of the
// refresh rate value drifts presented in the "Video Formats—Video ID Code and
// Aspect Ratios" table on p.40 of the CTA-861-G standard.
constexpr float kRefreshRateEpsilon = 0.5f;

// Because we do not offer hardware mirroring, the maximal number of external
// displays that can be configured is limited by the number of available CRTCs,
// which is usually three. Since the lifetime of the UMA using this value is one
// year (exp. Nov. 2021), five buckets are more than enough for
// its histogram (between 0 to 4 external monitors).
constexpr int kMaxDisplaysCount = 5;

// Consolidates the UMA name prefix creation to one location, since it is used
// in many different call-sites.
const std::string GetUmaNamePrefixForRequest(
    const DisplayConfigureRequest& request) {
  return request.display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL
             ? std::string("ConfigureDisplays.Internal.Modeset.")
             : std::string("ConfigureDisplays.External.Modeset.");
}

// Find the next best mode that is smaller than |request->mode|. The next best
// mode is found by comparing resolutions, and if those are similar, comparing
// refresh rates. If no mode is found, return nullptr.
const DisplayMode* FindNextMode(const DisplayConfigureRequest& request) {
  DCHECK(request.mode);

  // Internal displays are restricted to their native mode. We do not
  // attempt to downgrade their modes upon failure.
  if (request.display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
    return nullptr;

  if (request.display->modes().size() <= 1)
    return nullptr;

  const DisplayMode* best_mode = nullptr;
  for (const auto& mode : request.display->modes()) {
    if (*mode < *request.mode && (!best_mode || *mode > *best_mode))
      best_mode = mode.get();
  }

  return best_mode;
}

void LogIfInvalidRequestForInternalDisplay(
    const DisplayConfigureRequest& request) {
  if (request.display->type() != DISPLAY_CONNECTION_TYPE_INTERNAL)
    return;

  if (request.mode == nullptr)
    return;

  if (request.mode == request.display->native_mode())
    return;

  LOG(ERROR) << "A mode other than the preferred mode was requested for the "
                "internal display: preferred="
             << request.display->native_mode()->ToString()
             << " vs. requested=" << request.mode->ToString()
             << ". Current mode="
             << (request.display->current_mode()
                     ? request.display->current_mode()->ToString()
                     : "nullptr (disabled)")
             << ".";
}

// Samples used to define buckets used by DisplayResolution enum.
// The enum is used to record screen resolution statistics.
const int32_t kDisplayResolutionSamples[] = {1024, 1280, 1440, 1920,
                                             2560, 3840, 5120, 7680};

void UpdateResolutionUma(const DisplayConfigureRequest& request,
                         const std::string& uma_name) {
  // Display is powered off.
  if (!request.mode)
    return;

  // First, compute the index of the enum DisplayResolution.
  // The index has to match the definition of the enum in enums.xml.
  const uint32_t samples_list_size = std::size(kDisplayResolutionSamples);
  const gfx::Size size = request.mode->size();
  uint32_t width_idx = 0;
  uint32_t height_idx = 0;
  for (; width_idx < samples_list_size; width_idx++) {
    if (size.width() <= kDisplayResolutionSamples[width_idx])
      break;
  }
  for (; height_idx < samples_list_size; height_idx++) {
    if (size.height() <= kDisplayResolutionSamples[height_idx])
      break;
  }

  int display_resolution_index = 0;
  if (width_idx == samples_list_size || height_idx == samples_list_size) {
    // Check if we are in the overflow bucket.
    display_resolution_index = samples_list_size * samples_list_size + 1;
  } else {
    // Compute the index of DisplayResolution, starting from 1, since 0 is used
    // when powering off the display.
    display_resolution_index = width_idx * samples_list_size + height_idx + 1;
  }

  base::UmaHistogramExactLinear(uma_name, display_resolution_index,
                                samples_list_size * samples_list_size + 2);
}

// A list of common refresh rates that are used to help fit approximate refresh
// rate values into one of the common refresh rate bins.
constexpr float kCommonDisplayRefreshRates[] = {
    24.0, 25.0,  30.0,  45.0,  48.0,  50.0,  60.0, 75.0,
    90.0, 100.0, 120.0, 144.0, 165.0, 200.0, 240.0};

void UpdateRefreshRateUma(const DisplayConfigureRequest& request,
                          const std::string& uma_name) {
  // Display is powered off.
  if (!request.mode)
    return;

  base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
      uma_name, base::HistogramBase::kUmaTargetedHistogramFlag);

  // Check if the refresh value is within an epsilon from one of the common
  // refresh rate values.
  for (float common_rate : kCommonDisplayRefreshRates) {
    const bool is_within_epsilon = std::abs(request.mode->refresh_rate() -
                                            common_rate) < kRefreshRateEpsilon;
    if (is_within_epsilon) {
      histogram->Add(common_rate);
      return;
    }
  }

  // Since this is not a common refresh rate value, report it as is.
  histogram->Add(request.mode->refresh_rate());
}

void UpdateAttemptSucceededUma(
    const std::vector<DisplayConfigureRequest>& requests,
    bool display_success) {
  for (const auto& request : requests) {
    const std::string uma_name_prefix = GetUmaNamePrefixForRequest(request);
    base::UmaHistogramBoolean(uma_name_prefix + "AttemptSucceeded",
                              display_success);

    VLOG(2) << "Configured status=" << display_success
            << " display=" << request.display->display_id()
            << " origin=" << request.origin.ToString()
            << " mode=" << (request.mode ? request.mode->ToString() : "null")
            << " enable_vrr=" << request.enable_vrr;
  }
}

void UpdateFinalStatusUma(
    const std::vector<RequestAndStatusList>& requests_and_statuses) {
  int mst_external_displays = 0;
  size_t total_external_displays = requests_and_statuses.size();
  for (auto& request_and_status : requests_and_statuses) {
    const DisplayConfigureRequest* request = request_and_status.first;

    // Is this display SST (single-stream vs. MST multi-stream).
    const bool sst_display = request->display->base_connector_id() &&
                             request->display->path_topology().empty();
    if (!sst_display)
      mst_external_displays++;

    if (request->display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
      total_external_displays--;

    const std::string uma_name_prefix = GetUmaNamePrefixForRequest(*request);
    if (request_and_status.second) {
      UpdateResolutionUma(*request, uma_name_prefix + "Success.Resolution");
      UpdateRefreshRateUma(*request, uma_name_prefix + "Success.RefreshRate");
    }
    base::UmaHistogramBoolean(uma_name_prefix + "FinalStatus",
                              request_and_status.second);
  }

  base::UmaHistogramExactLinear(
      "ConfigureDisplays.Modeset.TotalExternalDisplaysCount",
      base::checked_cast<int>(total_external_displays), kMaxDisplaysCount);

  base::UmaHistogramExactLinear(
      "ConfigureDisplays.Modeset.MstExternalDisplaysCount",
      mst_external_displays, kMaxDisplaysCount);

  if (total_external_displays > 0) {
    const int mst_displays_percentage =
        100.0 * mst_external_displays / total_external_displays;
    UMA_HISTOGRAM_PERCENTAGE(
        "ConfigureDisplays.Modeset.MstExternalDisplaysPercentage",
        mst_displays_percentage);
  }
}

}  // namespace

DisplayConfigureRequest::DisplayConfigureRequest(DisplaySnapshot* display,
                                                 const DisplayMode* mode,
                                                 const gfx::Point& origin,
                                                 bool enable_vrr)
    : display(display), mode(mode), origin(origin), enable_vrr(enable_vrr) {}

DisplayConfigureRequest::DisplayConfigureRequest(DisplaySnapshot* display,
                                                 const DisplayMode* mode,
                                                 const gfx::Point& origin)
    : DisplayConfigureRequest(display, mode, origin, /*enable_vrr=*/false) {}

ConfigureDisplaysTask::ConfigureDisplaysTask(
    NativeDisplayDelegate* delegate,
    const std::vector<DisplayConfigureRequest>& requests,
    ResponseCallback callback,
    ConfigurationType configuration_type)
    : delegate_(delegate),
      requests_(requests),
      configuration_type_(configuration_type),
      callback_(std::move(callback)),
      task_status_(SUCCESS) {
  delegate_->AddObserver(this);
}

ConfigureDisplaysTask::RequestToOriginalMode::RequestToOriginalMode(
    DisplayConfigureRequest* request,
    const DisplayMode* original_mode)
    : request(request), original_mode(original_mode) {}

ConfigureDisplaysTask::~ConfigureDisplaysTask() {
  delegate_->RemoveObserver(this);
}

void ConfigureDisplaysTask::Run() {
  DCHECK(!requests_.empty());

  const bool is_first_attempt = pending_display_group_requests_.empty();
  std::vector<display::DisplayConfigurationParams> config_requests;
  for (const auto& request : requests_) {
    LogIfInvalidRequestForInternalDisplay(request);

    config_requests.emplace_back(request.display->display_id(), request.origin,
                                 request.mode, request.enable_vrr);

    if (is_first_attempt) {
      const std::string uma_name_prefix = GetUmaNamePrefixForRequest(request);
      UpdateResolutionUma(request,
                          uma_name_prefix + "OriginalRequest.Resolution");
    }
  }

  const auto& on_configured =
      is_first_attempt ? &ConfigureDisplaysTask::OnFirstAttemptConfigured
                       : &ConfigureDisplaysTask::OnRetryConfigured;

  uint32_t modeset_flags = display::kTestModeset;
  if (configuration_type_ == kConfigurationTypeSeamless)
    modeset_flags |= display::kSeamlessModeset;
  delegate_->Configure(
      config_requests,
      base::BindOnce(on_configured, weak_ptr_factory_.GetWeakPtr()),
      modeset_flags);
}

void ConfigureDisplaysTask::OnConfigurationChanged() {}

void ConfigureDisplaysTask::OnDisplaySnapshotsInvalidated() {
  // From now on, don't access |requests_[index]->display|; they're invalid.
  task_status_ = ERROR;
  weak_ptr_factory_.InvalidateWeakPtrs();
  std::move(callback_).Run(task_status_);
}

void ConfigureDisplaysTask::OnFirstAttemptConfigured(bool config_success) {
  UpdateAttemptSucceededUma(requests_, config_success);

  if (!config_success) {
    // Partition |requests_| into smaller groups via
    // |pending_display_group_requests_|, update the task's state, and initiate
    // the retry logic. The next time |delegate_|->Configure() terminates
    // OnRetryConfigured() will be executed instead.
    PartitionRequests();
    DCHECK(!pending_display_group_requests_.empty());
    // Prep the first group
    for (const auto& pair : pending_display_group_requests_.front())
      pair.request->mode = pair.original_mode;
    task_status_ = PARTIAL_SUCCESS;
    Run();
    return;
  }

  // This code execute only when the first modeset attempt fully succeeds.
  // Submit the current |requests_| for modeset.
  std::vector<display::DisplayConfigurationParams> config_requests;
  for (const auto& request : requests_) {
    final_requests_status_.emplace_back(&request, true);

    config_requests.emplace_back(request.display->display_id(), request.origin,
                                 request.mode, request.enable_vrr);
  }

  uint32_t modeset_flags = display::kCommitModeset;
  if (configuration_type_ == kConfigurationTypeSeamless)
    modeset_flags |= display::kSeamlessModeset;
  delegate_->Configure(config_requests,
                       base::BindOnce(&ConfigureDisplaysTask::OnConfigured,
                                      weak_ptr_factory_.GetWeakPtr()),
                       modeset_flags);
}

void ConfigureDisplaysTask::OnRetryConfigured(bool config_success) {
  UpdateAttemptSucceededUma(requests_, config_success);

  if (!config_success) {
    // If one of the largest display request can be downgraded, try again.
    // Otherwise this configuration task is a failure.
    if (DowngradeDisplayRequestGroup()) {
      Run();
      return;
    } else {
      // Disable all displays in the current group, since we failed to find an
      // alternative mode. Note that we skip modeset if the latest (or a
      // single) pending group fails. There is no point in disabling displays
      // that are already disabled from previous attempts and failed to change
      // mode.
      for (const auto& pair : pending_display_group_requests_.front())
        pair.request->mode = nullptr;
      task_status_ = ERROR;
    }
  } else {
    // This configuration attempt passed test-modeset. Cache it so we can use it
    // to modeset the displays once we are done testing, or if no other future
    // attempts succeed.
    last_successful_config_parameters_.clear();
    for (const auto& request : requests_) {
      last_successful_config_parameters_.emplace_back(
          request.display->display_id(), request.origin, request.mode,
          request.enable_vrr);
    }
  }

  // This code executes only when this display group request fully succeeds or
  // fails to modeset. Update the final status of this group.
  for (const auto& pair : pending_display_group_requests_.front())
    final_requests_status_.emplace_back(pair.request, config_success);

  // Subsequent modeset attempts will be done on the next pending display group,
  // if one exists.
  pending_display_group_requests_.pop();
  if (!pending_display_group_requests_.empty()) {
    // Prep the next group
    for (const auto& pair : pending_display_group_requests_.front())
      pair.request->mode = pair.original_mode;
    Run();
    return;
  }

  if (task_status_ == ERROR) {
    LOG(WARNING) << "One or more of the connected display groups failed to "
                    "pass test-modeset entirely and will be disabled.";

    if (last_successful_config_parameters_.empty()) {
      LOG(ERROR) << "Display configuration failed. No modeset was attempted.";

      UpdateFinalStatusUma(final_requests_status_);
      std::move(callback_).Run(task_status_);
      return;
    }
  }

  // Configure the displays using the last successful configuration parameter
  // list.
  uint32_t modeset_flags = display::kCommitModeset;
  if (configuration_type_ == kConfigurationTypeSeamless)
    modeset_flags |= display::kSeamlessModeset;
  delegate_->Configure(last_successful_config_parameters_,
                       base::BindOnce(&ConfigureDisplaysTask::OnConfigured,
                                      weak_ptr_factory_.GetWeakPtr()),
                       modeset_flags);
}

void ConfigureDisplaysTask::OnConfigured(bool config_success) {
  if (config_success) {
    for (const DisplayConfigureRequest& request : requests_) {
      request.display->set_current_mode(request.mode);
      request.display->set_origin(request.origin);
      if (request.display->IsVrrCapable()) {
        request.display->set_variable_refresh_rate_state(
            request.enable_vrr ? display::kVrrEnabled : display::kVrrDisabled);
      }
    }
  }

  UpdateFinalStatusUma(final_requests_status_);
  std::move(callback_).Run(task_status_);
}

void ConfigureDisplaysTask::PartitionRequests() {
  pending_display_group_requests_ = PartitionedRequestsQueue();

  base::flat_set<uint64_t> handled_connectors;
  for (size_t i = 0; i < requests_.size(); ++i) {
    uint64_t connector_id = requests_[i].display->base_connector_id();
    if (handled_connectors.find(connector_id) != handled_connectors.end())
      continue;

    std::vector<ConfigureDisplaysTask::RequestToOriginalMode> request_group;
    for (size_t j = i; j < requests_.size(); ++j) {
      if (connector_id == requests_[j].display->base_connector_id()) {
        // Disable all requests in preparation increment connector retries after
        // mapping them to their original request.
        request_group.emplace_back(&requests_[j], requests_[j].mode);
        requests_[j].mode = nullptr;
      }
    }

    handled_connectors.insert(connector_id);
    pending_display_group_requests_.push(request_group);
  }
}

bool ConfigureDisplaysTask::DowngradeDisplayRequestGroup() {
  auto cmp = [](DisplayConfigureRequest* lhs, DisplayConfigureRequest* rhs) {
    return *lhs->mode < *rhs->mode;
  };
  std::priority_queue<DisplayConfigureRequest*,
                      std::vector<DisplayConfigureRequest*>, decltype(cmp)>
      sorted_requests(cmp);

  for (const auto& pair : pending_display_group_requests_.front()) {
    if (pair.request->display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
      continue;

    if (!pair.request->mode)
      continue;

    sorted_requests.push(pair.request);
  }

  // Fail if there are no viable candidates to downgrade
  if (sorted_requests.empty())
    return false;

  while (!sorted_requests.empty()) {
    DisplayConfigureRequest* next_request = sorted_requests.top();
    sorted_requests.pop();

    const DisplayMode* next_mode = FindNextMode(*next_request);
    if (next_mode) {
      next_request->mode = next_mode;
      return true;
    }
  }

  return false;
}

}  // namespace display
