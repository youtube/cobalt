// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_context.h"

namespace metrics {

DesktopProfileSessionDurationsService::DesktopProfileSessionDurationsService(
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    DesktopSessionDurationTracker* tracker)
    : sync_metrics_recorder_(
          std::make_unique<syncer::SyncSessionDurationsMetricsRecorder>(
              sync_service,
              identity_manager)),
      password_metrics_recorder_(
          std::make_unique<
              password_manager::PasswordSessionDurationsMetricsRecorder>(
              pref_service,
              sync_service)),
      download_metrics_recorder_(
          std::make_unique<DownloadSessionDurationsMetricsRecorder>()) {
  session_duration_observation_.Observe(tracker);
  if (tracker->in_session()) {
    // The session was started before this service was created. Let's start
    // tracking now.
    OnSessionStarted(base::TimeTicks::Now());
  }
}

DesktopProfileSessionDurationsService::
    ~DesktopProfileSessionDurationsService() = default;

void DesktopProfileSessionDurationsService::Shutdown() {
  // The Profile is being destroyed, e.g. because the
  // DestroyProfileOnBrowserClose flag is enabled. Recorders expect every call
  // to OnSessionStarted() to have a corresponding OnSessionEnded().
  //
  // Use a |session_length| of zero, so each recorder can infer the duration
  // based on their internal state.
  OnSessionEnded(base::TimeDelta(), base::TimeTicks::Now());

  password_metrics_recorder_.reset();
  sync_metrics_recorder_.reset();
  download_metrics_recorder_.reset();
}

bool DesktopProfileSessionDurationsService::IsSignedIn() const {
  return sync_metrics_recorder_->IsSignedIn();
}

bool DesktopProfileSessionDurationsService::IsSyncing() const {
  return sync_metrics_recorder_->IsSyncing();
}

void DesktopProfileSessionDurationsService::OnSessionStarted(
    base::TimeTicks session_start) {
  sync_metrics_recorder_->OnSessionStarted(session_start);
  password_metrics_recorder_->OnSessionStarted(session_start);
  download_metrics_recorder_->OnSessionStarted(session_start);
}

void DesktopProfileSessionDurationsService::OnSessionEnded(
    base::TimeDelta session_length,
    base::TimeTicks session_end) {
  sync_metrics_recorder_->OnSessionEnded(session_length);
  password_metrics_recorder_->OnSessionEnded(session_length);
  download_metrics_recorder_->OnSessionEnded(session_end);
}

}  // namespace metrics
