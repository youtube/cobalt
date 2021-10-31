// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/html_link_element.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/cssom/testing/mock_css_parser.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom/window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace cobalt {
namespace dom {

class HTMLLinkElementMock : public HTMLLinkElement {
 public:
  explicit HTMLLinkElementMock(Document* document)
      : HTMLLinkElement(document) {}
  void Obtain() { obtained_ = true; }
  bool obtained_ = false;
  bool obtained() { return obtained_; }
};

class DocumentMock : public Document {
 public:
  explicit DocumentMock(HTMLElementContext* context) : Document(context) {}
  scoped_refptr<HTMLLinkElementMock> CreateElement(
      const std::string& local_name) {
    return scoped_refptr<HTMLLinkElementMock>(new HTMLLinkElementMock(this));
  }
};

class HtmlLinkElementTest : public ::testing::Test {
 protected:
  HtmlLinkElementTest()
      : dom_stat_tracker_(new DomStatTracker("HtmlLinkElementTest")),
        html_element_context_(&environment_settings_, NULL, NULL, &css_parser_,
                              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                              NULL, NULL, NULL, NULL, dom_stat_tracker_.get(),
                              "", base::kApplicationStateStarted, NULL, NULL),
        document_(new DocumentMock(&html_element_context_)),
        message_loop_(base::MessageLoop::TYPE_DEFAULT) {}

  scoped_refptr<DocumentMock> document() { return document_; }
  scoped_refptr<HTMLLinkElementMock> CreateDocumentWithLinkElement(
      std::string rel = "");

 private:
  std::unique_ptr<DomStatTracker> dom_stat_tracker_;
  testing::StubEnvironmentSettings environment_settings_;
  NiceMock<cssom::testing::MockCSSParser> css_parser_;
  HTMLElementContext html_element_context_;
  scoped_refptr<DocumentMock> document_;
  base::MessageLoop message_loop_;
};

scoped_refptr<HTMLLinkElementMock>
HtmlLinkElementTest::CreateDocumentWithLinkElement(std::string rel) {
  scoped_refptr<HTMLLinkElementMock> element_ =
      document_->CreateElement("link");
  if (!rel.empty()) {
    element_->SetAttribute("rel", rel);
  }
  document_->AppendChild(element_);
  return element_;
}

TEST_F(HtmlLinkElementTest, StylesheetRelAttribute) {
  scoped_refptr<HTMLLinkElementMock> el =
      CreateDocumentWithLinkElement("stylesheet");
  EXPECT_TRUE(el->obtained());
}

TEST_F(HtmlLinkElementTest, SplashScreenRelAttribute) {
  scoped_refptr<HTMLLinkElementMock> el =
      CreateDocumentWithLinkElement("splashscreen");
  EXPECT_TRUE(el->obtained());

  el = CreateDocumentWithLinkElement("music_splashscreen");
  EXPECT_TRUE(el->obtained());

  el = CreateDocumentWithLinkElement("music-_\\2_splashscreen");
  EXPECT_TRUE(el->obtained());
}

TEST_F(HtmlLinkElementTest, BadSplashScreenRelAttribute) {
  scoped_refptr<HTMLLinkElementMock> el =
      CreateDocumentWithLinkElement("bad*_splashscreen");
  EXPECT_FALSE(el->obtained());

  el = CreateDocumentWithLinkElement("badsplashscreen");
  EXPECT_FALSE(el->obtained());

  el = CreateDocumentWithLinkElement("splashscreen_bad");
  EXPECT_FALSE(el->obtained());
}

}  // namespace dom
}  // namespace cobalt
