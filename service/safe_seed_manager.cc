// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/safe_seed_manager.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"

namespace variations {

// As of the time of this writing, January 2018, users at the 99.5th percentile,
// across all platforms, tend to experience fewer than 3 consecutive crashes:
// [1], [2], [3], [4]. Note, however, that this is less true for the less-stable
// channels on some platforms.
// [1] All platforms, stable channel (consistently stable):
//     https://uma.googleplex.com/timeline_v2?sid=90ac80f4573249fb341a8e49501bfcfd
// [2] Most platforms, all channels (consistently stable other than occasional
//     spikes on Canary):
//     https://uma.googleplex.com/timeline_v2?sid=7af5ba1969db76689a401f982a1db539
// [3] A less stable platform, all channels:
//     https://uma.googleplex.com/timeline_v2?sid=07dbc8e4fa9f08e332fb609309a21882
// [4] Another less stable platform, all channels:
//     https://uma.googleplex.com/timeline_v2?sid=a7b529ef5d52863fae2d216e963c4cbc
// Overall, the only {platform, channel} combinations that spike above 3
// consecutive crashes are ones with very few users, plus Canary. It's probably
// not realistic to avoid false positives for these less-stable configurations.
constexpr int kCrashStreakThreshold = 3;

// Consecutive seed fetch failures are, unfortunately, a bit more common. As of
// January 2018, users at the 99.5th percentile tend to see fewer than 4
// consecutive fetch failures on mobile platforms; and users at the 99th
// percentile tend to see fewer than 5 or 6 consecutive failures on desktop
// platforms. It makes sense that the characteristics differ on mobile
// vs. desktop platforms, given that the two use different scheduling algorithms
// for the fetches. Graphs:
// [1] Android, all channels (consistently connected):
//     https://uma.googleplex.com/timeline_v2?sid=99d1d4c2490c60bcbde7afeb77c12a28
// [2] High-connectivity platforms, Stable and Beta channel (consistently
//     connected):
//     https://uma.googleplex.com/timeline_v2?sid=2db5b7278dad41cbf349f5f2cb30efd9
// [3] Other platforms, Stable and Beta channel (slightly less connected):
//     https://uma.googleplex.com/timeline_v2?sid=d4ba2f3751d211898f8e69214147c2ec
// [4] All platforms, Dev (even less connected):
//     https://uma.googleplex.com/timeline_v2?sid=5740fb22b17faa823822adfd8e00ec1a
// [5] All platforms, Canary (actually fairly well-connected!):
//     https://uma.googleplex.com/timeline_v2?sid=3e14d3e4887792bb614db9f3f2c1d48c
// Note the all of the graphs show a spike on a particular day, presumably due
// to server-side instability. Moreover, the Dev channel on desktop is an
// outlier – users on the Dev channel can experience just shy of 9 consecutive
// failures on some platforms.
// Decision: There is not an obvious threshold that both achieves a low
// false-positive rate and provides good coverage for true positives. For now,
// set a threshold that should minimize false-positives.
// TODO(isherman): Check in with the networking team about their thoughts on how
// to find a better balance here.
constexpr int kFetchFailureStreakThreshold = 25;

SafeSeedManager::SafeSeedManager(bool did_previous_session_exit_cleanly,
                                 PrefService* local_state)
    : local_state_(local_state) {
  // Increment the crash streak if the previous session crashed.
  // Note that the streak is not cleared if the previous run didn’t crash.
  // Instead, it’s incremented on each crash until Chrome is able to
  // successfully fetch a new seed. This way, a seed update that mostly
  // destabilizes Chrome will still result in a fallback to safe mode.
  int num_crashes = local_state->GetInteger(prefs::kVariationsCrashStreak);
  if (!did_previous_session_exit_cleanly) {
    ++num_crashes;
    local_state->SetInteger(prefs::kVariationsCrashStreak, num_crashes);
  }

  int num_failed_fetches =
      local_state->GetInteger(prefs::kVariationsFailedToFetchSeedStreak);
  base::UmaHistogramSparse("Variations.SafeMode.Streak.Crashes",
                           base::ClampToRange(num_crashes, 0, 100));
  base::UmaHistogramSparse("Variations.SafeMode.Streak.FetchFailures",
                           base::ClampToRange(num_failed_fetches, 0, 100));
}

SafeSeedManager::~SafeSeedManager() = default;

// static
void SafeSeedManager::RegisterPrefs(PrefRegistrySimple* registry) {
  // Prefs tracking failures along the way to fetching a seed.
  registry->RegisterIntegerPref(prefs::kVariationsCrashStreak, 0);
  registry->RegisterIntegerPref(prefs::kVariationsFailedToFetchSeedStreak, 0);
}

bool SafeSeedManager::ShouldRunInSafeMode() const {
  // Ignore any number of failures if the --force-fieldtrials flag is set. This
  // flag is only used by developers, and there's no need to make the
  // development process flakier.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kForceFieldTrials)) {
    return false;
  }

  int num_crashes = local_state_->GetInteger(prefs::kVariationsCrashStreak);
  int num_failed_fetches =
      local_state_->GetInteger(prefs::kVariationsFailedToFetchSeedStreak);
  return num_crashes >= kCrashStreakThreshold ||
         num_failed_fetches >= kFetchFailureStreakThreshold;
}

void SafeSeedManager::SetActiveSeedState(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    std::unique_ptr<ClientFilterableState> client_filterable_state,
    base::Time seed_fetch_time) {
  DCHECK(!has_set_active_seed_state_);
  has_set_active_seed_state_ = true;

  active_seed_state_ = std::make_unique<ActiveSeedState>(
      seed_data, base64_seed_signature, std::move(client_filterable_state),
      seed_fetch_time);
}

void SafeSeedManager::RecordFetchStarted() {
  // Pessimistically assume the fetch will fail. The failure streak will be
  // reset upon success.
  int num_failures_to_fetch =
      local_state_->GetInteger(prefs::kVariationsFailedToFetchSeedStreak);
  local_state_->SetInteger(prefs::kVariationsFailedToFetchSeedStreak,
                           num_failures_to_fetch + 1);
}

void SafeSeedManager::RecordSuccessfulFetch(VariationsSeedStore* seed_store) {
  // The first time a fetch succeeds for a given run of Chrome, save the active
  // seed+filter configuration as safe. Note that it's sufficient to do this
  // only on the first successful fetch because the active configuration does
  // not change while Chrome is running. Also, note that it's fine to do this
  // even if running in safe mode, as the saved seed in that case will just be
  // the existing safe seed.
  if (active_seed_state_) {
    seed_store->StoreSafeSeed(active_seed_state_->seed_data,
                              active_seed_state_->base64_seed_signature,
                              *active_seed_state_->client_filterable_state,
                              active_seed_state_->seed_fetch_time);

    // The active seed state is only needed for the first time this code path is
    // reached, so free up its memory once the data is no longer needed.
    active_seed_state_.reset();
  }

  // Note: It's important to clear the crash streak as well as the fetch
  // failures streak. Crashes that occur after a successful seed fetch do not
  // prevent updating to a new seed, and therefore do not necessitate falling
  // back to a safe seed.
  local_state_->SetInteger(prefs::kVariationsCrashStreak, 0);
  local_state_->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);
}

SafeSeedManager::ActiveSeedState::ActiveSeedState(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    std::unique_ptr<ClientFilterableState> client_filterable_state,
    base::Time seed_fetch_time)
    : seed_data(seed_data),
      base64_seed_signature(base64_seed_signature),
      client_filterable_state(std::move(client_filterable_state)),
      seed_fetch_time(seed_fetch_time) {}

SafeSeedManager::ActiveSeedState::~ActiveSeedState() = default;

}  // namespace variations
