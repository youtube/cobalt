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

#include "cobalt/render_tree/node_visitor.h"

#include <string>

#include "base/bind.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_3d_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/punch_through_video_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/text_node.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::render_tree::animations::AnimateNode;
using cobalt::render_tree::Brush;
using cobalt::render_tree::BrushVisitor;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::FilterNode;
using cobalt::render_tree::Font;
using cobalt::render_tree::FontMetrics;
using cobalt::render_tree::GlyphBuffer;
using cobalt::render_tree::Image;
using cobalt::render_tree::ImageNode;
using cobalt::render_tree::MatrixTransform3DNode;
using cobalt::render_tree::MatrixTransformNode;
using cobalt::render_tree::NodeVisitor;
using cobalt::render_tree::OpacityFilter;
using cobalt::render_tree::PunchThroughVideoNode;
using cobalt::render_tree::RectNode;
using cobalt::render_tree::RectShadowNode;
using cobalt::render_tree::TextNode;

class MockNodeVisitor : public NodeVisitor {
 public:
  MOCK_METHOD1(Visit, void(AnimateNode* animate));
  MOCK_METHOD1(Visit, void(CompositionNode* composition));
  MOCK_METHOD1(Visit, void(FilterNode* image));
  MOCK_METHOD1(Visit, void(ImageNode* image));
  MOCK_METHOD1(Visit, void(MatrixTransform3DNode* matrix_transform_3d_node));
  MOCK_METHOD1(Visit, void(MatrixTransformNode* matrix_transform_node));
  MOCK_METHOD1(Visit, void(PunchThroughVideoNode* punch_through_video_node));
  MOCK_METHOD1(Visit, void(RectNode* rect));
  MOCK_METHOD1(Visit, void(RectShadowNode* rect_shadow));
  MOCK_METHOD1(Visit, void(TextNode* text));
};

TEST(NodeVisitorTest, VisitsComposition) {
  CompositionNode::Builder builder;
  scoped_refptr<CompositionNode> composition(new CompositionNode(builder));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(composition.get()));
  composition->Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsFilter) {
  scoped_refptr<FilterNode> filter(new FilterNode(OpacityFilter(0.5f), NULL));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(filter.get()));
  filter->Accept(&mock_visitor);
}

namespace {

class DummyImage : public Image {
  const cobalt::math::Size& GetSize() const override { return size_; }

 private:
  cobalt::math::Size size_;
};

class DummyBrush : public Brush {
  void Accept(BrushVisitor* visitor) const override {
    UNREFERENCED_PARAMETER(visitor);
  }

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<DummyBrush>();
  }
};

bool SetBounds(const cobalt::math::Rect&) { return false; }

}  // namespace

TEST(NodeVisitorTest, VisitsAnimate) {
  scoped_refptr<DummyImage> dummy_image = make_scoped_refptr(new DummyImage());
  scoped_refptr<ImageNode> dummy_image_node(new ImageNode(dummy_image));
  scoped_refptr<AnimateNode> animate_node(new AnimateNode(dummy_image_node));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(animate_node.get()));
  animate_node->Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsImage) {
  scoped_refptr<DummyImage> image = make_scoped_refptr(new DummyImage());
  scoped_refptr<ImageNode> image_node(new ImageNode(image));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(image_node.get()));
  image_node->Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsMatrixTransform3D) {
  scoped_refptr<MatrixTransform3DNode> matrix_transform_3d_node(
      new MatrixTransform3DNode(
          NULL, glm::mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(matrix_transform_3d_node.get()));
  matrix_transform_3d_node->Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsMatrixTransform) {
  scoped_refptr<MatrixTransformNode> matrix_transform_node(
      new MatrixTransformNode(NULL, cobalt::math::Matrix3F::Identity()));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(matrix_transform_node.get()));
  matrix_transform_node->Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsPunchThroughVideo) {
  PunchThroughVideoNode::Builder builder(cobalt::math::RectF(),
                                         base::Bind(SetBounds));
  scoped_refptr<PunchThroughVideoNode> punch_through_video_node(
      new PunchThroughVideoNode(builder));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(punch_through_video_node.get()));
  punch_through_video_node->Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsRect) {
  scoped_refptr<RectNode> rect(
      new RectNode(cobalt::math::RectF(), scoped_ptr<Brush>(new DummyBrush())));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(rect.get()));
  rect->Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsRectShadow) {
  scoped_refptr<RectShadowNode> rect_shadow(new RectShadowNode(
      cobalt::math::RectF(),
      cobalt::render_tree::Shadow(cobalt::math::Vector2dF(1.0f, 1.0f), 1.0f,
                                  ColorRGBA(0, 0, 0, 1.0f))));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(rect_shadow.get()));
  rect_shadow->Accept(&mock_visitor);
}

TEST(NodeVisitorTest, VisitsText) {
  scoped_refptr<TextNode> text(
      new TextNode(cobalt::math::Vector2dF(0, 0),
                   new GlyphBuffer(cobalt::math::RectF(0, 0, 1, 1)),
                   ColorRGBA(0.0f, 0.0f, 0.0f)));
  MockNodeVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, Visit(text.get()));
  text->Accept(&mock_visitor);
}
