// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/node_visitor.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/text_node.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace render_tree {
namespace animations {

using ::cobalt::math::SizeF;
using ::cobalt::math::RectF;

// Helper function to create an animation set for a render tree that consists
// of only one animation targeting only one node in the render tree.
template <typename T>
scoped_refptr<AnimateNode> CreateSingleAnimation(
    const scoped_refptr<T>& target,
    typename Animation<T>::Function anim_function, base::TimeDelta expiry) {
  AnimateNode::Builder animations_builder;
  animations_builder.Add(target, anim_function, expiry);
  return new AnimateNode(animations_builder, target);
}

template <typename T>
scoped_refptr<AnimateNode> CreateSingleAnimation(
    const scoped_refptr<T>& target,
    typename Animation<T>::Function anim_function) {
  return CreateSingleAnimation(target, anim_function, base::TimeDelta::Max());
}

class ImageFake : public Image {
 public:
  const math::Size& GetSize() const override { return size_; }

 private:
  math::Size size_;
};

// The following tests ensure that simple single time-independent animations
// work fine on single-node render trees of different types.
void AnimateText(TextNode::Builder* text_node, base::TimeDelta time_elapsed) {
  UNREFERENCED_PARAMETER(time_elapsed);
  text_node->color = ColorRGBA(.5f, .5f, .5f);
}
TEST(AnimateNodeTest, EnsureSingleTextNodeAnimates) {
  scoped_refptr<TextNode> text_node(
      new TextNode(math::Vector2dF(0, 0), new GlyphBuffer(RectF(0, 0, 1, 1)),
                   ColorRGBA(1.0f, 1.0f, 1.0f)));

  scoped_refptr<AnimateNode> with_animations =
      CreateSingleAnimation(text_node, base::Bind(&AnimateText));

  scoped_refptr<AnimateNode> animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(1)).animated;
  TextNode* animated_text_node =
      dynamic_cast<TextNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_text_node);
  EXPECT_EQ(ColorRGBA(.5f, .5f, .5f), animated_text_node->data().color);
}

void AnimateImage(ImageNode::Builder* image_node,
                  base::TimeDelta time_elapsed) {
  UNREFERENCED_PARAMETER(time_elapsed);
  image_node->destination_rect = RectF(2.0f, 2.0f);
}
TEST(AnimateNodeTest, EnsureSingleImageNodeAnimates) {
  scoped_refptr<ImageNode> image_node(
      new ImageNode(make_scoped_refptr(new ImageFake()), RectF(1.0f, 1.0f)));

  scoped_refptr<AnimateNode> with_animations =
      CreateSingleAnimation(image_node, base::Bind(&AnimateImage));

  scoped_refptr<AnimateNode> animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(1)).animated;
  ImageNode* animated_image_node =
      dynamic_cast<ImageNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_image_node);
  EXPECT_TRUE(animated_image_node);
  EXPECT_EQ(RectF(2.0f, 2.0f), animated_image_node->data().destination_rect);
}

void AnimateRect(RectNode::Builder* rect_node, base::TimeDelta time_elapsed) {
  UNREFERENCED_PARAMETER(time_elapsed);
  rect_node->rect = RectF(2.0f, 2.0f);
}
TEST(AnimateNodeTest, EnsureSingleRectNodeAnimates) {
  scoped_refptr<RectNode> rect_node(new RectNode(
      RectF(1.0f, 1.0f),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0f, 1.0f, 1.0f)))));

  scoped_refptr<AnimateNode> with_animations =
      CreateSingleAnimation(rect_node, base::Bind(&AnimateRect));

  scoped_refptr<AnimateNode> animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(1)).animated;
  RectNode* animated_rect_node =
      dynamic_cast<RectNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_rect_node);
  EXPECT_TRUE(animated_rect_node);
  EXPECT_EQ(RectF(2.0f, 2.0f), animated_rect_node->data().rect);
}

// Test that time is being passed through correctly and that we can animate
// the same tree repeatedly with different time values to produce different
// results.
void AnimateImageAddWithTime(ImageNode::Builder* image_node,
                             base::TimeDelta time_elapsed) {
  image_node->destination_rect.Outset(
      static_cast<float>(time_elapsed.InSecondsF()),
      static_cast<float>(time_elapsed.InSecondsF()));
}
TEST(AnimateNodeTest, SingleImageNodeAnimatesOverTimeRepeatedlyCorrectly) {
  scoped_refptr<ImageNode> image_node(
      new ImageNode(make_scoped_refptr(new ImageFake()), RectF(1.0f, 1.0f)));

  scoped_refptr<AnimateNode> with_animations =
      CreateSingleAnimation(image_node, base::Bind(&AnimateImageAddWithTime));

  scoped_refptr<AnimateNode> animated_render_tree;
  ImageNode* animated_image_node;

  animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(1)).animated;
  animated_image_node =
      dynamic_cast<ImageNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_image_node);
  EXPECT_TRUE(animated_image_node);
  EXPECT_FLOAT_EQ(3.0f, animated_image_node->data().destination_rect.width());

  animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(2)).animated;
  animated_image_node =
      dynamic_cast<ImageNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_image_node);
  EXPECT_TRUE(animated_image_node);
  EXPECT_FLOAT_EQ(5.0f, animated_image_node->data().destination_rect.width());

  animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(4)).animated;
  animated_image_node =
      dynamic_cast<ImageNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_image_node);
  EXPECT_TRUE(animated_image_node);
  EXPECT_FLOAT_EQ(9.0f, animated_image_node->data().destination_rect.width());
}

// Test that nodes with multiple animations targeting them work correctly, and
// that the animations are applied in the correct order.
void AnimateImageScaleWithTime(ImageNode::Builder* image_node,
                               base::TimeDelta time_elapsed) {
  math::SizeF image_size = image_node->destination_rect.size();
  image_size.Scale(static_cast<float>(time_elapsed.InSecondsF()),
                   static_cast<float>(time_elapsed.InSecondsF()));
  image_node->destination_rect.set_size(image_size);
}
TEST(AnimateNodeTest, MultipleAnimationsTargetingOneNodeApplyInCorrectOrder) {
  scoped_refptr<ImageNode> image_node(
      new ImageNode(make_scoped_refptr(new ImageFake()), RectF(1.0f, 1.0f)));

  // Here we add 2 animations both targeting the same ImageNode.  We expect
  // them to be applied in the order that they appear in the list.  We test this
  // by issuing both an add and then a multiply to the image node's destination
  // size property, so that the result depends on the order of operations.
  AnimationList<ImageNode>::Builder animation_list_builder;
  animation_list_builder.animations.push_back(
      base::Bind(&AnimateImageAddWithTime));
  animation_list_builder.animations.push_back(
      base::Bind(&AnimateImageScaleWithTime));

  AnimateNode::Builder animations_builder;
  animations_builder.Add(image_node,
                         make_scoped_refptr(new AnimationList<ImageNode>(
                             animation_list_builder.Pass())));
  scoped_refptr<AnimateNode> with_animations(
      new AnimateNode(animations_builder, image_node));

  scoped_refptr<AnimateNode> animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(3)).animated;

  ImageNode* animated_image_node =
      dynamic_cast<ImageNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_image_node);

  EXPECT_FLOAT_EQ(21.0f, animated_image_node->data().destination_rect.width());
}

// Test that animating a transform node correctly adjusts parameters but
// leaves its child node untouched.
void AnimateTransform(MatrixTransformNode::Builder* transform_node,
                      base::TimeDelta time_elapsed) {
  UNREFERENCED_PARAMETER(time_elapsed);
  transform_node->transform = math::TranslateMatrix(2.0f, 2.0f);
}
TEST(AnimateNodeTest, AnimatingTransformNodeDoesNotAffectSourceChild) {
  scoped_refptr<ImageNode> image_node(
      new ImageNode(make_scoped_refptr(new ImageFake()), RectF(1.0f, 1.0f)));
  scoped_refptr<MatrixTransformNode> transform_node(
      new MatrixTransformNode(image_node, math::Matrix3F::Identity()));

  scoped_refptr<AnimateNode> with_animations =
      CreateSingleAnimation(transform_node, base::Bind(&AnimateTransform));

  scoped_refptr<AnimateNode> animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(1)).animated;

  MatrixTransformNode* animated_transform_node =
      dynamic_cast<MatrixTransformNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_transform_node);

  EXPECT_EQ(math::TranslateMatrix(2.0f, 2.0f),
            animated_transform_node->data().transform);
  EXPECT_EQ(image_node.get(), animated_transform_node->data().source.get());
}

// Test that animating a child correctly adjusts sub-node parameters but
// not parent node parameters, though both child and parent are different nodes
// now.  The animated child's sibling should be left untouched.
TEST(AnimateNodeTest,
     AnimatingCompositionChildNodeAffectsParentAsWellButNotSibling) {
  scoped_refptr<ImageNode> image_node_a(
      new ImageNode(make_scoped_refptr(new ImageFake()), RectF(1.0f, 1.0f)));
  scoped_refptr<ImageNode> image_node_b(
      new ImageNode(make_scoped_refptr(new ImageFake()), RectF(1.0f, 1.0f)));
  CompositionNode::Builder composition_node_builder;
  composition_node_builder.AddChild(image_node_a);
  composition_node_builder.AddChild(image_node_b);
  scoped_refptr<CompositionNode> composition_node(
      new CompositionNode(composition_node_builder.Pass()));

  AnimateNode::Builder animation_builder;
  animation_builder.Add(image_node_a, base::Bind(&AnimateImage));
  scoped_refptr<AnimateNode> with_animations =
      new AnimateNode(animation_builder, composition_node);

  scoped_refptr<AnimateNode> animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(1)).animated;

  CompositionNode* animated_composition_node =
      dynamic_cast<CompositionNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_composition_node);

  ImageNode* animated_image_node_a = dynamic_cast<ImageNode*>(
      animated_composition_node->data().children()[0].get());
  EXPECT_TRUE(animated_image_node_a);
  EXPECT_EQ(RectF(2.0f, 2.0f), animated_image_node_a->data().destination_rect);

  ImageNode* animated_image_node_b = dynamic_cast<ImageNode*>(
      animated_composition_node->data().children()[1].get());
  EXPECT_EQ(image_node_b.get(), animated_image_node_b);
}

TEST(AnimateNodeTest, SimpleSubAnimateNode) {
  // Test that we can properly animate trees that built upon multiple
  // AnimateNodes.
  scoped_refptr<ImageNode> image_node(
      new ImageNode(make_scoped_refptr(new ImageFake()), RectF(1.0f, 1.0f)));
  scoped_refptr<AnimateNode> image_animation =
      CreateSingleAnimation(image_node, base::Bind(&AnimateImage));

  scoped_refptr<MatrixTransformNode> transform_node(
      new MatrixTransformNode(image_animation, math::Matrix3F::Identity()));

  scoped_refptr<AnimateNode> with_animations =
      CreateSingleAnimation(transform_node, base::Bind(&AnimateTransform));

  scoped_refptr<AnimateNode> animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(1)).animated;

  MatrixTransformNode* animated_transform_node =
      dynamic_cast<MatrixTransformNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_transform_node);

  // Check that the matrix transform node is animated.
  EXPECT_EQ(math::TranslateMatrix(2.0f, 2.0f),
            animated_transform_node->data().transform);

  // Check also that the image node is animated.
  ImageNode* animated_image_node =
      dynamic_cast<ImageNode*>(animated_transform_node->data().source.get());
  EXPECT_TRUE(animated_image_node);
  EXPECT_EQ(RectF(2.0f, 2.0f), animated_image_node->data().destination_rect);
}

TEST(AnimateNodeTest, SubAnimateNodeWithTwoAncestors) {
  // Test that we can properly animate trees that built upon multiple
  // AnimateNodes.
  scoped_refptr<ImageNode> image_node(
      new ImageNode(make_scoped_refptr(new ImageFake()), RectF(1.0f, 1.0f)));
  scoped_refptr<AnimateNode> image_animation =
      CreateSingleAnimation(image_node, base::Bind(&AnimateImage));

  // This node is used to make it so there is more than one node on the path
  // between the two AnimateNodes.
  scoped_refptr<MatrixTransformNode> transform_node_a(
      new MatrixTransformNode(image_animation, math::Matrix3F::Identity()));

  scoped_refptr<MatrixTransformNode> transform_node_b(
      new MatrixTransformNode(transform_node_a, math::Matrix3F::Identity()));

  scoped_refptr<AnimateNode> with_animations =
      new AnimateNode(transform_node_b);

  scoped_refptr<AnimateNode> animated_render_tree =
      with_animations->Apply(base::TimeDelta::FromSeconds(1)).animated;

  MatrixTransformNode* animated_transform_node =
      dynamic_cast<MatrixTransformNode*>(animated_render_tree->source().get());
  EXPECT_TRUE(animated_transform_node);

  // Check that the image node is animated.
  MatrixTransformNode* animated_dummy_transform_node =
      dynamic_cast<MatrixTransformNode*>(
          animated_transform_node->data().source.get());
  EXPECT_TRUE(animated_dummy_transform_node);

  ImageNode* animated_image_node = dynamic_cast<ImageNode*>(
      animated_dummy_transform_node->data().source.get());
  EXPECT_TRUE(animated_image_node);
  EXPECT_EQ(RectF(2.0f, 2.0f), animated_image_node->data().destination_rect);
}

void BoundsAnimateRect(RectNode::Builder* rect_node,
                       base::TimeDelta time_elapsed) {
  UNREFERENCED_PARAMETER(time_elapsed);
  rect_node->rect = RectF(3.0f, 5.0f, 15.0f, 20.0f);
}
TEST(AnimateNodeTest, AnimationBounds) {
  scoped_refptr<RectNode> rect_node_static(new RectNode(
      RectF(1.0f, 1.0f),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0f, 1.0f, 1.0f)))));

  scoped_refptr<RectNode> rect_node_animated(new RectNode(
      RectF(4.0f, 4.0f, 10.0f, 10.0f),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0f, 1.0f, 1.0f)))));

  CompositionNode::Builder builder;
  builder.AddChild(rect_node_static);
  builder.AddChild(rect_node_animated);
  scoped_refptr<CompositionNode> composition(
      new CompositionNode(builder.Pass()));

  AnimateNode::Builder animations_builder;
  animations_builder.Add(rect_node_animated, base::Bind(&BoundsAnimateRect));
  scoped_refptr<AnimateNode> with_animations =
      new AnimateNode(animations_builder, composition);

  AnimateNode::AnimateResults results =
      with_animations->Apply(base::TimeDelta::FromSeconds(1));

  math::RectF animation_bounds =
      results.get_animation_bounds_since.Run(base::TimeDelta());

  EXPECT_EQ(RectF(3.0f, 5.0f, 15.0f, 20.0f), animation_bounds);
}

TEST(AnimateNodeTest, AnimationBoundsExpiration) {
  scoped_refptr<RectNode> rect_node_static(new RectNode(
      RectF(1.0f, 1.0f),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0f, 1.0f, 1.0f)))));

  scoped_refptr<RectNode> rect_node_animated(new RectNode(
      RectF(4.0f, 4.0f, 10.0f, 10.0f),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0f, 1.0f, 1.0f)))));

  CompositionNode::Builder builder;
  builder.AddChild(rect_node_static);
  builder.AddChild(rect_node_animated);
  scoped_refptr<CompositionNode> composition(
      new CompositionNode(builder.Pass()));

  AnimateNode::Builder animations_builder;
  animations_builder.Add(rect_node_animated, base::Bind(&BoundsAnimateRect),
                         base::TimeDelta::FromSeconds(2));
  scoped_refptr<AnimateNode> with_animations =
      new AnimateNode(animations_builder, composition);

  // Make sure that our animation bounds are updated when we apply animations
  // before the expiration.
  AnimateNode::AnimateResults results =
      with_animations->Apply(base::TimeDelta::FromSeconds(1));
  math::RectF animation_bounds =
      results.get_animation_bounds_since.Run(base::TimeDelta::FromSeconds(0));
  EXPECT_EQ(RectF(3.0f, 5.0f, 15.0f, 20.0f), animation_bounds);

  // Make sure that our animation bounds are updated when we apply animations
  // after the expiration, but we pass in a "since" value from before the
  // animations expire.
  results = with_animations->Apply(base::TimeDelta::FromSeconds(4));
  animation_bounds =
      results.get_animation_bounds_since.Run(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(RectF(3.0f, 5.0f, 15.0f, 20.0f), animation_bounds);

  // Make sure that our animation bounds are empty after our animations have
  // expired.
  results = with_animations->Apply(base::TimeDelta::FromSeconds(4));
  animation_bounds =
      results.get_animation_bounds_since.Run(base::TimeDelta::FromSeconds(3));
  EXPECT_EQ(0, animation_bounds.size().GetArea());
}

void BoundsAnimateRect2(RectNode::Builder* rect_node,
                        base::TimeDelta time_elapsed) {
  UNREFERENCED_PARAMETER(time_elapsed);
  rect_node->rect = RectF(2.0f, 6.0f, 10.0f, 25.0f);
}
TEST(AnimateNodeTest, AnimationBoundsUnionsMultipleAnimations) {
  scoped_refptr<RectNode> rect_node_1(new RectNode(
      RectF(1.0f, 1.0f),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0f, 1.0f, 1.0f)))));

  scoped_refptr<RectNode> rect_node_2(new RectNode(
      RectF(4.0f, 4.0f, 10.0f, 10.0f),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0f, 1.0f, 1.0f)))));

  CompositionNode::Builder builder;
  builder.AddChild(rect_node_1);
  builder.AddChild(rect_node_2);
  scoped_refptr<CompositionNode> composition(
      new CompositionNode(builder.Pass()));

  AnimateNode::Builder animations_builder;
  animations_builder.Add(rect_node_1, base::Bind(&BoundsAnimateRect));
  animations_builder.Add(rect_node_2, base::Bind(&BoundsAnimateRect2));
  scoped_refptr<AnimateNode> with_animations =
      new AnimateNode(animations_builder, composition);

  // Make sure that our animation bounds are the union from our two animated
  // resulting boxes.
  AnimateNode::AnimateResults results =
      with_animations->Apply(base::TimeDelta::FromSeconds(1));
  math::RectF animation_bounds =
      results.get_animation_bounds_since.Run(base::TimeDelta::FromSeconds(0));
  EXPECT_EQ(RectF(2.0f, 5.0f, 16.0f, 26.0f), animation_bounds);
}

void AnimateTranslate(CompositionNode::Builder* composition_node,
                      base::TimeDelta time_elapsed) {
  UNREFERENCED_PARAMETER(time_elapsed);
  composition_node->set_offset(math::Vector2dF(4.0f, 4.0f));
}
// This test makes sure that the animation bounds are calculated correctly when
// multiple nodes on the path from the root to a leaf node are animated.  Our
// results should use the *animated* path to the node to calculate the bounds,
// versus the non-animated path.
TEST(AnimateNodeTest, AnimationBoundsWorksForCompoundedTransformations) {
  scoped_refptr<RectNode> rect_node(new RectNode(
      RectF(8.0f, 8.0f),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0f, 1.0f, 1.0f)))));

  CompositionNode::Builder composition_node_builder_1;
  composition_node_builder_1.AddChild(rect_node);
  scoped_refptr<CompositionNode> composition_node_1(
      new CompositionNode(composition_node_builder_1.Pass()));

  CompositionNode::Builder composition_node_builder_2;
  composition_node_builder_2.AddChild(composition_node_1);
  scoped_refptr<CompositionNode> composition_node_2(
      new CompositionNode(composition_node_builder_2.Pass()));

  AnimateNode::Builder animations_builder;
  animations_builder.Add(composition_node_1, base::Bind(&AnimateTranslate));
  animations_builder.Add(composition_node_2, base::Bind(&AnimateTranslate));
  scoped_refptr<AnimateNode> with_animations =
      new AnimateNode(animations_builder, composition_node_2);

  // Make sure that our animation bounds are the union from our two animated
  // resulting boxes.
  AnimateNode::AnimateResults results =
      with_animations->Apply(base::TimeDelta::FromSeconds(1));
  math::RectF animation_bounds =
      results.get_animation_bounds_since.Run(base::TimeDelta::FromSeconds(0));
  EXPECT_EQ(RectF(8.0f, 8.0f, 8.0f, 8.0f), animation_bounds);
}

}  // namespace animations
}  // namespace render_tree
}  // namespace cobalt
