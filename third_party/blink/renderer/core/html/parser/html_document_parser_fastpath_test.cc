// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_document_parser_fastpath.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

TEST(HTMLDocumentParserFastpathTest, SanityCheck) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  document->body()->AppendChild(div);
  DocumentFragment* fragment = DocumentFragment::Create(*document);
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(TryParsingHTMLFragment(
      "<div>test</div>", *document, *fragment, *div,
      ParserContentPolicy::kAllowScriptingContent, false));
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTagType.CompositeMaskV2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.CompositeMaskV2", 0);
}

TEST(HTMLDocumentParserFastpathTest, SetInnerHTMLUsesFastPathSuccess) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("<div>test</div>");
  // This was html the fast path handled, so there should be one histogram with
  // success.
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLFastPathParser.ParseResult",
                                      HtmlFastPathResult::kSucceeded, 1);
}

TEST(HTMLDocumentParserFastpathTest, SetInnerHTMLUsesFastPathFailure) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("<div");
  // The fast path should not have handled this, so there should be one
  // histogram with a value other then success.
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectBucketCount("Blink.HTMLFastPathParser.ParseResult",
                                     HtmlFastPathResult::kSucceeded, 0);
}

TEST(HTMLDocumentParserFastpathTest, LongTextIsSplit) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  std::vector<LChar> chars(Text::kDefaultLengthLimit + 1, 'a');
  div->setInnerHTML(String(chars.data(), static_cast<unsigned>(chars.size())));
  Text* text_node = To<Text>(div->firstChild());
  ASSERT_TRUE(text_node);
  // Text is split at 64k for performance. See
  // HTMLConstructionSite::FlushPendingText for more details.
  EXPECT_EQ(Text::kDefaultLengthLimit, text_node->length());
}

TEST(HTMLDocumentParserFastpathTest, MaximumHTMLParserDOMTreeDepth) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  StringBuilder string_builder;
  const unsigned depth =
      HTMLConstructionSite::kMaximumHTMLParserDOMTreeDepth + 2;
  // Create a very nested tree, with the deepest containing the id `deepest`.
  for (unsigned i = 0; i < depth - 1; ++i) {
    string_builder.Append("<div>");
  }
  string_builder.Append("<div id='deepest'>");
  string_builder.Append("</div>");
  for (unsigned i = 0; i < depth - 1; ++i) {
    string_builder.Append("</div>");
  }
  div->setInnerHTML(string_builder.ToString());

  // Because kMaximumHTMLParserDOMTreeDepth was encountered, the deepest
  // node should have siblings.
  Element* deepest = div->getElementById(AtomicString("deepest"));
  ASSERT_TRUE(deepest);
  EXPECT_EQ(deepest->parentNode()->CountChildren(), 3u);
}

TEST(HTMLDocumentParserFastpathTest, LogUnsupportedTags) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("<table></table>");
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 1);
  histogram_tester.ExpectBucketCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 2, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask0V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask1V2", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask2V2", 0);
}

TEST(HTMLDocumentParserFastpathTest, LogUnsupportedTagsWithValidTag) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("<div><table></table></div>");
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 1);
  // Table is in the second chunk of values, so 2 should be set.
  histogram_tester.ExpectBucketCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 2, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask0V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask1V2", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask2V2", 0);
}

TEST(HTMLDocumentParserFastpathTest, LogUnsupportedContextTag) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* dl = MakeGarbageCollected<HTMLTextAreaElement>(*document);

  base::HistogramTester histogram_tester;
  dl->setInnerHTML("some text");
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.CompositeMaskV2", 1);
  // Textarea is in the third chunk of values, so 3 should be set.
  histogram_tester.ExpectBucketCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.CompositeMaskV2", 4, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask0V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask1V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask2V2", 1);
}

TEST(HTMLDocumentParserFastpathTest, LogSvg) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("<svg></svg>");
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 1);
  // Svg is in the third chunk of values, so 4 should be set.
  histogram_tester.ExpectBucketCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 4, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask0V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask1V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask2V2", 1);
}

TEST(HTMLDocumentParserFastpathTest, LogUnsupportedContextTagBody) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* body = MakeGarbageCollected<HTMLBodyElement>(*document);

  base::HistogramTester histogram_tester;
  body->setInnerHTML("some text");
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.CompositeMaskV2", 1);
  // Body is in the third chunk of values, so 4 should be set.
  histogram_tester.ExpectBucketCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.CompositeMaskV2", 4, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask0V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask1V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask2V2", 1);
}

TEST(HTMLDocumentParserFastpathTest, HTMLInputElementCheckedState) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div1 = MakeGarbageCollected<HTMLDivElement>(*document);
  auto* div2 = MakeGarbageCollected<HTMLDivElement>(*document);
  document->body()->AppendChild(div1);
  document->body()->AppendChild(div2);

  // Set the state for new controls, which triggers a different code path in
  // HTMLInputElement::ParseAttribute.
  div1->setInnerHTML("<select form='ff'></select>");
  DocumentState* document_state = document->GetFormController().ControlStates();
  Vector<String> state1 = document_state->ToStateVector();
  document->GetFormController().SetStateForNewControls(state1);
  EXPECT_TRUE(document->GetFormController().HasControlStates());

  div2->setInnerHTML("<input checked='true'>");
  HTMLInputElement* input_element = To<HTMLInputElement>(div2->firstChild());
  ASSERT_TRUE(input_element);
  EXPECT_TRUE(input_element->Checked());
}

TEST(HTMLDocumentParserFastpathTest, CharacterReferenceCases) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  // Various subtle cases of character references that have caused problems.
  // The assertions are handled by DCHECKs in the code, specifically in
  // serialization.cc.
  div->setInnerHTML("Genius Nicer Dicer Plus | 18&nbsp&hellip;");
  div->setInnerHTML("&nbsp&a");
  div->setInnerHTML("&nbsp&");
  div->setInnerHTML("&nbsp-");
}

TEST(HTMLDocumentParserFastpathTest, HandlesCompleteCharacterReference) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("&cent;");
  Text* text_node = To<Text>(div->firstChild());
  ASSERT_TRUE(text_node);
  EXPECT_EQ(text_node->data(), String(u"\u00A2"));
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLFastPathParser.ParseResult",
                                      HtmlFastPathResult::kSucceeded, 1);
}

TEST(HTMLDocumentParserFastpathTest, FailsWithNestedLis) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("<li><li></li></li>");
  // The html results in two children (nested <li>s implicitly close the open
  // <li>, resulting in two sibling <li>s, not one). The fast path parser does
  // not handle this case.
  EXPECT_EQ(2u, div->CountChildren());
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectBucketCount("Blink.HTMLFastPathParser.ParseResult",
                                     HtmlFastPathResult::kSucceeded, 0);
}

TEST(HTMLDocumentParserFastpathTest, HandlesLi) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("<div><li></li></div>");
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLFastPathParser.ParseResult",
                                      HtmlFastPathResult::kSucceeded, 1);
}

TEST(HTMLDocumentParserFastpathTest, NullMappedToReplacementChar) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  // Constructor that takes size is needed because of \0 in string.
  div->setInnerHTML(
      String("<div id='x' name='x\0y'></div>", static_cast<size_t>(29)));
  Element* new_div = div->getElementById(AtomicString("x"));
  ASSERT_TRUE(new_div);
  // Null chars are generally mapped to \uFFFD (at least this test should
  // trigger the replacement).
  EXPECT_EQ(AtomicString(String(u"x\uFFFDy")), new_div->GetNameAttribute());
}

}  // namespace
}  // namespace blink
