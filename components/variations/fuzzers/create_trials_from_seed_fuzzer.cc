// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_processor.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations_test_utils.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace variations {
namespace {

class TestOverrideStringCallback {
 public:
  typedef std::map<uint32_t, std::u16string> OverrideMap;

  TestOverrideStringCallback()
      : callback_(base::BindRepeating(&TestOverrideStringCallback::Override,
                                      base::Unretained(this))) {}
  TestOverrideStringCallback(const TestOverrideStringCallback&) = delete;
  TestOverrideStringCallback& operator=(const TestOverrideStringCallback&) =
      delete;

  virtual ~TestOverrideStringCallback() {}

  const VariationsSeedProcessor::UIStringOverrideCallback& callback() const {
    return callback_;
  }

  const OverrideMap& overrides() const { return overrides_; }

 private:
  void Override(uint32_t hash, const std::u16string& string) {
    overrides_[hash] = string;
  }

  VariationsSeedProcessor::UIStringOverrideCallback callback_;
  OverrideMap overrides_;
};

struct Environment {
  Environment() { base::CommandLine::Init(0, nullptr); }

  base::AtExitManager at_exit_manager;
};

std::unique_ptr<ClientFilterableState> MockChromeClientFilterableState() {
  auto client_state = CreateDummyClientFilterableState();
  client_state->locale = "en-CA";
  client_state->reference_date = base::Time::Now();
  client_state->version = base::Version("20.0.0.0");
  client_state->channel = Study::STABLE;
  client_state->form_factor = Study::PHONE;
  client_state->platform = Study::PLATFORM_ANDROID;
  return client_state;
}

}  // namespace

void CreateTrialsFromStudyFuzzer(const VariationsSeed& seed) {
  base::FieldTrialList field_trial_list;
  base::FeatureList feature_list;

  TestOverrideStringCallback override_callback;
  // TODO(b/244252663): Add more coverage of client_state and entropy
  // provider arguments.
  auto client_state = MockChromeClientFilterableState();
  EntropyProviders entropy_providers("client_id", {7999, 8000});

  VariationsSeedProcessor().CreateTrialsFromSeed(
      seed, *client_state, override_callback.callback(), entropy_providers,
      &feature_list);
}

DEFINE_PROTO_FUZZER(const VariationsSeed& seed) {
  static Environment env;
  CreateTrialsFromStudyFuzzer(seed);
}

}  // namespace variations
