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
  scoped_refptr<Element> root = new Element();

  scoped_refptr<Node> comment = root->AppendChild(new Comment("comment"));
  scoped_refptr<Node> text = new Text("text");

  // Checks that we can't attach text nodes to comment nodes.
  EXPECT_EQ(NULL, comment->AppendChild(text));
  EXPECT_EQ(NULL, comment->InsertBefore(text, NULL));
}

TEST_F(CommentTest, NodeValue) {
  scoped_refptr<Comment> comment = new Comment("comment");
  EXPECT_EQ("comment", comment->node_value().value());
}

TEST_F(CommentTest, TextContent) {
  scoped_refptr<Comment> comment = new Comment("comment");
  EXPECT_EQ("comment", comment->text_content().value());
}

TEST_F(CommentTest, InnerHTML) {
  scoped_refptr<Element> root = new Element();

  root->AppendChild(new Text("t1"));
  root->AppendChild(new Comment("comment"));
  root->AppendChild(new Text("t2"));

  EXPECT_EQ("t1<!--comment-->t2", root->inner_html());
}

}  // namespace dom
}  // namespace cobalt
