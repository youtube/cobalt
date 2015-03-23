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

#include "cobalt/dom/comment.h"

#include "cobalt/dom/element.h"
#include "cobalt/dom/stats.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/text.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

//////////////////////////////////////////////////////////////////////////
// CommentTest
//////////////////////////////////////////////////////////////////////////

class CommentTest : public ::testing::Test {
 protected:
  CommentTest();
  ~CommentTest() OVERRIDE;
};

CommentTest::CommentTest() {
  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());
}

CommentTest::~CommentTest() {
  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(CommentTest, CommentCheckAttach) {
  scoped_refptr<Element> root = Element::Create();

  scoped_refptr<Node> comment = root->AppendChild(Comment::Create("comment"));
  scoped_refptr<Node> text = Text::Create("text");

  // Checks that we can't attach text nodes to comment nodes.
  EXPECT_EQ(NULL, comment->AppendChild(text));
  EXPECT_EQ(NULL, comment->InsertBefore(text, NULL));
}

TEST_F(CommentTest, TextContentHasNoComments) {
  scoped_refptr<Element> root = Element::Create();

  root->AppendChild(Text::Create("t1"));
  root->AppendChild(Comment::Create("comment"));
  root->AppendChild(Text::Create("t2"));

  EXPECT_EQ("t1t2", root->text_content());
}

TEST_F(CommentTest, InnerHTML) {
  scoped_refptr<Element> root = Element::Create();

  root->AppendChild(Text::Create("t1"));
  root->AppendChild(Comment::Create("comment"));
  root->AppendChild(Text::Create("t2"));

  EXPECT_EQ("t1<!--comment-->t2", root->inner_html());
}

}  // namespace dom
}  // namespace cobalt
