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

#include "cobalt/dom/html_element.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/html_div_element.h"
#include "cobalt/dom/html_element_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class HTMLElementTest : public ::testing::Test {
 protected:
  HTMLElementTest()
      : html_element_context_(NULL, NULL, NULL, NULL, NULL),
        document_(new Document(&html_element_context_, Document::Options())) {}
  ~HTMLElementTest() OVERRIDE {}

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
};

TEST_F(HTMLElementTest, Duplicate) {
  scoped_refptr<HTMLElement> html_element = new HTMLDivElement(document_);
  html_element->SetAttribute("a", "1");
  html_element->SetAttribute("b", "2");
  scoped_refptr<HTMLElement> new_html_element =
      html_element->Duplicate()->AsElement()->AsHTMLElement();
  ASSERT_TRUE(new_html_element);
  EXPECT_TRUE(new_html_element->AsHTMLDivElement());
  EXPECT_EQ(2, new_html_element->attributes()->length());
  EXPECT_EQ("1", new_html_element->GetAttribute("a").value());
  EXPECT_EQ("2", new_html_element->GetAttribute("b").value());
}

}  // namespace dom
}  // namespace cobalt
