// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom_parser/html_decoder.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element_context.h"
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

const int kDOMMaxElementDepth = 32;

class MockErrorCallback : public base::Callback<void(const std::string&)> {
 public:
  MOCK_METHOD1(Run, void(const std::string&));
};

class HTMLDecoderTest : public ::testing::Test {
 protected:
  HTMLDecoderTest();
  ~HTMLDecoderTest() override {}

  loader::FetcherFactory fetcher_factory_;
  scoped_ptr<Parser> dom_parser_;
  dom::testing::StubCSSParser stub_css_parser_;
  dom::testing::StubScriptRunner stub_script_runner_;
  scoped_ptr<dom::DomStatTracker> dom_stat_tracker_;
  dom::HTMLElementContext html_element_context_;
  scoped_refptr<dom::Document> document_;
  scoped_refptr<dom::Element> root_;
  base::SourceLocation source_location_;
  MockErrorCallback mock_error_callback_;
  scoped_ptr<HTMLDecoder> html_decoder_;
  MessageLoop message_loop_;
};

HTMLDecoderTest::HTMLDecoderTest()
    : fetcher_factory_(NULL /* network_module */),
      dom_parser_(new Parser()),
      dom_stat_tracker_(new dom::DomStatTracker("HTMLDecoderTest")),
      html_element_context_(
          &fetcher_factory_, &stub_css_parser_, dom_parser_.get(),
          NULL /* can_play_type_handler */, NULL /* web_media_player_factory */,
          &stub_script_runner_, NULL /* script_value_factory */, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL, dom_stat_tracker_.get(), "",
          base::kApplicationStateStarted),
      document_(new dom::Document(&html_element_context_)),
      root_(new dom::Element(document_, base::Token("element"))),
      source_location_(base::SourceLocation("[object HTMLDecoderTest]", 1, 1)) {
}

TEST_F(HTMLDecoderTest, CanParseEmptyDocument) {
  const std::string input = "";
  html_decoder_.reset(
      new HTMLDecoder(document_, document_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("html", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("body", element->local_name());
}

// TODO: Currently HTMLDecoder is using libxml2 SAX parser. It doesn't
// correctly add the implied tags according to HTML5. Enable the disabled tests
// after switching to new parser.
TEST_F(HTMLDecoderTest, DISABLED_CanParseDocumentWithOnlyNulls) {
  unsigned char temp[] = {0x0, 0x0};

  html_decoder_.reset(
      new HTMLDecoder(document_, document_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(reinterpret_cast<char*>(temp), sizeof(temp));
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("html", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("body", element->local_name());
}

TEST_F(HTMLDecoderTest, DISABLED_CanParseDocumentWithOnlySpaces) {
  const std::string input = "   ";
  html_decoder_.reset(
      new HTMLDecoder(document_, document_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("html", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("body", element->local_name());
}

TEST_F(HTMLDecoderTest, DISABLED_CanParseDocumentWithOnlyHTML) {
  const std::string input = "<html>";
  html_decoder_.reset(
      new HTMLDecoder(document_, document_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("html", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("body", element->local_name());
}

TEST_F(HTMLDecoderTest, CanParseDocumentWithOnlyBody) {
  const std::string input = "<body>";
  html_decoder_.reset(
      new HTMLDecoder(document_, document_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("html", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("body", element->local_name());
}

TEST_F(HTMLDecoderTest, DecodingWholeDocumentShouldAddImpliedTags) {
  const std::string input = "<p></p>";
  html_decoder_.reset(
      new HTMLDecoder(document_, document_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("html", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("body", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());
  EXPECT_FALSE(element->HasChildNodes());
}

TEST_F(HTMLDecoderTest, DecodingDocumentFragmentShouldNotAddImpliedTags) {
  const std::string input = "<p></p>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());
  EXPECT_FALSE(element->HasChildNodes());
}

TEST_F(HTMLDecoderTest, CanParseAttributesWithAndWithoutValue) {
  const std::string input = "<div a b=2 c d></div>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("div", element->local_name());
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
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("div", element->local_name());
  EXPECT_TRUE(element->HasAttributes());
  EXPECT_EQ(2, element->attributes()->length());
  EXPECT_EQ("a", element->attributes()->Item(0)->name());
  EXPECT_EQ("b=2", element->attributes()->Item(0)->value());
  EXPECT_EQ("c", element->attributes()->Item(1)->name());
  EXPECT_EQ("", element->attributes()->Item(1)->value());
}

TEST_F(HTMLDecoderTest, CanParseSelfClosingTags) {
  const std::string input = "<p><p>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());

  element = element->next_element_sibling();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());
}

TEST_F(HTMLDecoderTest, CanParseNormalCharacters) {
  const std::string input = "<p>text</p>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());

  dom::Text* text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("text", text->data());
}

TEST_F(HTMLDecoderTest, ShouldIgnoreNonUTF8Input) {
  unsigned char temp[] = {0xff, 0xff};

  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(reinterpret_cast<char*>(temp), sizeof(temp));
  html_decoder_->Finish();

  ASSERT_FALSE(root_->first_child());
}

// Test a decimal and hex escaped supplementary (not in BMP) character.
TEST_F(HTMLDecoderTest, CanParseEscapedCharacters) {
  const std::string input = "<p>&#128169;</p><p>&#x1f4a9;</p>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());

  dom::Text* text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("💩", text->data());

  element = element->next_element_sibling();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());

  text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("💩", text->data());
}

// Test an escaped invalid Unicode character.
TEST_F(HTMLDecoderTest, CanParseEscapedInvalidUnicodeCharacters) {
  const std::string input = "<p>&#xe62b;</p>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());

  dom::Text* text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("\xEE\x98\xAB", text->data());
}

// Test a UTF8 encoded supplementary (not in BMP) character.
TEST_F(HTMLDecoderTest, CanParseUTF8EncodedSupplementaryCharacters) {
  const std::string input = "<p>💩</p>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());

  dom::Text* text = element->first_child()->AsText();
  ASSERT_TRUE(text);
  EXPECT_EQ("💩", text->data());
}

TEST_F(HTMLDecoderTest, CanParseUTF8SplitInChunks) {
  const std::string input = "<p>💩</p>";

  for (size_t first_chunk_size = 0; first_chunk_size < input.length();
       first_chunk_size++) {
    root_ = new dom::Element(document_, base::Token("element"));
    html_decoder_.reset(
        new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                        source_location_, base::Closure(),
                        base::Bind(&MockErrorCallback::Run,
                                   base::Unretained(&mock_error_callback_)),
                        true, csp::kCSPRequired));

    // This could cut the input in the middle of a UTF8 character.
    html_decoder_->DecodeChunk(input.c_str(), first_chunk_size);
    html_decoder_->DecodeChunk(input.c_str() + first_chunk_size,
                               input.length() - first_chunk_size);
    html_decoder_->Finish();

    dom::Element* element = root_->first_element_child();
    ASSERT_TRUE(element);
    EXPECT_EQ("p", element->local_name());

    dom::Text* text = element->first_child()->AsText();
    ASSERT_TRUE(text);
    EXPECT_EQ("💩", text->data());
  }
}

// Misnested tags: <b><i></b></i>
//   https://www.w3.org/TR/html5/syntax.html#misnested-tags:-b-i-/b-/i
//
// The current version DOES NOT handle the error as outlined in the link above.
TEST_F(HTMLDecoderTest, CanParseMisnestedTags1) {
  const std::string input = "<p>1<b>2<i>3</b>4</i>5</p>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("b", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("i", element->local_name());
}

// Misnested tags: <b><p></b></p>
//   https://www.w3.org/TR/html5/syntax.html#misnested-tags:-b-p-/b-/p
//
// The current version DOES NOT handle the error as outlined in the link above.
TEST_F(HTMLDecoderTest, CanParseMisnestedTags2) {
  const std::string input = "<b>1<p>2</b>3</p>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("b", element->local_name());

  element = element->next_element_sibling();
  ASSERT_TRUE(element);
  EXPECT_EQ("p", element->local_name());
}

TEST_F(HTMLDecoderTest, TagNamesShouldBeCaseInsensitive) {
  const std::string input = "<DIV></DIV>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("div", element->local_name());
}

TEST_F(HTMLDecoderTest, AttributesShouldBeCaseInsensitive) {
  const std::string input = "<div A></div>";
  html_decoder_.reset(
      new HTMLDecoder(document_, root_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  dom::Element* element = root_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("div", element->local_name());
  EXPECT_TRUE(element->HasAttributes());
  EXPECT_EQ(1, element->attributes()->length());
  EXPECT_EQ("a", element->attributes()->Item(0)->name());
}

TEST_F(HTMLDecoderTest, LibxmlDecodingErrorShouldTerminateParsing) {
  const std::string input =
      "<html>"
      "<head>"
      "<meta content='text/html; charset=gb2312' http-equiv=Content-Type>"
      "</head>"
      "<body>陈绮贞</body>"
      "</html>";
  html_decoder_.reset(
      new HTMLDecoder(document_, document_, NULL, kDOMMaxElementDepth,
                      source_location_, base::Closure(),
                      base::Bind(&MockErrorCallback::Run,
                                 base::Unretained(&mock_error_callback_)),
                      true, csp::kCSPRequired));
  html_decoder_->DecodeChunk(input.c_str(), input.length());
  html_decoder_->Finish();

  root_ = document_->first_element_child();
  ASSERT_TRUE(root_);
  EXPECT_EQ("html", root_->local_name());

  dom::Element* head = root_->first_element_child();
  ASSERT_TRUE(head);
  EXPECT_EQ("head", head->local_name());
}

}  // namespace dom_parser
}  // namespace cobalt
