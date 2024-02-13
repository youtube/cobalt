// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/testing/fake_document.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/dom/text.h"
#include "cobalt/web/testing/gtest_workarounds.h"
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

  std::unique_ptr<testing::StubWindow> window_;
  std::unique_ptr<HTMLElementContext> html_element_context_;
  std::unique_ptr<DomStatTracker> dom_stat_tracker_;
  scoped_refptr<Document> document_;
};

CommentTest::CommentTest()
    : window_(new testing::StubWindow),
      dom_stat_tracker_(new DomStatTracker("CommentTest")) {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  window_->InitializeWindow();
  html_element_context_.reset(new HTMLElementContext(
      window_->web_context()->environment_settings(), NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      dom_stat_tracker_.get(), "", base::kApplicationStateStarted, NULL, NULL));
  document_ = new testing::FakeDocument(html_element_context_.get());
}

CommentTest::~CommentTest() {
  window_.reset();
  document_ = nullptr;
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
  scoped_refptr<Element> root =
      new Element(document_, base_token::Token("root"));

  scoped_refptr<Node> comment =
      root->AppendChild(new Comment(document_, "comment"));
  scoped_refptr<Node> text = new Text(document_, "text");

  // Checks that we can't attach text nodes to comment nodes.
  EXPECT_EQ(NULL, comment->AppendChild(text).get());
  EXPECT_EQ(NULL, comment->InsertBefore(text, NULL).get());
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
  scoped_refptr<Element> root =
      new Element(document_, base_token::Token("root"));

  root->AppendChild(new Text(document_, "t1"));
  root->AppendChild(new Comment(document_, "comment"));
  root->AppendChild(new Text(document_, "t2"));

  EXPECT_EQ("t1<!--comment-->t2", root->inner_html());
}

}  // namespace dom
}  // namespace cobalt
