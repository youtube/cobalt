/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/html_element_factory.h"

#include "base/message_loop.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_anchor_element.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_br_element.h"
#include "cobalt/dom/html_div_element.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_heading_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/html_image_element.h"
#include "cobalt/dom/html_link_element.h"
#include "cobalt/dom/html_paragraph_element.h"
#include "cobalt/dom/html_script_element.h"
#include "cobalt/dom/html_span_element.h"
#include "cobalt/dom/html_style_element.h"
#include "cobalt/dom/html_unknown_element.h"
#include "cobalt/dom/html_video_element.h"
#include "cobalt/dom/testing/stub_css_parser.h"
#include "cobalt/dom/testing/stub_script_runner.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class HTMLElementFactoryTest : public ::testing::Test {
 protected:
  HTMLElementFactoryTest()
      : fetcher_factory_(NULL /* network_module */),
        dom_parser_(new dom_parser::Parser()),
        html_element_context_(&fetcher_factory_, &stub_css_parser_, dom_parser_,
                              NULL /* web_media_player_factory */,
                              &stub_script_runner_, NULL /* image_cache */),
        document_(new Document(&html_element_context_, Document::Options())) {}
  ~HTMLElementFactoryTest() OVERRIDE {}

  loader::FetcherFactory fetcher_factory_;
  Parser* dom_parser_;
  testing::StubCSSParser stub_css_parser_;
  testing::StubScriptRunner stub_script_runner_;
  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
  HTMLElementFactory html_element_factory_;
  MessageLoop message_loop_;
};

TEST_F(HTMLElementFactoryTest, CreateHTMLElement) {
  scoped_refptr<HTMLElement> html_element;

  html_element = html_element_factory_.CreateHTMLElement(document_, "a");
  EXPECT_TRUE(html_element->AsHTMLAnchorElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "body");
  EXPECT_TRUE(html_element->AsHTMLBodyElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "br");
  EXPECT_TRUE(html_element->AsHTMLBRElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "div");
  EXPECT_TRUE(html_element->AsHTMLDivElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "head");
  EXPECT_TRUE(html_element->AsHTMLHeadElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "html");
  EXPECT_TRUE(html_element->AsHTMLHtmlElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "img");
  EXPECT_TRUE(html_element->AsHTMLImageElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "link");
  EXPECT_TRUE(html_element->AsHTMLLinkElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "meta");
  EXPECT_TRUE(html_element->AsHTMLMetaElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "p");
  EXPECT_TRUE(html_element->AsHTMLParagraphElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "script");
  EXPECT_TRUE(html_element->AsHTMLScriptElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "span");
  EXPECT_TRUE(html_element->AsHTMLSpanElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "style");
  EXPECT_TRUE(html_element->AsHTMLStyleElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "video");
  EXPECT_TRUE(html_element->AsHTMLVideoElement());

  html_element = html_element_factory_.CreateHTMLElement(document_, "h1");
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "h2");
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "h3");
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "h4");
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "h5");
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  html_element = html_element_factory_.CreateHTMLElement(document_, "h6");
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());

  html_element = html_element_factory_.CreateHTMLElement(document_, "foo");
  EXPECT_TRUE(html_element->AsHTMLUnknownElement());
}

}  // namespace dom
}  // namespace cobalt
