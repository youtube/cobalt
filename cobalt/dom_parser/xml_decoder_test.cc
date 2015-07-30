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

#include "cobalt/dom_parser/xml_decoder.h"

#include "base/callback.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/xml_document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/text.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom_parser {

class MockErrorCallback : public base::Callback<void(const std::string&)> {
 public:
  MOCK_METHOD1(Run, void(const std::string&));
};

class XMLDecoderTest : public ::testing::Test {
 protected:
  XMLDecoderTest();
  ~XMLDecoderTest() OVERRIDE {}

  scoped_refptr<dom::XMLDocument> document_;
  base::SourceLocation source_location_;
  MockErrorCallback mock_error_callback_;
  XMLDecoder* xml_decoder_;
};

XMLDecoderTest::XMLDecoderTest()
    : document_(new dom::XMLDocument(dom::Document::Options())),
      source_location_(base::SourceLocation("[object XMLDecoderTest]", 1, 1)) {}

TEST_F(XMLDecoderTest, ShouldNotAddImpliedTags) {
  const std::string input = "<ENTITY></ENTITY>";
  xml_decoder_ = new XMLDecoder(
      document_, document_, NULL, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("ENTITY", element->tag_name());
  EXPECT_FALSE(element->HasChildNodes());
}

TEST_F(XMLDecoderTest, CanParseAttributesWithValue) {
  const std::string input = "<ENTITY a=\"1\" b=\"2\"></ENTITY>";
  xml_decoder_ = new XMLDecoder(
      document_, document_, NULL, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("ENTITY", element->tag_name());
  EXPECT_TRUE(element->HasAttributes());
  EXPECT_EQ(2, element->attributes()->length());
  EXPECT_EQ("a", element->attributes()->Item(0)->name());
  EXPECT_EQ("1", element->attributes()->Item(0)->value());
  EXPECT_EQ("b", element->attributes()->Item(1)->name());
  EXPECT_EQ("2", element->attributes()->Item(1)->value());
}

TEST_F(XMLDecoderTest, TagNamesShouldBeCaseSensitive) {
  const std::string input = "<ENTITY><entity></entity></ENTITY>";
  xml_decoder_ = new XMLDecoder(
      document_, document_, NULL, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("ENTITY", element->tag_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("entity", element->tag_name());
}

TEST_F(XMLDecoderTest, AttributesShouldBeCaseSensitive) {
  const std::string input = "<ENTITY A=\"1\" a=\"2\"></ENTITY>";
  xml_decoder_ = new XMLDecoder(
      document_, document_, NULL, source_location_, base::Closure(),
      base::Bind(&MockErrorCallback::Run,
                 base::Unretained(&mock_error_callback_)));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("ENTITY", element->tag_name());
  EXPECT_TRUE(element->HasAttributes());
  EXPECT_EQ(2, element->attributes()->length());
  EXPECT_EQ("A", element->attributes()->Item(0)->name());
  EXPECT_EQ("1", element->attributes()->Item(0)->value());
  EXPECT_EQ("a", element->attributes()->Item(1)->name());
  EXPECT_EQ("2", element->attributes()->Item(1)->value());
}

}  // namespace dom_parser
}  // namespace cobalt
