/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/text.h"

#include "cobalt/dom/element.h"
#include "cobalt/dom/stats.h"
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
  ~TextTest() OVERRIDE;
};

TextTest::TextTest() { EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks()); }

TextTest::~TextTest() { EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks()); }

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(TextTest, CheckAttach) {
  scoped_refptr<Element> root = new Element();

  scoped_refptr<Node> text = root->AppendChild(new Text("text"));
  scoped_refptr<Node> other_text = new Text("other_text");

  // Checks that we can't attach text nodes to other text nodes.
  EXPECT_EQ(NULL, text->AppendChild(other_text));
  EXPECT_EQ(NULL, text->InsertBefore(other_text, NULL));
}

TEST_F(TextTest, NodeValue) {
  scoped_refptr<Text> text = new Text("text");
  EXPECT_EQ("text", text->node_value().value());
}

TEST_F(TextTest, TextContent) {
  scoped_refptr<Text> text = new Text("text");
  EXPECT_EQ("text", text->text_content().value());
}

}  // namespace dom
}  // namespace cobalt
