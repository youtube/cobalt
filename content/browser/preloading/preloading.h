// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_H_

#include "content/public/browser/preloading.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

// Defines various //content triggering mechanisms which trigger different
// preloading operations mentioned in content/public/browser/preloading.h.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// If you change this, please follow the process in
// go/preloading-dashboard-updates to update the mapping reflected in
// dashboard, or if you are not a Googler, please file an FYI bug on
// https://crbug.new with component Internals>Preload.
namespace content_preloading_predictor {
// Advance numbering by +1 when adding a new element.
//
// Please limit content-internal `PreloadingPredictor` between 50 to 99
// (inclusive) as 0 to 49 are reserved for content-public definitions, and 100
// and beyond are reserved for embedder definitions. Both the value and the name
// should be unique across all the namespaces.

// This API allows an origin to list possible navigation URLs that the user
// might navigate to in order to perform preloading operations.
// For more details please see:
// https://wicg.github.io/nav-speculation/prerendering.html#speculation-rules
static constexpr PreloadingPredictor kSpeculationRules(50, "SpeculationRules");

// When a mouse down of a mouse back button is seen.
static constexpr PreloadingPredictor kMouseBackButton(51, "MouseBackButton");

// Same with the kSpeculationRules, but the rules are injected from an isolated
// world, i.e. extensions or embedder's built-in features.
static constexpr PreloadingPredictor kSpeculationRulesFromIsolatedWorld(
    52,
    "SpeculationRulesFromIsolatedWorld");

// TODO(crbug.com/1309934): Add more predictors as we integrate Preloading
// logging.
}  // namespace content_preloading_predictor

CONTENT_EXPORT base::StringPiece PreloadingTypeToString(PreloadingType type);

PreloadingPredictor GetPredictorForSpeculationRules(
    blink::mojom::SpeculationInjectionWorld world);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_H_
