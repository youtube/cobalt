// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/smart_lock_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"

SmartLockMetricsRecorder::UsageRecorder::UsageRecorder() = default;

SmartLockMetricsRecorder::UsageRecorder::~UsageRecorder() = default;

SmartLockMetricsRecorder::SmartLockMetricsRecorder() = default;

SmartLockMetricsRecorder::~SmartLockMetricsRecorder() {}

// static
void SmartLockMetricsRecorder::RecordSmartLockUnlockAuthMethodChoice(
    SmartLockAuthMethodChoice auth_method_choice) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthMethodChoice.Unlock",
                            auth_method_choice);
}

// static
void SmartLockMetricsRecorder::RecordSmartLockSignInAuthMethodChoice(
    SmartLockAuthMethodChoice auth_method_choice) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthMethodChoice.SignIn",
                            auth_method_choice);
}

// static
void SmartLockMetricsRecorder::RecordAuthResultUnlockSuccess(bool success) {
  RecordAuthResultSuccess(success);
  UMA_HISTOGRAM_BOOLEAN("SmartLock.AuthResult.Unlock", success);
}

// static
void SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(
    SmartLockAuthResultFailureReason failure_reason) {
  RecordAuthResultUnlockSuccess(false);
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthResult.Unlock.Failure",
                            failure_reason);
}

// static
void SmartLockMetricsRecorder::RecordAuthMethodChoiceUnlockPasswordState(
    SmartLockAuthEventPasswordState password_state) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthMethodChoice.Unlock.PasswordState",
                            password_state);
}

// static
void SmartLockMetricsRecorder::RecordAuthMethodChoiceSignInPasswordState(
    SmartLockAuthEventPasswordState password_state) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthMethodChoice.SignIn.PasswordState",
                            password_state);
}

// static
void SmartLockMetricsRecorder::SetUsageRecorderInstance(
    SmartLockMetricsRecorder::UsageRecorder* usage_recorder) {
  SmartLockMetricsRecorder::g_usage_recorder = usage_recorder;
}

// static
void SmartLockMetricsRecorder::RecordAuthResultSuccess(bool success) {
  UMA_HISTOGRAM_BOOLEAN("SmartLock.AuthResult", success);

  if (SmartLockMetricsRecorder::g_usage_recorder) {
    SmartLockMetricsRecorder::g_usage_recorder->RecordUsage(success);
  }
}

SmartLockMetricsRecorder::UsageRecorder*
    SmartLockMetricsRecorder::g_usage_recorder = nullptr;
