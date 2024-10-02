// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_span.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// The spec turned into a discussion that may change. Put this logic on hold
// until CSSWG resolves the issue.
// https://github.com/w3c/csswg-drafts/issues/337
#define SEGMENT_BREAK_TRANSFORMATION_FOR_EAST_ASIAN_WIDTH 0

using ::testing::ElementsAre;

class NGInlineNodeForTest : public NGInlineNode {
 public:
  using NGInlineNode::NGInlineNode;

  std::string Text() const { return Data().text_content.Utf8(); }
  HeapVector<NGInlineItem>& Items() { return MutableData()->items; }
  static HeapVector<NGInlineItem>& Items(NGInlineNodeData& data) {
    return data.items;
  }

  void Append(const String& text, LayoutObject* layout_object) {
    NGInlineNodeData* data = MutableData();
    unsigned start = data->text_content.length();
    data->text_content = data->text_content + text;
    data->items.push_back(NGInlineItem(NGInlineItem::kText, start,
                                       start + text.length(), layout_object));
  }

  void Append(UChar character) {
    NGInlineNodeData* data = MutableData();
    data->text_content = data->text_content + character;
    unsigned end = data->text_content.length();
    data->items.push_back(
        NGInlineItem(NGInlineItem::kBidiControl, end - 1, end, nullptr));
    data->is_bidi_enabled_ = true;
  }

  void ClearText() {
    NGInlineNodeData* data = MutableData();
    data->text_content = String();
    data->items.clear();
  }

  void SegmentText() {
    NGInlineNodeData* data = MutableData();
    data->is_bidi_enabled_ = true;
    NGInlineNode::SegmentText(data);
  }

  void CollectInlines() { NGInlineNode::CollectInlines(MutableData()); }
  void ShapeText() { NGInlineNode::ShapeText(MutableData()); }
};

class NGInlineNodeTest : public RenderingTest {
 protected:
  void SetupHtml(const char* id, String html) {
    SetBodyInnerHTML(html);
    layout_block_flow_ = To<LayoutNGBlockFlow>(GetLayoutObjectByElementId(id));
    layout_object_ = layout_block_flow_->FirstChild();
  }

  void UseLayoutObjectAndAhem() {
    // Get Ahem from document. Loading "Ahem.woff" using |createTestFont| fails
    // on linux_chromium_asan_rel_ng.
    LoadAhem();
    SetupHtml("t", "<div id=t style='font:10px Ahem'>test</div>");
  }

  NGInlineNodeForTest CreateInlineNode(
      LayoutNGBlockFlow* layout_block_flow = nullptr) {
    if (layout_block_flow)
      layout_block_flow_ = layout_block_flow;
    if (!layout_block_flow_)
      SetupHtml("t", "<div id=t style='font:10px'>test</div>");
    NGInlineNodeForTest node(layout_block_flow_);
    node.InvalidatePrepareLayoutForTest();
    return node;
  }

  MinMaxSizes ComputeMinMaxSizes(NGInlineNode node) {
    const auto space =
        NGConstraintSpaceBuilder(node.Style().GetWritingMode(),
                                 node.Style().GetWritingDirection(),
                                 /* is_new_fc */ false)
            .ToConstraintSpace();

    return node
        .ComputeMinMaxSizes(node.Style().GetWritingMode(), space,
                            MinMaxSizesFloatInput())
        .sizes;
  }

  const String& GetText() const {
    NGInlineNodeData* data = layout_block_flow_->GetNGInlineNodeData();
    CHECK(data);
    return data->text_content;
  }

  HeapVector<NGInlineItem>& Items() {
    NGInlineNodeData* data = layout_block_flow_->GetNGInlineNodeData();
    CHECK(data);
    return NGInlineNodeForTest::Items(*data);
  }

  void ForceLayout() { GetDocument().body()->OffsetTop(); }

  Vector<unsigned> ToEndOffsetList(
      NGInlineItemSegments::const_iterator segments) {
    Vector<unsigned> end_offsets;
    for (const RunSegmenter::RunSegmenterRange& segment : segments)
      end_offsets.push_back(segment.end);
    return end_offsets;
  }

  void TestAnyItemsAreDirty(const LayoutBlockFlow& block_flow, bool expected) {
    NGFragmentItems::DirtyLinesFromNeedsLayout(block_flow);
    for (const NGPhysicalBoxFragment& fragment :
         block_flow.PhysicalFragments()) {
      if (const NGFragmentItems* items = fragment.Items()) {
        // Check |NGFragmentItem::IsDirty| directly without using
        // |EndOfReusableItems|. This is different from the line cache logic,
        // but some items may not be reusable even if |!IsDirty()|.
        for (const NGFragmentItem& item : items->Items()) {
          if (item.IsDirty()) {
            EXPECT_TRUE(expected);
            return;
          }
        }
      }
    }
    EXPECT_FALSE(expected);
  }

  // "Google Sans" has ligatures, e.g. "fi", "tt", etc.
  void LoadGoogleSans() {
    LoadFontFromFile(GetFrame(),
                     test::CoreTestDataPath("GoogleSans-Regular.ttf"),
                     "Google Sans");
  }

  Persistent<LayoutNGBlockFlow> layout_block_flow_;
  Persistent<LayoutObject> layout_object_;
  FontCachePurgePreventer purge_preventer_;
};

#define TEST_ITEM_TYPE_OFFSET(item, type, start, end) \
  EXPECT_EQ(NGInlineItem::type, item.Type());         \
  EXPECT_EQ(start, item.StartOffset());               \
  EXPECT_EQ(end, item.EndOffset())

#define TEST_ITEM_TYPE_OFFSET_LEVEL(item, type, start, end, level) \
  EXPECT_EQ(NGInlineItem::type, item.Type());                      \
  EXPECT_EQ(start, item.StartOffset());                            \
  EXPECT_EQ(end, item.EndOffset());                                \
  EXPECT_EQ(level, item.BidiLevel())

#define TEST_ITEM_OFFSET_DIR(item, start, end, direction) \
  EXPECT_EQ(start, item.StartOffset());                   \
  EXPECT_EQ(end, item.EndOffset());                       \
  EXPECT_EQ(direction, item.Direction())

TEST_F(NGInlineNodeTest, CollectInlinesText) {
  SetupHtml("t", "<div id=t>Hello <span>inline</span> world.</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_FALSE(node.IsBidiEnabled());
  HeapVector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 6u);
  TEST_ITEM_TYPE_OFFSET(items[1], kOpenTag, 6u, 6u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 6u, 12u);
  TEST_ITEM_TYPE_OFFSET(items[3], kCloseTag, 12u, 12u);
  TEST_ITEM_TYPE_OFFSET(items[4], kText, 12u, 19u);
  EXPECT_EQ(5u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesBR) {
  SetupHtml("t", u"<div id=t>Hello<br>World</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_EQ("Hello\nWorld", node.Text());
  EXPECT_FALSE(node.IsBidiEnabled());
  HeapVector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 5u);
  TEST_ITEM_TYPE_OFFSET(items[1], kControl, 5u, 6u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 6u, 11u);
  EXPECT_EQ(3u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesFloat) {
  SetupHtml("t",
            "<div id=t>"
            "abc"
            "<span style='float:right'>DEF</span>"
            "ghi"
            "<span style='float:left'>JKL</span>"
            "mno"
            "</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_EQ("abc\uFFFCghi\uFFFCmno", node.Text())
      << "floats are appeared as an object replacement character";
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(5u, items.size());
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 3u);
  TEST_ITEM_TYPE_OFFSET(items[1], kFloating, 3u, 4u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 4u, 7u);
  TEST_ITEM_TYPE_OFFSET(items[3], kFloating, 7u, 8u);
  TEST_ITEM_TYPE_OFFSET(items[4], kText, 8u, 11u);
}

TEST_F(NGInlineNodeTest, CollectInlinesInlineBlock) {
  SetupHtml("t",
            "<div id=t>"
            "abc<span style='display:inline-block'>DEF</span>jkl"
            "</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_EQ("abc\uFFFCjkl", node.Text())
      << "inline-block is appeared as an object replacement character";
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(3u, items.size());
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 3u);
  TEST_ITEM_TYPE_OFFSET(items[1], kAtomicInline, 3u, 4u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 4u, 7u);
}

TEST_F(NGInlineNodeTest, CollectInlinesUTF16) {
  SetupHtml("t", u"<div id=t>Hello \u3042</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  // |CollectInlines()| sets |IsBidiEnabled()| for any UTF-16 strings.
  EXPECT_TRUE(node.IsBidiEnabled());
  // |SegmentText()| analyzes the string and resets |IsBidiEnabled()| if all
  // characters are LTR.
  node.SegmentText();
  EXPECT_FALSE(node.IsBidiEnabled());
}

TEST_F(NGInlineNodeTest, CollectInlinesRtl) {
  SetupHtml("t", u"<div id=t>Hello \u05E2</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
}

TEST_F(NGInlineNodeTest, CollectInlinesRtlWithSpan) {
  SetupHtml("t", u"<div id=t dir=rtl>\u05E2 <span>\u05E2</span> \u05E2</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
  HeapVector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[0], kText, 0u, 2u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[1], kOpenTag, 2u, 2u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[2], kText, 2u, 3u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[3], kCloseTag, 3u, 3u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[4], kText, 3u, 5u, 1u);
  EXPECT_EQ(5u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesMixedText) {
  SetupHtml("t", u"<div id=t>Hello, \u05E2 <span>\u05E2</span></div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
  HeapVector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[0], kText, 0u, 7u, 0u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[1], kText, 7u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[2], kOpenTag, 9u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[3], kText, 9u, 10u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[4], kCloseTag, 10u, 10u, 1u);
  EXPECT_EQ(5u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesMixedTextEndWithON) {
  SetupHtml("t", u"<div id=t>Hello, \u05E2 <span>\u05E2!</span></div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
  HeapVector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[0], kText, 0u, 7u, 0u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[1], kText, 7u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[2], kOpenTag, 9u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[3], kText, 9u, 10u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[4], kText, 10u, 11u, 0u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[5], kCloseTag, 11u, 11u, 0u);
  EXPECT_EQ(6u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesTextCombineBR) {
  InsertStyleElement(
      "#t { text-combine-upright: all; writing-mode: vertical-rl; }");
  SetupHtml("t", u"<div id=t>a<br>z</div>");
  NGInlineNodeForTest node =
      CreateInlineNode(To<LayoutNGBlockFlow>(layout_object_.Get()));
  node.CollectInlines();
  EXPECT_EQ("a z", node.Text());
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(3u, items.size());
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 1u);
  TEST_ITEM_TYPE_OFFSET(items[1], kText, 1u, 2u) << "<br> isn't control";
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 2u, 3u);
}

// http://crbug.com/1222633
TEST_F(NGInlineNodeTest, CollectInlinesTextCombineListItemMarker) {
  InsertStyleElement(
      "#t { text-combine-upright: all; writing-mode: vertical-rl; }");
  SetupHtml("t", u"<li id=t>ab</li>");
  // LayoutNGListItem {LI}
  //   LayoutNGOutsideListMarker {::marker}
  //      LayoutNGTextCombine (anonymous)
  //        LayoutText (anonymous) "\x{2022} "
  //   LayoutNGTextCombine (anonymous)
  //     LayoutText {#text} "a"
  NGInlineNodeForTest node = CreateInlineNode(
      To<LayoutNGTextCombine>(layout_object_->SlowFirstChild()));
  node.CollectInlines();
  EXPECT_EQ("\u2022", node.Text());
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(1u, items.size());
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 1u);
  EXPECT_TRUE(items[0].IsSymbolMarker());
}

TEST_F(NGInlineNodeTest, CollectInlinesTextCombineNewline) {
  InsertStyleElement(
      "#t { text-combine-upright: all; writing-mode: vertical-rl; }");
  SetupHtml("t", u"<pre id=t>a\nz</pre>");
  NGInlineNodeForTest node =
      CreateInlineNode(To<LayoutNGBlockFlow>(layout_object_.Get()));
  node.CollectInlines();
  EXPECT_EQ("a z", node.Text());
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(3u, items.size());
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 1u);
  TEST_ITEM_TYPE_OFFSET(items[1], kText, 1u, 2u) << "newline isn't control";
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 2u, 3u);
}

TEST_F(NGInlineNodeTest, CollectInlinesTextCombineWBR) {
  InsertStyleElement(
      "#t { text-combine-upright: all; writing-mode: vertical-rl; }");
  SetupHtml("t", u"<div id=t>a<wbr>z</div>");
  NGInlineNodeForTest node =
      CreateInlineNode(To<LayoutNGBlockFlow>(layout_object_.Get()));
  node.CollectInlines();
  EXPECT_EQ("a\u200Bz", node.Text());
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(3u, items.size());
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 1u);
  TEST_ITEM_TYPE_OFFSET(items[1], kText, 1u, 2u) << "<wbr> isn't control";
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 2u, 3u);
}

TEST_F(NGInlineNodeTest, SegmentASCII) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append("Hello", layout_object_);
  node.SegmentText();
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(1u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 5u, TextDirection::kLtr);
}

TEST_F(NGInlineNodeTest, SegmentHebrew) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", layout_object_);
  node.SegmentText();
  ASSERT_EQ(1u, node.Items().size());
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(1u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 5u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentSplit1To2) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append(u"Hello \u05E2\u05D1\u05E8\u05D9\u05EA", layout_object_);
  node.SegmentText();
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(2u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 11u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentSplit3To4) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append("Hel", layout_object_);
  node.Append(u"lo \u05E2", layout_object_);
  node.Append(u"\u05D1\u05E8\u05D9\u05EA", layout_object_);
  node.SegmentText();
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(4u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 3u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 3u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[2], 6u, 7u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 7u, 11u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentBidiOverride) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append("Hello ", layout_object_);
  node.Append(kRightToLeftOverrideCharacter);
  node.Append("ABC", layout_object_);
  node.Append(kPopDirectionalFormattingCharacter);
  node.SegmentText();
  HeapVector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(4u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 7u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[2], 7u, 10u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 10u, 11u, TextDirection::kLtr);
}

static NGInlineNodeForTest CreateBidiIsolateNode(NGInlineNodeForTest node,
                                                 LayoutObject* layout_object) {
  node.Append("Hello ", layout_object);
  node.Append(kRightToLeftIsolateCharacter);
  node.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA ", layout_object);
  node.Append(kLeftToRightIsolateCharacter);
  node.Append("A", layout_object);
  node.Append(kPopDirectionalIsolateCharacter);
  node.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", layout_object);
  node.Append(kPopDirectionalIsolateCharacter);
  node.Append(" World", layout_object);
  node.SegmentText();
  return node;
}

TEST_F(NGInlineNodeTest, SegmentBidiIsolate) {
  NGInlineNodeForTest node = CreateInlineNode();
  node = CreateBidiIsolateNode(node, layout_object_);
  HeapVector<NGInlineItem>& items = node.Items();
  EXPECT_EQ(9u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 7u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[2], 7u, 13u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 13u, 14u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[4], 14u, 15u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[5], 15u, 16u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[6], 16u, 21u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[7], 21u, 22u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[8], 22u, 28u, TextDirection::kLtr);
}

TEST_F(NGInlineNodeTest, MinMaxSizes) {
  LoadAhem();
  SetupHtml("t", "<div id=t style='font:10px Ahem'>AB CDEF</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSizes sizes = ComputeMinMaxSizes(node);
  EXPECT_EQ(40, sizes.min_size);
  EXPECT_EQ(70, sizes.max_size);
}

TEST_F(NGInlineNodeTest, MinMaxSizesElementBoundary) {
  LoadAhem();
  SetupHtml("t", "<div id=t style='font:10px Ahem'>A B<span>C D</span></div>");
  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSizes sizes = ComputeMinMaxSizes(node);
  // |min_content| should be the width of "BC" because there is an element
  // boundary between "B" and "C" but no break opportunities.
  EXPECT_EQ(20, sizes.min_size);
  EXPECT_EQ(60, sizes.max_size);
}

TEST_F(NGInlineNodeTest, MinMaxSizesFloats) {
  LoadAhem();
  SetupHtml("t", R"HTML(
    <style>
      #left { float: left; width: 50px; }
    </style>
    <div id=t style="font: 10px Ahem">
      XXX <div id="left"></div> XXXX
    </div>
  )HTML");

  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSizes sizes = ComputeMinMaxSizes(node);

  EXPECT_EQ(50, sizes.min_size);
  EXPECT_EQ(130, sizes.max_size);
}

TEST_F(NGInlineNodeTest, MinMaxSizesCloseTagAfterForcedBreak) {
  LoadAhem();
  SetupHtml("t", R"HTML(
    <style>
      span { border: 30px solid blue; }
    </style>
    <div id=t style="font: 10px Ahem">
      <span>12<br></span>
    </div>
  )HTML");

  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSizes sizes = ComputeMinMaxSizes(node);
  // The right border of the `</span>` is included in the line even if it
  // appears after `<br>`. crbug.com/991320.
  EXPECT_EQ(80, sizes.min_size);
  EXPECT_EQ(80, sizes.max_size);
}

TEST_F(NGInlineNodeTest, MinMaxSizesFloatsClearance) {
  LoadAhem();
  SetupHtml("t", R"HTML(
    <style>
      #left { float: left; width: 40px; }
      #right { float: right; clear: left; width: 50px; }
    </style>
    <div id=t style="font: 10px Ahem">
      XXX <div id="left"></div><div id="right"></div><div id="left"></div> XXX
    </div>
  )HTML");

  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSizes sizes = ComputeMinMaxSizes(node);

  EXPECT_EQ(50, sizes.min_size);
  EXPECT_EQ(160, sizes.max_size);
}

// For http://crbug.com/1112560
TEST_F(NGInlineNodeTest, MinMaxSizesSaturated) {
  LoadAhem();
  SetupHtml("t", R"HTML(
    <style>
    b {
        display: inline-block;
        border-inline-start: groove;
        width:1e8px;
    }
    #t {
        float: left;
        font: 10px Ahem;
    }
    </style>
    <div id=t><b></b> <img></div>)HTML");

  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSizes sizes = ComputeMinMaxSizes(node);
  EXPECT_EQ(LayoutUnit(33554431), sizes.min_size.Round());
  // Note: |sizes.max_size.Round()| isn't |LayoutUnit::Max()| on some platform.
}

TEST_F(NGInlineNodeTest, MinMaxSizesTabulationWithBreakWord) {
  LoadAhem();
  SetupHtml("t", R"HTML(
    <style>
    #t {
      font: 10px Ahem;
      white-space: pre-wrap;
      word-break: break-word;
    }
    </style>
    <div id=t>&#9;&#9;<span>X</span></div>
  )HTML");

  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSizes sizes = ComputeMinMaxSizes(node);
  EXPECT_EQ(10, sizes.min_size);
  EXPECT_EQ(170, sizes.max_size);
}

// For http://crbug.com/1116713
TEST_F(NGInlineNodeTest, MinMaxSizesNeedsLayout) {
  LoadAhem();
  SetupHtml("t",
            "<style>#t { width: 2ch; }</style>"
            "<div id=t> a <b>b</b></div>");

  auto& text = To<Text>(*GetElementById("t")->firstChild());
  LayoutText& layout_text = *text.GetLayoutObject();
  EXPECT_FALSE(layout_text.NeedsLayout());

  text.replaceData(0, 1, u"X", ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(layout_text.NeedsLayout());

  NGInlineNodeForTest node = CreateInlineNode();
  ComputeMinMaxSizes(node);
  EXPECT_TRUE(layout_text.NeedsLayout());
}

TEST_F(NGInlineNodeTest, AssociatedItemsWithControlItem) {
  SetBodyInnerHTML(
      "<pre id=t style='-webkit-rtl-ordering:visual'>ab\nde</pre>");
  auto* const layout_text = To<LayoutText>(
      GetDocument().getElementById("t")->firstChild()->GetLayoutObject());
  ASSERT_TRUE(layout_text->HasValidInlineItems());
  Vector<const NGInlineItem*> items;
  for (const NGInlineItem& item : layout_text->InlineItems())
    items.push_back(&item);
  ASSERT_EQ(5u, items.size());
  TEST_ITEM_TYPE_OFFSET((*items[0]), kText, 1u, 3u);
  TEST_ITEM_TYPE_OFFSET((*items[1]), kBidiControl, 3u, 4u);
  TEST_ITEM_TYPE_OFFSET((*items[2]), kControl, 4u, 5u);
  TEST_ITEM_TYPE_OFFSET((*items[3]), kBidiControl, 5u, 6u);
  TEST_ITEM_TYPE_OFFSET((*items[4]), kText, 6u, 8u);
}

TEST_F(NGInlineNodeTest, NeedsCollectInlinesOnSetText) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <span id="previous"></span>
      <span id="parent">old</span>
      <span id="next"></span>
    </div>
  )HTML");

  Element* container = GetElementById("container");
  Element* parent = GetElementById("parent");
  auto* text = To<Text>(parent->firstChild());
  EXPECT_FALSE(text->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(parent->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(container->GetLayoutObject()->NeedsCollectInlines());

  text->setData("new");
  GetDocument().UpdateStyleAndLayoutTree();

  // The text and ancestors up to the container should be marked.
  EXPECT_TRUE(text->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_TRUE(parent->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_TRUE(container->GetLayoutObject()->NeedsCollectInlines());

  // Siblings of |parent| should stay clean.
  Element* previous = GetElementById("previous");
  Element* next = GetElementById("next");
  EXPECT_FALSE(previous->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(next->GetLayoutObject()->NeedsCollectInlines());
}

struct StyleChangeData {
  const char* css;
  enum ChangedElements {
    kText = 1,
    kParent = 2,
    kContainer = 4,

    kNone = 0,
    kTextAndParent = kText | kParent,
    kParentAndAbove = kParent | kContainer,
    kAll = kText | kParentAndAbove,
  };
  unsigned needs_collect_inlines;
  absl::optional<bool> is_line_dirty;
  absl::optional<bool> invalidate_ink_overflow;
} style_change_data[] = {
    // Changing color, text-decoration, outline, etc. should not re-run
    // |CollectInlines()|.
    {"#parent.after { color: red; }", StyleChangeData::kNone, false},
    {"#parent.after { text-decoration-line: underline; }",
     StyleChangeData::kNone, false, true},
    {"#parent { background: orange; }"  // Make sure it's not culled.
     "#parent.after { outline: auto; }",
     StyleChangeData::kNone, false, false},
    // Changing fonts should re-run |CollectInlines()|.
    {"#parent.after { font-size: 200%; }", StyleChangeData::kAll, true},
    // Changing from/to out-of-flow should re-rerun |CollectInlines()|.
    {"#parent.after { position: absolute; }", StyleChangeData::kContainer,
     true},
    {"#parent { position: absolute; }"
     "#parent.after { position: initial; }",
     StyleChangeData::kContainer, true},
    // List markers are captured in |NGInlineItem|.
    {"#parent.after { display: list-item; }", StyleChangeData::kContainer},
    {"#parent { display: list-item; list-style-type: none; }"
     "#parent.after { list-style-type: disc; }",
     StyleChangeData::kParent},
    {"#parent { display: list-item; }"
     "#container.after { list-style-type: none; }",
     StyleChangeData::kParent},
    // Changing properties related with bidi resolution should re-run
    // |CollectInlines()|.
    {"#parent.after { unicode-bidi: bidi-override; }",
     StyleChangeData::kParentAndAbove, true},
    {"#container.after { unicode-bidi: bidi-override; }",
     StyleChangeData::kContainer, false},
};

std::ostream& operator<<(std::ostream& os, const StyleChangeData& data) {
  return os << data.css;
}

class StyleChangeTest : public NGInlineNodeTest,
                        public testing::WithParamInterface<StyleChangeData> {};

INSTANTIATE_TEST_SUITE_P(NGInlineNodeTest,
                         StyleChangeTest,
                         testing::ValuesIn(style_change_data));

TEST_P(StyleChangeTest, NeedsCollectInlinesOnStyle) {
  auto data = GetParam();
  SetBodyInnerHTML(String(R"HTML(
    <style>
    )HTML") + data.css +
                   R"HTML(
    </style>
    <div id="container">
      <span id="previous"></span>
      <span id="parent">text</span>
      <span id="next"></span>
    </div>
  )HTML");

  Element* container = GetElementById("container");
  Element* parent = GetElementById("parent");
  auto* text = To<Text>(parent->firstChild());
  EXPECT_FALSE(text->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(parent->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(container->GetLayoutObject()->NeedsCollectInlines());

  container->classList().Add("after");
  parent->classList().Add("after");
  GetDocument().UpdateStyleAndLayoutTree();

  // The text and ancestors up to the container should be marked.
  unsigned changes = StyleChangeData::kNone;
  if (text->GetLayoutObject()->NeedsCollectInlines())
    changes |= StyleChangeData::kText;
  if (parent->GetLayoutObject()->NeedsCollectInlines())
    changes |= StyleChangeData::kParent;
  if (container->GetLayoutObject()->NeedsCollectInlines())
    changes |= StyleChangeData::kContainer;
  EXPECT_EQ(changes, data.needs_collect_inlines);

  // Siblings of |parent| should stay clean.
  Element* previous = GetElementById("previous");
  Element* next = GetElementById("next");
  EXPECT_FALSE(previous->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(next->GetLayoutObject()->NeedsCollectInlines());

  if (data.is_line_dirty) {
    TestAnyItemsAreDirty(*To<LayoutBlockFlow>(container->GetLayoutObject()),
                         *data.is_line_dirty);
  }

  if (data.invalidate_ink_overflow) {
    const LayoutObject* parent_layout_object = parent->GetLayoutObject();
    for (const LayoutObject* child = parent_layout_object->SlowFirstChild();
         child; child = child->NextInPreOrder(parent_layout_object)) {
      if (child->IsText()) {
        NGInlineCursor cursor;
        for (cursor.MoveTo(*child); cursor;
             cursor.MoveToNextForSameLayoutObject()) {
          const NGFragmentItem* item = cursor.CurrentItem();
          EXPECT_EQ(item->IsInkOverflowComputed(),
                    !*data.invalidate_ink_overflow);
        }
      }
    }
  }

  ForceLayout();  // Ensure running layout does not crash.
}

using CreateNode = Node* (*)(Document&);
static CreateNode node_creators[] = {
    [](Document& document) -> Node* { return document.createTextNode("new"); },
    [](Document& document) -> Node* {
      return document.CreateRawElement(html_names::kSpanTag);
    },
    [](Document& document) -> Node* {
      Element* element = document.CreateRawElement(html_names::kSpanTag);
      element->classList().Add("abspos");
      return element;
    },
    [](Document& document) -> Node* {
      Element* element = document.CreateRawElement(html_names::kSpanTag);
      element->classList().Add("float");
      return element;
    }};

class NodeInsertTest : public NGInlineNodeTest,
                       public testing::WithParamInterface<CreateNode> {};

INSTANTIATE_TEST_SUITE_P(NGInlineNodeTest,
                         NodeInsertTest,
                         testing::ValuesIn(node_creators));

TEST_P(NodeInsertTest, NeedsCollectInlinesOnInsert) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .abspos { position: absolute; }
    .float { float: left; }
    </style>
    <div id="container">
      <span id="previous"></span>
      <span id="parent"></span>
      <span id="next"></span>
    </div>
  )HTML");

  Element* container = GetElementById("container");
  Element* parent = GetElementById("parent");
  EXPECT_FALSE(parent->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(container->GetLayoutObject()->NeedsCollectInlines());

  Node* insert = (*GetParam())(GetDocument());
  parent->appendChild(insert);
  GetDocument().UpdateStyleAndLayoutTree();

  // Ancestors up to the container should be marked.
  EXPECT_TRUE(parent->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_TRUE(container->GetLayoutObject()->NeedsCollectInlines());

  // Siblings of |parent| should stay clean.
  Element* previous = GetElementById("previous");
  Element* next = GetElementById("next");
  EXPECT_FALSE(previous->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(next->GetLayoutObject()->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, NeedsCollectInlinesOnInsertToOutOfFlowButton) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #xflex { display: flex; }
    </style>
    <div id="container">
      <button id="flex" style="position: absolute"></button>
    </div>
  )HTML");

  Element* container = GetElementById("container");
  Element* parent = ElementTraversal::FirstChild(*container);
  Element* child = GetDocument().CreateRawElement(html_names::kDivTag);
  parent->appendChild(child);
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_FALSE(container->GetLayoutObject()->NeedsCollectInlines());
}

class NodeRemoveTest : public NGInlineNodeTest,
                       public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(
    NGInlineNodeTest,
    NodeRemoveTest,
    testing::Values(nullptr, "span", "abspos", "float", "inline-block", "img"));

TEST_P(NodeRemoveTest, NeedsCollectInlinesOnRemove) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .abspos { position: absolute; }
    .float { float: left; }
    .inline-block { display: inline-block; }
    </style>
    <div id="container">
      <span id="previous"></span>
      <span id="parent">
        text
        <span id="span">span</span>
        <span id="abspos">abspos</span>
        <span id="float">float</span>
        <span id="inline-block">inline-block</span>
        <img id="img">
      </span>
      <span id="next"></span>
    </div>
  )HTML");

  Element* container = GetElementById("container");
  Element* parent = GetElementById("parent");
  EXPECT_FALSE(parent->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(container->GetLayoutObject()->NeedsCollectInlines());

  const char* id = GetParam();
  if (id) {
    Element* target = GetElementById(GetParam());
    target->remove();
  } else {
    Node* target = parent->firstChild();
    target->remove();
  }
  GetDocument().UpdateStyleAndLayoutTree();

  // Ancestors up to the container should be marked.
  EXPECT_TRUE(parent->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_TRUE(container->GetLayoutObject()->NeedsCollectInlines());

  // Siblings of |parent| should stay clean.
  Element* previous = GetElementById("previous");
  Element* next = GetElementById("next");
  EXPECT_FALSE(previous->GetLayoutObject()->NeedsCollectInlines());
  EXPECT_FALSE(next->GetLayoutObject()->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, NeedsCollectInlinesOnForceLayout) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <span id="target">
        <span id="child" style="position: absolute">X</span>
      </span>
    </div>
  )HTML");

  LayoutObject* container = GetLayoutObjectByElementId("container");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutObject* child = GetLayoutObjectByElementId("child");
  child->ForceLayout();
  EXPECT_FALSE(container->NeedsCollectInlines());
  EXPECT_FALSE(target->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, CollectInlinesShouldNotClearFirstInlineFragment) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      text
    </div>
  )HTML");

  // Appending a child should set |NeedsCollectInlines|.
  Element* container = GetElementById("container");
  container->appendChild(GetDocument().createTextNode("add"));
  auto* block_flow = To<LayoutBlockFlow>(container->GetLayoutObject());
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(block_flow->NeedsCollectInlines());

  // |IsBlockLevel| should run |CollectInlines|.
  NGInlineNode node(block_flow);
  node.IsBlockLevel();
  EXPECT_FALSE(block_flow->NeedsCollectInlines());

  // Running |CollectInlines| should not clear |FirstInlineFragment|.
  LayoutObject* first_child = container->firstChild()->GetLayoutObject();
  EXPECT_TRUE(first_child->HasInlineFragments());
}

TEST_F(NGInlineNodeTest, SegmentBidiChangeSetsNeedsLayout) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" dir="rtl">
      abc-<span id="span">xyz</span>
    </div>
  )HTML");

  // Because "-" is a neutral character, changing the following character to RTL
  // will change its bidi level.
  Element* span = GetElementById("span");
  span->setTextContent(u"\u05D1");

  // |NGInlineNode::SegmentBidiRuns| sets |NeedsLayout|. Run the lifecycle only
  // up to |PrepareLayout|.
  GetDocument().UpdateStyleAndLayoutTree();
  LayoutBlockFlow* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGInlineNode node(container);
  node.PrepareLayoutIfNeeded();

  LayoutText* abc = To<LayoutText>(container->FirstChild());
  EXPECT_TRUE(abc->NeedsLayout());
}

TEST_F(NGInlineNodeTest, InvalidateAddSpan) {
  SetupHtml("t", "<div id=t>before</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  auto* parent = To<Element>(layout_block_flow_->GetNode());
  Element* span = GetDocument().CreateRawElement(html_names::kSpanTag);
  parent->appendChild(span);

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, open/close items should be added.
  ForceLayout();
  EXPECT_EQ(item_count_before + 2, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateRemoveSpan) {
  SetupHtml("t", "<div id=t><span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->remove();
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, InvalidateAddInnerSpan) {
  SetupHtml("t", "<div id=t><span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  Element* parent = GetElementById("x");
  ASSERT_TRUE(parent);
  Element* span = GetDocument().CreateRawElement(html_names::kSpanTag);
  parent->appendChild(span);

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, open/close items should be added.
  ForceLayout();
  EXPECT_EQ(item_count_before + 2, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateRemoveInnerSpan) {
  SetupHtml("t", "<div id=t><span><span id=x></span></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->remove();
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, InvalidateSetText) {
  SetupHtml("t", "<div id=t>before</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  auto* text = To<LayoutText>(layout_block_flow_->FirstChild());
  text->SetTextIfNeeded(String("after").Impl());
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, InvalidateAddAbsolute) {
  SetupHtml("t",
            "<style>span { position: absolute; }</style>"
            "<div id=t>before</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  auto* parent = To<Element>(layout_block_flow_->GetNode());
  Element* span = GetDocument().CreateRawElement(html_names::kSpanTag);
  parent->appendChild(span);

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, an OOF item should be added.
  ForceLayout();
  EXPECT_EQ(item_count_before + 1, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateRemoveAbsolute) {
  SetupHtml("t",
            "<style>span { position: absolute; }</style>"
            "<div id=t>before<span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->remove();
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, InvalidateChangeToAbsolute) {
  SetupHtml("t",
            "<style>#y { position: absolute; }</style>"
            "<div id=t>before<span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->SetIdAttribute("y");

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, an open/close items should be replaced with an
  // OOF item.
  ForceLayout();
  EXPECT_EQ(item_count_before - 1, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateChangeFromAbsolute) {
  SetupHtml("t",
            "<style>#x { position: absolute; }</style>"
            "<div id=t>before<span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->SetIdAttribute("y");

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, an OOF item should be replaced with open/close
  // items..
  ForceLayout();
  EXPECT_EQ(item_count_before + 1, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateAddFloat) {
  SetupHtml("t",
            "<style>span { float: left; }</style>"
            "<div id=t>before</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  auto* parent = To<Element>(layout_block_flow_->GetNode());
  Element* span = GetDocument().CreateRawElement(html_names::kSpanTag);
  parent->appendChild(span);

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, an float item should be added.
  ForceLayout();
  EXPECT_EQ(item_count_before + 1, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateRemoveFloat) {
  SetupHtml("t",
            "<style>span { float: left; }</style>"
            "<div id=t>before<span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->remove();
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, SpaceRestoredByInsertingWord) {
  SetupHtml("t", "<div id=t>before <span id=x></span> after</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  EXPECT_EQ(String("before after"), GetText());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  Text* text = Text::Create(GetDocument(), "mid");
  span->appendChild(text);
  // EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());

  ForceLayout();
  EXPECT_EQ(String("before mid after"), GetText());
}

TEST_F(NGInlineNodeTest, RemoveInlineNodeDataIfBlockBecomesEmpty1) {
  SetupHtml("container", "<div id=container><b id=remove><i>foo</i></b></div>");
  ASSERT_TRUE(layout_block_flow_->HasNGInlineNodeData());

  Element* to_remove = GetElementById("remove");
  to_remove->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(layout_block_flow_->HasNGInlineNodeData());
}

TEST_F(NGInlineNodeTest, RemoveInlineNodeDataIfBlockBecomesEmpty2) {
  SetupHtml("container", "<div id=container><b><i>foo</i></b></div>");
  ASSERT_TRUE(layout_block_flow_->HasNGInlineNodeData());

  GetElementById("container")->setInnerHTML("");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(layout_block_flow_->HasNGInlineNodeData());
}

TEST_F(NGInlineNodeTest, RemoveInlineNodeDataIfBlockObtainsBlockChild) {
  SetupHtml("container",
            "<div id=container><b id=blockify><i>foo</i></b></div>");
  ASSERT_TRUE(layout_block_flow_->HasNGInlineNodeData());

  GetElementById("blockify")
      ->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kBlock);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(layout_block_flow_->HasNGInlineNodeData());
}

// Test inline objects are initialized when |SplitFlow()| moves them.
TEST_F(NGInlineNodeTest, ClearFirstInlineFragmentOnSplitFlow) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      <span id=outer_span>
        <span id=inner_span>1234</span>
      </span>
    </div>
  )HTML");

  // Keep the text fragment to compare later.
  Element* inner_span = GetElementById("inner_span");
  Node* text = inner_span->firstChild();
  NGInlineCursor before_split;
  before_split.MoveTo(*text->GetLayoutObject());
  EXPECT_TRUE(before_split);

  // Append <div> to <span>. causing SplitFlow().
  Element* outer_span = GetElementById("outer_span");
  Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
  outer_span->appendChild(div);

  // Update tree but do NOT update layout. At this point, there's no guarantee,
  // but there are some clients (e.g., Scroll Anchor) who try to read
  // associated fragments.
  //
  // NGPaintFragment is owned by LayoutNGBlockFlow. Because the original owner
  // no longer has an inline formatting context, the NGPaintFragment subtree is
  // destroyed, and should not be accessible.
  GetDocument().UpdateStyleAndLayoutTree();
  const LayoutObject* layout_text = text->GetLayoutObject();
  EXPECT_TRUE(layout_text->IsInLayoutNGInlineFormattingContext());
  EXPECT_TRUE(layout_text->HasInlineFragments());

  // Update layout. There should be a different instance of the text fragment.
  UpdateAllLifecyclePhasesForTest();
  NGInlineCursor after_layout;
  after_layout.MoveTo(*text->GetLayoutObject());
  EXPECT_TRUE(after_layout);

  // Check it is the one owned by the new root inline formatting context.
  LayoutBlock* inner_span_cb = inner_span->GetLayoutObject()->ContainingBlock();
  LayoutObject* container = GetLayoutObjectByElementId("container");
  EXPECT_EQ(inner_span_cb, container);
  NGInlineCursor inner_span_cb_cursor(*To<LayoutBlockFlow>(inner_span_cb));
  inner_span_cb_cursor.MoveToFirstLine();
  inner_span_cb_cursor.MoveToFirstChild();
  EXPECT_TRUE(inner_span_cb_cursor);
  EXPECT_EQ(inner_span_cb_cursor.Current().GetLayoutObject(),
            outer_span->GetLayoutObject());
}

TEST_F(NGInlineNodeTest, AddChildToSVGRoot) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      text
      <svg id="svg"></svg>
    </div>
  )HTML");

  Element* svg = GetElementById("svg");
  svg->appendChild(GetDocument().CreateRawElement(svg_names::kTextTag));
  GetDocument().UpdateStyleAndLayoutTree();

  LayoutObject* container = GetLayoutObjectByElementId("container");
  EXPECT_FALSE(container->NeedsCollectInlines());
}

// https://crbug.com/911220
TEST_F(NGInlineNodeTest, PreservedNewlineWithBidiAndRelayout) {
  SetupHtml("container",
            "<style>span{unicode-bidi:isolate}</style>"
            "<pre id=container>foo<span>\n</span>bar<br></pre>");
  EXPECT_EQ(String(u"foo\u2066\u2069\n\u2066\u2069bar\n"), GetText());

  Node* new_text = Text::Create(GetDocument(), "baz");
  GetElementById("container")->appendChild(new_text);
  UpdateAllLifecyclePhasesForTest();

  // The bidi context popping and re-entering should be preserved around '\n'.
  EXPECT_EQ(String(u"foo\u2066\u2069\n\u2066\u2069bar\nbaz"), GetText());
}

TEST_F(NGInlineNodeTest, PreservedNewlineWithRemovedBidiAndRelayout) {
  SetupHtml("container",
            "<pre id=container>foo<span dir=rtl>\nbar</span></pre>");
  EXPECT_EQ(String(u"foo\u2067\u2069\n\u2067bar\u2069"), GetText());

  GetDocument().QuerySelector("span")->removeAttribute(html_names::kDirAttr);
  UpdateAllLifecyclePhasesForTest();

  // The bidi control characters around '\n' should not preserve
  EXPECT_EQ("foo\nbar", GetText());
}

TEST_F(NGInlineNodeTest, PreservedNewlineWithRemovedLtrDirAndRelayout) {
  SetupHtml("container",
            "<pre id=container>foo<span dir=ltr>\nbar</span></pre>");
  EXPECT_EQ(String(u"foo\u2066\u2069\n\u2066bar\u2069"), GetText());

  GetDocument().QuerySelector("span")->removeAttribute(html_names::kDirAttr);
  UpdateAllLifecyclePhasesForTest();

  // The bidi control characters around '\n' should not preserve
  EXPECT_EQ("foo\nbar", GetText());
}

// https://crbug.com/969089
TEST_F(NGInlineNodeTest, InsertedWBRWithLineBreakInRelayout) {
  SetupHtml("container", "<div id=container><span>foo</span>\nbar</div>");
  EXPECT_EQ("foo bar", GetText());

  Element* div = GetElementById("container");
  Element* wbr = GetDocument().CreateElementForBinding("wbr");
  div->insertBefore(wbr, div->lastChild());
  UpdateAllLifecyclePhasesForTest();

  // The '\n' should be collapsed by the inserted <wbr>
  EXPECT_EQ(String(u"foo\u200Bbar"), GetText());
}

TEST_F(NGInlineNodeTest, CollapsibleSpaceFollowingBRWithNoWrapStyle) {
  SetupHtml("t", "<div id=t><span style=white-space:pre><br></span> </div>");
  EXPECT_EQ("\n", GetText());

  GetDocument().QuerySelector("span")->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("\n", GetText());
}

TEST_F(NGInlineNodeTest, CollapsibleSpaceFollowingNewlineWithPreStyle) {
  SetupHtml("t", "<div id=t><span style=white-space:pre>\n</span> </div>");
  EXPECT_EQ("\n", GetText());

  GetDocument().QuerySelector("span")->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("", GetText());
}

#if SEGMENT_BREAK_TRANSFORMATION_FOR_EAST_ASIAN_WIDTH
// https://crbug.com/879088
TEST_F(NGInlineNodeTest, RemoveSegmentBreakFromJapaneseInRelayout) {
  SetupHtml("container",
            u"<div id=container>"
            u"<span>\u30ED\u30B0\u30A4\u30F3</span>"
            u"\n"
            u"<span>\u767B\u9332</span>"
            u"<br></div>");
  EXPECT_EQ(String(u"\u30ED\u30B0\u30A4\u30F3\u767B\u9332\n"), GetText());

  Node* new_text = Text::Create(GetDocument(), "foo");
  GetElementById("container")->appendChild(new_text);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(String(u"\u30ED\u30B0\u30A4\u30F3\u767B\u9332\nfoo"), GetText());
}

// https://crbug.com/879088
TEST_F(NGInlineNodeTest, RemoveSegmentBreakFromJapaneseInRelayout2) {
  SetupHtml("container",
            u"<div id=container>"
            u"<span>\u30ED\u30B0\u30A4\u30F3</span>"
            u"\n"
            u"<span> \u767B\u9332</span>"
            u"<br></div>");
  EXPECT_EQ(String(u"\u30ED\u30B0\u30A4\u30F3\u767B\u9332\n"), GetText());

  Node* new_text = Text::Create(GetDocument(), "foo");
  GetElementById("container")->appendChild(new_text);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(String(u"\u30ED\u30B0\u30A4\u30F3\u767B\u9332\nfoo"), GetText());
}
#endif

TEST_F(NGInlineNodeTest, SegmentRanges) {
  SetupHtml("container",
            "<div id=container>"
            u"\u306Forange\u304C"
            "<span>text</span>"
            "</div>");

  NGInlineItemsData* items_data = layout_block_flow_->GetNGInlineNodeData();
  ASSERT_TRUE(items_data);
  NGInlineItemSegments* segments = items_data->segments.get();
  ASSERT_TRUE(segments);

  // Test EndOffset for the full text. All segment boundaries including the end
  // of the text content should be returned.
  Vector<unsigned> expect_0_12 = {1u, 7u, 8u, 12u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(0, 12, 0)), expect_0_12);

  // Test ranges for each segment that start with 1st item.
  Vector<unsigned> expect_0_1 = {1u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(0, 1, 0)), expect_0_1);
  Vector<unsigned> expect_2_3 = {3u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(2, 3, 0)), expect_2_3);
  Vector<unsigned> expect_7_8 = {8u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(7, 8, 0)), expect_7_8);

  // Test ranges that acrosses multiple segments.
  Vector<unsigned> expect_0_8 = {1u, 7u, 8u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(0, 8, 0)), expect_0_8);
  Vector<unsigned> expect_2_8 = {7u, 8u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(2, 8, 0)), expect_2_8);
  Vector<unsigned> expect_2_10 = {7u, 8u, 10u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(2, 10, 0)), expect_2_10);
  Vector<unsigned> expect_7_10 = {8u, 10u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(7, 10, 0)), expect_7_10);

  // Test ranges that starts with 2nd item.
  Vector<unsigned> expect_8_9 = {9u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(8, 9, 1)), expect_8_9);
  Vector<unsigned> expect_8_10 = {10u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(8, 10, 1)), expect_8_10);
  Vector<unsigned> expect_9_12 = {12u};
  EXPECT_EQ(ToEndOffsetList(segments->Ranges(9, 12, 1)), expect_9_12);
}

// https://crbug.com/1275383
TEST_F(NGInlineNodeTest, ReusingWithPreservedCase1) {
  SetupHtml("container",
            "<div id=container>"
            "a"
            "<br id='remove'>"
            "<span style='white-space: pre-wrap'> ijkl </span>"
            "</div>");
  EXPECT_EQ(String(u"a\n \u200Bijkl "), GetText());
  GetElementById("remove")->remove();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(String(u"a ijkl "), GetText());
}

// https://crbug.com/1275383
TEST_F(NGInlineNodeTest, ReusingWithPreservedCase2) {
  SetupHtml("container",
            "<div id=container style='white-space: pre-wrap'>"
            "a "
            "<br id='remove'>"
            "<span> ijkl </span>"
            "</div>");
  EXPECT_EQ(String(u"a \n \u200Bijkl "), GetText());
  GetElementById("remove")->remove();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(String(u"a  ijkl "), GetText());
}

// https://crbug.com/1275383
TEST_F(NGInlineNodeTest, ReusingWithPreservedCase3) {
  SetupHtml("container",
            "<div id=container style='white-space: pre-wrap'>"
            " "
            "<br id='remove'>"
            "<span> ijkl </span>"
            "</div>");
  EXPECT_EQ(String(u" \u200B\n \u200Bijkl "), GetText());
  GetElementById("remove")->remove();
  UpdateAllLifecyclePhasesForTest();
  // TODO(jfernandez): This should be "  \u200Bijkl ", but there is clearly a
  // bug that causes the first control item to be preserved, while the second is
  // ignored (due to the presence of the previous control break).
  // https://crbug.com/1276358
  EXPECT_EQ(String(u" \u200B ijkl "), GetText());
}

// https://crbug.com/1021677
TEST_F(NGInlineNodeTest, ReusingWithCollapsed) {
  SetupHtml("container",
            "<div id=container>"
            "abc "
            "<img style='float:right'>"
            "<br id='remove'>"
            "<b style='white-space:pre'>x</b>"
            "</div>");
  GetElementById("remove")->remove();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(String(u"abc \uFFFCx"), GetText());
}

// https://crbug.com/109654
TEST_F(NGInlineNodeTest, ReusingRTLAsLTR) {
  SetupHtml("container",
            "<div id=container>"
            "<span id='text' dir=rtl>"
            "[Text]text"
            "</span>"
            "</div>");
  EXPECT_EQ(String(u"\u2067[Text]text\u2069"), GetText());
  EXPECT_EQ(Items().size(), 8u);
  TEST_ITEM_OFFSET_DIR(Items()[0], 0u, 1u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(Items()[1], 1u, 1u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(Items()[2], 1u, 2u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(Items()[3], 2u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(Items()[4], 6u, 7u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(Items()[5], 7u, 11u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(Items()[6], 11u, 11u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(Items()[7], 11u, 12u, TextDirection::kLtr);
  GetElementById("text")->removeAttribute(html_names::kDirAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(String("[Text]text"), GetText());
  EXPECT_EQ(Items().size(), 3u);
  TEST_ITEM_OFFSET_DIR(Items()[0], 0u, 0u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(Items()[1], 0u, 10u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(Items()[2], 10u, 10u, TextDirection::kLtr);
}

TEST_F(NGInlineNodeTest, ReuseFirstNonSafe) {
  SetBodyInnerHTML(R"HTML(
    <style>
    p {
      font-size: 50px;
    }
    </style>
    <p id="p">
      <span>A</span>V
    </p>
  )HTML");
  auto* block_flow = To<LayoutNGBlockFlow>(GetLayoutObjectByElementId("p"));
  const NGInlineNodeData* data = block_flow->GetNGInlineNodeData();
  ASSERT_TRUE(data);
  const auto& items = data->items;

  // We shape "AV" together, which usually has kerning between "A" and "V", then
  // split the |ShapeResult| to two |NGInlineItem|s. The |NGInlineItem| for "V"
  // is not safe to reuse even if its style does not change.
  const NGInlineItem& item_v = items[3];
  EXPECT_EQ(item_v.Type(), NGInlineItem::kText);
  EXPECT_EQ(
      StringView(data->text_content, item_v.StartOffset(), item_v.Length()),
      "V");
  EXPECT_TRUE(NGInlineNode::NeedsShapingForTesting(item_v));
}

TEST_F(NGInlineNodeTest, ReuseFirstNonSafeRtl) {
  SetBodyInnerHTML(R"HTML(
    <style>
    p {
      font-size: 50px;
      unicode-bidi: bidi-override;
      direction: rtl;
    }
    </style>
    <p id="p">
      <span>A</span>V
    </p>
  )HTML");
  auto* block_flow = To<LayoutNGBlockFlow>(GetLayoutObjectByElementId("p"));
  const NGInlineNodeData* data = block_flow->GetNGInlineNodeData();
  ASSERT_TRUE(data);
  const auto& items = data->items;
  const NGInlineItem& item_v = items[4];
  EXPECT_EQ(item_v.Type(), NGInlineItem::kText);
  EXPECT_EQ(
      StringView(data->text_content, item_v.StartOffset(), item_v.Length()),
      "V");
  EXPECT_TRUE(NGInlineNode::NeedsShapingForTesting(item_v));
}

// http://crbug.com/1409702
TEST_F(NGInlineNodeTest, ShouldNotResueLigature) {
  LoadGoogleSans();
  InsertStyleElement("#sample { font-family: 'Google Sans'; }");
  SetBodyContent("<div id=sample>abf<span>i</span></div>");
  Element& sample = *GetElementById("sample");

  // `shape_result_before` has a ligature "fi".
  const LayoutText& layout_text =
      *To<Text>(sample.firstChild())->GetLayoutObject();
  const ShapeResult& shape_result_before =
      *layout_text.InlineItems().begin()->TextShapeResult();
  ASSERT_EQ(3u, shape_result_before.NumGlyphs());

  const LayoutText& layout_text_i =
      *To<Text>(sample.lastChild()->firstChild())->GetLayoutObject();
  const ShapeResult& shape_result_i =
      *layout_text_i.InlineItems().begin()->TextShapeResult();
  ASSERT_EQ(0u, shape_result_i.NumGlyphs());

  // To <div id=sample>abf</div>
  sample.lastChild()->remove();
  UpdateAllLifecyclePhasesForTest();

  const ShapeResult& shape_result_after =
      *layout_text.InlineItems().begin()->TextShapeResult();
  EXPECT_NE(&shape_result_before, &shape_result_after);
}

TEST_F(NGInlineNodeTest, InitialLetter) {
  ScopedCSSInitialLetterForTest enable_initial_letter_scope(true);
  LoadAhem();
  InsertStyleElement(
      "p { font: 20px/24px Ahem; }"
      "p::first-letter { initial-letter: 3 }");
  SetBodyContent("<p id=sample>This paragraph has an initial letter.</p>");
  auto& sample = *GetElementById("sample");
  auto& block_flow = *To<LayoutBlockFlow>(sample.GetLayoutObject());
  auto& initial_letter_box = *To<LayoutBlockFlow>(
      sample.GetPseudoElement(kPseudoIdFirstLetter)->GetLayoutObject());

  EXPECT_TRUE(NGInlineNode(&block_flow).HasInitialLetterBox());
  EXPECT_TRUE(NGBlockNode(&initial_letter_box).IsInitialLetterBox());
  EXPECT_TRUE(NGInlineNode(&initial_letter_box).IsInitialLetterBox());
  EXPECT_TRUE(initial_letter_box.GetPhysicalFragment(0)->IsInitialLetterBox());

  const NGInlineNodeData& data = *block_flow.GetNGInlineNodeData();
  const NGInlineItem& initial_letter_item = data.items[0];
  EXPECT_EQ(NGInlineItem::kInitialLetterBox, initial_letter_item.Type());
}

TEST_F(NGInlineNodeTest, TextCombineUsesScalingX) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "  font: 10px/20px Ahem;"
      "  text-combine-upright: all;"
      "  writing-mode: vertical-rl;"
      "}");
  SetBodyInnerHTML("<div id=t1>0123456789</div><div id=t2>0</div>");

  EXPECT_TRUE(To<LayoutNGTextCombine>(
                  GetLayoutObjectByElementId("t1")->SlowFirstChild())
                  ->UsesScaleX())
      << "We paint combined text '0123456789' with scaling in X-axis.";
  EXPECT_FALSE(To<LayoutNGTextCombine>(
                   GetLayoutObjectByElementId("t2")->SlowFirstChild())
                   ->UsesScaleX())
      << "We paint combined text '0' without scaling in X-axis.";
}

// http://crbug.com/1226930
TEST_F(NGInlineNodeTest, TextCombineWordSpacing) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "  font: 10px/20px Ahem;"
      "  letter-spacing: 1px;"
      "  text-combine-upright: all;"
      "  word-spacing: 1px;"
      "  writing-mode: vertical-rl;"
      "}");
  SetBodyInnerHTML("<div id=t1>ab</div>");
  const auto& text =
      *To<Text>(GetElementById("t1")->firstChild())->GetLayoutObject();
  const auto& font_description = text.StyleRef().GetFont().GetFontDescription();

  EXPECT_EQ(0, font_description.LetterSpacing());
  EXPECT_EQ(0, font_description.WordSpacing());
}

// crbug.com/1034464 bad.svg
TEST_F(NGInlineNodeTest, FindSvgTextChunksCrash1) {
  SetBodyInnerHTML(
      "<svg><text id='text' xml:space='preserve'>"
      "<tspan unicode-bidi='embed' x='0'>(</tspan>"
      "<tspan y='-2' unicode-bidi='embed' x='3'>)</tspan>"
      "<tspan y='-2' x='6'>&#x05d2;</tspan>"
      "<tspan y='-2' unicode-bidi='embed' x='10'>(</tspan>"
      "</text></svg>");

  auto* block_flow = To<LayoutNGSVGText>(GetLayoutObjectByElementId("text"));
  const NGInlineNodeData* data = block_flow->GetNGInlineNodeData();
  EXPECT_TRUE(data);
  // Pass if no null pointer dereferences.
}

// crbug.com/1034464 good.svg
TEST_F(NGInlineNodeTest, FindSvgTextChunksCrash2) {
  SetBodyInnerHTML(
      "<svg><text id='text' xml:space='preserve'>\n"
      "<tspan unicode-bidi='embed' x='0'>(</tspan>\n"
      "<tspan y='-2' unicode-bidi='embed' x='3'>)</tspan>\n"
      "<tspan y='-2' x='6'>&#x05d2;</tspan>\n"
      "<tspan y='-2' unicode-bidi='embed' x='10'>(</tspan>\n"
      "</text></svg>");

  auto* block_flow = To<LayoutNGSVGText>(GetLayoutObjectByElementId("text"));
  const NGInlineNodeData* data = block_flow->GetNGInlineNodeData();
  EXPECT_TRUE(data);
  // Pass if no DCHECK() failures.
}

// crbug.com/1403838
TEST_F(NGInlineNodeTest, FindSvgTextChunksCrash3) {
  SetBodyInnerHTML(R"SVG(
      <svg><text id='text'>
      <tspan x='0' id='target'>PA</tspan>
      <tspan x='0' y='24'>PASS</tspan>
      </text></svg>)SVG");
  auto* tspan = GetElementById("target");
  // A trail surrogate, then a lead surrogate.
  constexpr UChar kText[2] = {0xDE48, 0xD864};
  tspan->appendChild(GetDocument().createTextNode(String(kText, 2u)));
  tspan->appendChild(GetDocument().createTextNode(String(kText, 2u)));
  tspan->appendChild(GetDocument().createTextNode(String(kText, 2u)));
  tspan->appendChild(GetDocument().createTextNode(String(kText, 2u)));
  tspan->appendChild(GetDocument().createTextNode(String(kText, 2u)));
  tspan->appendChild(GetDocument().createTextNode(String(kText, 2u)));
  UpdateAllLifecyclePhasesForTest();
  // Pass if no CHECK() failures in FindSvgTextChunks().
}

}  // namespace blink
