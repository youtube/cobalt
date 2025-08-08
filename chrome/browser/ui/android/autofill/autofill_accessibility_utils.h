// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_ACCESSIBILITY_UTILS_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_ACCESSIBILITY_UTILS_H_

#include <string>

namespace autofill {

void AnnounceTextForA11y(const std::u16string& message);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_ACCESSIBILITY_UTILS_H_
