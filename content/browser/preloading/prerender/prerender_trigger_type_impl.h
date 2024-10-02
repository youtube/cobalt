// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_TRIGGER_TYPE_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_TRIGGER_TYPE_IMPL_H_

#include "content/public/browser/prerender_trigger_type.h"

namespace content {

// Checks if the type is kSpeculationRule*. Recommends to use this function to
// keep the code robust against adding more trigger types in the future.
bool IsSpeculationRuleType(PrerenderTriggerType type);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_TRIGGER_TYPE_IMPL_H_
