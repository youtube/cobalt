// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_node_data_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data.mojom.h"
#include "ui/accessibility/mojom/ax_relative_bounds_mojom_traits.h"

using mojo::test::SerializeAndDeserialize;

namespace {

template <typename EnumType>
void TestValidEnumAttribute(ax::mojom::IntAttribute attribute,
                            EnumType valid_value) {
  ui::AXNodeData input, output;
  input.AddIntAttribute(attribute, static_cast<int32_t>(valid_value));
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(static_cast<int32_t>(valid_value),
            output.GetIntAttribute(attribute));
}

template <typename EnumType>
void TestInvalidEnumAttribute(ax::mojom::IntAttribute attribute) {
  {
    ui::AXNodeData input, output;
    input.AddIntAttribute(attribute,
                          static_cast<int32_t>(EnumType::kMaxValue) + 1);
    EXPECT_FALSE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  }
  {
    ui::AXNodeData input, output;
    input.AddIntAttribute(attribute,
                          static_cast<int32_t>(EnumType::kMinValue) - 1);
    EXPECT_FALSE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  }
}

}  // namespace

TEST(AXNodeDataMojomTraitsTest, ID) {
  ui::AXNodeData input, output;
  input.id = 42;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(42, output.id);
}

TEST(AXNodeDataMojomTraitsTest, Role) {
  ui::AXNodeData input, output;
  input.role = ax::mojom::Role::kButton;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(ax::mojom::Role::kButton, output.role);
}

TEST(AXNodeDataMojomTraitsTest, State) {
  ui::AXNodeData input, output;
  input.state = ui::AXStates(0U);
  input.AddState(ax::mojom::State::kCollapsed);
  input.AddState(ax::mojom::State::kHorizontal);
  input.AddState(ax::mojom::State::kMaxValue);
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_TRUE(output.HasState(ax::mojom::State::kCollapsed));
  EXPECT_TRUE(output.HasState(ax::mojom::State::kHorizontal));
  EXPECT_TRUE(output.HasState(ax::mojom::State::kMaxValue));
  EXPECT_FALSE(output.HasState(ax::mojom::State::kFocusable));
  EXPECT_FALSE(output.HasState(ax::mojom::State::kMultiline));
}

TEST(AXNodeDataMojomTraitsTest, Actions) {
  ui::AXNodeData input, output;
  input.actions = 0;
  input.AddAction(ax::mojom::Action::kDoDefault);
  input.AddAction(ax::mojom::Action::kDecrement);
  input.AddAction(ax::mojom::Action::kMaxValue);
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_TRUE(output.HasAction(ax::mojom::Action::kDoDefault));
  EXPECT_TRUE(output.HasAction(ax::mojom::Action::kDecrement));
  EXPECT_TRUE(output.HasAction(ax::mojom::Action::kMaxValue));
  EXPECT_FALSE(output.HasAction(ax::mojom::Action::kFocus));
  EXPECT_FALSE(output.HasAction(ax::mojom::Action::kBlur));
}

TEST(AXNodeDataMojomTraitsTest, StringAttributes) {
  ui::AXNodeData input, output;
  input.AddStringAttribute(ax::mojom::StringAttribute::kName, "Mojo");
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ("Mojo",
            output.GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST(AXNodeDataMojomTraitsTest, IntAttributes) {
  ui::AXNodeData input, output;
  input.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 42);
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(42, output.GetIntAttribute(ax::mojom::IntAttribute::kScrollX));
}

TEST(AXNodeDataMojomTraitsTest, IntAttributesValidEnumsBulk) {
  TestValidEnumAttribute(ax::mojom::IntAttribute::kAriaCurrentState,
                         ax::mojom::AriaCurrentState::kTrue);
  TestValidEnumAttribute(
      ax::mojom::IntAttribute::kAriaNotificationInterruptDeprecated,
      ax::mojom::AriaNotificationInterrupt::kAll);
  TestValidEnumAttribute(
      ax::mojom::IntAttribute::kAriaNotificationPriorityDeprecated,
      ax::mojom::AriaNotificationPriority::kHigh);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kCheckedState,
                         ax::mojom::CheckedState::kTrue);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kDefaultActionVerb,
                         ax::mojom::DefaultActionVerb::kClick);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kDescriptionFrom,
                         ax::mojom::DescriptionFrom::kAriaDescription);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kDetailsFrom,
                         ax::mojom::DetailsFrom::kAriaDetails);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kHasPopup,
                         ax::mojom::HasPopup::kTrue);
  TestValidEnumAttribute(
      ax::mojom::IntAttribute::kImageAnnotationStatus,
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kInvalidState,
                         ax::mojom::InvalidState::kTrue);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kIsPopup,
                         ax::mojom::IsPopup::kAuto);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kListStyle,
                         ax::mojom::ListStyle::kCircle);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kNameFrom,
                         ax::mojom::NameFrom::kAttribute);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kRestriction,
                         ax::mojom::Restriction::kDisabled);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kSortDirection,
                         ax::mojom::SortDirection::kAscending);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kTextAlign,
                         ax::mojom::TextAlign::kCenter);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kTextDirection,
                         ax::mojom::WritingDirection::kLtr);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kTextOverlineStyle,
                         ax::mojom::TextDecorationStyle::kSolid);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kTextPosition,
                         ax::mojom::TextPosition::kSubscript);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kTextStrikethroughStyle,
                         ax::mojom::TextDecorationStyle::kSolid);
  TestValidEnumAttribute(ax::mojom::IntAttribute::kTextUnderlineStyle,
                         ax::mojom::TextDecorationStyle::kSolid);
}

TEST(AXNodeDataMojomTraitsTest, IntAttributesInvalidEnumsBulk) {
  TestInvalidEnumAttribute<ax::mojom::AriaCurrentState>(
      ax::mojom::IntAttribute::kAriaCurrentState);
  TestInvalidEnumAttribute<ax::mojom::AriaNotificationInterrupt>(
      ax::mojom::IntAttribute::kAriaNotificationInterruptDeprecated);
  TestInvalidEnumAttribute<ax::mojom::AriaNotificationPriority>(
      ax::mojom::IntAttribute::kAriaNotificationPriorityDeprecated);
  TestInvalidEnumAttribute<ax::mojom::CheckedState>(
      ax::mojom::IntAttribute::kCheckedState);
  TestInvalidEnumAttribute<ax::mojom::DefaultActionVerb>(
      ax::mojom::IntAttribute::kDefaultActionVerb);
  TestInvalidEnumAttribute<ax::mojom::DescriptionFrom>(
      ax::mojom::IntAttribute::kDescriptionFrom);
  TestInvalidEnumAttribute<ax::mojom::DetailsFrom>(
      ax::mojom::IntAttribute::kDetailsFrom);
  TestInvalidEnumAttribute<ax::mojom::HasPopup>(
      ax::mojom::IntAttribute::kHasPopup);
  TestInvalidEnumAttribute<ax::mojom::ImageAnnotationStatus>(
      ax::mojom::IntAttribute::kImageAnnotationStatus);
  TestInvalidEnumAttribute<ax::mojom::InvalidState>(
      ax::mojom::IntAttribute::kInvalidState);
  TestInvalidEnumAttribute<ax::mojom::IsPopup>(
      ax::mojom::IntAttribute::kIsPopup);
  TestInvalidEnumAttribute<ax::mojom::ListStyle>(
      ax::mojom::IntAttribute::kListStyle);
  TestInvalidEnumAttribute<ax::mojom::NameFrom>(
      ax::mojom::IntAttribute::kNameFrom);
  TestInvalidEnumAttribute<ax::mojom::Restriction>(
      ax::mojom::IntAttribute::kRestriction);
  TestInvalidEnumAttribute<ax::mojom::SortDirection>(
      ax::mojom::IntAttribute::kSortDirection);
  TestInvalidEnumAttribute<ax::mojom::TextAlign>(
      ax::mojom::IntAttribute::kTextAlign);
  TestInvalidEnumAttribute<ax::mojom::WritingDirection>(
      ax::mojom::IntAttribute::kTextDirection);
  TestInvalidEnumAttribute<ax::mojom::TextDecorationStyle>(
      ax::mojom::IntAttribute::kTextOverlineStyle);
  TestInvalidEnumAttribute<ax::mojom::TextPosition>(
      ax::mojom::IntAttribute::kTextPosition);
  TestInvalidEnumAttribute<ax::mojom::TextDecorationStyle>(
      ax::mojom::IntAttribute::kTextStrikethroughStyle);
  TestInvalidEnumAttribute<ax::mojom::TextDecorationStyle>(
      ax::mojom::IntAttribute::kTextUnderlineStyle);
}

TEST(AXNodeDataMojomTraitsTest, IntAttributesValidTextStyle) {
  ui::AXNodeData input, output;
  input.AddTextStyle(ax::mojom::TextStyle::kBold);
  input.AddTextStyle(ax::mojom::TextStyle::kItalic);
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_TRUE(output.HasTextStyle(ax::mojom::TextStyle::kBold));
  EXPECT_TRUE(output.HasTextStyle(ax::mojom::TextStyle::kItalic));
}

TEST(AXNodeDataMojomTraitsTest, IntAttributesInvalidTextStyle) {
  ui::AXNodeData input, output;
  input.AddIntAttribute(ax::mojom::IntAttribute::kTextStyle, 1 << 20);
  EXPECT_FALSE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
}

TEST(AXNodeDataMojomTraitsTest, FloatAttributes) {
  ui::AXNodeData input, output;
  input.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 42);
  input.AddFloatAttribute(ax::mojom::FloatAttribute::kFontWeight, 100);
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(42, output.GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize));
  EXPECT_EQ(100,
            output.GetFloatAttribute(ax::mojom::FloatAttribute::kFontWeight));
}

TEST(AXNodeDataMojomTraitsTest, BoolAttributes) {
  ui::AXNodeData input, output;
  input.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_TRUE(output.GetBoolAttribute(ax::mojom::BoolAttribute::kBusy));
}

TEST(AXNodeDataMojomTraitsTest, IntListAttributes) {
  ui::AXNodeData input, output;
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds, {1, 2});
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(
      std::vector<int32_t>({1, 2}),
      output.GetIntListAttribute(ax::mojom::IntListAttribute::kControlsIds));
}

TEST(AXNodeDataMojomTraitsTest, IntListAttributesInvalid) {
  ui::AXNodeData input, output;
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes, {1, 2});
  EXPECT_FALSE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts, {1, 2});
  EXPECT_FALSE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                            {1, 2, 3});
  EXPECT_FALSE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));

  input.AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int32_t>(ax::mojom::MarkerType::kHighlight)});
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts, {1});
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds, {2});
  EXPECT_FALSE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));

  // Valid combinations.
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes, {2});
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));

  input.AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int32_t>(ax::mojom::MarkerType::kHighlight)});
  input.AddIntListAttribute(ax::mojom::IntListAttribute::kHighlightTypes, {7});
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
}

TEST(AXNodeDataMojomTraitsTest, StringListAttributes) {
  ui::AXNodeData input, output;
  input.AddStringListAttribute(
      ax::mojom::StringListAttribute::kCustomActionDescriptions,
      {"foo", "bar"});
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(std::vector<std::string>({"foo", "bar"}),
            output.GetStringListAttribute(
                ax::mojom::StringListAttribute::kCustomActionDescriptions));
}

TEST(AXNodeDataMojomTraitsTest, ChildIds) {
  ui::AXNodeData input, output;
  input.child_ids = {3, 4};
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(std::vector<int32_t>({3, 4}), output.child_ids);
}

TEST(AXNodeDataMojomTraitsTest, OffsetContainerID) {
  ui::AXNodeData input, output;
  input.relative_bounds.offset_container_id = 10;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(10, output.relative_bounds.offset_container_id);
}

TEST(AXNodeDataMojomTraitsTest, RelativeBounds) {
  ui::AXNodeData input, output;
  input.relative_bounds.bounds = gfx::RectF(1, 2, 3, 4);
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_EQ(1, output.relative_bounds.bounds.x());
  EXPECT_EQ(2, output.relative_bounds.bounds.y());
  EXPECT_EQ(3, output.relative_bounds.bounds.width());
  EXPECT_EQ(4, output.relative_bounds.bounds.height());
}

TEST(AXNodeDataMojomTraitsTest, Transform) {
  ui::AXNodeData input, output;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_FALSE(output.relative_bounds.transform);

  input.relative_bounds.transform = std::make_unique<gfx::Transform>();
  input.relative_bounds.transform->Scale(2.0, 2.0);
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXNodeData>(input, output));
  EXPECT_TRUE(output.relative_bounds.transform);
  EXPECT_FALSE(output.relative_bounds.transform->IsIdentity());
}
