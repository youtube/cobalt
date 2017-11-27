// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/text.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

//////////////////////////////////////////////////////////////////////////
// TextTest
//////////////////////////////////////////////////////////////////////////

class TextTest : public ::testing::Test {
 protected:
  TextTest();
  ~TextTest() override;

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
};

TextTest::TextTest() {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  document_ = new Document(&html_element_context_);
}

TextTest::~TextTest() {
  document_ = NULL;
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(TextTest, Duplicate) {
  scoped_refptr<Text> text = new Text(document_, "text");
  scoped_refptr<Text> new_text = text->Duplicate()->AsText();
  ASSERT_TRUE(new_text);
  EXPECT_EQ("text", new_text->data());
}

TEST_F(TextTest, CheckAttach) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));

  scoped_refptr<Node> text = root->AppendChild(new Text(document_, "text"));
  scoped_refptr<Node> other_text = new Text(document_, "other_text");

  // Checks that we can't attach text nodes to other text nodes.
  EXPECT_EQ(NULL, text->AppendChild(other_text));
  EXPECT_EQ(NULL, text->InsertBefore(other_text, NULL));
}

TEST_F(TextTest, NodeValue) {
  scoped_refptr<Text> text = new Text(document_, "text");
  EXPECT_EQ("text", text->node_value().value());
}

TEST_F(TextTest, TextContent) {
  scoped_refptr<Text> text = new Text(document_, "text");
  EXPECT_EQ("text", text->text_content().value());
}

}  // namespace dom
}  // namespace cobalt
