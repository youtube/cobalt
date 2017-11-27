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

#include "cobalt/dom/node_list.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class NodeListTest : public ::testing::Test {
 protected:
  NodeListTest()
      : dom_stat_tracker_(new DomStatTracker("NodeListTest")),
        html_element_context_(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                              NULL, NULL, NULL, NULL, NULL, NULL,
                              dom_stat_tracker_.get(), "",
                              base::kApplicationStateStarted),
        document_(new Document(&html_element_context_)) {}

  ~NodeListTest() override {}

  scoped_ptr<DomStatTracker> dom_stat_tracker_;
  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
};

TEST_F(NodeListTest, NodeListTest) {
  scoped_refptr<NodeList> node_list = new NodeList();
  EXPECT_EQ(0, node_list->length());

  node_list->AppendNode(document_->CreateElement("div"));
  node_list->AppendNode(document_->CreateElement("div"));
  node_list->AppendNode(document_->CreateElement("div"));
  EXPECT_EQ(3, node_list->length());
  EXPECT_TRUE(node_list->Item(0));
  EXPECT_TRUE(node_list->Item(1));
  EXPECT_TRUE(node_list->Item(2));
  EXPECT_FALSE(node_list->Item(3));
}
}  // namespace dom
}  // namespace cobalt
