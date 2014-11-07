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

#include "cobalt/render_tree/node_visitor.h"

#include "cobalt/render_tree/container_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/text_node.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace render_tree {

class MockNodeVisitor : public NodeVisitor {
 public:
  MOCK_METHOD1(Visit, void(ContainerNode* container));
  MOCK_METHOD1(Visit, void(ImageNode* image));
  MOCK_METHOD1(Visit, void(RectNode* rect));
  MOCK_METHOD1(Visit, void(TextNode* text));
};

TEST(NodeVisitorTest, VisitsContainer) {
  ContainerNode container;
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(&container));
  container.Accept(&mock_visitor);
}

namespace {
class DummyImage : public Image {
  int GetWidth() const OVERRIDE { return 0; }
  int GetHeight() const OVERRIDE { return 0; }
};
}  // namespace

TEST(NodeVisitorTest, VisitsImage) {
  scoped_refptr<DummyImage> image = make_scoped_refptr(new DummyImage());
  ImageNode image_node(image);
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(&image_node));
  image_node.Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsRect) {
  RectNode rect;
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(&rect));
  rect.Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsText) {
  TextNode text;
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(&text));
  text.Accept(&mock_visitor);
}

}  // namespace render_tree
}  // namespace cobalt
