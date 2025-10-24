// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/flex_layout_algorithm.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/flex/devtools_flex_info.h"
#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class FlexLayoutAlgorithmTest : public BaseLayoutAlgorithmTest {
 protected:
  const DevtoolsFlexInfo* LayoutForDevtools(const String& body_content) {
    SetBodyInnerHTML(body_content);
    return LayoutForDevtools();
  }

  const DevtoolsFlexInfo* LayoutForDevtools() {
    LayoutObject* generic_flex = GetLayoutObjectByElementId("flexbox");
    EXPECT_NE(generic_flex, nullptr);
    auto* flex = DynamicTo<LayoutFlexibleBox>(generic_flex);
    if (!flex) {
      return nullptr;
    }
    flex->SetNeedsLayoutForDevtools();
    UpdateAllLifecyclePhasesForTest();
    return flex->FlexLayoutData();
  }
};

TEST_F(FlexLayoutAlgorithmTest, DetailsFlexDoesntCrash) {
  SetBodyInnerHTML(R"HTML(
    <details style="display:flex"></details>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // No crash is good.
}

TEST_F(FlexLayoutAlgorithmTest, ReplacedAspectRatioPrecision) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-direction: column; width: 50px">
      <svg width="29" height="22" style="width: auto; height: auto;
                                         margin: auto"></svg>
    </div>
  )HTML");

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize));
  BlockNode box(GetDocument().body()->GetLayoutBox());

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(box, space);
  EXPECT_EQ(PhysicalSize(84, 22), fragment->Size());
  ASSERT_EQ(1u, fragment->Children().size());
  fragment = To<PhysicalBoxFragment>(fragment->Children()[0].get());
  EXPECT_EQ(PhysicalSize(50, 22), fragment->Size());
  ASSERT_EQ(1u, fragment->Children().size());
  EXPECT_EQ(PhysicalSize(29, 22), fragment->Children()[0]->Size());
}

TEST_F(FlexLayoutAlgorithmTest, GapIntersectionsBasic) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    body {
        margin: 0px;
    }
    #flexbox {
      border: 2px dotted rgb(96 139 168);
      display: flex;
      column-gap: 10px;
      row-gap: 10px;
      row-rule-style: solid;
      width: 170px;
      flex-wrap: wrap;
    }
    .items {
      background-color: rgb(96 139 168 / 0.2);
      flex-shrink: 1;
      width: 50px;
      height: 50px;
    }
    </style>
    <div id="flexbox">
      <div class="items">One</div>
      <div class="items">Two</div>
      <div class="items">Three</div>
      <div class="items">Four</div>
      <div class="items">Five</div>
      <div class="items">Six</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("flexbox"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(200)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  FlexLayoutAlgorithm algorithm({node, fragment_geometry, space});

  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();

  const Vector<GapIntersectionList> expected_row_intersections = {
      {GapIntersection(LayoutUnit(2), LayoutUnit(57)),
       GapIntersection(LayoutUnit(57), LayoutUnit(57)),
       GapIntersection(LayoutUnit(117), LayoutUnit(57)),
       GapIntersection(LayoutUnit(172), LayoutUnit(57))}};
  const Vector<GapIntersectionList> expected_column_intersections = {
      {GapIntersection(LayoutUnit(57), LayoutUnit(2)),
       GapIntersection(LayoutUnit(57), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(117), LayoutUnit(2)),
       GapIntersection(LayoutUnit(117), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(57), LayoutUnit(57)),
       GapIntersection(LayoutUnit(57), LayoutUnit(112))},
      {GapIntersection(LayoutUnit(117), LayoutUnit(57)),
       GapIntersection(LayoutUnit(117), LayoutUnit(112))}};

  const Vector<GapIntersectionList> row_intersections =
      gap_geometry->GetGapIntersections(kForRows);
  const Vector<GapIntersectionList> column_intersections =
      gap_geometry->GetGapIntersections(kForColumns);
  EXPECT_EQ(row_intersections.size(), 1);
  EXPECT_EQ(column_intersections.size(), 4);

  VerifyGapIntersections(expected_row_intersections, row_intersections);
}

TEST_F(FlexLayoutAlgorithmTest, GapIntersectionsOneLine) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    body {
        margin: 0px;
    }
    #flexbox {
      border: 2px dotted rgb(96 139 168);
      display: flex;
      column-gap: 20px;
      row-gap: 10px;
      row-rule-style: solid;
      width: 170px;
      flex-wrap: wrap;
    }
    .items {
      background-color: rgb(96 139 168 / 0.2);
      flex-shrink: 1;
      height: 50px;
    }

    #item1 {
      width: 50px;
    }

    #item2 {
      width: 100px;
    }
    </style>
    <div id="flexbox">
      <div class="items" id="item1">One</div>
      <div class="items" id="item2">Two</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("flexbox"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(200)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  FlexLayoutAlgorithm algorithm({node, fragment_geometry, space});

  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();

  const Vector<GapIntersectionList> expected_row_intersections = {};
  const Vector<GapIntersectionList> expected_column_intersections = {
      {GapIntersection(LayoutUnit(62), LayoutUnit(2)),
       GapIntersection(LayoutUnit(62), LayoutUnit(52))}};

  const Vector<GapIntersectionList> row_intersections =
      gap_geometry->GetGapIntersections(kForRows);
  const Vector<GapIntersectionList> column_intersections =
      gap_geometry->GetGapIntersections(kForColumns);
  EXPECT_EQ(row_intersections.size(), 0);
  EXPECT_EQ(column_intersections.size(), 1);

  VerifyGapIntersections(expected_row_intersections, row_intersections);
  VerifyGapIntersections(expected_column_intersections, column_intersections);
}

TEST_F(FlexLayoutAlgorithmTest, GapIntersectionsNonAlignedColumn) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
body {
    margin: 0px;
}

#flexbox {
  border: 2px dotted rgb(96 139 168);
  display: flex;
  column-gap: 10px;
  row-gap: 10px;
  row-rule-style: solid;
  width: 170px;
  flex-wrap: wrap;
}

.items {
  background-color: rgb(96 139 168 / 0.2);
  flex-shrink: 1;
  width: 50px;
  height: 50px;
}

#spanner {
  width: 100px;
}
</style>

<div id="flexbox">
  <div class="items">One</div>
  <div class="items">Two</div>
  <div class="items">Three</div>
  <div class="items" id="spanner">Four</div>
  <div class="items">Five</div>
  <div class="items">Six</div>
  <div class="items">Seven</div>
  <div class="items">Eight</div>
</div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("flexbox"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(200)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  FlexLayoutAlgorithm algorithm({node, fragment_geometry, space});

  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();

  const Vector<GapIntersectionList> expected_row_intersections = {
      {GapIntersection(LayoutUnit(2), LayoutUnit(57)),
       GapIntersection(LayoutUnit(57), LayoutUnit(57)),
       GapIntersection(LayoutUnit(107), LayoutUnit(57)),
       GapIntersection(LayoutUnit(117), LayoutUnit(57)),
       GapIntersection(LayoutUnit(172), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(2), LayoutUnit(117)),
       GapIntersection(LayoutUnit(57), LayoutUnit(117)),
       GapIntersection(LayoutUnit(107), LayoutUnit(117)),
       GapIntersection(LayoutUnit(117), LayoutUnit(117)),
       GapIntersection(LayoutUnit(172), LayoutUnit(117))}};
  const Vector<GapIntersectionList> expected_column_intersections = {
      {GapIntersection(LayoutUnit(57), LayoutUnit(2)),
       GapIntersection(LayoutUnit(57), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(117), LayoutUnit(2)),
       GapIntersection(LayoutUnit(117), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(107), LayoutUnit(57)),
       GapIntersection(LayoutUnit(107), LayoutUnit(117))},
      {GapIntersection(LayoutUnit(57), LayoutUnit(117)),
       GapIntersection(LayoutUnit(57), LayoutUnit(172))},
      {GapIntersection(LayoutUnit(117), LayoutUnit(117)),
       GapIntersection(LayoutUnit(117), LayoutUnit(172))}};

  const Vector<GapIntersectionList> row_intersections =
      gap_geometry->GetGapIntersections(kForRows);
  const Vector<GapIntersectionList> column_intersections =
      gap_geometry->GetGapIntersections(kForColumns);
  EXPECT_EQ(row_intersections.size(), 2);
  EXPECT_EQ(column_intersections.size(), 5);

  VerifyGapIntersections(expected_row_intersections, row_intersections);
  VerifyGapIntersections(expected_column_intersections, column_intersections);
}

TEST_F(FlexLayoutAlgorithmTest, GapIntersectionsNonAlignedColumn2) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
body {
    margin: 0px;
}

#flexbox {
  border: 2px dotted rgb(96 139 168);
  display: flex;
  column-gap: 10px;
  row-gap: 10px;
  row-rule-style: solid;
  width: 170px;
  flex-wrap: wrap;
}

.items {
  background-color: rgb(96 139 168 / 0.2);
  flex-shrink: 1;
  width: 50px;
  height: 50px;
}

#item4 {
  width: 100px;
}

#item6 {
  width: 160px;
}
</style>

<div id="flexbox">
  <div class="items">One</div>
  <div class="items">Two</div>
  <div class="items">Three</div>
  <div class="items" id="item4">Four</div>
  <div class="items">Five</div>
  <div id="item6" class="items">Six</div>
</div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("flexbox"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(200)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  FlexLayoutAlgorithm algorithm({node, fragment_geometry, space});

  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();

  const Vector<GapIntersectionList> expected_row_intersections = {
      {GapIntersection(LayoutUnit(2), LayoutUnit(57)),
       GapIntersection(LayoutUnit(57), LayoutUnit(57)),
       GapIntersection(LayoutUnit(107), LayoutUnit(57)),
       GapIntersection(LayoutUnit(117), LayoutUnit(57)),
       GapIntersection(LayoutUnit(172), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(2), LayoutUnit(117)),
       GapIntersection(LayoutUnit(107), LayoutUnit(117)),
       GapIntersection(LayoutUnit(172), LayoutUnit(117))}};
  const Vector<GapIntersectionList> expected_column_intersections = {
      {GapIntersection(LayoutUnit(57), LayoutUnit(2)),
       GapIntersection(LayoutUnit(57), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(117), LayoutUnit(2)),
       GapIntersection(LayoutUnit(117), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(107), LayoutUnit(57)),
       GapIntersection(LayoutUnit(107), LayoutUnit(117))}};

  const Vector<GapIntersectionList> row_intersections =
      gap_geometry->GetGapIntersections(kForRows);
  const Vector<GapIntersectionList> column_intersections =
      gap_geometry->GetGapIntersections(kForColumns);
  EXPECT_EQ(row_intersections.size(), 2);
  EXPECT_EQ(column_intersections.size(), 3);

  VerifyGapIntersections(expected_row_intersections, row_intersections);
}

TEST_F(FlexLayoutAlgorithmTest, GapIntersectionsVerticalFlexAlignedCenter) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
body {
    margin: 0px;
}

#flexbox > * {
  border: 2px solid rgb(96 139 168);
  border-radius: 5px;
  background-color: rgb(96 139 168 / 0.2);
}
#flexbox {
  border: 2px dotted rgb(96 139 168);
  border-width: 4px 2px 2px 2px;
  display: flex;
  column-gap: 10px;
  row-gap: 30px;
  row-rule-style: solid;
  height: 300px;
  width: 300px;
  flex-wrap: wrap;
  align-content: center;
  writing-mode: vertical-lr;
}

.items {
  width: 70px;
  height: 70px;
}
</style>

<div id="flexbox">
  <div class="items">One</div>
  <div class="items">Two</div>
  <div class="items">Three</div>
  <div class="items">Four</div>
  <div class="items">Five</div>
  <div class="items">Six</div>
</div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("flexbox"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kVerticalLr, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(500)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  FlexLayoutAlgorithm algorithm({node, fragment_geometry, space});

  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();

  const Vector<GapIntersectionList> expected_row_intersections = {
      {GapIntersection(LayoutUnit(4), LayoutUnit(152)),
       GapIntersection(LayoutUnit(83), LayoutUnit(152)),
       GapIntersection(LayoutUnit(167), LayoutUnit(152)),
       GapIntersection(LayoutUnit(304), LayoutUnit(152))}};
  const Vector<GapIntersectionList> expected_column_intersections = {
      {GapIntersection(LayoutUnit(83), LayoutUnit(63)),
       GapIntersection(LayoutUnit(83), LayoutUnit(152))},
      {GapIntersection(LayoutUnit(167), LayoutUnit(63)),
       GapIntersection(LayoutUnit(167), LayoutUnit(152))},
      {GapIntersection(LayoutUnit(83), LayoutUnit(152)),
       GapIntersection(LayoutUnit(83), LayoutUnit(241))},
      {GapIntersection(LayoutUnit(167), LayoutUnit(152)),
       GapIntersection(LayoutUnit(167), LayoutUnit(241))}};

  const Vector<GapIntersectionList> row_intersections =
      gap_geometry->GetGapIntersections(kForRows);
  const Vector<GapIntersectionList> column_intersections =
      gap_geometry->GetGapIntersections(kForColumns);
  EXPECT_EQ(row_intersections.size(), 1);
  EXPECT_EQ(column_intersections.size(), 4);

  VerifyGapIntersections(expected_row_intersections, row_intersections);
}

TEST_F(FlexLayoutAlgorithmTest, GapIntersectionsVerticalFlexAlignedStart) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
body {
    margin: 0px;
}

#flexbox > * {
  border: 2px solid rgb(96 139 168);
  border-radius: 5px;
  background-color: rgb(96 139 168 / 0.2);
}
#flexbox {
  border: 2px dotted rgb(96 139 168);
  border-width: 4px 2px 2px 2px;
  display: flex;
  column-gap: 10px;
  row-gap: 30px;
  row-rule-style: solid;
  height: 300px;
  width: 300px;
  flex-wrap: wrap;
  align-content: start;
  writing-mode: vertical-lr;
}

.items {
  width: 70px;
  height: 70px;
}
</style>

<div id="flexbox">
  <div class="items">One</div>
  <div class="items">Two</div>
  <div class="items">Three</div>
  <div class="items">Four</div>
  <div class="items">Five</div>
  <div class="items">Six</div>
</div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("flexbox"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kVerticalLr, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(500)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  FlexLayoutAlgorithm algorithm({node, fragment_geometry, space});

  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();

  const Vector<GapIntersectionList> expected_row_intersections = {
      {GapIntersection(LayoutUnit(4), LayoutUnit(91)),
       GapIntersection(LayoutUnit(83), LayoutUnit(91)),
       GapIntersection(LayoutUnit(167), LayoutUnit(91)),
       GapIntersection(LayoutUnit(304), LayoutUnit(91))}};
  const Vector<GapIntersectionList> expected_column_intersections = {
      {GapIntersection(LayoutUnit(83), LayoutUnit(2)),
       GapIntersection(LayoutUnit(83), LayoutUnit(91))},
      {GapIntersection(LayoutUnit(167), LayoutUnit(2)),
       GapIntersection(LayoutUnit(167), LayoutUnit(91))},
      {GapIntersection(LayoutUnit(83), LayoutUnit(91)),
       GapIntersection(LayoutUnit(83), LayoutUnit(180))},
      {GapIntersection(LayoutUnit(167), LayoutUnit(91)),
       GapIntersection(LayoutUnit(167), LayoutUnit(180))}};

  const Vector<GapIntersectionList> row_intersections =
      gap_geometry->GetGapIntersections(kForRows);
  const Vector<GapIntersectionList> column_intersections =
      gap_geometry->GetGapIntersections(kForColumns);
  EXPECT_EQ(row_intersections.size(), 1);
  EXPECT_EQ(column_intersections.size(), 4);

  VerifyGapIntersections(expected_row_intersections, row_intersections);
}

TEST_F(FlexLayoutAlgorithmTest, GapIntersectionsVerticalFlexAlignedStretch) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
body {
    margin: 0px;
}

#flexbox > * {
  border: 2px solid rgb(96 139 168);
  border-radius: 5px;
  background-color: rgb(96 139 168 / 0.2);
}
#flexbox {
  border: 2px dotted rgb(96 139 168);
  border-width: 4px 2px 2px 2px;
  display: flex;
  column-gap: 10px;
  row-gap: 30px;
  row-rule-style: solid;
  height: 300px;
  width: 300px;
  flex-wrap: wrap;
  align-content: stretch;
  writing-mode: vertical-lr;
}

.items {
  width: 70px;
  height: 70px;
}
</style>

<div id="flexbox">
  <div class="items">One</div>
  <div class="items">Two</div>
  <div class="items">Three</div>
  <div class="items">Four</div>
  <div class="items">Five</div>
  <div class="items">Six</div>
</div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("flexbox"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kVerticalLr, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(500)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  FlexLayoutAlgorithm algorithm({node, fragment_geometry, space});

  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();

  const Vector<GapIntersectionList> expected_row_intersections = {
      {GapIntersection(LayoutUnit(4), LayoutUnit(152)),
       GapIntersection(LayoutUnit(83), LayoutUnit(152)),
       GapIntersection(LayoutUnit(167), LayoutUnit(152)),
       GapIntersection(LayoutUnit(304), LayoutUnit(152))}};
  const Vector<GapIntersectionList> expected_column_intersections = {
      {GapIntersection(LayoutUnit(83), LayoutUnit(2)),
       GapIntersection(LayoutUnit(83), LayoutUnit(152))},
      {GapIntersection(LayoutUnit(167), LayoutUnit(2)),
       GapIntersection(LayoutUnit(167), LayoutUnit(152))},
      {GapIntersection(LayoutUnit(83), LayoutUnit(152)),
       GapIntersection(LayoutUnit(83), LayoutUnit(302))},
      {GapIntersection(LayoutUnit(167), LayoutUnit(152)),
       GapIntersection(LayoutUnit(167), LayoutUnit(302))}};

  const Vector<GapIntersectionList> row_intersections =
      gap_geometry->GetGapIntersections(kForRows);
  const Vector<GapIntersectionList> column_intersections =
      gap_geometry->GetGapIntersections(kForColumns);
  EXPECT_EQ(row_intersections.size(), 1);
  EXPECT_EQ(column_intersections.size(), 4);

  VerifyGapIntersections(expected_row_intersections, row_intersections);
}

TEST_F(FlexLayoutAlgorithmTest, GapIntersectionsColumnFlexDirection) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    body {
        margin: 0px;
    }

    #flexbox {
        border: 2px solid rgb(96 139 168);
        display: flex;
        column-gap: 20px;
        row-gap: 10px;
        width: 120px;
        height: 170px;
        flex-wrap: wrap;
        flex-direction: column;
        column-rule-style: solid;
        column-rule-color: red;
        column-rule-width: 10px;
    }

    .items {
        background-color: rgb(96 139 168 / 0.2);
        flex-shrink: 1;
        width: 50px;
        height: 50px;
    }

</style>

<div id="flexbox">
    <div class="items">One</div>
    <div class="items">Two</div>
    <div class="items">Three</div>
    <div class="items">Four</div>
    <div class="items">Five</div>
    <div class="items">Six</div>
</div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("flexbox"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(500)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  FlexLayoutAlgorithm algorithm({node, fragment_geometry, space});

  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();

  const Vector<GapIntersectionList> expected_row_intersections = {
      {GapIntersection(LayoutUnit(2), LayoutUnit(57)),
       GapIntersection(LayoutUnit(62), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(2), LayoutUnit(117)),
       GapIntersection(LayoutUnit(62), LayoutUnit(117))},
      {GapIntersection(LayoutUnit(62), LayoutUnit(57)),
       GapIntersection(LayoutUnit(122), LayoutUnit(57))},
      {GapIntersection(LayoutUnit(62), LayoutUnit(117)),
       GapIntersection(LayoutUnit(122), LayoutUnit(117))},
  };
  const Vector<GapIntersectionList> expected_column_intersections = {
      {
          GapIntersection(LayoutUnit(62), LayoutUnit(2)),
          GapIntersection(LayoutUnit(62), LayoutUnit(57)),
          GapIntersection(LayoutUnit(62), LayoutUnit(117)),
          GapIntersection(LayoutUnit(62), LayoutUnit(172)),
      },
  };

  const Vector<GapIntersectionList> row_intersections =
      gap_geometry->GetGapIntersections(kForRows);
  const Vector<GapIntersectionList> column_intersections =
      gap_geometry->GetGapIntersections(kForColumns);
  EXPECT_EQ(row_intersections.size(), 4);
  EXPECT_EQ(column_intersections.size(), 1);

  VerifyGapIntersections(expected_row_intersections, row_intersections);
  VerifyGapIntersections(expected_column_intersections, column_intersections);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsBasic) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px;" id=flexbox>
      <div style="flex-grow: 1; height: 50px;"></div>
      <div style="flex-grow: 1"></div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
  EXPECT_EQ(devtools->lines[0].items.size(), 2u);
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(0, 0, 50, 50));
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(0, 0, 50, 50));
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsWrap) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; flex-wrap: wrap;" id=flexbox>
      <div style="min-width: 100px; height: 50px;"></div>
      <div style="flex: 1 0 20px; height: 90px;"></div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 2u);
  EXPECT_EQ(devtools->lines[0].items.size(), 1u);
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(0, 0, 100, 50));
  EXPECT_EQ(devtools->lines[1].items.size(), 1u);
  EXPECT_EQ(devtools->lines[1].items[0].rect, PhysicalRect(0, 50, 100, 90));
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsCoordinates) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; flex-wrap: wrap; border-top: 2px solid; padding-top: 3px; border-left: 3px solid; padding-left: 5px; margin-left: 19px;" id=flexbox>
      <div style="margin-left: 5px; min-width: 95px; height: 50px;"></div>
      <div style="flex: 1 0 20px; height: 90px;"></div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 2u);
  EXPECT_EQ(devtools->lines[0].items.size(), 1u);
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(8, 5, 100, 50));
  EXPECT_EQ(devtools->lines[1].items.size(), 1u);
  EXPECT_EQ(devtools->lines[1].items[0].rect, PhysicalRect(8, 55, 100, 90));
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsOverflow) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; border-left: 1px solid; border-right: 3px solid;" id=flexbox>
      <div style="min-width: 150px; height: 75px;"></div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines[0].items[0].rect, PhysicalRect(1, 0, 150, 75));
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsWithRelPosItem) {
  // Devtools' heuristic algorithm shows two lines for this case, but layout
  // knows there's only one line.
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
  <style>
  .item {
    flex: 0 0 50px;
    height: 50px;
  }
  </style>
  <div style="display: flex;" id=flexbox>
    <div class=item></div>
    <div class=item style="position: relative; top: 60px; left: -10px"></div>
  </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsBaseline) {
  LoadAhem();
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; align-items: baseline; flex-wrap: wrap; width: 250px; margin: 10px;" id=flexbox>
      <div style="width: 100px; margin: 10px; font: 10px/2 Ahem;">Test</div>
      <div style="width: 100px; margin: 10px; font: 10px/1 Ahem;">Test</div>
      <div style="width: 100px; margin: 10px; font: 10px/1 Ahem;">Test</div>
      <div style="width: 100px; margin: 10px; font: 10px/1 Ahem;">Test</div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 2u);
  EXPECT_EQ(devtools->lines[0].items.size(), 2u);
  EXPECT_GT(devtools->lines[0].items[0].baseline,
            devtools->lines[0].items[1].baseline);
  EXPECT_EQ(devtools->lines[1].items.size(), 2u);
  EXPECT_EQ(devtools->lines[1].items[0].baseline,
            devtools->lines[1].items[1].baseline);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsOneImageItemCrash) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex;" id=flexbox><img></div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsColumnWrap) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex; flex-flow: column wrap; width: 300px; height: 100px;" id=flexbox>
      <div style="height: 200px">
        <div style="height: 90%"></div>
      </div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsColumnWrapOrtho) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex; flex-flow: column wrap; width: 300px; height: 100px;" id=flexbox>
      <div style="height: 200px; writing-mode: vertical-lr;">
        <div style="width: 90%"></div>
      </div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsRowWrapOrtho) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex; flex-flow: wrap; width: 300px; height: 100px;" id=flexbox>
      <div style="height: 200px; writing-mode: vertical-lr;">
        <div style="width: 90%"></div>
        <div style="height: 90%"></div>
      </div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsLegacyItem) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="display: flex;" id=flexbox>
      <div style="columns: 1">
        <div style="display:flex;"></div>
        <div style="display:grid;"></div>
        <div style="display:table;"></div>
      </div>
    </div>
  )HTML");
  DCHECK(devtools);
  EXPECT_EQ(devtools->lines.size(), 1u);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsFragmentedItemDoesntCrash) {
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <div style="columns: 2; height: 300px; width: 300px; background: orange;">
      <div style="display: flex; background: blue;" id=flexbox>
        <div style="width: 100px; height: 300px; background: grey;"></div>
      </div>
    </div>
  )HTML");
  // We don't currently set DevtoolsFlexInfo when fragmenting.
  DCHECK(!devtools);
}

TEST_F(FlexLayoutAlgorithmTest, DevtoolsAutoScrollbar) {
  // Pass if we get a devtools info object and don't crash.
  const DevtoolsFlexInfo* devtools = LayoutForDevtools(R"HTML(
    <style>
      ::-webkit-scrollbar {
        width: 10px;
      }
    </style>
    <div id="flexbox" style="display:flex; height:100px;">
      <div style="overflow:auto; width:100px;">
        <div id="inner" style="height:200px;"></div>
      </div>
    </div>
  )HTML");
  EXPECT_TRUE(devtools);

  // Make the inner child short enough to eliminate the need for a scrollbar.
  Element* inner = GetElementById("inner");
  inner->SetInlineStyleProperty(CSSPropertyID::kHeight, "50px");

  devtools = LayoutForDevtools();
  EXPECT_TRUE(devtools);
}

}  // namespace
}  // namespace blink
