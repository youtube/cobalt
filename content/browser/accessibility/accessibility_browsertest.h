// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/public/test/content_browser_test.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/native_widget_types.h"

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_BROWSERTEST_H_

namespace content {

class AccessibilityBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityBrowserTest() = default;

  AccessibilityBrowserTest(const AccessibilityBrowserTest&) = delete;
  AccessibilityBrowserTest& operator=(const AccessibilityBrowserTest&) = delete;

  ~AccessibilityBrowserTest() override = default;

 protected:
  gfx::NativeViewAccessible GetRendererAccessible();
  void ExecuteScript(const std::u16string& script);
  void LoadInitialAccessibilityTreeFromHtml(
      const std::string& html,
      ui::AXMode accessibility_mode = ui::kAXModeComplete);

  void LoadInputField();
  void LoadTextareaField();
  void LoadSampleParagraph(ui::AXMode accessibility_mode = ui::kAXModeComplete);
  void LoadSampleParagraphInScrollableEditable();
  void LoadSampleParagraphInScrollableDocument(
      ui::AXMode accessibility_mode = ui::kAXModeComplete);

  static std::string InputContentsString();
  static std::string TextAreaContentsString();
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_BROWSERTEST_H_
