// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_http_header_provider.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_switches.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"

namespace variations {
namespace {

// Maximum age permitted for a variations seed, in days.
const int kMaxVariationsSeedAgeDays = 30;

enum VariationsSeedExpiry {
  VARIATIONS_SEED_EXPIRY_NOT_EXPIRED,
  VARIATIONS_SEED_EXPIRY_FETCH_TIME_MISSING,
  VARIATIONS_SEED_EXPIRY_EXPIRED,
  VARIATIONS_SEED_EXPIRY_ENUM_SIZE,
};

// Set of different possible values to report for the
// Variations.LoadPermanentConsistencyCountryResult histogram. Values are
// persisted to logs, and should therefore never be renumbered nor reused.
enum LoadPermanentConsistencyCountryResult {
  LOAD_COUNTRY_NO_PREF_NO_SEED = 0,
  LOAD_COUNTRY_NO_PREF_HAS_SEED,
  LOAD_COUNTRY_INVALID_PREF_NO_SEED,
  LOAD_COUNTRY_INVALID_PREF_HAS_SEED,
  LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_EQ,
  LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_NEQ,
  LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_EQ,
  LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_NEQ,
  LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_EQ,
  LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_NEQ,
  LOAD_COUNTRY_MAX,
};

// Gets current form factor and converts it from enum DeviceFormFactor to enum
// Study_FormFactor.
Study::FormFactor GetCurrentFormFactor() {
  switch (ui::GetDeviceFormFactor()) {
    case ui::DEVICE_FORM_FACTOR_PHONE:
      return Study::PHONE;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      return Study::TABLET;
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
      return Study::DESKTOP;
  }
  NOTREACHED();
  return Study::DESKTOP;
}

// Returns the date that should be used by the VariationsSeedProcessor to do
// expiry and start date checks.
base::Time GetReferenceDateForExpiryChecks(PrefService* local_state) {
  const base::Time seed_date = local_state->GetTime(prefs::kVariationsSeedDate);
  const base::Time build_time = base::GetBuildTime();
  // Use the build time for date checks if either the seed date is invalid or
  // the build time is newer than the seed date.
  base::Time reference_date = seed_date;
  if (seed_date.is_null() || seed_date < build_time)
    reference_date = build_time;
  return reference_date;
}

// Wrapper around channel checking, used to enable channel mocking for
// testing. If a fake channel flag is provided, it will take precedence.
// Otherwise, this will return the current browser channel (which could be
// UNKNOWN).
Study::Channel GetChannelForVariations(version_info::Channel product_channel) {
  const std::string forced_channel =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kFakeVariationsChannel);
  if (!forced_channel.empty()) {
    if (forced_channel == "stable")
      return Study::STABLE;
    if (forced_channel == "beta")
      return Study::BETA;
    if (forced_channel == "dev")
      return Study::DEV;
    if (forced_channel == "canary")
      return Study::CANARY;
    DVLOG(1) << "Invalid channel provided: " << forced_channel;
  }

  switch (product_channel) {
    case version_info::Channel::CANARY:
      return Study::CANARY;
    case version_info::Channel::DEV:
      return Study::DEV;
    case version_info::Channel::BETA:
      return Study::BETA;
    case version_info::Channel::STABLE:
      return Study::STABLE;
    case version_info::Channel::UNKNOWN:
      return Study::UNKNOWN;
  }
  NOTREACHED();
  return Study::UNKNOWN;
}

// Records UMA histogram with the result of the variations seed expiry check.
void RecordCreateTrialsSeedExpiry(VariationsSeedExpiry expiry_check_result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.CreateTrials.SeedExpiry",
                            expiry_check_result,
                            VARIATIONS_SEED_EXPIRY_ENUM_SIZE);
}

// Records the loaded seed's age to an UMA histogram.
void RecordSeedFreshness(base::TimeDelta seed_age) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.SeedFreshness", seed_age.InMinutes(),
                              1, base::TimeDelta::FromDays(30).InMinutes(), 50);
}

// If an invalid command-line to force field trials was specified, exit the
// browser with a helpful error message, so that the user can correct their
// mistake.
void ExitWithMessage(const std::string& message) {
  puts(message.c_str());
  exit(1);
}

// Returns the current application locale (e.g. "en-US").
std::string GetApplicationLocale(PrefService* local_state) {
  if (!local_state->HasPrefPath(language::prefs::kApplicationLocale))
    return std::string();
  std::string locale =
      local_state->GetString(language::prefs::kApplicationLocale);
  return l10n_util::GetApplicationLocale(locale);
}

}  // namespace

VariationsFieldTrialCreator::VariationsFieldTrialCreator(
    PrefService* local_state,
    VariationsServiceClient* client,
    const UIStringOverrider& ui_string_overrider)
    : VariationsFieldTrialCreator(local_state,
                                  client,
                                  ui_string_overrider,
                                  nullptr) {}

VariationsFieldTrialCreator::VariationsFieldTrialCreator(
    PrefService* local_state,
    VariationsServiceClient* client,
    const UIStringOverrider& ui_string_overrider,
    std::unique_ptr<SeedResponse> initial_seed)
    : client_(client),
      ui_string_overrider_(ui_string_overrider),
      seed_store_(local_state, std::move(initial_seed)),
      create_trials_from_seed_called_(false),
      has_platform_override_(false),
      platform_override_(Study::PLATFORM_WINDOWS) {}

VariationsFieldTrialCreator::~VariationsFieldTrialCreator() {}

std::string VariationsFieldTrialCreator::GetLatestCountry() const {
  const std::string override_country =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVariationsOverrideCountry);
  return !override_country.empty()
             ? override_country
             : local_state()->GetString(prefs::kVariationsCountry);
}

bool VariationsFieldTrialCreator::CreateTrialsFromSeed(
    std::unique_ptr<const base::FieldTrial::EntropyProvider>
        low_entropy_provider,
    base::FeatureList* feature_list,
    SafeSeedManager* safe_seed_manager) {
  TRACE_EVENT0("startup", "VariationsFieldTrialCreator::CreateTrialsFromSeed");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!create_trials_from_seed_called_);
  create_trials_from_seed_called_ = true;

  base::TimeTicks start_time = base::TimeTicks::Now();

  const base::Version& current_version = version_info::GetVersion();
  if (!current_version.IsValid())
    return false;

  std::unique_ptr<ClientFilterableState> client_filterable_state =
      GetClientFilterableStateForVersion(current_version);
  VariationsSeed seed;
  bool run_in_safe_mode = safe_seed_manager->ShouldRunInSafeMode() &&
                          LoadSafeSeed(&seed, client_filterable_state.get());

  std::string seed_data;
  std::string base64_seed_signature;
  if (!run_in_safe_mode && !LoadSeed(&seed, &seed_data, &base64_seed_signature))
    return false;

  UMA_HISTOGRAM_BOOLEAN("Variations.SafeMode.FellBackToSafeMode2",
                        run_in_safe_mode);

  // Note that passing |&ui_string_overrider_| via base::Unretained below is
  // safe because the callback is executed synchronously. It is not possible to
  // pass UIStringOverrider directly to VariationSeedProcessor as the variations
  // component should not depend on //ui/base.
  VariationsSeedProcessor().CreateTrialsFromSeed(
      seed, *client_filterable_state,
      base::Bind(&UIStringOverrider::OverrideUIString,
                 base::Unretained(&ui_string_overrider_)),
      low_entropy_provider.get(), feature_list);

  // Store into the |safe_seed_manager| the combined server and client data used
  // to create the field trials. But, as an optimization, skip this step when
  // running in safe mode – once running in safe mode, there can never be a need
  // to save the active state to the safe seed prefs.
  if (!run_in_safe_mode) {
    safe_seed_manager->SetActiveSeedState(seed_data, base64_seed_signature,
                                          std::move(client_filterable_state),
                                          seed_store_.GetLastFetchTime());
  }

  UMA_HISTOGRAM_TIMES("Variations.SeedProcessingTime",
                      base::TimeTicks::Now() - start_time);
  return true;
}

std::unique_ptr<ClientFilterableState>
VariationsFieldTrialCreator::GetClientFilterableStateForVersion(
    const base::Version& version) {
  std::unique_ptr<ClientFilterableState> state =
      std::make_unique<ClientFilterableState>();
  state->locale = GetApplicationLocale(local_state());
  state->reference_date = GetReferenceDateForExpiryChecks(local_state());
  state->version = version;
  state->channel = GetChannelForVariations(client_->GetChannel());
  state->form_factor = GetCurrentFormFactor();
  state->platform = GetPlatform();
  state->hardware_class = GetShortHardwareClass();
#if defined(OS_ANDROID)
  // This is set on Android only currently, because the IsLowEndDevice() API
  // on other platforms has no intrinsic meaning outside of a field trial that
  // controls its value. Since this is before server-side field trials are
  // evaluated, that field trial would not be able to apply for this case.
  state->is_low_end_device = base::SysInfo::IsLowEndDevice();
#endif
  state->supports_permanent_consistency =
      client_->GetSupportsPermanentConsistency();
  state->session_consistency_country = GetLatestCountry();
  state->permanent_consistency_country = LoadPermanentConsistencyCountry(
      version, state->session_consistency_country);
  return state;
}

std::string VariationsFieldTrialCreator::LoadPermanentConsistencyCountry(
    const base::Version& version,
    const std::string& latest_country) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(version.IsValid());

  const std::string override_country =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVariationsOverrideCountry);
  if (!override_country.empty())
    return override_country;

  const base::ListValue* list_value =
      local_state()->GetList(prefs::kVariationsPermanentConsistencyCountry);
  std::string stored_version_string;
  std::string stored_country;

  // Determine if the saved pref value is present and valid.
  const bool is_pref_empty = list_value->empty();
  const bool is_pref_valid = list_value->GetSize() == 2 &&
                             list_value->GetString(0, &stored_version_string) &&
                             list_value->GetString(1, &stored_country) &&
                             base::Version(stored_version_string).IsValid();

  // Determine if the version from the saved pref matches |version|.
  const bool does_version_match =
      is_pref_valid && version == base::Version(stored_version_string);

  // Determine if the country in the saved pref matches the country in
  // |latest_country|.
  const bool does_country_match = is_pref_valid && !latest_country.empty() &&
                                  stored_country == latest_country;

  // Record a histogram for how the saved pref value compares to the current
  // version and the country code in the variations seed.
  LoadPermanentConsistencyCountryResult result;
  if (is_pref_empty) {
    result = !latest_country.empty() ? LOAD_COUNTRY_NO_PREF_HAS_SEED
                                     : LOAD_COUNTRY_NO_PREF_NO_SEED;
  } else if (!is_pref_valid) {
    result = !latest_country.empty() ? LOAD_COUNTRY_INVALID_PREF_HAS_SEED
                                     : LOAD_COUNTRY_INVALID_PREF_NO_SEED;
  } else if (latest_country.empty()) {
    result = does_version_match ? LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_EQ
                                : LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_NEQ;
  } else if (does_version_match) {
    result = does_country_match ? LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_EQ
                                : LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_NEQ;
  } else {
    result = does_country_match ? LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_EQ
                                : LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_NEQ;
  }
  UMA_HISTOGRAM_ENUMERATION("Variations.LoadPermanentConsistencyCountryResult",
                            result, LOAD_COUNTRY_MAX);

  // Use the stored country if one is available and was fetched since the last
  // time Chrome was updated.
  if (does_version_match)
    return stored_country;

  if (latest_country.empty()) {
    if (!is_pref_valid)
      local_state()->ClearPref(prefs::kVariationsPermanentConsistencyCountry);
    // If we've never received a country code from the server, use an empty
    // country so that it won't pass any filters that specifically include
    // countries, but so that it will pass any filters that specifically exclude
    // countries.
    return std::string();
  }

  // Otherwise, update the pref with the current Chrome version and country.
  StorePermanentCountry(version, latest_country);
  return latest_country;
}

void VariationsFieldTrialCreator::StorePermanentCountry(
    const base::Version& version,
    const std::string& country) {
  base::ListValue new_list_value;
  new_list_value.AppendString(version.GetString());
  new_list_value.AppendString(country);
  local_state()->Set(prefs::kVariationsPermanentConsistencyCountry,
                     new_list_value);
}

void VariationsFieldTrialCreator::OverrideVariationsPlatform(
    Study::Platform platform_override) {
  has_platform_override_ = true;
  platform_override_ = platform_override;
}

// static
std::string VariationsFieldTrialCreator::GetShortHardwareClass() {
#if defined(OS_CHROMEOS)
  std::string board = base::SysInfo::GetLsbReleaseBoard();
  // GetLsbReleaseBoard() may be suffixed with a "-signed-" and other extra
  // info. Strip it.
  const size_t index = board.find("-signed-");
  if (index != std::string::npos)
    board.resize(index);

  return base::ToUpperASCII(board);
#else
  return std::string();
#endif  // OS_CHROMEOS
}

bool VariationsFieldTrialCreator::LoadSeed(VariationsSeed* seed,
                                           std::string* seed_data,
                                           std::string* base64_signature) {
  if (!GetSeedStore()->LoadSeed(seed, seed_data, base64_signature))
    return false;

  const base::Time last_fetch_time = seed_store_.GetLastFetchTime();
  if (last_fetch_time.is_null()) {
    // If the last fetch time is missing and we have a seed, then this must be
    // the first run of Chrome. Store the current time as the last fetch time.
    seed_store_.RecordLastFetchTime();
    RecordCreateTrialsSeedExpiry(VARIATIONS_SEED_EXPIRY_FETCH_TIME_MISSING);
    return true;
  }

  // Reject the seed if it is more than 30 days old.
  const base::TimeDelta seed_age = base::Time::Now() - last_fetch_time;
  if (seed_age.InDays() > kMaxVariationsSeedAgeDays) {
    RecordCreateTrialsSeedExpiry(VARIATIONS_SEED_EXPIRY_EXPIRED);
    return false;
  }

  // Record that a suitably fresh seed was loaded.
  RecordCreateTrialsSeedExpiry(VARIATIONS_SEED_EXPIRY_NOT_EXPIRED);
  RecordSeedFreshness(seed_age);
  return true;
}

bool VariationsFieldTrialCreator::LoadSafeSeed(
    VariationsSeed* seed,
    ClientFilterableState* client_state) {
  base::Time safe_seed_fetch_time;
  if (!GetSeedStore()->LoadSafeSeed(seed, client_state, &safe_seed_fetch_time))
    return false;

  // Record the safe seed's age. Note, however, that the safe seed fetch time
  // pref was added about a milestone later than most of the other safe seed
  // prefs, so it might be absent. If it's absent, don't attempt to guess what
  // value to record; just skip recording the metric.
  if (!safe_seed_fetch_time.is_null())
    RecordSeedFreshness(base::Time::Now() - safe_seed_fetch_time);

  return true;
}

bool VariationsFieldTrialCreator::SetupFieldTrials(
    const char* kEnableGpuBenchmarking,
    const char* kEnableFeatures,
    const char* kDisableFeatures,
    const std::set<std::string>& unforceable_field_trials,
    const std::vector<std::string>& variation_ids,
    std::unique_ptr<const base::FieldTrial::EntropyProvider>
        low_entropy_provider,
    std::unique_ptr<base::FeatureList> feature_list,
    PlatformFieldTrials* platform_field_trials,
    SafeSeedManager* safe_seed_manager) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableBenchmarking) ||
      command_line->HasSwitch(kEnableGpuBenchmarking)) {
    base::FieldTrial::EnableBenchmarking();
  }

  if (command_line->HasSwitch(switches::kForceFieldTrialParams)) {
    bool result = AssociateParamsFromString(
        command_line->GetSwitchValueASCII(switches::kForceFieldTrialParams));
    if (!result) {
      // Some field trial params implement things like csv or json with a
      // particular param. If some control characters are not %-encoded, it can
      // lead to confusing error messages, so add a hint here.
      ExitWithMessage(base::StringPrintf(
          "Invalid --%s list specified. Make sure you %%-"
          "encode the following characters in param values: %%:/.,",
          switches::kForceFieldTrialParams));
    }
  }

  // Ensure any field trials specified on the command line are initialized.
  if (command_line->HasSwitch(::switches::kForceFieldTrials)) {
    // Create field trials without activating them, so that this behaves in a
    // consistent manner with field trials created from the server.
    bool result = base::FieldTrialList::CreateTrialsFromString(
        command_line->GetSwitchValueASCII(::switches::kForceFieldTrials),
        unforceable_field_trials);
    if (!result) {
      ExitWithMessage(base::StringPrintf("Invalid --%s list specified.",
                                         ::switches::kForceFieldTrials));
    }
  }

  VariationsHttpHeaderProvider* http_header_provider =
      VariationsHttpHeaderProvider::GetInstance();
  // Force the variation ids selected in chrome://flags and/or specified using
  // the command-line flag.
  auto result = http_header_provider->ForceVariationIds(
      variation_ids,
      command_line->GetSwitchValueASCII(switches::kForceVariationIds));
  switch (result) {
    case VariationsHttpHeaderProvider::ForceIdsResult::INVALID_SWITCH_ENTRY:
      ExitWithMessage(base::StringPrintf("Invalid --%s list specified.",
                                         switches::kForceVariationIds));
      break;
    case VariationsHttpHeaderProvider::ForceIdsResult::INVALID_VECTOR_ENTRY:
      // It should not be possible to have invalid variation ids from the
      // vector param (which corresponds to chrome://flags).
      NOTREACHED();
      break;
    case VariationsHttpHeaderProvider::ForceIdsResult::SUCCESS:
      break;
  }

  feature_list->InitializeFromCommandLine(
      command_line->GetSwitchValueASCII(kEnableFeatures),
      command_line->GetSwitchValueASCII(kDisableFeatures));

#if defined(FIELDTRIAL_TESTING_ENABLED)
  if (!command_line->HasSwitch(switches::kDisableFieldTrialTestingConfig) &&
      !command_line->HasSwitch(::switches::kForceFieldTrials) &&
      !command_line->HasSwitch(switches::kVariationsServerURL)) {
    AssociateDefaultFieldTrialConfig(feature_list.get(), GetPlatform());
  }
#endif  // defined(FIELDTRIAL_TESTING_ENABLED)

  bool has_seed = CreateTrialsFromSeed(std::move(low_entropy_provider),
                                       feature_list.get(), safe_seed_manager);

  platform_field_trials->SetupFeatureControllingFieldTrials(has_seed,
                                                            feature_list.get());

  base::FeatureList::SetInstance(std::move(feature_list));

  // This must be called after |local_state_| is initialized.
  platform_field_trials->SetupFieldTrials();

  return has_seed;
}

VariationsSeedStore* VariationsFieldTrialCreator::GetSeedStore() {
  return &seed_store_;
}

Study::Platform VariationsFieldTrialCreator::GetPlatform() {
  if (has_platform_override_)
    return platform_override_;
  return ClientFilterableState::GetCurrentPlatform();
}

}  // namespace variations
