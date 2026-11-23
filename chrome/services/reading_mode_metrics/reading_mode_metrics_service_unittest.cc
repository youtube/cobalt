// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/reading_mode_metrics/reading_mode_metrics_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "chrome/services/reading_mode_metrics/reading_mode_metrics.rs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"

namespace reading_mode {

class ReadingModeMetricsServiceTest : public testing::Test {
 protected:
  ui::AXTreeUpdate CreateMockAXTreeUpdate() {
    // AXTree Representation:
    // kRootWebArea (id: 1)
    //   ├── kHeading (id: 2)
    //   │     └── kStaticText (id: 8): "My Article Title"
    //   ├── kList (id: 3)
    //   │     └── kListItem (id: 4)
    //   │           └── kStaticText (id: 9): "Shopping item"
    //   └── kParagraph (id: 5)
    //         ├── kStaticText (id: 6): "This is bold content" [Bold]
    //         └── kStaticText (id: 7): "This is italic content" [Italic]

    ui::AXTreeUpdate update;

    // 1. Root Web Area
    ui::AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;

    // 2. Heading (H1)
    ui::AXNodeData heading;
    heading.id = 2;
    heading.role = ax::mojom::Role::kHeading;

    ui::AXNodeData heading_text;
    heading_text.id = 8;
    heading_text.role = ax::mojom::Role::kStaticText;
    heading_text.SetName("My Article Title");

    // 3. List block
    ui::AXNodeData list;
    list.id = 3;
    list.role = ax::mojom::Role::kList;

    // 4. List item
    ui::AXNodeData list_item;
    list_item.id = 4;
    list_item.role = ax::mojom::Role::kListItem;

    ui::AXNodeData list_item_text;
    list_item_text.id = 9;
    list_item_text.role = ax::mojom::Role::kStaticText;
    list_item_text.SetName("Shopping item");

    // 5. Paragraph block containing formatted static text leaves
    ui::AXNodeData paragraph;
    paragraph.id = 5;
    paragraph.role = ax::mojom::Role::kParagraph;

    // 6. Bold leaf node
    ui::AXNodeData bold_leaf;
    bold_leaf.id = 6;
    bold_leaf.role = ax::mojom::Role::kStaticText;
    bold_leaf.SetName("This is bold content");
    bold_leaf.AddTextStyle(ax::mojom::TextStyle::kBold);

    // 7. Italic leaf node
    ui::AXNodeData italic_leaf;
    italic_leaf.id = 7;
    italic_leaf.role = ax::mojom::Role::kStaticText;
    italic_leaf.SetName("This is italic content");
    italic_leaf.AddTextStyle(ax::mojom::TextStyle::kItalic);

    // Setup parent-child links
    root.child_ids = {2, 3, 5};
    heading.child_ids = {8};
    list.child_ids = {4};
    list_item.child_ids = {9};
    paragraph.child_ids = {6, 7};

    update.nodes = {root,      heading,   heading_text,
                    list,      list_item, list_item_text,
                    paragraph, bold_leaf, italic_leaf};
    update.root_id = 1;
    return update;
  }

  ui::AXTreeUpdate CreateComplexMockAXTreeUpdate() {
    // AXTree Representation:
    // kRootWebArea (id: 1)
    //   ├── kHeading (id: 2)
    //   │     └── kStaticText (id: 11): "Rust Programming Language "
    //   ├── kParagraph (id: 3)
    //   │     ├── kStaticText (id: 12): "By "
    //   │     └── kStaticText (id: 13): "Graydon Hoare " [Bold]
    //   ├── kHeading (id: 5)
    //   │     └── kStaticText (id: 14): "Core Architecture "
    //   ├── kParagraph (id: 6)
    //   │     ├── kStaticText (id: 15): "Some code: "
    //   │     ├── kCode (id: 19)
    //   │     │     └── kStaticText (id: 16): "fn main() "
    //   │     └── kStaticText (id: 17): ". Rust achieves memory safety without
    //   a GC. " ├── kList (id: 8) │     ├── kListItem (id: 20) │     │     └──
    //   kStaticText (id: 22): "Memory safety via ownership " │     └──
    //   kListItem (id: 21) │           └── kStaticText (id: 23): "Concurrency
    //   without data races " ├── kBlockquote (id: 9) │     └── kStaticText (id:
    //   24): "Safety is not just about preventing crashes. " └── kLink (id: 10)
    //         └── kStaticText (id: 25): "Read the official guide "

    ui::AXTreeUpdate update;

    // 1. Root Web Area
    ui::AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;

    // 2. Heading (H1)
    ui::AXNodeData h1;
    h1.id = 2;
    h1.role = ax::mojom::Role::kHeading;

    ui::AXNodeData h1_text;
    h1_text.id = 11;
    h1_text.role = ax::mojom::Role::kStaticText;
    h1_text.SetName("Rust Programming Language ");

    // 3. Paragraph author
    ui::AXNodeData author_p;
    author_p.id = 3;
    author_p.role = ax::mojom::Role::kParagraph;

    ui::AXNodeData author_text1;
    author_text1.id = 12;
    author_text1.role = ax::mojom::Role::kStaticText;
    author_text1.SetName("By ");

    ui::AXNodeData author_text2;
    author_text2.id = 13;
    author_text2.role = ax::mojom::Role::kStaticText;
    author_text2.SetName("Graydon Hoare ");
    author_text2.AddTextStyle(ax::mojom::TextStyle::kBold);

    // 4. Section Heading (H2)
    ui::AXNodeData h2;
    h2.id = 5;
    h2.role = ax::mojom::Role::kHeading;

    ui::AXNodeData h2_text;
    h2_text.id = 14;
    h2_text.role = ax::mojom::Role::kStaticText;
    h2_text.SetName("Core Architecture ");

    // 5. Paragraph body
    ui::AXNodeData body_p;
    body_p.id = 6;
    body_p.role = ax::mojom::Role::kParagraph;

    ui::AXNodeData body_text1;
    body_text1.id = 15;
    body_text1.role = ax::mojom::Role::kStaticText;
    body_text1.SetName("Some code: ");

    ui::AXNodeData code_wrapper;
    code_wrapper.id = 19;
    code_wrapper.role = ax::mojom::Role::kCode;

    ui::AXNodeData code_text;
    code_text.id = 16;
    code_text.role = ax::mojom::Role::kStaticText;
    code_text.SetName("fn main() ");

    ui::AXNodeData body_text2;
    body_text2.id = 17;
    body_text2.role = ax::mojom::Role::kStaticText;
    body_text2.SetName(". Rust achieves memory safety without a GC. ");

    // 6. Feature List
    ui::AXNodeData feature_list;
    feature_list.id = 8;
    feature_list.role = ax::mojom::Role::kList;

    ui::AXNodeData li1;
    li1.id = 20;
    li1.role = ax::mojom::Role::kListItem;

    ui::AXNodeData li1_text;
    li1_text.id = 22;
    li1_text.role = ax::mojom::Role::kStaticText;
    li1_text.SetName("Memory safety via ownership ");

    ui::AXNodeData li2;
    li2.id = 21;
    li2.role = ax::mojom::Role::kListItem;

    ui::AXNodeData li2_text;
    li2_text.id = 23;
    li2_text.role = ax::mojom::Role::kStaticText;
    li2_text.SetName("Concurrency without data races ");

    // 7. Quote
    ui::AXNodeData blockquote;
    blockquote.id = 9;
    blockquote.role = ax::mojom::Role::kBlockquote;

    ui::AXNodeData blockquote_text;
    blockquote_text.id = 24;
    blockquote_text.role = ax::mojom::Role::kStaticText;
    blockquote_text.SetName("Safety is not just about preventing crashes. ");

    // 8. Link
    ui::AXNodeData link;
    link.id = 10;
    link.role = ax::mojom::Role::kLink;

    ui::AXNodeData link_text;
    link_text.id = 25;
    link_text.role = ax::mojom::Role::kStaticText;
    link_text.SetName("Read the official guide ");

    // Set parent-child connections
    root.child_ids = {2, 3, 5, 6, 8, 9, 10};
    h1.child_ids = {11};
    author_p.child_ids = {12, 13};
    h2.child_ids = {14};
    body_p.child_ids = {15, 19, 17};
    code_wrapper.child_ids = {16};
    feature_list.child_ids = {20, 21};
    li1.child_ids = {22};
    li2.child_ids = {23};
    blockquote.child_ids = {24};
    link.child_ids = {25};

    update.nodes = {root,         h1,           h1_text,      author_p,
                    author_text1, author_text2, h2,           h2_text,
                    body_p,       body_text1,   code_wrapper, code_text,
                    body_text2,   feature_list, li1,          li1_text,
                    li2,          li2_text,     blockquote,   blockquote_text,
                    link,         link_text};
    update.root_id = 1;
    return update;
  }

  ui::AXTreeUpdate CreateHeavyMockAXTreeUpdate() {
    // AXTree Representation:
    // kRootWebArea (id: 1)
    //   ├── kHeading (id: 2)
    //   │     └── kStaticText (id: 10): "Advanced Rust Concurrency "
    //   ├── kParagraph (id: 3)
    //   │     ├── kStaticText (id: 11): "In this article, we discuss "
    //   │     ├── kStaticText (id: 12): "data races " [Bold]
    //   │     ├── kStaticText (id: 13): "and how to avoid them. Check the "
    //   │     ├── kLink (id: 14)
    //   │     │     └── kStaticText (id: 16): "Concurrency Guide "
    //   │     └── kStaticText (id: 15): "for resources. "
    //   ├── kHeading (id: 4)
    //   │     └── kStaticText (id: 17): "Common Synchronization Primitives "
    //   ├── kList (id: 5)
    //   │     ├── kListItem (id: 18)
    //   │     │     └── kStaticText (id: 20): "Mutex for mutually exclusive
    //   access " │     └── kListItem (id: 19) │           └── kStaticText (id:
    //   21): "RwLock for multiple readers or single writer " ├── kHeading (id:
    //   6) │     └── kStaticText (id: 22): "Channels " ├── kList (id: 7) │ ├──
    //   kListItem (id: 23) │     │     └── kStaticText (id: 26): "mpsc channels
    //   for multiple producers " │     ├── kListItem (id: 24) │     │     └──
    //   kStaticText (id: 27): "oneshot channels for single values " │     └──
    //   kListItem (id: 25) │           └── kStaticText (id: 28): "thread spawn
    //   for thread spawning " └── kBlockquote (id: 8)
    //         └── kStaticText (id: 29): "Do not communicate by sharing memory;
    //         instead, share memory by communicating. "

    ui::AXTreeUpdate update;

    // 1. Root Web Area
    ui::AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;

    // 2. Heading (H1)
    ui::AXNodeData h1;
    h1.id = 2;
    h1.role = ax::mojom::Role::kHeading;

    ui::AXNodeData h1_text;
    h1_text.id = 10;
    h1_text.role = ax::mojom::Role::kStaticText;
    h1_text.SetName("Advanced Rust Concurrency ");

    // 3. Paragraph 1 (text, bold text, text, link, text)
    ui::AXNodeData p1;
    p1.id = 3;
    p1.role = ax::mojom::Role::kParagraph;

    ui::AXNodeData p1_text1;
    p1_text1.id = 11;
    p1_text1.role = ax::mojom::Role::kStaticText;
    p1_text1.SetName("In this article, we discuss ");

    ui::AXNodeData p1_text2;
    p1_text2.id = 12;
    p1_text2.role = ax::mojom::Role::kStaticText;
    p1_text2.SetName("data races ");
    p1_text2.AddTextStyle(ax::mojom::TextStyle::kBold);

    ui::AXNodeData p1_text3;
    p1_text3.id = 13;
    p1_text3.role = ax::mojom::Role::kStaticText;
    p1_text3.SetName("and how to avoid them. Check the ");

    ui::AXNodeData link;
    link.id = 14;
    link.role = ax::mojom::Role::kLink;

    ui::AXNodeData link_text;
    link_text.id = 16;
    link_text.role = ax::mojom::Role::kStaticText;
    link_text.SetName("Concurrency Guide ");

    ui::AXNodeData p1_text4;
    p1_text4.id = 15;
    p1_text4.role = ax::mojom::Role::kStaticText;
    p1_text4.SetName("for resources. ");

    // 4. Heading H2
    ui::AXNodeData h2_1;
    h2_1.id = 4;
    h2_1.role = ax::mojom::Role::kHeading;

    ui::AXNodeData h2_1_text;
    h2_1_text.id = 17;
    h2_1_text.role = ax::mojom::Role::kStaticText;
    h2_1_text.SetName("Common Synchronization Primitives ");

    // 5. List 1
    ui::AXNodeData list1;
    list1.id = 5;
    list1.role = ax::mojom::Role::kList;

    ui::AXNodeData li1_1;
    li1_1.id = 18;
    li1_1.role = ax::mojom::Role::kListItem;

    ui::AXNodeData li1_1_text;
    li1_1_text.id = 20;
    li1_1_text.role = ax::mojom::Role::kStaticText;
    li1_1_text.SetName("Mutex for mutually exclusive access ");

    ui::AXNodeData li1_2;
    li1_2.id = 19;
    li1_2.role = ax::mojom::Role::kListItem;

    ui::AXNodeData li1_2_text;
    li1_2_text.id = 21;
    li1_2_text.role = ax::mojom::Role::kStaticText;
    li1_2_text.SetName("RwLock for multiple readers or single writer ");

    // 6. Heading H2 (Channels)
    ui::AXNodeData h2_2;
    h2_2.id = 6;
    h2_2.role = ax::mojom::Role::kHeading;

    ui::AXNodeData h2_2_text;
    h2_2_text.id = 22;
    h2_2_text.role = ax::mojom::Role::kStaticText;
    h2_2_text.SetName("Channels ");

    // 7. List 2
    ui::AXNodeData list2;
    list2.id = 7;
    list2.role = ax::mojom::Role::kList;

    ui::AXNodeData li2_1;
    li2_1.id = 23;
    li2_1.role = ax::mojom::Role::kListItem;

    ui::AXNodeData li2_1_text;
    li2_1_text.id = 26;
    li2_1_text.role = ax::mojom::Role::kStaticText;
    li2_1_text.SetName("mpsc channels for multiple producers ");

    ui::AXNodeData li2_2;
    li2_2.id = 24;
    li2_2.role = ax::mojom::Role::kListItem;

    ui::AXNodeData li2_2_text;
    li2_2_text.id = 27;
    li2_2_text.role = ax::mojom::Role::kStaticText;
    li2_2_text.SetName("oneshot channels for single values ");

    ui::AXNodeData li2_3;
    li2_3.id = 25;
    li2_3.role = ax::mojom::Role::kListItem;

    ui::AXNodeData li2_3_text;
    li2_3_text.id = 28;
    li2_3_text.role = ax::mojom::Role::kStaticText;
    li2_3_text.SetName("thread spawn for thread spawning ");

    // 8. Blockquote
    ui::AXNodeData blockquote;
    blockquote.id = 8;
    blockquote.role = ax::mojom::Role::kBlockquote;

    ui::AXNodeData blockquote_text;
    blockquote_text.id = 29;
    blockquote_text.role = ax::mojom::Role::kStaticText;
    blockquote_text.SetName(
        "Do not communicate by sharing memory; instead, share memory by "
        "communicating. ");

    // Set parent-child paths
    root.child_ids = {2, 3, 4, 5, 6, 7, 8};
    h1.child_ids = {10};
    p1.child_ids = {11, 12, 13, 14, 15};
    link.child_ids = {16};
    h2_1.child_ids = {17};
    list1.child_ids = {18, 19};
    li1_1.child_ids = {20};
    li1_2.child_ids = {21};
    h2_2.child_ids = {22};
    list2.child_ids = {23, 24, 25};
    li2_1.child_ids = {26};
    li2_2.child_ids = {27};
    li2_3.child_ids = {28};
    blockquote.child_ids = {29};

    update.nodes = {root,       h1,         h1_text,    p1,
                    p1_text1,   p1_text2,   p1_text3,   link,
                    link_text,  p1_text4,   h2_1,       h2_1_text,
                    list1,      li1_1,      li1_1_text, li1_2,
                    li1_2_text, h2_2,       h2_2_text,  list2,
                    li2_1,      li2_1_text, li2_2,      li2_2_text,
                    li2_3,      li2_3_text, blockquote, blockquote_text};
    update.root_id = 1;
    return update;
  }
};

TEST_F(ReadingModeMetricsServiceTest, ExtractsAXTreeStructuralProperties) {
  ui::AXTree tree(CreateMockAXTreeUpdate());
  OriginalStructure structure;

  // Run structural extraction using the exposed testing wrapper.
  ExtractOriginalStructureForTesting(tree.root(), structure,
                                     /*inside_code=*/false);

  // 1. Validate Headings
  ASSERT_EQ(structure.headings.size(), 1u);
  EXPECT_EQ(std::string(structure.headings[0]), "My Article Title");

  // 2. Validate Grouped Lists
  ASSERT_EQ(structure.lists.size(), 1u);
  ASSERT_EQ(structure.lists[0].items.size(), 1u);
  EXPECT_EQ(std::string(structure.lists[0].items[0]), "Shopping item");

  // 3. Validate Styles Formatting
  ASSERT_EQ(structure.bold_fragments.size(), 1u);
  EXPECT_EQ(std::string(structure.bold_fragments[0]), "This is bold content");

  ASSERT_EQ(structure.italic_fragments.size(), 1u);
  EXPECT_EQ(std::string(structure.italic_fragments[0]),
            "This is italic content");
}

TEST_F(ReadingModeMetricsServiceTest, HandlesVoidElements) {
  auto results = reading_mode::parse_distilled_html(
      "text <br> blurred <img src=\"test.png\"> text");
  ASSERT_EQ(results.size(), 3u);

  EXPECT_EQ(std::string(results[0].text), "text");
  EXPECT_TRUE(results[0].tag_path.empty());

  EXPECT_EQ(std::string(results[1].text), "blurred");
  EXPECT_TRUE(results[1].tag_path.empty());

  EXPECT_EQ(std::string(results[2].text), "text");
  EXPECT_TRUE(results[2].tag_path.empty());
}

TEST_F(ReadingModeMetricsServiceTest, HandlesUnescapedBrackets) {
  auto results = reading_mode::parse_distilled_html("a < b");
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(std::string(results[0].text), "a < b");
  EXPECT_TRUE(results[0].tag_path.empty());
}

TEST_F(ReadingModeMetricsServiceTest, HandlesAttributesWithGreaterThan) {
  auto results = reading_mode::parse_distilled_html(
      "<img alt=\"next >\" src=\"test.png\">text");
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(std::string(results[0].text), "text");
  EXPECT_TRUE(results[0].tag_path.empty());
}

TEST_F(ReadingModeMetricsServiceTest, PreventsWordConcatenation) {
  auto results =
      reading_mode::parse_distilled_html("<h1>Hello</h1><p>World</p>");
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(std::string(results[0].text), "Hello");
  ASSERT_EQ(results[0].tag_path.size(), 1u);
  EXPECT_EQ(std::string(results[0].tag_path[0]), "H1");

  EXPECT_EQ(std::string(results[1].text), "World");
  ASSERT_EQ(results[1].tag_path.size(), 1u);
  EXPECT_EQ(std::string(results[1].tag_path[0]), "P");
}

TEST_F(ReadingModeMetricsServiceTest, DirectHtmlParserUnclosedTags) {
  auto unclosed_results =
      reading_mode::parse_distilled_html("<li>Unclosed <b>nested</b> text");
  ASSERT_EQ(unclosed_results.size(), 3u);

  EXPECT_EQ(std::string(unclosed_results[0].text), "Unclosed");
  ASSERT_EQ(unclosed_results[0].tag_path.size(), 1u);
  EXPECT_EQ(std::string(unclosed_results[0].tag_path[0]), "LI");

  EXPECT_EQ(std::string(unclosed_results[1].text), "nested");
  ASSERT_EQ(unclosed_results[1].tag_path.size(), 2u);
  EXPECT_EQ(std::string(unclosed_results[1].tag_path[0]), "LI");
  EXPECT_EQ(std::string(unclosed_results[1].tag_path[1]), "B");

  EXPECT_EQ(std::string(unclosed_results[2].text), "text");
  ASSERT_EQ(unclosed_results[2].tag_path.size(), 1u);
  EXPECT_EQ(std::string(unclosed_results[2].tag_path[0]), "LI");
}

TEST_F(ReadingModeMetricsServiceTest, DirectHtmlParserNestedStructures) {
  auto nested_results =
      reading_mode::parse_distilled_html("<div><ul><li>nested</li></ul></div>");
  ASSERT_EQ(nested_results.size(), 1u);
  EXPECT_EQ(std::string(nested_results[0].text), "nested");
  ASSERT_EQ(nested_results[0].tag_path.size(), 3u);
  EXPECT_EQ(std::string(nested_results[0].tag_path[0]), "DIV");
  EXPECT_EQ(std::string(nested_results[0].tag_path[1]), "UL");
  EXPECT_EQ(std::string(nested_results[0].tag_path[2]), "LI");
}

TEST_F(ReadingModeMetricsServiceTest, DirectHtmlParserGarbageEmptyInputs) {
  auto empty_results =
      reading_mode::parse_distilled_html("<garbage>   </garbage>");
  EXPECT_TRUE(empty_results.empty());
}

TEST_F(ReadingModeMetricsServiceTest, RestrictsHeaderTags) {
  OriginalStructure structure;
  structure.headings.push_back(rust::String("Good"));
  structure.headings.push_back(rust::String("Bad"));

  auto metrics = evaluate("Good Bad", "<h1>Good</h1><h7>Bad</h7>", structure);
  EXPECT_FLOAT_EQ(metrics.struct_score, 0.75f);
}

TEST_F(ReadingModeMetricsServiceTest,
       ComplexArticleEvaluationIdealDistillation) {
  mojo::PendingReceiver<mojom::DistillationEvaluator> receiver;
  ReadingModeMetricsService service(std::move(receiver));

  // Ideal Distillation (100% preservation)
  // We use single line layout (no leading indentation or extraneous newlines
  // outside tags) so character counts of the HTML matches static text nodes.
  std::string distilled_html =
      "<div class=\"content\"><h1>Rust Programming Language </h1>"
      "<p>By <strong>Graydon Hoare </strong></p>"
      "<h2>Core Architecture </h2>"
      "<p>Some code: <code>fn main() </code>. Rust achieves memory safety "
      "without a GC. </p>"
      "<ul><li>Memory safety via ownership </li>"
      "<li>Concurrency without data races </li></ul>"
      "<blockquote>Safety is not just about preventing crashes. </blockquote>"
      "<p><a href=\"https://rust-lang.org\">Read the official guide </a></p>"
      "</div>";

  mojom::EvaluationStatus status;
  mojom::DistillationMetricsPtr metrics;

  service.Evaluate(CreateComplexMockAXTreeUpdate(), distilled_html,
                   base::BindOnce(
                       [](mojom::EvaluationStatus* out_status,
                          mojom::DistillationMetricsPtr* out_metrics,
                          base::expected<mojom::DistillationMetricsPtr,
                                         mojom::EvaluationStatus> result) {
                         if (result.has_value()) {
                           *out_status = mojom::EvaluationStatus::kSuccess;
                           *out_metrics = std::move(result.value());
                         } else {
                           *out_status = result.error();
                         }
                       },
                       &status, &metrics));

  EXPECT_EQ(status, mojom::EvaluationStatus::kSuccess);
  ASSERT_TRUE(metrics);
  EXPECT_GT(metrics->rouge_l_f1, 0.95f);
  EXPECT_FLOAT_EQ(metrics->struct_score, 1.0f);
  EXPECT_FLOAT_EQ(metrics->format_score, 1.0f);
  EXPECT_FLOAT_EQ(metrics->link_density_ratio,
                  0.0f);  // 0% reduction since links are preserved
}

TEST_F(ReadingModeMetricsServiceTest, ComplexArticleEvaluationImperfectDistillation) {
  mojo::PendingReceiver<mojom::DistillationEvaluator> receiver;
  ReadingModeMetricsService service(std::move(receiver));

  // Imperfect Distillation (Omitted formats but text survives)
  // - Headings: H1 survives inside a <p>, H2 survives inside H2.
  //   Heading ratio = (0 + 1) / 2 = 0.5.
  // - Lists: LI1 survives in <li>, LI2 survives in <p>.
  //   List completeness = (1 + 0) / 2 = 0.5.
  //   Structural Score = (Heading ratio [0.5] + List completeness [0.5]) / 2 =
  //   0.5.
  // - Formats: Bold/Code survive in plain text. Blockquote is formatted. Italic
  // is empty.
  //   Bold score = 0, Italic score = 1.0, Code score = 0, Blockquote score
  //   = 1.0. Formatting Score = (0 + 1.0 + 0 + 1.0) / 4 = 0.5.
  // - Link: The <a> tag is stripped, text survives as plain.
  //   Link density reduction = (orig - 0) / orig = 1.0.
  std::string distilled_html =
      "<div class=\"content\"><p>Rust Programming Language </p>"
      "<p>By Graydon Hoare </p>"
      "<h2>Core Architecture </h2>"
      "<p>Some code: fn main() . Rust achieves memory safety without a GC. </p>"
      "<ul><li>Memory safety via ownership </li></ul>"
      "<p>Concurrency without data races </p>"
      "<blockquote>Safety is not just about preventing crashes. </blockquote>"
      "Read the official guide </div>";

  mojom::EvaluationStatus status;
  mojom::DistillationMetricsPtr metrics;

  service.Evaluate(CreateComplexMockAXTreeUpdate(), distilled_html,
                   base::BindOnce(
                       [](mojom::EvaluationStatus* out_status,
                          mojom::DistillationMetricsPtr* out_metrics,
                          base::expected<mojom::DistillationMetricsPtr,
                                         mojom::EvaluationStatus> result) {
                         if (result.has_value()) {
                           *out_status = mojom::EvaluationStatus::kSuccess;
                           *out_metrics = std::move(result.value());
                         } else {
                           *out_status = result.error();
                         }
                       },
                       &status, &metrics));

  EXPECT_EQ(status, mojom::EvaluationStatus::kSuccess);
  ASSERT_TRUE(metrics);
  EXPECT_GT(metrics->rouge_l_recall, 0.95f);  // text survived
  EXPECT_FLOAT_EQ(metrics->struct_score, 0.5f);
  EXPECT_FLOAT_EQ(metrics->format_score, 0.5f);
  EXPECT_FLOAT_EQ(metrics->link_density_ratio,
                  1.0f);  // 100% reduction of links
}

TEST_F(ReadingModeMetricsServiceTest, HeavyArticleEvaluationIdealDistillation) {
  mojo::PendingReceiver<mojom::DistillationEvaluator> receiver;
  ReadingModeMetricsService service(std::move(receiver));

  // Ideal Distillation (100% preservation)
  std::string distilled_html =
      "<div class=\"content\"><h1>Advanced Rust Concurrency </h1>"
      "<p>In this article, we discuss <strong>data races </strong>and how to "
      "avoid them. Check the <a href=\"https://rust.org/concurrency\">"
      "Concurrency Guide </a>for resources. </p>"
      "<h2>Common Synchronization Primitives </h2>"
      "<ul><li>Mutex for mutually exclusive access </li>"
      "<li>RwLock for multiple readers or single writer </li></ul>"
      "<h2>Channels </h2>"
      "<ul><li>mpsc channels for multiple producers </li>"
      "<li>oneshot channels for single values </li>"
      "<li>thread spawn for thread spawning </li></ul>"
      "<blockquote>Do not communicate by sharing memory; instead, "
      "share memory by communicating. </blockquote>"
      "</div>";

  mojom::EvaluationStatus status;
  mojom::DistillationMetricsPtr metrics;

  service.Evaluate(CreateHeavyMockAXTreeUpdate(), distilled_html,
                   base::BindOnce(
                       [](mojom::EvaluationStatus* out_status,
                          mojom::DistillationMetricsPtr* out_metrics,
                          base::expected<mojom::DistillationMetricsPtr,
                                         mojom::EvaluationStatus> result) {
                         if (result.has_value()) {
                           *out_status = mojom::EvaluationStatus::kSuccess;
                           *out_metrics = std::move(result.value());
                         } else {
                           *out_status = result.error();
                         }
                       },
                       &status, &metrics));

  EXPECT_EQ(status, mojom::EvaluationStatus::kSuccess);
  ASSERT_TRUE(metrics);
  EXPECT_GT(metrics->rouge_l_f1, 0.95f);
  EXPECT_FLOAT_EQ(metrics->struct_score, 1.0f);
  EXPECT_FLOAT_EQ(metrics->format_score, 1.0f);
  EXPECT_FLOAT_EQ(metrics->link_density_ratio,
                  0.0f);  // 0% reduction since links are preserved
}

TEST_F(ReadingModeMetricsServiceTest, HeavyArticleEvaluationImperfectDistillation) {
  mojo::PendingReceiver<mojom::DistillationEvaluator> receiver;
  ReadingModeMetricsService service(std::move(receiver));

  // Imperfect / Omitted Layout Distillation
  // - Headings: H1 becomes regular <p>, synchronization H2 survives in H2,
  // channels H2 becomes regular <p>.
  //   Heading ratio = (0 + 1 + 0) / 3 = 0.33333333f.
  // - Lists: List 1 survives fully. List 2 item 3 becomes a regular paragraph
  // <p>.
  //   List 1 completeness = (1.0 + 1.0) / 2 = 1.0.
  //   List 2 completeness = (1.0 + 1.0 + 0.0) / 3 = 0.66666667.
  //   Overall list completeness = (1.0 + 0.66666667) / 2 = 0.83333333.
  //   Structural score = (0.33333333 + 0.83333333) / 2 = 0.58333333.
  // - Formats: Bold/Strong survives, blockquote survives. (No italic exists).
  //   Bold score = 1.0, Italic score = 1.0, Code score = 1.0 (no code exists),
  //   Blockquote score = 1.0. Format score = 1.0.
  // - Link: Omitted (stripped).
  //   Link density ratio = 1.0.
  std::string distilled_html =
      "<div class=\"content\"><p>Advanced Rust Concurrency </p>"
      "<p>In this article, we discuss <strong>data races </strong>and how to "
      "avoid them. Check the Concurrency Guide for resources. </p>"
      "<h2>Common Synchronization Primitives </h2>"
      "<ul><li>Mutex for mutually exclusive access </li>"
      "<li>RwLock for multiple readers or single writer </li></ul>"
      "<p>Channels </p>"
      "<ul><li>mpsc channels for multiple producers </li>"
      "<li>oneshot channels for single values </li></ul>"
      "<p>thread spawn for thread spawning </p>"
      "<blockquote>Do not communicate by sharing memory; instead, "
      "share memory by communicating. </blockquote>"
      "</div>";

  mojom::EvaluationStatus status;
  mojom::DistillationMetricsPtr metrics;

  service.Evaluate(CreateHeavyMockAXTreeUpdate(), distilled_html,
                   base::BindOnce(
                       [](mojom::EvaluationStatus* out_status,
                          mojom::DistillationMetricsPtr* out_metrics,
                          base::expected<mojom::DistillationMetricsPtr,
                                         mojom::EvaluationStatus> result) {
                         if (result.has_value()) {
                           *out_status = mojom::EvaluationStatus::kSuccess;
                           *out_metrics = std::move(result.value());
                         } else {
                           *out_status = result.error();
                         }
                       },
                       &status, &metrics));

  EXPECT_EQ(status, mojom::EvaluationStatus::kSuccess);
  ASSERT_TRUE(metrics);
  EXPECT_GT(metrics->rouge_l_recall, 0.95f);
  EXPECT_NEAR(metrics->struct_score, 0.58333333f, 0.0001f);
  EXPECT_FLOAT_EQ(metrics->format_score, 1.0f);
  EXPECT_FLOAT_EQ(metrics->link_density_ratio,
                  1.0f);  // 100% reduction of links
}

TEST_F(ReadingModeMetricsServiceTest, RougeLMetricsCalculation) {
  OriginalStructure structure;

  // Original: 9 words
  std::string original = "the quick brown fox jumps over the lazy dog";
  // Distilled: 12 words
  // Matching words (LCS) = "the", "brown", "fox", "over", "the", "dog" (6
  // words, non-continuous)
  std::string distilled =
      "the swift brown fox leaps over the sleeping dog extra words here";

  auto metrics = evaluate(original.c_str(), distilled.c_str(), structure);

  // Precision = 6 / 12 = 0.5f
  // Recall = 6 / 9 = 0.66666667f
  // F1 = 2 * (0.5 * 2/3) / (0.5 + 2/3) = 0.57142857f
  // F2 = 5 * (0.5 * 2/3) / (4 * 0.5 + 2/3) = 0.625f

  EXPECT_FLOAT_EQ(metrics.rouge_l_precision, 0.5f);
  EXPECT_NEAR(metrics.rouge_l_recall, 2.0f / 3.0f, 0.0001f);
  EXPECT_NEAR(metrics.rouge_l_f1, 4.0f / 7.0f, 0.0001f);
  EXPECT_FLOAT_EQ(metrics.rouge_l_f2, 0.625f);
  // Verify F1 and F2 are different:
  EXPECT_NE(metrics.rouge_l_f1, metrics.rouge_l_f2);
}

TEST_F(ReadingModeMetricsServiceTest, LinkDensityReductionFractional) {
  OriginalStructure structure;
  // Let's set density values manually to simulate C++ extraction:
  // Original link text length = 10, total text length = 100 (density = 10%)
  structure.original_link_text_len = 10;
  structure.original_total_text_len = 100;

  // Distilled HTML contains exactly 5 characters of link, and 95 characters of
  // other text. Distilled link length = 5, total non-tag length = 100 (density
  // = 5%)
  std::string distilled_html =
      "<a>links</a>"
      "95-characters-of-text-here-should-be-exactly-95-chars"
      "..........................................";

  auto metrics = evaluate("doesnt matter", distilled_html.c_str(), structure);

  // Link density reduction = (orig - dist) / orig = (0.10 - 0.05) / 0.10 =
  // 0.50f (50% reduction)
  EXPECT_FLOAT_EQ(metrics.link_density_ratio, 0.5f);
}

TEST_F(ReadingModeMetricsServiceTest, AXTreesExtractsNestedProperties) {
  ui::AXTreeUpdate update;

  // 1. Root Web Area
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  // 2. Heading containing italic style
  ui::AXNodeData heading;
  heading.id = 2;
  heading.role = ax::mojom::Role::kHeading;

  ui::AXNodeData heading_italic_text;
  heading_italic_text.id = 3;
  heading_italic_text.role = ax::mojom::Role::kStaticText;
  heading_italic_text.SetName("Title ");
  heading_italic_text.AddTextStyle(ax::mojom::TextStyle::kItalic);

  // 3. Paragraph containing bold and code elements
  ui::AXNodeData paragraph;
  paragraph.id = 4;
  paragraph.role = ax::mojom::Role::kParagraph;

  ui::AXNodeData normal_text;
  normal_text.id = 5;
  normal_text.role = ax::mojom::Role::kStaticText;
  normal_text.SetName("Normal ");

  ui::AXNodeData bold_text;
  bold_text.id = 6;
  bold_text.role = ax::mojom::Role::kStaticText;
  bold_text.SetName("Bold ");
  bold_text.AddTextStyle(ax::mojom::TextStyle::kBold);

  ui::AXNodeData code_wrapper;
  code_wrapper.id = 7;
  code_wrapper.role = ax::mojom::Role::kCode;

  ui::AXNodeData code_text;
  code_text.id = 8;
  code_text.role = ax::mojom::Role::kStaticText;
  code_text.SetName("code");

  // Set child IDs
  root.child_ids = {2, 4};
  heading.child_ids = {3};
  paragraph.child_ids = {5, 6, 7};
  code_wrapper.child_ids = {8};

  update.nodes = {root,        heading,   heading_italic_text, paragraph,
                  normal_text, bold_text, code_wrapper,        code_text};
  update.root_id = 1;

  ui::AXTree tree(update);
  OriginalStructure structure;

  // Run structural extraction.
  ExtractOriginalStructureForTesting(tree.root(), structure,
                                     /*inside_code=*/false);

  // Validate Headings: Heading text should aggregate correctly
  ASSERT_EQ(structure.headings.size(), 1u);
  EXPECT_EQ(std::string(structure.headings[0]), "Title ");

  // Validate Bold formatting:
  ASSERT_EQ(structure.bold_fragments.size(), 1u);
  EXPECT_EQ(std::string(structure.bold_fragments[0]), "Bold ");

  // Validate Italic formatting:
  ASSERT_EQ(structure.italic_fragments.size(), 1u);
  EXPECT_EQ(std::string(structure.italic_fragments[0]), "Title ");

  // Validate Code formatting:
  ASSERT_EQ(structure.code_fragments.size(), 1u);
  EXPECT_EQ(std::string(structure.code_fragments[0]), "code");
}

TEST_F(ReadingModeMetricsServiceTest, MalformedHtmlEvaluationWellFormed) {
  mojo::PendingReceiver<mojom::DistillationEvaluator> receiver;
  ReadingModeMetricsService service(std::move(receiver));

  std::string good_html = "<div class=\"content\"><h1>Title </h1><p>Some text </p></div>";
  mojom::EvaluationStatus status;
  mojom::DistillationMetricsPtr metrics;

  service.Evaluate(CreateComplexMockAXTreeUpdate(), good_html,
                   base::BindOnce(
                       [](mojom::EvaluationStatus* out_status,
                          mojom::DistillationMetricsPtr* out_metrics,
                          base::expected<mojom::DistillationMetricsPtr,
                                         mojom::EvaluationStatus> result) {
                         if (result.has_value()) {
                           *out_status = mojom::EvaluationStatus::kSuccess;
                           *out_metrics = std::move(result.value());
                         } else {
                           *out_status = result.error();
                         }
                       },
                       &status, &metrics));

  EXPECT_EQ(status, mojom::EvaluationStatus::kSuccess);
  ASSERT_TRUE(metrics);
}

TEST_F(ReadingModeMetricsServiceTest, MalformedHtmlEvaluationMalformed) {
  mojo::PendingReceiver<mojom::DistillationEvaluator> receiver;
  ReadingModeMetricsService service(std::move(receiver));

  std::string malformed_html = "<div class=\"content\"><h1>Title </h1><p>Some text </div>";
  mojom::EvaluationStatus status;
  mojom::DistillationMetricsPtr metrics;

  service.Evaluate(CreateComplexMockAXTreeUpdate(), malformed_html,
                   base::BindOnce(
                       [](mojom::EvaluationStatus* out_status,
                          mojom::DistillationMetricsPtr* out_metrics,
                          base::expected<mojom::DistillationMetricsPtr,
                                         mojom::EvaluationStatus> result) {
                         if (result.has_value()) {
                           *out_status = mojom::EvaluationStatus::kSuccess;
                           *out_metrics = std::move(result.value());
                         } else {
                           *out_status = result.error();
                         }
                       },
                       &status, &metrics));

  EXPECT_EQ(status, mojom::EvaluationStatus::kMalformedDistilledHtml);
  EXPECT_FALSE(metrics);
}

}  // namespace reading_mode
