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

#include "cobalt/dom/comment.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element_context.h"
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
  ~CommentTest() override;

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
};

CommentTest::CommentTest() {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  document_ = new Document(&html_element_context_);
}

CommentTest::~CommentTest() {
  document_ = NULL;
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(CommentTest, Duplicate) {
  scoped_refptr<Comment> comment = new Comment(document_, "comment");
  scoped_refptr<Comment> new_comment = comment->Duplicate()->AsComment();
  ASSERT_TRUE(new_comment);
  EXPECT_EQ("comment", new_comment->data());
}

TEST_F(CommentTest, CommentCheckAttach) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));

  scoped_refptr<Node> comment =
      root->AppendChild(new Comment(document_, "comment"));
  scoped_refptr<Node> text = new Text(document_, "text");

  // Checks that we can't attach text nodes to comment nodes.
  EXPECT_EQ(NULL, comment->AppendChild(text));
  EXPECT_EQ(NULL, comment->InsertBefore(text, NULL));
}

TEST_F(CommentTest, NodeValue) {
  scoped_refptr<Comment> comment = new Comment(document_, "comment");
  EXPECT_EQ("comment", comment->node_value().value());
}

TEST_F(CommentTest, TextContent) {
  scoped_refptr<Comment> comment = new Comment(document_, "comment");
  EXPECT_EQ("comment", comment->text_content().value());
}

TEST_F(CommentTest, InnerHTML) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));

  root->AppendChild(new Text(document_, "t1"));
  root->AppendChild(new Comment(document_, "comment"));
  root->AppendChild(new Text(document_, "t2"));

  EXPECT_EQ("t1<!--comment-->t2", root->inner_html());
}

}  // namespace dom
}  // namespace cobalt
