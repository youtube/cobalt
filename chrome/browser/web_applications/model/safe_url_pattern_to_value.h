// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_SAFE_URL_PATTERN_TO_VALUE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_SAFE_URL_PATTERN_TO_VALUE_H_

#include "base/values.h"

namespace blink {
struct SafeUrlPattern;
}

namespace web_app {

// Converts a `SafeUrlPattern` to a `DictValue` for debugging.
base::DictValue ToValue(const blink::SafeUrlPattern& pattern);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_SAFE_URL_PATTERN_TO_VALUE_H_
