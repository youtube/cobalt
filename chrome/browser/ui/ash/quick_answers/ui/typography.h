// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_UI_TYPOGRAPHY_H_
#define CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_UI_TYPOGRAPHY_H_

#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "ui/gfx/font_list.h"

namespace quick_answers {

const gfx::FontList GetFirstLineFontList(Design design);
int GetFirstLineHeight(Design design);

const gfx::FontList GetSecondLineFontList(Design design);
int GetSecondLineHeight(Design design);

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_UI_TYPOGRAPHY_H_
