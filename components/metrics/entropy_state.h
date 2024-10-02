// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_ENTROPY_STATE_H_
#define COMPONENTS_METRICS_ENTROPY_STATE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_registry_simple.h"

class PrefService;

namespace metrics {

// A class to get entropy source values from the PrefService.
class EntropyState final {
 public:
  // Creates the EntropyState with the given |local_state| to get
  // the entropy source value from this helper class.
  explicit EntropyState(PrefService* local_state);

  EntropyState(const EntropyState&) = delete;
  EntropyState& operator=(const EntropyState&) = delete;

  // Clears low_entropy_source and old_low_entropy_source in the prefs.
  static void ClearPrefs(PrefService* local_state);

  // Registers low_entropy_source and old_low_entropy_source in the prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the high entropy source for this client, which is composed of a
  // client ID and the low entropy source. This is intended to be unique for
  // each install. |initial_client_id| is the client_id that was used to
  // randomize field trials and must not be empty.
  std::string GetHighEntropySource(const std::string& initial_client_id);

  // Returns the low entropy source that is used to randomize field trials on
  // startup for this client. Generates a new value if there is none. See the
  // |low_entropy_source_| comment for more info.
  int GetLowEntropySource();

  // Returns the pseudo low entropy source for this client. Generates a new
  // value if there is none. See the |pseudo_low_entropy_source_| comment
  // for more info.
  int GetPseudoLowEntropySource();

  // Returns the old low entropy source for this client. Does not generate a new
  // value, but instead returns |kLowEntropySourceNotSet|, if there is none. See
  // the |old_low_entropy_source_| comment for more info.
  int GetOldLowEntropySource();

  // The argument used to generate a non-identifying entropy source. We want no
  // more than 13 bits of entropy, so use this max to return a number in the
  // range [0, 7999] as the entropy source (12.97 bits of entropy).
  //
  // The value should be kept consistent with
  // LowEntropySource.MAX_LOW_ENTROPY_SIZE in Java.
  static constexpr int kMaxLowEntropySize = 8000;

 private:
  FRIEND_TEST_ALL_PREFIXES(EntropyStateTest, LowEntropySourceNotReset);
  FRIEND_TEST_ALL_PREFIXES(EntropyStateTest, PseudoLowEntropySourceNotReset);
  FRIEND_TEST_ALL_PREFIXES(EntropyStateTest, HaveNoLowEntropySource);
  FRIEND_TEST_ALL_PREFIXES(EntropyStateTest, HaveOnlyNewLowEntropySource);
  FRIEND_TEST_ALL_PREFIXES(EntropyStateTest, HaveOnlyOldLowEntropySource);
  FRIEND_TEST_ALL_PREFIXES(EntropyStateTest, CorruptNewLowEntropySources);
  FRIEND_TEST_ALL_PREFIXES(EntropyStateTest, CorruptOldLowEntropySources);

  // Default value for prefs::kMetricsLowEntropySource.
  static constexpr int kLowEntropySourceNotSet = -1;

  // Loads the low entropy source values from prefs. Creates the new source
  // value if it doesn't exist, but doesn't create the old source value. After
  // this function finishes, |low_entropy_source_| will be set, but
  // |old_low_entropy_source_| may still be |kLowEntropySourceNotSet|.
  void UpdateLowEntropySources();

  // Checks whether a value is on the range of allowed low entropy source
  // values.
  static bool IsValidLowEntropySource(int value);

  // The local state prefs store.
  const raw_ptr<PrefService> local_state_;

  // The non-identifying low entropy source values. These values seed the
  // pseudorandom generators which pick experimental groups. The "old" value is
  // thought to be biased in the wild, and is no longer used for experiments
  // requiring low entropy. Clients which already have an "old" value continue
  // incorporating it into the high entropy source, to avoid changing those
  // group assignments. New clients only have the new source.
  //
  // The pseudo-low entropy source is not used for experiment diversion, but
  // only for statistical validation. (Since it's not used for experiment
  // diversion, it won't be subject to drift over time as experiment effects
  // accumulate in actual low entropy source buckets.)
  //
  // During startup these are set to the values used for randomizing field
  // trials and won't be changed within the session even after calling
  // |ClearPrefs|
  int low_entropy_source_ = kLowEntropySourceNotSet;
  int old_low_entropy_source_ = kLowEntropySourceNotSet;
  int pseudo_low_entropy_source_ = kLowEntropySourceNotSet;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_ENTROPY_STATE_H_
