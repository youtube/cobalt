// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/entropy_state.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/metrics/jni_headers/LowEntropySource_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace metrics {

namespace {

// Generates a new non-identifying entropy source used to seed persistent
// activities. Make it static so that the new low entropy source value will
// only be generated on first access. And thus, even though we may write the
// new low entropy source value to prefs multiple times, it stays the same
// value.
int GenerateLowEntropySource() {
#if BUILDFLAG(IS_ANDROID)
  // Note: As in the non-Android case below, the Java implementation also uses
  // a static cache, so subsequent invocations will return the same value.
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_LowEntropySource_generateLowEntropySource(env);
#else
  static const int low_entropy_source =
      base::RandInt(0, EntropyState::kMaxLowEntropySize - 1);
  return low_entropy_source;
#endif  // BUILDFLAG(IS_ANDROID)
}

// Generates a new non-identifying low entropy source using the same method
// that's used for the actual low entropy source. This one, however, is only
// used for statistical validation, and *not* for randomization or experiment
// assignment.
int GeneratePseudoLowEntropySource() {
#if BUILDFLAG(IS_ANDROID)
  // Note: As in the non-Android case below, the Java implementation also uses
  // a static cache, so subsequent invocations will return the same value.
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_LowEntropySource_generatePseudoLowEntropySource(env);
#else
  static const int pseudo_low_entropy_source =
      base::RandInt(0, EntropyState::kMaxLowEntropySize - 1);
  return pseudo_low_entropy_source;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace

EntropyState::EntropyState(PrefService* local_state)
    : local_state_(local_state) {}

// static
constexpr int EntropyState::kLowEntropySourceNotSet;

// static
void EntropyState::ClearPrefs(PrefService* local_state) {
  local_state->ClearPref(prefs::kMetricsLowEntropySource);
  local_state->ClearPref(prefs::kMetricsOldLowEntropySource);
  local_state->ClearPref(prefs::kMetricsPseudoLowEntropySource);
}

// static
void EntropyState::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kMetricsLowEntropySource,
                                kLowEntropySourceNotSet);
  registry->RegisterIntegerPref(prefs::kMetricsOldLowEntropySource,
                                kLowEntropySourceNotSet);
  registry->RegisterIntegerPref(prefs::kMetricsPseudoLowEntropySource,
                                kLowEntropySourceNotSet);
}

std::string EntropyState::GetHighEntropySource(
    const std::string& initial_client_id) {
  DCHECK(!initial_client_id.empty());
  // For metrics reporting-enabled users, we combine the client ID and low
  // entropy source to get the final entropy source.
  // This has two useful properties:
  //  1) It makes the entropy source less identifiable for parties that do not
  //     know the low entropy source.
  //  2) It makes the final entropy source resettable.

  // If this install has an old low entropy source, continue using it, to avoid
  // changing the group assignments of studies using high entropy. New installs
  // only have the new low entropy source. If the number of installs with old
  // sources ever becomes small enough (see UMA.LowEntropySourceValue), we could
  // remove it, and just use the new source here.
  int low_entropy_source = GetOldLowEntropySource();
  if (low_entropy_source == kLowEntropySourceNotSet)
    low_entropy_source = GetLowEntropySource();

  return initial_client_id + base::NumberToString(low_entropy_source);
}

int EntropyState::GetLowEntropySource() {
  UpdateLowEntropySources();
  return low_entropy_source_;
}

int EntropyState::GetPseudoLowEntropySource() {
  UpdateLowEntropySources();
  return pseudo_low_entropy_source_;
}

int EntropyState::GetOldLowEntropySource() {
  UpdateLowEntropySources();
  return old_low_entropy_source_;
}

void EntropyState::UpdateLowEntropySources() {
  // The default value for |low_entropy_source_| and the default pref value are
  // both |kLowEntropySourceNotSet|, which indicates the value has not been set.
  if (low_entropy_source_ != kLowEntropySourceNotSet &&
      pseudo_low_entropy_source_ != kLowEntropySourceNotSet)
    return;

  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  // Only try to load the value from prefs if the user did not request a reset.
  // Otherwise, skip to generating a new value. We would have already returned
  // if both |low_entropy_source_| and |pseudo_low_entropy_source_| were set,
  // ensuring we only do this reset on the first call to
  // UpdateLowEntropySources().
  if (!command_line->HasSwitch(switches::kResetVariationState)) {
    int new_pref = local_state_->GetInteger(prefs::kMetricsLowEntropySource);
    if (IsValidLowEntropySource(new_pref))
      low_entropy_source_ = new_pref;
    int old_pref = local_state_->GetInteger(prefs::kMetricsOldLowEntropySource);
    if (IsValidLowEntropySource(old_pref))
      old_low_entropy_source_ = old_pref;
    int pseudo_pref =
        local_state_->GetInteger(prefs::kMetricsPseudoLowEntropySource);
    if (IsValidLowEntropySource(pseudo_pref))
      pseudo_low_entropy_source_ = pseudo_pref;
  }

  // If the new source is missing or corrupt (or requested to be reset), then
  // (re)create it. Don't bother recreating the old source if it's corrupt,
  // because we only keep the old source around for consistency, and we can't
  // maintain a consistent value if we recreate it.
  if (low_entropy_source_ == kLowEntropySourceNotSet) {
    low_entropy_source_ = GenerateLowEntropySource();
    DCHECK(IsValidLowEntropySource(low_entropy_source_));
    local_state_->SetInteger(prefs::kMetricsLowEntropySource,
                             low_entropy_source_);
  }

  // If the pseudo source is missing or corrupt (or requested to be reset), then
  // (re)create it. Don't bother recreating the old source if it's corrupt,
  // because we only keep the old source around for consistency, and we can't
  // maintain a consistent value if we recreate it.
  if (pseudo_low_entropy_source_ == kLowEntropySourceNotSet) {
    pseudo_low_entropy_source_ = GeneratePseudoLowEntropySource();
    DCHECK(IsValidLowEntropySource(pseudo_low_entropy_source_));
    local_state_->SetInteger(prefs::kMetricsPseudoLowEntropySource,
                             pseudo_low_entropy_source_);
  }

  // If the old source was present but corrupt (or requested to be reset), then
  // we'll never use it again, so delete it.
  if (old_low_entropy_source_ == kLowEntropySourceNotSet &&
      local_state_->HasPrefPath(prefs::kMetricsOldLowEntropySource)) {
    local_state_->ClearPref(prefs::kMetricsOldLowEntropySource);
  }

  DCHECK_NE(low_entropy_source_, kLowEntropySourceNotSet);
}

// static
bool EntropyState::IsValidLowEntropySource(int value) {
  return value >= 0 && value < kMaxLowEntropySize;
}

}  // namespace metrics
