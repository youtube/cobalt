// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_metrics.h"

#import "base/metrics/histogram_functions.h"

namespace {
// Key of the UMA IOS.TabGrid.CloseTabs histogram.
const char kTabGridCloseMultipleTabsHistogram[] = "IOS.TabGrid.CloseTabs";
}  // namespace

// Key of the UMA IOS.TabSwitcher.PageChangeInteraction histogram.
const char kUMATabSwitcherPageChangeInteractionHistogram[] =
    "IOS.TabSwitcher.PageChangeInteraction";

// Key of the UMA IOS.Incognito.GridStatus histogram.
const char kUMAIncognitoGridStatusHistogram[] = "IOS.Incognito.GridStatus";

void RecordTabGridCloseTabsCount(int count) {
  base::UmaHistogramCounts100(kTabGridCloseMultipleTabsHistogram, count);
}

void RecordIncognitoGridStatus(IncognitoGridStatus status) {
  base::UmaHistogramEnumeration(kUMAIncognitoGridStatusHistogram, status);
}
