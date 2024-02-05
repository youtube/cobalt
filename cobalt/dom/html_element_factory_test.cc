// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/html_element_factory.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/html_anchor_element.h"
#include "cobalt/dom/html_audio_element.h"
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
#include "cobalt/dom/html_meta_element.h"
#include "cobalt/dom/html_paragraph_element.h"
#include "cobalt/dom/html_script_element.h"
#include "cobalt/dom/html_span_element.h"
#include "cobalt/dom/html_style_element.h"
#include "cobalt/dom/html_title_element.h"
#include "cobalt/dom/html_unknown_element.h"
#include "cobalt/dom/html_video_element.h"
#include "cobalt/dom/lottie_player.h"
#include "cobalt/dom/testing/stub_css_parser.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader_factory.h"
#include "cobalt/script/testing/stub_script_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class HTMLElementFactoryTest : public ::testing::Test {
 protected:
  HTMLElementFactoryTest()
      : fetcher_factory_(NULL /* network_module */),
        loader_factory_("Test" /* name */, &fetcher_factory_,
                        NULL /* resource loader */, null_debugger_hooks_,
                        0 /* encoded_image_cache_capacity */,
                        base::ThreadType::kDefault),
        dom_parser_(new dom_parser::Parser()),
        dom_stat_tracker_(new DomStatTracker("HTMLElementFactoryTest")),
        html_element_context_(
            &environment_settings_, &fetcher_factory_, &loader_factory_,
            &stub_css_parser_, dom_parser_.get(),
            NULL /* can_play_type_handler */,
            NULL /* web_media_player_factory */, &stub_script_runner_,
            NULL /* script_value_factory */, NULL /* media_source_registry */,
            NULL /* resource_provider */, NULL /* animated_image_tracker */,
            NULL /* image_cache */,
            NULL /* reduced_image_cache_capacity_manager */,
            NULL /* remote_typeface_cache */, NULL /* mesh_cache */,
            dom_stat_tracker_.get(), "" /* language */,
            base::kApplicationStateStarted,
            NULL /* synchronous_loader_interrupt */, NULL /* performance */),
        document_(new Document(&html_element_context_)) {}
  ~HTMLElementFactoryTest() override {}

  testing::StubEnvironmentSettings environment_settings_;
  base::NullDebuggerHooks null_debugger_hooks_;
  loader::FetcherFactory fetcher_factory_;
  loader::LoaderFactory loader_factory_;
  std::unique_ptr<Parser> dom_parser_;
  testing::StubCSSParser stub_css_parser_;
  script::testing::StubScriptRunner stub_script_runner_;
  std::unique_ptr<DomStatTracker> dom_stat_tracker_;
  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
  HTMLElementFactory html_element_factory_;
  base::MessageLoop message_loop_;
};

TEST_F(HTMLElementFactoryTest, CreateHTMLElement) {
  scoped_refptr<HTMLElement> html_element;

  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("a"));
  EXPECT_TRUE(html_element->AsHTMLAnchorElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLAnchorElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("audio"));
  EXPECT_TRUE(html_element->AsHTMLAudioElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLAudioElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("body"));
  EXPECT_TRUE(html_element->AsHTMLBodyElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLBodyElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("br"));
  EXPECT_TRUE(html_element->AsHTMLBRElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLBRElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("div"));
  EXPECT_TRUE(html_element->AsHTMLDivElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLDivElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("head"));
  EXPECT_TRUE(html_element->AsHTMLHeadElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLHeadElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("html"));
  EXPECT_TRUE(html_element->AsHTMLHtmlElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLHtmlElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("img"));
  EXPECT_TRUE(html_element->AsHTMLImageElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLImageElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("link"));
  EXPECT_TRUE(html_element->AsHTMLLinkElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLLinkElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("meta"));
  EXPECT_TRUE(html_element->AsHTMLMetaElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLMetaElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("p"));
  EXPECT_TRUE(html_element->AsHTMLParagraphElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLParagraphElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("script"));
  EXPECT_TRUE(html_element->AsHTMLScriptElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLScriptElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("span"));
  EXPECT_TRUE(html_element->AsHTMLSpanElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLSpanElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("style"));
  EXPECT_TRUE(html_element->AsHTMLStyleElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLStyleElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("title"));
  EXPECT_TRUE(html_element->AsHTMLTitleElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLTitleElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("video"));
  EXPECT_TRUE(html_element->AsHTMLVideoElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLVideoElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("lottie-player"));
  EXPECT_TRUE(html_element->AsLottiePlayer());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object LottiePlayer]");

  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("h1"));
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLHeadingElement]");
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("h2"));
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("h3"));
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("h4"));
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("h5"));
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());
  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("h6"));
  EXPECT_TRUE(html_element->AsHTMLHeadingElement());

  html_element = html_element_factory_.CreateHTMLElement(
      document_, base_token::Token("foo"));
  EXPECT_TRUE(html_element->AsHTMLUnknownElement());
  EXPECT_EQ(html_element->GetInlineSourceLocation().file_path,
            "[object HTMLUnknownElement]");
}

}  // namespace dom
}  // namespace cobalt
