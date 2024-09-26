// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_session_durations_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/android/metrics/android_session_durations_service_factory.h"
#include "chrome/browser/android/metrics/jni_headers/AndroidSessionDurationsServiceState_jni.h"
#include "chrome/browser/profiles/profile_android.h"

namespace {
class IncognitoSessionDurationsMetricsRecorder {
 public:
  IncognitoSessionDurationsMetricsRecorder() = default;

  ~IncognitoSessionDurationsMetricsRecorder() { OnAppEnterBackground(); }

  void OnAppEnterForeground() {
    // When Chrome recovers from a crash while in Incognito, it creates the
    // Incognito profile first and then brings it to foreground. In this case we
    // should prevent double counting.
    if (is_foreground_)
      return;
    is_foreground_ = true;

    // |session_start_| is null prior to start of a new session. Therefore we
    // only need to record the current time.
    if (session_start_.is_null()) {
      session_start_ = base::Time::Now();
      return;
    }

    // Record the previously reported duration, so that subtracting this
    // histogram from 'Profile.Incognito.MovedToBackgroundAfterDuration' would
    // offset for the sessions that were recorded there, but were resumed
    // later.
    base::UmaHistogramCustomCounts(
        "Profile.Incognito.ResumedAfterReportedDuration",
        last_reported_duration_.InMinutes(), 1, base::Days(28).InMinutes(), 50);
  }

  void OnAppEnterBackground() {
    // This function may be called when Chrome is already in background and a
    // proper shutdown of the service takes place.
    if (!is_foreground_)
      return;
    is_foreground_ = false;

    last_reported_duration_ = base::Time::Now() - session_start_;
    base::UmaHistogramCustomCounts(
        "Profile.Incognito.MovedToBackgroundAfterDuration",
        last_reported_duration_.InMinutes(), 1, base::Days(28).InMinutes(), 50);
  }

  void SetSessionStartTimeForTesting(base::Time session_start) {
    session_start_ = session_start;
  }

  void GetStatusForSessionRestore(base::Time& session_start,
                                  base::TimeDelta& last_reported_duration) {
    session_start = session_start_;
    last_reported_duration = last_reported_duration_;
  }

  void RestoreSession(base::Time session_start,
                      base::TimeDelta last_reported_duration) {
    session_start_ = session_start;
    last_reported_duration_ = last_reported_duration;
    is_foreground_ = false;
    OnAppEnterForeground();
  }

 private:
  base::Time session_start_;
  base::TimeDelta last_reported_duration_;
  bool is_foreground_ = false;
};
}  // namespace

AndroidSessionDurationsService::AndroidSessionDurationsService() = default;

AndroidSessionDurationsService::~AndroidSessionDurationsService() = default;

void AndroidSessionDurationsService::InitializeForRegularProfile(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager) {
  DCHECK(!incognito_session_metrics_recorder_);
  DCHECK(!sync_session_metrics_recorder_);
  sync_session_metrics_recorder_ =
      std::make_unique<syncer::SyncSessionDurationsMetricsRecorder>(
          sync_service, identity_manager);

  // The AndroidSessionDurationsService object is created as soon as
  // the profile is initialized. On Android, the profile is initialized as part
  // of the native code initialization, which is done soon after the application
  // enters foreground and before any of the Chrome UI is shown. Let's start
  // tracking the session now.
  OnAppEnterForeground(base::TimeTicks::Now());
}

void AndroidSessionDurationsService::InitializeForIncognitoProfile() {
  DCHECK(!incognito_session_metrics_recorder_);
  DCHECK(!sync_session_metrics_recorder_);
  incognito_session_metrics_recorder_ =
      std::make_unique<IncognitoSessionDurationsMetricsRecorder>();
  OnAppEnterForeground(base::TimeTicks::Now());
}

bool AndroidSessionDurationsService::IsSignedIn() const {
  return sync_session_metrics_recorder_->IsSignedIn();
}

bool AndroidSessionDurationsService::IsSyncing() const {
  return sync_session_metrics_recorder_->IsSyncing();
}

void AndroidSessionDurationsService::Shutdown() {
  sync_session_metrics_recorder_.reset();
  incognito_session_metrics_recorder_.reset();
}

void AndroidSessionDurationsService::OnAppEnterForeground(
    base::TimeTicks session_start) {
  if (sync_session_metrics_recorder_)
    sync_session_metrics_recorder_->OnSessionStarted(session_start);
  else
    incognito_session_metrics_recorder_->OnAppEnterForeground();
}

void AndroidSessionDurationsService::OnAppEnterBackground(
    base::TimeDelta session_length) {
  if (sync_session_metrics_recorder_)
    sync_session_metrics_recorder_->OnSessionEnded(session_length);
  else
    incognito_session_metrics_recorder_->OnAppEnterBackground();
}

void AndroidSessionDurationsService::SetSessionStartTimeForTesting(
    base::Time session_start) {
  if (incognito_session_metrics_recorder_) {
    incognito_session_metrics_recorder_
        ->SetSessionStartTimeForTesting(  // IN-TEST
            session_start);
    return;
  }

  NOTIMPLEMENTED();
}

void AndroidSessionDurationsService::GetIncognitoSessionData(
    base::Time& session_start,
    base::TimeDelta& last_reported_duration) {
  DCHECK(incognito_session_metrics_recorder_);
  incognito_session_metrics_recorder_->GetStatusForSessionRestore(
      session_start, last_reported_duration);
}

void AndroidSessionDurationsService::RestoreIncognitoSession(
    base::Time session_start,
    base::TimeDelta last_reported_duration) {
  DCHECK(incognito_session_metrics_recorder_);
  incognito_session_metrics_recorder_->RestoreSession(session_start,
                                                      last_reported_duration);
}

// Returns a java object consisting of data required to restore the service.
// This function only covers Incognito profiles.
// static
base::android::ScopedJavaLocalRef<jobject>
JNI_AndroidSessionDurationsServiceState_GetAndroidSessionDurationsServiceState(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  CHECK(profile->IsIncognitoProfile());

  AndroidSessionDurationsService* duration_service =
      AndroidSessionDurationsServiceFactory::GetForProfile(profile);
  base::Time session_start_time;
  base::TimeDelta last_reported_duration;
  duration_service->GetIncognitoSessionData(session_start_time,
                                            last_reported_duration);

  return Java_AndroidSessionDurationsServiceState_Constructor(
      env, session_start_time.ToJavaTime(), last_reported_duration.InMinutes());
}

// Restores the service from an archived android object.
// This function only covers Incognito profiles.
// static
void JNI_AndroidSessionDurationsServiceState_RestoreAndroidSessionDurationsServiceState(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jobject>& j_duration_service) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  CHECK(profile->IsIncognitoProfile());

  AndroidSessionDurationsService* duration_service =
      AndroidSessionDurationsServiceFactory::GetForProfile(profile);

  base::Time session_start_time = base::Time::FromJavaTime(
      Java_AndroidSessionDurationsServiceState_getSessionStartTime(
          env, j_duration_service));
  base::TimeDelta last_reported_duration = base::Minutes(
      Java_AndroidSessionDurationsServiceState_getLastReportedDuration(
          env, j_duration_service));

  duration_service->RestoreIncognitoSession(session_start_time,
                                            last_reported_duration);
}
