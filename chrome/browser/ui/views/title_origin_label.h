// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TITLE_ORIGIN_LABEL_H_
#define CHROME_BROWSER_UI_VIEWS_TITLE_ORIGIN_LABEL_H_

#include <memory>

#include "ui/views/controls/label.h"

// Creates a new label to be used for dialog titles that contain an origin that
// need to be elided from the front. The label will also ignored by screen
// readers (since the bubbles handle the context).
// TODO(crbug.com/987715): For now this is a simplistic implementation that
// elides the entire string from the front, which works well for English strings
// that start with the origin, but not so well for other languages.
std::unique_ptr<views::Label> CreateTitleOriginLabel(
    const std::u16string& text);

#endif  // CHROME_BROWSER_UI_VIEWS_TITLE_ORIGIN_LABEL_H_
