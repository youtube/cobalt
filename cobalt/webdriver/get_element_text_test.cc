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
#include "cobalt/base/clock.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_parser.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_br_element.h"
#include "cobalt/dom/html_div_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/html_paragraph_element.h"
#include "cobalt/dom/text.h"
#include "cobalt/webdriver/algorithms.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace webdriver {
namespace {
const char kBlankDocument[] = "<html><head/><body/><html>";
class GetElementTextTest : public ::testing::Test {
 protected:
  GetElementTextTest()
      : css_parser_(css_parser::Parser::Create()),
        dom_stat_tracker_(new dom::DomStatTracker("GetElementTextTest")),
        html_element_context_(NULL, css_parser_.get(), NULL, NULL, NULL, NULL,
                              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                              dom_stat_tracker_.get(), "",
                              base::kApplicationStateStarted, NULL) {}

  void SetUp() override {
    dom::Document::Options options;
    options.viewport_size = math::Size(1920, 1080);
    options.navigation_start_clock = new base::SystemMonotonicClock();
    document_ = new dom::Document(&html_element_context_, options);
    document_->AppendChild(new dom::HTMLHtmlElement(document_.get()));
    document_->html()->AppendChild(new dom::HTMLHeadElement(document_.get()));
    document_->html()->AppendChild(new dom::HTMLBodyElement(document_.get()));
    div_ = new dom::HTMLDivElement(document_.get());
    document_->body()->AppendChild(div_);

    document_->SampleTimelineTime();
  }

  void AppendText(const char* text_content) {
    div_->AppendChild(new dom::Text(document_.get(), text_content));
  }

  void AppendBR() {
    div_->AppendChild(new dom::HTMLBRElement(document_.get()));
  }

  void AppendParagraphWithDisplayStyle(const char* display_style) {
    scoped_refptr<dom::HTMLParagraphElement> p(
        new dom::HTMLParagraphElement(document_.get()));
    p->style()->set_display(display_style, NULL);
    div_->AppendChild(p);
  }

  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom::DomStatTracker> dom_stat_tracker_;
  dom::HTMLElementContext html_element_context_;
  scoped_refptr<dom::Document> document_;
  scoped_refptr<dom::HTMLDivElement> div_;
};
}  // namespace

TEST_F(GetElementTextTest, ZeroSpaceWidthIsRemoved) {
  AppendText(u8"a\u200bb\u200ec\u200f\u200bd");
  EXPECT_STREQ("abcd", algorithms::GetElementText(div_.get()).c_str());
}

TEST_F(GetElementTextTest, NewLinesAreConvertedToSpaces) {
  AppendText("a\r\nb\rc\nd");
  EXPECT_STREQ("a b c d", algorithms::GetElementText(div_.get()).c_str());
}

TEST_F(GetElementTextTest, NoWrapStyle) {
  div_->style()->set_white_space("nowrap", NULL);
  AppendText(u8"a\n\nb\nc\td\u2028e\u2029f\u00a0g");
  EXPECT_STREQ("a b c d e f g", algorithms::GetElementText(div_.get()).c_str());
}

TEST_F(GetElementTextTest, Uppercase) {
  div_->style()->set_text_transform("uppercase", NULL);
  AppendText("AbCdE fGhIj kl-mn-Op");
  EXPECT_STREQ("ABCDE FGHIJ KL-MN-OP",
               algorithms::GetElementText(div_.get()).c_str());
}

TEST_F(GetElementTextTest, BrElementBecomesNewLine) {
  AppendText("a");
  AppendBR();
  AppendText("b");
  AppendBR();
  AppendBR();
  AppendText("c");

  EXPECT_STREQ("a\nb\n\nc", algorithms::GetElementText(div_.get()).c_str());
}

TEST_F(GetElementTextTest, Blocks) {
  AppendText("a");
  AppendParagraphWithDisplayStyle("block");
  AppendText("b");
  AppendParagraphWithDisplayStyle("block");
  AppendParagraphWithDisplayStyle("block");
  AppendText("c");

  EXPECT_STREQ("a\nb\nc", algorithms::GetElementText(div_.get()).c_str());
}

TEST_F(GetElementTextTest, LinesAreTrimmed) {
  AppendText(" a \n");
  AppendBR();
  AppendText("  b  \n\n");
  AppendBR();
  AppendText("\nc  ");
  AppendBR();
  AppendText("  ");

  EXPECT_STREQ("a\nb\nc", algorithms::GetElementText(div_.get()).c_str());
}

TEST_F(GetElementTextTest, WholeCodePointsAreProcessed) {
  AppendText(u8"a\u200b\u2020\u2029\u2022\U0002070Eb");
  EXPECT_STREQ(u8"a\u2020 \u2022\U0002070Eb",
               algorithms::GetElementText(div_.get()).c_str());
}

}  // namespace webdriver
}  // namespace cobalt
