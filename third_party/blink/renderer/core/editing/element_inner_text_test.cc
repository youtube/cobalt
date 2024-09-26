// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ElementInnerTest : public EditingTestBase {};

// http://crbug.com/877498
TEST_F(ElementInnerTest, ListItemWithLeadingWhiteSpace) {
  SetBodyContent("<li id=target> abc</li>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/877470
TEST_F(ElementInnerTest, SVGElementAsTableCell) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-cell'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/878725
TEST_F(ElementInnerTest, SVGElementAsTableRow) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-row'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

// https://crbug.com/947422
TEST_F(ElementInnerTest, OverflowingListItemWithFloatFirstLetter) {
  InsertStyleElement(
      "div { display: list-item; overflow: hidden; }"
      "div::first-letter { float: right; }");
  SetBodyContent("<div id=target>foo</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("foo", target.innerText());
}

// https://crbug.com/1164747
TEST_F(ElementInnerTest, GetInnerTextWithoutUpdate) {
  SetBodyContent("<div id=target>ab<span>c</span></div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
  EXPECT_EQ("abc", target.GetInnerTextWithoutUpdate());
}

}  // namespace blink
