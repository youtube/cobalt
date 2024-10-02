// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

class LayoutNGTextControlSingleLineTest : public RenderingTest {};

TEST_F(LayoutNGTextControlSingleLineTest, VisualOverflowCleared) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #input {
        width: 50px; height: 50px; box-shadow: 5px 5px 5px black;
      }
    </style>
    <input id=input type="text"></input.
  )HTML");
  auto* input = To<LayoutBox>(GetLayoutObjectByElementId("input"));
  EXPECT_EQ(LayoutRect(-3, -3, 74, 72), input->SelfVisualOverflowRect());
  To<Element>(input->GetNode())
      ->setAttribute(html_names::kStyleAttr, "box-shadow: initial");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(LayoutRect(0, 0, 58, 56), input->SelfVisualOverflowRect());
}

}  // anonymous namespace

}  // namespace blink
