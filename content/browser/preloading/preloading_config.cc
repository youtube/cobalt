// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_config.h"

#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "content/browser/preloading/preloading.h"
#include "content/common/features.h"
#include "content/public/browser/preloading.h"

namespace content {

namespace {

// Allows configuring preloading features via a JSON string. This string should
// contain a JSON array of objects. Each object should specify a preloading_type
// key (a string to specify which preloading type is being configured) and a
// predictor key (a string to specify which predictor is being configured). Then
// each object can specify some parameters to tune. Supported parameters are:
//  * holdback: whether this preloading_type, predictor combination should be
//    held back for counterfactual evaluation.
//  * sampling_likelihood: the fraction of preloading attempts that will be
//    logged in UKM. See crbug.com/1411841#c3 to see how the sampling_likelihood
//    default values are determined.
constexpr base::FeatureParam<std::string> kPreloadingConfigParam{
    &features::kPreloadingConfig, "preloading_config", R"(
[{
  "preloading_type": "Prerender",
  "preloading_predictor": "SpeculationRulesFromIsolatedWorld",
  "sampling_likelihood": 1.000000
}, {
  "preloading_type": "Prefetch",
  "preloading_predictor": "OmniboxSearchPredictor",
  "sampling_likelihood": 1.000000
}, {
  "preloading_type": "Prefetch",
  "preloading_predictor": "OmniboxMousePredictor",
  "sampling_likelihood": 1.000000
}, {
  "preloading_type": "Prerender",
  "preloading_predictor": "SpeculationRules",
  "sampling_likelihood": 1.000000
}, {
  "preloading_type": "NoStatePrefetch",
  "preloading_predictor": "OmniboxDirectURLInput",
  "sampling_likelihood": 1.000000
}, {
  "preloading_type": "Prerender",
  "preloading_predictor": "PointerDownOnBookmarkBar",
  "sampling_likelihood": 1.000000
}, {
  "preloading_type": "Prerender",
  "preloading_predictor": "MouseHoverOnBookmarkBar",
  "sampling_likelihood": 0.501609
}, {
  "preloading_type": "Prerender",
  "preloading_predictor": "BackGestureNavigation",
  "sampling_likelihood": 0.444169
}, {
  "preloading_type": "Prerender",
  "preloading_predictor": "MouseBackButton",
  "sampling_likelihood": 0.147112
}, {
  "preloading_type": "Prerender",
  "preloading_predictor": "DefaultSearchEngine",
  "sampling_likelihood": 0.134034
}, {
  "preloading_type": "Prefetch",
  "preloading_predictor": "DefaultSearchEngine",
  "sampling_likelihood": 0.030895
}, {
  "preloading_type": "Prerender",
  "preloading_predictor": "OmniboxDirectURLInput",
  "sampling_likelihood": 0.015387
}, {
  "preloading_type": "Prerender",
  "preloading_predictor": "BackButtonHover",
  "sampling_likelihood": 0.013294
}, {
  "preloading_type": "NoStatePrefetch",
  "preloading_predictor": "LinkRel",
  "sampling_likelihood": 0.010198
}, {
  "preloading_type": "Prefetch",
  "preloading_predictor": "SpeculationRules",
  "sampling_likelihood": 0.005237
}, {
  "preloading_type": "Preconnect",
  "preloading_predictor": "PointerDownOnAnchor",
  "sampling_likelihood": 0.000240
}]
)"};

static PreloadingConfig* g_config_override = nullptr;

}  // namespace

PreloadingConfig& PreloadingConfig::GetInstance() {
  static base::NoDestructor<PreloadingConfig> config;
  static bool initialized = false;
  if (!initialized) {
    config->ParseConfig();
    initialized = true;
  }

  if (g_config_override) {
    return *g_config_override;
  }
  return *config;
}

PreloadingConfig::PreloadingConfig() = default;

PreloadingConfig* PreloadingConfig::OverrideForTesting(
    PreloadingConfig* config_override) {
  raw_ptr<PreloadingConfig> old_override = g_config_override;
  g_config_override = config_override;
  return old_override;
}

void PreloadingConfig::ParseConfig() {
  entries_.clear();

  if (!base::FeatureList::IsEnabled(features::kPreloadingConfig)) {
    return;
  }
  // Throughout parsing the config, if we fail to parse, we silently skip the
  // config and use the default values.
  absl::optional<base::Value> config_value =
      base::JSONReader::Read(kPreloadingConfigParam.Get());
  if (!config_value) {
    return;
  }
  base::Value::List* entries = config_value->GetIfList();
  if (!entries) {
    return;
  }

  for (const base::Value& entry : *entries) {
    const base::Value::Dict* config_dict = entry.GetIfDict();
    DCHECK(config_dict);
    if (!config_dict) {
      continue;
    }

    const std::string* preloading_type =
        config_dict->FindString("preloading_type");
    DCHECK(preloading_type);
    if (!preloading_type) {
      continue;
    }

    const std::string* preloading_predictor =
        config_dict->FindString("preloading_predictor");
    DCHECK(preloading_predictor);
    if (!preloading_predictor) {
      continue;
    }

    entries_.emplace(Key(*preloading_type, *preloading_predictor),
                     Entry::FromDict(config_dict));
  }
}

PreloadingConfig::~PreloadingConfig() = default;

bool PreloadingConfig::ShouldHoldback(PreloadingType preloading_type,
                                      PreloadingPredictor predictor) {
  Entry entry = entries_[Key::FromEnums(preloading_type, predictor)];
  return entry.holdback_;
}

void PreloadingConfig::SetHoldbackForTesting(PreloadingType preloading_type,
                                             PreloadingPredictor predictor,
                                             bool holdback) {
  Entry entry;
  entry.holdback_ = holdback;
  entries_.emplace(
      Key(PreloadingTypeToString(preloading_type), predictor.name()), entry);
}

void PreloadingConfig::SetHoldbackForTesting(base::StringPiece preloading_type,
                                             base::StringPiece predictor,
                                             bool holdback) {
  Entry entry;
  entry.holdback_ = holdback;
  entries_.emplace(Key(preloading_type, predictor), entry);
}

double PreloadingConfig::SamplingLikelihood(PreloadingType preloading_type,
                                            PreloadingPredictor predictor) {
  Entry entry = entries_[Key::FromEnums(preloading_type, predictor)];
  return entry.sampling_likelihood_;
}

PreloadingConfig::Key::Key(base::StringPiece preloading_type,
                           base::StringPiece predictor)
    : preloading_type_(preloading_type), predictor_(predictor) {}

PreloadingConfig::Key PreloadingConfig::Key::FromEnums(
    PreloadingType preloading_type,
    PreloadingPredictor predictor) {
  return Key(PreloadingTypeToString(preloading_type), predictor.name());
}

PreloadingConfig::Entry PreloadingConfig::Entry::FromDict(
    const base::Value::Dict* dict) {
  Entry entry;
  absl::optional<bool> holdback = dict->FindBool("holdback");
  if (holdback) {
    entry.holdback_ = *holdback;
  }
  absl::optional<double> sampling_likelihood =
      dict->FindDouble("sampling_likelihood");
  if (sampling_likelihood) {
    entry.sampling_likelihood_ = *sampling_likelihood;
  }
  return entry;
}

bool PreloadingConfig::KeyCompare::operator()(
    const PreloadingConfig::Key& lhs,
    const PreloadingConfig::Key& rhs) const {
  return std::tie(lhs.preloading_type_, lhs.predictor_) <
         std::tie(rhs.preloading_type_, rhs.predictor_);
}

}  // namespace content
