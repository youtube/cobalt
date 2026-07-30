// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class VirtualKeyboardTest : public testing::Test {
 protected:
  VirtualKeyboardTest()
      : holder_(DummyPageHolder::CreateAndCommitNavigation(
            KURL("https://example.com"),
            gfx::Size(411, 777))) {}

  VirtualKeyboard& GetVirtualKeyboard() {
    return *VirtualKeyboard::virtualKeyboard(
        *holder_->GetFrame().DomWindow()->navigator());
  }

  int ViewportWidth() const {
    return holder_->GetFrame().DomWindow()->innerWidth();
  }
  int ViewportHeight() const {
    return holder_->GetFrame().DomWindow()->innerHeight();
  }

  String EnvValue(UADefinedVariable variable) {
    DocumentStyleEnvironmentVariables& vars =
        holder_->GetDocument().GetStyleEngine().EnsureEnvironmentVariables();
    CSSVariableData* data = vars.ResolveVariable(
        StyleEnvironmentVariables::GetVariableName(variable, nullptr), {});
    EXPECT_TRUE(data);
    return data ? data->Serialize() : String();
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> holder_;
};

TEST_F(VirtualKeyboardTest,
       KeyboardInsetEnvironmentVariablesUseViewportInsets) {
  const int viewport_width = ViewportWidth();
  const int viewport_height = ViewportHeight();
  ASSERT_GT(viewport_width, 0);
  ASSERT_GT(viewport_height, 0);

  const int keyboard_height = 343;
  const int keyboard_top = viewport_height - keyboard_height;
  ASSERT_GT(keyboard_top, 0);

  GetVirtualKeyboard().VirtualKeyboardOverlayChanged(
      gfx::Rect(0, keyboard_top, viewport_width, keyboard_height));

  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(keyboard_top),
            EnvValue(UADefinedVariable::kKeyboardInsetTop));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetLeft));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetBottom));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetRight));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(viewport_width),
            EnvValue(UADefinedVariable::kKeyboardInsetWidth));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(keyboard_height),
            EnvValue(UADefinedVariable::kKeyboardInsetHeight));
}

TEST_F(VirtualKeyboardTest,
       KeyboardInsetEnvironmentVariablesAreZeroWhenKeyboardHidden) {
  const int viewport_width = ViewportWidth();
  ASSERT_GT(viewport_width, 0);

  GetVirtualKeyboard().VirtualKeyboardOverlayChanged(
      gfx::Rect(0, 10, viewport_width, 100));
  GetVirtualKeyboard().VirtualKeyboardOverlayChanged(gfx::Rect(10, 20, 100, 0));

  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetTop));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetLeft));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetBottom));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetRight));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetWidth));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetHeight));
}

}  // namespace blink
