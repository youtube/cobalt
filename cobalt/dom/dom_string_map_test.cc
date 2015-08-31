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

#include "cobalt/dom/dom_string_map.h"

#include <string>

#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class DOMStringMapTest : public ::testing::Test {
 protected:
  DOMStringMapTest();
  ~DOMStringMapTest() OVERRIDE {}

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
  scoped_refptr<Element> root_;
};

DOMStringMapTest::DOMStringMapTest()
    : html_element_context_(NULL, NULL, NULL, NULL, NULL, NULL),
      document_(new Document(&html_element_context_, Document::Options())),
      root_(new Element(document_)) {}

TEST_F(DOMStringMapTest, Getter) {
  scoped_refptr<DOMStringMap> dom_string_map = new DOMStringMap(root_);
  EXPECT_FALSE(dom_string_map->CanQueryNamedProperty("foo"));

  root_->SetAttribute("data-foo", "bar");
  EXPECT_TRUE(dom_string_map->CanQueryNamedProperty("foo"));
  EXPECT_EQ("bar", dom_string_map->AnonymousNamedGetter("foo"));
}

TEST_F(DOMStringMapTest, Setter) {
  scoped_refptr<DOMStringMap> dom_string_map = new DOMStringMap(root_);
  EXPECT_FALSE(dom_string_map->CanQueryNamedProperty("foo"));

  dom_string_map->AnonymousNamedSetter("foo", "bar");
  EXPECT_TRUE(dom_string_map->CanQueryNamedProperty("foo"));
  EXPECT_EQ("bar", root_->GetAttribute("data-foo").value());
  EXPECT_EQ("bar", dom_string_map->AnonymousNamedGetter("foo"));
}

}  // namespace dom
}  // namespace cobalt
