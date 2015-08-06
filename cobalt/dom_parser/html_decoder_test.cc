/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom_parser/html_decoder.h"

#include "base/callback.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/testing/stub_css_parser.h"
#include "cobalt/dom/testing/stub_script_runner.h"
#include "cobalt/dom/text.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom_parser {

class MockErrorCallback : public base::Callback<void(const std::string&)> {
 public:
  MOCK_METHOD1(Run, void(const std::string&));
};

class HTMLDecoderTest : public ::testing::Test {
 protected:
  HTMLDecoderTest();
  ~HTMLDecoderTest() OVERRIDE {}

  loader::FetcherFactory fetcher_factory_;
  Parser* dom_parser_;
  dom::testing::StubCSSParser stub_css_parser_;
  dom::testing::StubScriptRunner stub_script_runner_;
  dom::HTMLElementContext html_element_context_;
  scoped_refptr<dom::Document> document_;
  base::SourceLocation source_location_;
  MockErrorCallback mock_error_callback_;
  HTMLDecoder* html_decoder_;
};

HTMLDecoderTest::HTMLDecoderTest()
    : fetcher_factory_(NULL /* network_module */),
      dom_parser_(new Parser()),
      html_element_context_(&fetcher_factory_, &stub_css_parser_, dom_parser_,
                            NULL /* web_media_player_factory */,
                            &stub_script_runner_),
      document_(
          new dom::Document(&html_element_context_, dom::Document::Options())),
      source_location_(base::SourceLocation("[object HTMLDecoderTest]", 1, 1)) {
}

TEST_F(HTMLDecoderTest, DocumentFullShouldAddImpliedTags) {
  const std::string input = "<p></p>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFull, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("html", element->tag_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("body", element->tag_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());
  EXPECT_FALSE(element->HasChildNodes());
}

TEST_F(HTMLDecoderTest, DocumentFragmentShouldNotAddImpliedTags) {
  const std::string input = "<p></p>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());
  EXPECT_FALSE(element->HasChildNodes());
}

TEST_F(HTMLDecoderTest, CanParseAttributesWithAndWithoutValue) {
  const std::string input = "<div a b=2 c d></div>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("div", element->tag_name());
  EXPECT_TRUE(element->HasAttributes());
  EXPECT_EQ(4, element->attributes()->length());
  EXPECT_EQ("a", element->attributes()->Item(0)->name());
  EXPECT_EQ("", element->attributes()->Item(0)->value());
  EXPECT_EQ("b", element->attributes()->Item(1)->name());
  EXPECT_EQ("2", element->attributes()->Item(1)->value());
  EXPECT_EQ("c", element->attributes()->Item(2)->name());
  EXPECT_EQ("", element->attributes()->Item(2)->value());
  EXPECT_EQ("d", element->attributes()->Item(3)->name());
  EXPECT_EQ("", element->attributes()->Item(3)->value());
}

TEST_F(HTMLDecoderTest, CanParseIncompleteAttributesAssignment) {
  const std::string input = "<div a= b=2 c=></div>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("div", element->tag_name());
  EXPECT_TRUE(element->HasAttributes());
  EXPECT_EQ(2, element->attributes()->length());
  EXPECT_EQ("a", element->attributes()->Item(0)->name());
  EXPECT_EQ("b=2", element->attributes()->Item(0)->value());
  EXPECT_EQ("c", element->attributes()->Item(1)->name());
  EXPECT_EQ("", element->attributes()->Item(1)->value());
}

TEST_F(HTMLDecoderTest, CanParseSelfClosingTags) {
  const std::string input = "<p><p>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());

  element = element->next_element_sibling();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());
}

TEST_F(HTMLDecoderTest, CanParseNormalCharacters) {
  const std::string input = "<p>text</p>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());

  dom::Text* text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("text", text->data());
}

// Test a decimal and hex escaped supplementary (not in BMP) character.
TEST_F(HTMLDecoderTest, CanParseEscapedCharacters) {
  const std::string input = "<p>&#128169;</p><p>&#x1f4a9;</p>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());

  dom::Text* text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("💩", text->data());

  element = element->next_element_sibling();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());

  text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("💩", text->data());
}

// Test an escaped invalid Unicode character.
TEST_F(HTMLDecoderTest, CanParseEscapedInvalidUnicodeCharacters) {
  const std::string input = "<p>&#xe62b;</p>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());

  dom::Text* text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("\xEE\x98\xAB", text->data());
}

// Test a UTF8 encoded supplementary (not in BMP) character.
TEST_F(HTMLDecoderTest, CanParseUTF8EncodedSupplementaryCharacters) {
  const std::string input = "<p>💩</p>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());

  dom::Text* text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("💩", text->data());
}

// Misnested tags: <b><i></b></i>
//   http://www.w3.org/TR/html5/syntax.html#misnested-tags:-b-i-/b-/i
//
// The current version DOES NOT handle the error as outlined in the link above.
TEST_F(HTMLDecoderTest, CanParseMisnestedTags1) {
  const std::string input = "<p>1<b>2<i>3</b>4</i>5</p>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));

  EXPECT_CALL(mock_error_callback_,
              Run("Opening and ending tag mismatch: b and i"));
  EXPECT_CALL(mock_error_callback_, Run("Unexpected end tag : i"));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("b", element->tag_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("i", element->tag_name());
}

// Misnested tags: <b><p></b></p>
//   http://www.w3.org/TR/html5/syntax.html#misnested-tags:-b-p-/b-/p
//
// The current version DOES NOT handle the error as outlined in the link above.
TEST_F(HTMLDecoderTest, CanParseMisnestedTags2) {
  const std::string input = "<b>1<p>2</b>3</p>";
  html_decoder_ = new HTMLDecoder(
      &html_element_context_, document_, document_, NULL,
      HTMLDecoder::kDocumentFragment, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));

  EXPECT_CALL(mock_error_callback_, Run("Unexpected end tag : b"));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("b", element->tag_name());

  element = element->next_element_sibling();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->tag_name());
}

}  // namespace dom_parser
}  // namespace cobalt
