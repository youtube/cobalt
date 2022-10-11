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

#include "cobalt/dom/xml_document.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/testing/stub_window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class XMLDocumentTestInWindow : public ::testing::Test {
 protected:
  XMLDocumentTestInWindow();
  ~XMLDocumentTestInWindow() override;

  std::unique_ptr<testing::StubWindow> window_;
  std::unique_ptr<HTMLElementContext> html_element_context_;
};

XMLDocumentTestInWindow::XMLDocumentTestInWindow()
    : window_(new testing::StubWindow) {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  window_->InitializeWindow();
  html_element_context_.reset(new HTMLElementContext(
      window_->web_context()->environment_settings(), NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      "", base::kApplicationStateStarted, NULL, NULL));
}

XMLDocumentTestInWindow::~XMLDocumentTestInWindow() {
  window_.reset();
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

TEST(XMLDocumentTest, IsXMLDocument) {
  HTMLElementContext html_element_context;
  scoped_refptr<Document> document = new XMLDocument(&html_element_context);
  EXPECT_TRUE(document->IsXMLDocument());
}

TEST(XMLDocumentTest, HasNoBrowsingContext) {
  HTMLElementContext html_element_context;
  scoped_refptr<Document> document = new XMLDocument(&html_element_context);
  EXPECT_FALSE(document->HasBrowsingContext());
}

TEST_F(XMLDocumentTestInWindow, IsXMLDocument) {
  scoped_refptr<Document> document =
      new XMLDocument(html_element_context_.get());
  EXPECT_TRUE(document->IsXMLDocument());
}

TEST_F(XMLDocumentTestInWindow, HasNoBrowsingContext) {
  scoped_refptr<Document> document =
      new XMLDocument(html_element_context_.get());
  EXPECT_FALSE(document->HasBrowsingContext());
}

}  // namespace dom
}  // namespace cobalt
