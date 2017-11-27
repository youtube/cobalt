// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/test/document_loader.h"
#include "cobalt/webdriver/algorithms.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace webdriver {
namespace {

class IsDisplayedTest : public ::testing::Test {
 protected:
  void SetUp() override {
    GURL url("file:///cobalt/webdriver_test/displayed_test.html");
    document_loader_.Load(url);
  }
  dom::Document* document() { return document_loader_.document(); }

  test::DocumentLoader document_loader_;
};

}  // namespace

TEST_F(IsDisplayedTest, BodyIsDisplayed) {
  EXPECT_TRUE(algorithms::IsDisplayed(document()->body().get()));
  document()->body()->style()->set_display("none", NULL);
  EXPECT_TRUE(algorithms::IsDisplayed(document()->body().get()));
}

TEST_F(IsDisplayedTest, ElementIsShown) {
  // No style set.
  EXPECT_TRUE(
      algorithms::IsDisplayed(document()->GetElementById("displayed").get()));
  // display: none
  EXPECT_FALSE(
      algorithms::IsDisplayed(document()->GetElementById("none").get()));
  // visiblity: hidden
  EXPECT_FALSE(
      algorithms::IsDisplayed(document()->GetElementById("hidden").get()));
}

TEST_F(IsDisplayedTest, HiddenByParentDisplayStyle) {
  // display: none
  EXPECT_FALSE(algorithms::IsDisplayed(
      document()->GetElementById("hiddenparent").get()));
  // No style set, but is a child of hiddenparent.
  EXPECT_FALSE(
      algorithms::IsDisplayed(document()->GetElementById("hiddenchild").get()));
  // No style set, but is a child of hiddenchild.
  EXPECT_FALSE(
      algorithms::IsDisplayed(document()->GetElementById("hiddenlink").get()));
}

TEST_F(IsDisplayedTest, ZeroHeightOrWidthIsNotDisplayed) {
  EXPECT_FALSE(
      algorithms::IsDisplayed(document()->GetElementById("zeroheight").get()));
  EXPECT_FALSE(
      algorithms::IsDisplayed(document()->GetElementById("zerowidth").get()));
}

TEST_F(IsDisplayedTest, ElementWithNestedBlockLevelElementIsDisplayed) {
  EXPECT_TRUE(algorithms::IsDisplayed(
      document()->GetElementById("containsNestedBlock").get()));
}

TEST_F(IsDisplayedTest, ZeroOpacityIsNotDisplayed) {
  EXPECT_FALSE(
      algorithms::IsDisplayed(document()->GetElementById("transparent").get()));
}

TEST_F(IsDisplayedTest, ZeroSizeDivHasDescendentWithSize) {
  EXPECT_TRUE(algorithms::IsDisplayed(
      document()->GetElementById("zerosizeddiv").get()));
}

TEST_F(IsDisplayedTest, ElementHiddenByOverflow) {
  // Div with zero size width and height and overflow: hidden.
  EXPECT_FALSE(
      algorithms::IsDisplayed(document()->GetElementById("bug5022").get()));
}

}  // namespace webdriver
}  // namespace cobalt
