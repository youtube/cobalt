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

#include "cobalt/dom/dom_parser.h"

#include <memory>
#include <string>

#include "base/threading/platform_thread.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/testing/stub_css_parser.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom/testing/stub_script_runner.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class DOMParserTest : public ::testing::Test {
 protected:
  DOMParserTest();
  ~DOMParserTest() override {}

  testing::StubEnvironmentSettings environment_settings_;
  base::NullDebuggerHooks null_debugger_hooks_;
  loader::FetcherFactory fetcher_factory_;
  loader::LoaderFactory loader_factory_;
  testing::StubCSSParser stub_css_parser_;
  std::unique_ptr<dom_parser::Parser> dom_parser_parser_;
  testing::StubScriptRunner stub_script_runner_;
  HTMLElementContext html_element_context_;
  scoped_refptr<DOMParser> dom_parser_;
};

DOMParserTest::DOMParserTest()
    : fetcher_factory_(NULL /* network_module */),
      loader_factory_("Test" /* name */, &fetcher_factory_,
                      NULL /* resource provider */, null_debugger_hooks_,
                      0 /* encoded_image_cache_capacity */,
                      base::ThreadPriority::DEFAULT),
      dom_parser_parser_(new dom_parser::Parser()),
      html_element_context_(
          &environment_settings_, &fetcher_factory_, &loader_factory_,
          &stub_css_parser_, dom_parser_parser_.get(),
          NULL /* can_play_type_handler */, NULL /* web_media_player_factory */,
          &stub_script_runner_, NULL /* script_value_factory */,
          NULL /* media_source_registry */, NULL /* resource_provider */,
          NULL /* animated_image_tracker */, NULL /* image_cache */,
          NULL /* reduced_image_cache_capacity_manager */,
          NULL /* remote_typeface_cache */, NULL /* mesh_cache */,
          NULL /* dom_stat_tracker */, "" /* language */,
          base::kApplicationStateStarted,
          NULL /* synchronous_loader_interrupt */,
          NULL /* performance */),
      dom_parser_(new DOMParser(&html_element_context_)) {}

TEST_F(DOMParserTest, ParsesXML) {
  const std::string input =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<MPD xmlns:xsi=\"https://www.w3.org/2001/XMLSchema-instance\""
      " xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      " xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\">\n"
      "  <Period>\n"
      "    <SegmentList>\n"
      "      <Initialization sourceURL=\"mp4-main-multi-h264bl_full-.mp4\""
      "       range=\"0-591\"/>\n"
      "      <SegmentURL media=\"mp4-main-multi-h264bl_full-1.m4s\""
      "       mediaRange=\"592-\"/>\n"
      "      <SegmentURL media=\"mp4-main-multi-h264bl_full-2.m4s\"/>\n"
      "    </SegmentList>\n"
      "  </Period>\n"
      "</MPD>\n";
  scoped_refptr<Document> document =
      dom_parser_->ParseFromString(input, kDOMParserSupportedTypeTextXml);
  EXPECT_TRUE(document->IsXMLDocument());
}

}  // namespace dom
}  // namespace cobalt
