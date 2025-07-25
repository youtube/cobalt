// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_PROMO_SOURCE_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_PROMO_SOURCE_H_

// An histogram to report the source of the default browser promo.
// Used for UMA, do not reorder.
enum class DefaultBrowserPromoSource {
  kSettings = 0,
  kOmnibox,
  kExternalIntent,
  kSetUpList,
  kMaxValue = kSetUpList,
};

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_PROMO_SOURCE_H_
