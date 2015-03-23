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

#include "cobalt/dom/node.h"

#include "cobalt/dom/comment.h"
#include "cobalt/dom/stats.h"
#include "cobalt/dom/testing/fake_node.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/text.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

//////////////////////////////////////////////////////////////////////////
// NodeTest
//////////////////////////////////////////////////////////////////////////

class NodeTest : public ::testing::Test {
 protected:
  NodeTest();
  ~NodeTest() OVERRIDE;
};

NodeTest::NodeTest() { EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks()); }

NodeTest::~NodeTest() { EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks()); }

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(NodeTest, CreateNode) {
  scoped_refptr<Node> node = FakeNode::Create();
  ASSERT_NE(NULL, node);
}

TEST_F(NodeTest, AppendChild) {
  scoped_refptr<Node> root = FakeNode::Create();

  scoped_refptr<Node> child1 = root->AppendChild(FakeNode::Create());
  ASSERT_NE(NULL, child1);
  scoped_refptr<Node> child2 = root->AppendChild(FakeNode::Create());
  ASSERT_NE(NULL, child2);

  // Properly handle NULL input.
  EXPECT_EQ(NULL, root->AppendChild(NULL));

  scoped_refptr<Node> children[] = {child1, child2};
  ExpectNodeChildrenEq(children, root);
}

TEST_F(NodeTest, AppendChildTwice) {
  scoped_refptr<Node> root = FakeNode::Create();

  scoped_refptr<Node> child1 = root->AppendChild(FakeNode::Create());
  scoped_refptr<Node> child2 = root->AppendChild(FakeNode::Create());

  // Append child1 for the second time.
  root->AppendChild(child1);

  scoped_refptr<Node> children[] = {child2, child1};
  ExpectNodeChildrenEq(children, root);
}

TEST_F(NodeTest, Contains) {
  scoped_refptr<Node> root = FakeNode::Create();
  scoped_refptr<Node> node = FakeNode::Create();

  scoped_refptr<Node> child1 = root->AppendChild(FakeNode::Create());
  scoped_refptr<Node> child2 = root->AppendChild(FakeNode::Create());

  EXPECT_TRUE(root->Contains(child1));
  EXPECT_TRUE(root->Contains(child2));

  EXPECT_FALSE(root->Contains(root));
  EXPECT_FALSE(root->Contains(node));
}

TEST_F(NodeTest, AppendChildWithParent) {
  scoped_refptr<Node> root1 = FakeNode::Create();
  scoped_refptr<Node> root2 = FakeNode::Create();

  scoped_refptr<Node> child = FakeNode::Create();
  root1->AppendChild(child);
  root2->AppendChild(child);

  EXPECT_FALSE(root1->Contains(child));
  EXPECT_TRUE(root2->Contains(child));

  EXPECT_EQ(root2, child->parent_node());
}

TEST_F(NodeTest, InsertBefore) {
  scoped_refptr<Node> root = FakeNode::Create();

  scoped_refptr<Node> child1 = root->InsertBefore(FakeNode::Create(), NULL);
  ASSERT_NE(NULL, child1);
  scoped_refptr<Node> child3 = root->InsertBefore(FakeNode::Create(), NULL);
  ASSERT_NE(NULL, child3);
  scoped_refptr<Node> child2 = root->InsertBefore(FakeNode::Create(), child3);
  ASSERT_NE(NULL, child2);

  // Properly handle NULL input.
  EXPECT_EQ(NULL, root->InsertBefore(NULL, NULL));
  // Properly handle a reference node that is not a child of root.
  EXPECT_EQ(NULL, root->InsertBefore(FakeNode::Create(), FakeNode::Create()));

  scoped_refptr<Node> children[] = {child1, child2, child3};
  ExpectNodeChildrenEq(children, root);
}

TEST_F(NodeTest, InsertBeforeTwice) {
  scoped_refptr<Node> root = FakeNode::Create();

  scoped_refptr<Node> child1 = root->InsertBefore(FakeNode::Create(), NULL);
  scoped_refptr<Node> child2 = root->InsertBefore(FakeNode::Create(), NULL);

  EXPECT_NE(NULL, root->InsertBefore(child2, child1));

  scoped_refptr<Node> children[] = {child2, child1};
  ExpectNodeChildrenEq(children, root);
}

TEST_F(NodeTest, InsertBeforeSelf) {
  scoped_refptr<Node> root = FakeNode::Create();

  scoped_refptr<Node> child1 = root->InsertBefore(FakeNode::Create(), NULL);
  scoped_refptr<Node> child2 = root->InsertBefore(FakeNode::Create(), NULL);

  EXPECT_NE(NULL, root->InsertBefore(child2, child2));

  scoped_refptr<Node> children[] = {child1, child2};
  ExpectNodeChildrenEq(children, root);
}

TEST_F(NodeTest, RemoveChild) {
  scoped_refptr<Node> root = FakeNode::Create();

  scoped_refptr<Node> child1 = root->AppendChild(FakeNode::Create());
  scoped_refptr<Node> child2 = root->AppendChild(FakeNode::Create());
  scoped_refptr<Node> child3 = root->AppendChild(FakeNode::Create());

  EXPECT_NE(NULL, root->RemoveChild(child1));
  EXPECT_NE(NULL, root->RemoveChild(child3));

  // Properly handle NULL input.
  EXPECT_EQ(NULL, root->RemoveChild(NULL));
  // Properly handle a node that is not a child of root.
  EXPECT_EQ(NULL, root->RemoveChild(child1));

  scoped_refptr<Node> children[] = {child2};
  ExpectNodeChildrenEq(children, root);
}

TEST_F(NodeTest, HasChildNodes) {
  scoped_refptr<Node> root = FakeNode::Create();

  EXPECT_FALSE(root->HasChildNodes());

  root->AppendChild(FakeNode::Create());
  EXPECT_TRUE(root->HasChildNodes());
}

}  // namespace dom
}  // namespace cobalt
