// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/accessibility/internal/text_alternative_helper.h"
#include "cobalt/dom/html_image_element.h"
#include "cobalt/dom/text.h"
#include "cobalt/test/empty_document.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace accessibility {
namespace internal {
class TextAlternativeHelperTest : public ::testing::Test {
 protected:
  dom::Document* document() { return empty_document_.document(); }

  TextAlternativeHelper text_alternative_helper_;
  test::EmptyDocument empty_document_;
};

TEST_F(TextAlternativeHelperTest, ConcatenatesAlternatives) {
  EXPECT_TRUE(text_alternative_helper_.AppendTextIfNonEmpty("test"));
  EXPECT_TRUE(text_alternative_helper_.AppendTextIfNonEmpty("dog"));
  EXPECT_TRUE(text_alternative_helper_.AppendTextIfNonEmpty("cat"));
  EXPECT_STREQ("test dog cat",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, IgnoresEmptyStrings) {
  EXPECT_FALSE(text_alternative_helper_.AppendTextIfNonEmpty(""));
  EXPECT_STREQ("", text_alternative_helper_.GetTextAlternative().c_str());
  EXPECT_FALSE(text_alternative_helper_.AppendTextIfNonEmpty("     "));
  EXPECT_TRUE(text_alternative_helper_.GetTextAlternative().empty());
}

TEST_F(TextAlternativeHelperTest, TrimsAlternatives) {
  EXPECT_TRUE(text_alternative_helper_.AppendTextIfNonEmpty("  test "));
  EXPECT_TRUE(text_alternative_helper_.AppendTextIfNonEmpty("dog  "));
  EXPECT_TRUE(text_alternative_helper_.AppendTextIfNonEmpty(" cat"));
  EXPECT_STREQ("test dog cat",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, HiddenOnlyIfAriaHiddenSet) {
  scoped_refptr<dom::Element> element = document()->CreateElement("div");
  // No aria-hidden attribute set.
  EXPECT_FALSE(TextAlternativeHelper::IsAriaHidden(element));

  // aria-hidden attribute set to false.
  element->SetAttribute(base::Tokens::aria_hidden().c_str(),
                        base::Tokens::false_token().c_str());
  EXPECT_FALSE(TextAlternativeHelper::IsAriaHidden(element));

  // aria-hidden attribute set to true.
  element->SetAttribute(base::Tokens::aria_hidden().c_str(),
                        base::Tokens::true_token().c_str());
  EXPECT_TRUE(TextAlternativeHelper::IsAriaHidden(element));

  // aria-hidden set to some arbitrary value.
  element->SetAttribute(base::Tokens::aria_hidden().c_str(), "banana");
  EXPECT_FALSE(TextAlternativeHelper::IsAriaHidden(element));
}

TEST_F(TextAlternativeHelperTest, GetTextAlternativeFromTextNode) {
  scoped_refptr<dom::Element> element = document()->CreateElement("div");
  element->AppendChild(document()->CreateTextNode("text node contents"));
  text_alternative_helper_.AppendTextAlternative(element);
  EXPECT_STREQ("text node contents",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, GetAltAttributeFromHTMLImage) {
  scoped_refptr<dom::Element> element =
      document()->CreateElement(dom::HTMLImageElement::kTagName);
  element->SetAttribute(base::Tokens::alt().c_str(), "image alt text");
  EXPECT_TRUE(text_alternative_helper_.TryAppendFromAltProperty(element));
  EXPECT_STREQ("image alt text",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, EmptyAltAttribute) {
  scoped_refptr<dom::Element> element =
      document()->CreateElement(dom::HTMLImageElement::kTagName);
  // No alt attribute.
  EXPECT_FALSE(text_alternative_helper_.TryAppendFromAltProperty(element));
  EXPECT_TRUE(text_alternative_helper_.GetTextAlternative().empty());

  // Empty alt attribute.
  element->SetAttribute(base::Tokens::alt().c_str(), "");
  EXPECT_FALSE(text_alternative_helper_.TryAppendFromAltProperty(element));
  EXPECT_TRUE(text_alternative_helper_.GetTextAlternative().empty());
}

TEST_F(TextAlternativeHelperTest, AltAttributeOnArbitraryElement) {
  scoped_refptr<dom::Element> element = document()->CreateElement("div");
  element->SetAttribute(base::Tokens::alt().c_str(), "alt text");
  EXPECT_FALSE(text_alternative_helper_.TryAppendFromAltProperty(element));
  EXPECT_TRUE(text_alternative_helper_.GetTextAlternative().empty());
}

TEST_F(TextAlternativeHelperTest, GetTextFromAriaLabel) {
  scoped_refptr<dom::Element> element = document()->CreateElement("div");
  element->SetAttribute(base::Tokens::aria_label().c_str(), "label text");
  EXPECT_TRUE(text_alternative_helper_.TryAppendFromLabel(element));
  EXPECT_STREQ("label text",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, IgnoreEmptyAriaLabel) {
  scoped_refptr<dom::Element> element = document()->CreateElement("test");
  element->SetAttribute(base::Tokens::aria_label().c_str(), "   ");
  EXPECT_FALSE(text_alternative_helper_.TryAppendFromLabel(element));
  EXPECT_TRUE(text_alternative_helper_.GetTextAlternative().empty());
}

TEST_F(TextAlternativeHelperTest, GetTextFromAriaLabelledBy) {
  scoped_refptr<dom::Element> target_element = document()->CreateElement("div");
  target_element->AppendChild(document()->CreateTextNode("labelledby target"));
  target_element->set_id("target_element");
  document()->AppendChild(target_element);

  scoped_refptr<dom::Element> labelledby_element =
      document()->CreateElement("div");
  labelledby_element->SetAttribute(base::Tokens::aria_labelledby().c_str(),
                                   "target_element");
  document()->AppendChild(labelledby_element);

  EXPECT_TRUE(text_alternative_helper_.TryAppendFromLabelledByOrDescribedBy(
      labelledby_element, base::Tokens::aria_labelledby()));
  EXPECT_STREQ("labelledby target",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, InvalidLabelledByReference) {
  scoped_refptr<dom::Element> target_element = document()->CreateElement("div");
  target_element->AppendChild(document()->CreateTextNode("labelledby target"));
  target_element->set_id("target_element");
  document()->AppendChild(target_element);

  scoped_refptr<dom::Element> labelledby_element =
      document()->CreateElement("div");
  labelledby_element->SetAttribute(base::Tokens::aria_labelledby().c_str(),
                                   "bad_reference");
  document()->AppendChild(labelledby_element);

  EXPECT_FALSE(text_alternative_helper_.TryAppendFromLabelledByOrDescribedBy(
      labelledby_element, base::Tokens::aria_labelledby()));
  EXPECT_TRUE(text_alternative_helper_.GetTextAlternative().empty());
}

TEST_F(TextAlternativeHelperTest, MultipleLabelledByReferences) {
  scoped_refptr<dom::Element> target_element = document()->CreateElement("div");
  target_element->AppendChild(document()->CreateTextNode("target1"));
  target_element->set_id("target_element1");
  document()->AppendChild(target_element);
  target_element = document()->CreateElement("div");
  target_element->AppendChild(document()->CreateTextNode("target2"));
  target_element->set_id("target_element2");
  document()->AppendChild(target_element);
  target_element = document()->CreateElement("div");
  target_element->AppendChild(document()->CreateTextNode("target3"));
  target_element->set_id("target_element3");
  document()->AppendChild(target_element);

  scoped_refptr<dom::Element> labelledby_element =
      document()->CreateElement("div");
  labelledby_element->SetAttribute(
      base::Tokens::aria_labelledby().c_str(),
      "target_element1 target_element2 bad_reference target_element3");
  document()->AppendChild(labelledby_element);

  EXPECT_TRUE(text_alternative_helper_.TryAppendFromLabelledByOrDescribedBy(
      labelledby_element, base::Tokens::aria_labelledby()));
  EXPECT_STREQ("target1 target2 target3",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, LabelledByReferencesSelf) {
  scoped_refptr<dom::Element> target_element = document()->CreateElement("div");
  target_element->AppendChild(document()->CreateTextNode("other-text"));
  target_element->set_id("other_id");
  document()->AppendChild(target_element);

  scoped_refptr<dom::Element> labelledby_element =
      document()->CreateElement("div");
  labelledby_element->set_id("self_id");
  labelledby_element->SetAttribute(base::Tokens::aria_label().c_str(),
                                   "self-label");
  labelledby_element->SetAttribute(base::Tokens::aria_labelledby().c_str(),
                                   "other_id self_id");
  document()->AppendChild(labelledby_element);

  EXPECT_TRUE(text_alternative_helper_.TryAppendFromLabelledByOrDescribedBy(
      labelledby_element, base::Tokens::aria_labelledby()));
  EXPECT_STREQ("other-text self-label",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, NoRecursiveLabelledBy) {
  scoped_refptr<dom::Element> element = document()->CreateElement("div");
  element->SetAttribute(base::Tokens::aria_labelledby().c_str(),
                        "first_labelled_by_id");
  document()->AppendChild(element);

  scoped_refptr<dom::Element> first_labelledby_element =
      document()->CreateElement("div");
  first_labelledby_element->AppendChild(
      document()->CreateTextNode("first-labelledby-text-contents"));
  first_labelledby_element->set_id("first_labelled_by_id");
  first_labelledby_element->SetAttribute(
      base::Tokens::aria_labelledby().c_str(), "second_labelled_by_id");
  document()->AppendChild(first_labelledby_element);

  scoped_refptr<dom::Element> second_labelledby_element =
      document()->CreateElement("div");
  second_labelledby_element->AppendChild(
      document()->CreateTextNode("second-labelledby-text-contents"));
  second_labelledby_element->set_id("second_labelled_by_id");
  document()->AppendChild(second_labelledby_element);

  // In the non-recursive case, we should follow the labelledby attribute.
  text_alternative_helper_.AppendTextAlternative(first_labelledby_element);
  EXPECT_STREQ("second-labelledby-text-contents",
               text_alternative_helper_.GetTextAlternative().c_str());

  // In the recursive case, we should not follow the second labelledby
  // attribute.
  text_alternative_helper_ = TextAlternativeHelper();
  text_alternative_helper_.AppendTextAlternative(element);
  EXPECT_STREQ("first-labelledby-text-contents",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, GetTextFromSubtree) {
  scoped_refptr<dom::Element> element = document()->CreateElement("div");
  document()->AppendChild(element);

  scoped_refptr<dom::Element> child = document()->CreateElement("div");
  child->AppendChild(document()->CreateTextNode("child1-text"));
  element->AppendChild(child);

  scoped_refptr<dom::Element> child_element = document()->CreateElement("div");
  element->AppendChild(child_element);

  child = document()->CreateElement("div");
  child->AppendChild(document()->CreateTextNode("child2-text"));
  child_element->AppendChild(child);

  child = document()->CreateElement("div");
  child->AppendChild(document()->CreateTextNode("child3-text"));
  child_element->AppendChild(child);

  text_alternative_helper_.AppendTextAlternative(element);
  EXPECT_STREQ("child1-text child2-text child3-text",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, DontFollowReferenceLoops) {
  scoped_refptr<dom::Element> element = document()->CreateElement("div");
  element->set_id("root_element_id");
  document()->AppendChild(element);

  scoped_refptr<dom::Element> child_element1 = document()->CreateElement("div");
  child_element1->SetAttribute(base::Tokens::aria_labelledby().c_str(),
                               "root_element_id");
  element->AppendChild(child_element1);

  scoped_refptr<dom::Element> child_element2 = document()->CreateElement("div");
  child_element1->AppendChild(child_element2);

  child_element2->AppendChild(document()->CreateTextNode("child2-text"));

  text_alternative_helper_.AppendTextAlternative(element);
  EXPECT_STREQ("child2-text",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, GetTextFromAriaDescribedBy) {
  scoped_refptr<dom::Element> target_element = document()->CreateElement("div");
  target_element->AppendChild(document()->CreateTextNode("describedby target"));
  target_element->set_id("target_element");
  document()->AppendChild(target_element);

  scoped_refptr<dom::Element> describedby_element =
      document()->CreateElement("div");
  describedby_element->SetAttribute(base::Tokens::aria_describedby().c_str(),
                                    "target_element");
  document()->AppendChild(describedby_element);

  EXPECT_TRUE(text_alternative_helper_.TryAppendFromLabelledByOrDescribedBy(
      describedby_element, base::Tokens::aria_describedby()));
  EXPECT_STREQ("describedby target",
               text_alternative_helper_.GetTextAlternative().c_str());
}

TEST_F(TextAlternativeHelperTest, HasBothLabelledByAndDescribedBy) {
  // aria-describedby is ignored in this case.
  scoped_refptr<dom::Element> element = document()->CreateElement("div");
  document()->AppendChild(element);

  element->SetAttribute(base::Tokens::aria_labelledby().c_str(), "calendar");
  element->SetAttribute(base::Tokens::aria_describedby().c_str(), "info");

  scoped_refptr<dom::Element> labelledby_element =
      document()->CreateElement("div");
  labelledby_element->set_id("calendar");
  labelledby_element->AppendChild(document()->CreateTextNode("Calendar"));
  element->AppendChild(labelledby_element);

  scoped_refptr<dom::Element> describedby_element =
      document()->CreateElement("div");
  describedby_element->set_id("info");
  describedby_element->AppendChild(
      document()->CreateTextNode("Calendar content description."));
  element->AppendChild(describedby_element);

  text_alternative_helper_.AppendTextAlternative(element);
  EXPECT_STREQ("Calendar",
               text_alternative_helper_.GetTextAlternative().c_str());
}

}  // namespace internal
}  // namespace accessibility
}  // namespace cobalt
