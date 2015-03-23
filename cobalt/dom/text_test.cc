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
  scoped_refptr<Element> root = Element::Create();

  scoped_refptr<Node> text = root->AppendChild(Text::Create("text"));
  scoped_refptr<Node> other_text = Text::Create("other_text");

  // Checks that we can't attach text nodes to other text nodes.
  EXPECT_EQ(NULL, text->AppendChild(other_text));
  EXPECT_EQ(NULL, text->InsertBefore(other_text, NULL));
}

TEST_F(TextTest, TextContent) {
  scoped_refptr<Element> root = Element::Create();

  root->AppendChild(Element::Create())->AppendChild(Text::Create("This "));
  root->AppendChild(Element::Create())->AppendChild(Text::Create("is "));
  root->AppendChild(Element::Create())->AppendChild(Text::Create("Sparta."));
  EXPECT_EQ("This is Sparta.", root->text_content());

  const char* kTextContent = "New text content";
  root->set_text_content(kTextContent);
  EXPECT_EQ(kTextContent, root->text_content());

  // Check that we only have a single text child node.
  scoped_refptr<Node> child = root->first_child();
  EXPECT_NE(NULL, child);
  EXPECT_EQ(child, root->last_child());

  scoped_refptr<Text> text_child = child->AsText();
  ASSERT_NE(NULL, text_child);
}

}  // namespace dom
}  // namespace cobalt
