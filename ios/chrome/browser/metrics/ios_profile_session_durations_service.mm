// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_profile_session_durations_service.h"

#import "components/password_manager/core/browser/password_session_durations_metrics_recorder.h"
#import "components/sync/driver/sync_session_durations_metrics_recorder.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSProfileSessionDurationsService::IOSProfileSessionDurationsService(
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager) {
  CHECK(sync_service);
  CHECK(pref_service);
  CHECK(identity_manager);

  sync_metrics_recorder_ =
      std::make_unique<syncer::SyncSessionDurationsMetricsRecorder>(
          sync_service, identity_manager);

  password_metrics_recorder_ = std::make_unique<
      password_manager::PasswordSessionDurationsMetricsRecorder>(pref_service,
                                                                 sync_service);

  // `IOSProfileSessionDurationsService` is called explicitly each time a
  // session starts or ends. So there is no need to mimic what is done on
  // Android and to start a session in the constuctor of the service.
}

IOSProfileSessionDurationsService::~IOSProfileSessionDurationsService() =
    default;

void IOSProfileSessionDurationsService::Shutdown() {
  sync_metrics_recorder_.reset();
  password_metrics_recorder_.reset();
}

void IOSProfileSessionDurationsService::OnSessionStarted(
    base::TimeTicks session_start) {
  is_session_active_ = true;
  sync_metrics_recorder_->OnSessionStarted(session_start);
  password_metrics_recorder_->OnSessionStarted(session_start);
}

void IOSProfileSessionDurationsService::OnSessionEnded(
    base::TimeDelta session_length) {
  sync_metrics_recorder_->OnSessionEnded(session_length);
  password_metrics_recorder_->OnSessionEnded(session_length);
  is_session_active_ = false;
}

bool IOSProfileSessionDurationsService::IsSessionActive() {
  return is_session_active_;
}

bool IOSProfileSessionDurationsService::IsSignedIn() const {
  return sync_metrics_recorder_->IsSignedIn();
}

bool IOSProfileSessionDurationsService::IsSyncing() const {
  return sync_metrics_recorder_->IsSyncing();
}

IOSProfileSessionDurationsService::IOSProfileSessionDurationsService() =
    default;
