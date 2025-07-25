// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_ACCESSIBILITY_H_
#define PDF_ACCESSIBILITY_H_

#include <stdint.h>

#include <vector>

namespace chrome_pdf {

class PDFEngine;
struct AccessibilityCharInfo;
struct AccessibilityPageInfo;
struct AccessibilityPageObjects;
struct AccessibilityTextRunInfo;

// Retrieve `page_info`, `text_runs`, `chars`, and `page_objects` from
// `engine` for the page at 0-indexed `page_index`. Returns true on success with
// all out parameters filled, or false on failure with all out parameters
// untouched.
bool GetAccessibilityInfo(PDFEngine* engine,
                          int32_t page_index,
                          AccessibilityPageInfo& page_info,
                          std::vector<AccessibilityTextRunInfo>& text_runs,
                          std::vector<AccessibilityCharInfo>& chars,
                          AccessibilityPageObjects& page_objects);

}  // namespace chrome_pdf

#endif  // PDF_ACCESSIBILITY_H_
