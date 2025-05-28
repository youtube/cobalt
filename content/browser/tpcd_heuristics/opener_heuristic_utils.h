// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_UTILS_H_
#define CONTENT_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_UTILS_H_

#include "content/common/content_export.h"

class GURL;

namespace content {

// Common identity providers that open pop-ups, to help estimate the impact of
// third-party cookie blocking and prioritize mitigations. These values are
// emitted in metrics and should not be renumbered.
enum class PopupProvider {
  kUnknown = 0,
  kGoogle = 1,
};

CONTENT_EXPORT PopupProvider GetPopupProvider(const GURL& popup_url);

}  // namespace content

#endif  // CONTENT_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_UTILS_H_
