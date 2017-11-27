// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/node_list_live.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class NodeListLiveTest : public ::testing::Test {
 protected:
  NodeListLiveTest()
      : dom_stat_tracker_("NodeListLiveTest"),
        html_element_context_(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                              NULL, NULL, NULL, NULL, NULL, NULL,
                              &dom_stat_tracker_, "",
                              base::kApplicationStateStarted),
        document_(new Document(&html_element_context_)) {}

  ~NodeListLiveTest() override {}

  DomStatTracker dom_stat_tracker_;
  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
};

TEST_F(NodeListLiveTest, NodeListLiveTest) {
  scoped_refptr<Element> root = document_->CreateElement("div");
  scoped_refptr<NodeListLive> node_list_live =
      NodeListLive::CreateWithChildren(root);
  EXPECT_EQ(0, node_list_live->length());

  scoped_refptr<Element> child1 = document_->CreateElement("div");
  scoped_refptr<Element> child2 = document_->CreateElement("div");
  scoped_refptr<Element> child3 = document_->CreateElement("div");
  root->AppendChild(child1);
  root->AppendChild(child2);
  root->AppendChild(child3);
  EXPECT_EQ(3, node_list_live->length());
  EXPECT_EQ(child1, node_list_live->Item(0));
  EXPECT_EQ(child2, node_list_live->Item(1));
  EXPECT_EQ(child3, node_list_live->Item(2));
  EXPECT_FALSE(node_list_live->Item(3));

  root->RemoveChild(child2);
  EXPECT_EQ(2, node_list_live->length());
  EXPECT_EQ(child1, node_list_live->Item(0));
  EXPECT_EQ(child3, node_list_live->Item(1));
  EXPECT_FALSE(node_list_live->Item(2));
}
}  // namespace dom
}  // namespace cobalt
