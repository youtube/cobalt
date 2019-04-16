// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "base/memory/ref_counted.h"
#include "cobalt/accessibility/internal/live_region.h"
#include "cobalt/dom/element.h"
#include "cobalt/test/empty_document.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace accessibility {
namespace internal {
class LiveRegionTest : public ::testing::Test {
 protected:
  dom::Document* document() { return empty_document_.document(); }

  scoped_refptr<dom::Element> CreateLiveRegion(const char* type) {
    scoped_refptr<dom::Element> live_region = document()->CreateElement("div");
    live_region->SetAttribute(base::Tokens::aria_live().c_str(), type);
    return live_region;
  }

 private:
  // For posting tasks by any triggered code.
  base::MessageLoop message_loop_;
  test::EmptyDocument empty_document_;
};

TEST_F(LiveRegionTest, GetLiveRegionRoot) {
  std::unique_ptr<LiveRegion> live_region;

  scoped_refptr<dom::Element> live_region_element =
      CreateLiveRegion(base::Tokens::polite().c_str());
  ASSERT_TRUE(live_region_element);

  // GetLiveRegionForNode with the live region root.
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_EQ(live_region->root(), live_region_element);

  // GetLiveRegionForNode with a descendent node of a live region.
  scoped_refptr<dom::Element> child = document()->CreateElement("div");
  live_region_element->AppendChild(child);
  live_region = LiveRegion::GetLiveRegionForNode(child);
  ASSERT_TRUE(live_region);
  EXPECT_EQ(live_region->root(), live_region_element);

  // Element that is not in a live region.
  scoped_refptr<dom::Element> not_live = document()->CreateElement("div");
  live_region = LiveRegion::GetLiveRegionForNode(not_live);
  EXPECT_FALSE(live_region);
}

TEST_F(LiveRegionTest, NestedLiveRegion) {
  scoped_refptr<dom::Element> live_region_element =
      CreateLiveRegion(base::Tokens::polite().c_str());
  ASSERT_TRUE(live_region_element);

  scoped_refptr<dom::Element> nested_region_element =
      CreateLiveRegion(base::Tokens::polite().c_str());
  ASSERT_TRUE(nested_region_element);
  live_region_element->AppendChild(nested_region_element);

  // The closest ancestor live region is this element's live region.
  scoped_refptr<dom::Element> child = document()->CreateElement("div");
  nested_region_element->AppendChild(child);
  std::unique_ptr<LiveRegion> live_region =
      LiveRegion::GetLiveRegionForNode(child);
  ASSERT_TRUE(live_region);
  EXPECT_EQ(live_region->root(), nested_region_element);

  // Ignore live regions that are not active.
  nested_region_element->SetAttribute(base::Tokens::aria_live().c_str(),
                                      base::Tokens::off().c_str());
  live_region = LiveRegion::GetLiveRegionForNode(child);
  ASSERT_TRUE(live_region);
  EXPECT_EQ(live_region->root(), live_region_element);
}

TEST_F(LiveRegionTest, LiveRegionType) {
  std::unique_ptr<LiveRegion> live_region;

  // aria-live="polite"
  scoped_refptr<dom::Element> polite_element =
      CreateLiveRegion(base::Tokens::polite().c_str());
  ASSERT_TRUE(polite_element);
  live_region = LiveRegion::GetLiveRegionForNode(polite_element);
  ASSERT_TRUE(live_region);
  EXPECT_FALSE(live_region->IsAssertive());

  // aria-live="assertive"
  scoped_refptr<dom::Element> assertive_element =
      CreateLiveRegion(base::Tokens::assertive().c_str());
  ASSERT_TRUE(assertive_element);
  live_region = LiveRegion::GetLiveRegionForNode(assertive_element);
  ASSERT_TRUE(live_region);
  EXPECT_TRUE(live_region->IsAssertive());

  // aria-live="off"
  scoped_refptr<dom::Element> off_element =
      CreateLiveRegion(base::Tokens::off().c_str());
  ASSERT_TRUE(off_element);
  live_region = LiveRegion::GetLiveRegionForNode(off_element);
  EXPECT_FALSE(live_region);

  // aria-live=<invalid token>
  scoped_refptr<dom::Element> invalid_element = CreateLiveRegion("banana");
  ASSERT_TRUE(invalid_element);
  live_region = LiveRegion::GetLiveRegionForNode(invalid_element);
  EXPECT_FALSE(live_region);
}

TEST_F(LiveRegionTest, IsMutationRelevant) {
  scoped_refptr<dom::Element> live_region_element =
      CreateLiveRegion(base::Tokens::polite().c_str());
  ASSERT_TRUE(live_region_element);

  // GetLiveRegionForNode with the live region root.
  std::unique_ptr<LiveRegion> live_region =
      LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  // Default is that additions and text are relevant.
  EXPECT_TRUE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeAddition));
  EXPECT_TRUE(live_region->IsMutationRelevant(LiveRegion::kMutationTypeText));
  EXPECT_FALSE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeRemoval));

  // Only removals are relevant.
  live_region_element->SetAttribute(base::Tokens::aria_relevant().c_str(),
                                    "removals");
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_FALSE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeAddition));
  EXPECT_FALSE(live_region->IsMutationRelevant(LiveRegion::kMutationTypeText));
  EXPECT_TRUE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeRemoval));

  // Removals and additions are relevant.
  live_region_element->SetAttribute(base::Tokens::aria_relevant().c_str(),
                                    "removals additions");
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_TRUE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeAddition));
  EXPECT_FALSE(live_region->IsMutationRelevant(LiveRegion::kMutationTypeText));
  EXPECT_TRUE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeRemoval));

  // An invalid token.
  live_region_element->SetAttribute(base::Tokens::aria_relevant().c_str(),
                                    "text dog additions");
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_TRUE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeAddition));
  EXPECT_TRUE(live_region->IsMutationRelevant(LiveRegion::kMutationTypeText));
  EXPECT_FALSE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeRemoval));

  // "all" token.
  live_region_element->SetAttribute(base::Tokens::aria_relevant().c_str(),
                                    "all");
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_TRUE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeAddition));
  EXPECT_TRUE(live_region->IsMutationRelevant(LiveRegion::kMutationTypeText));
  EXPECT_TRUE(
      live_region->IsMutationRelevant(LiveRegion::kMutationTypeRemoval));
}

TEST_F(LiveRegionTest, IsAtomic) {
  std::unique_ptr<LiveRegion> live_region;

  scoped_refptr<dom::Element> live_region_element =
      CreateLiveRegion(base::Tokens::polite().c_str());
  ASSERT_TRUE(live_region_element);

  // Default is non-atomic.
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_FALSE(live_region->IsAtomic(live_region_element));

  // aria-atomic=true
  live_region_element->SetAttribute(base::Tokens::aria_atomic().c_str(),
                                    "true");
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_TRUE(live_region->IsAtomic(live_region_element));

  // aria-atomic=false
  live_region_element->SetAttribute(base::Tokens::aria_atomic().c_str(),
                                    "false");
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_FALSE(live_region->IsAtomic(live_region_element));

  // Get the value of aria-atomic from ancestor elements.
  live_region_element->SetAttribute(base::Tokens::aria_atomic().c_str(),
                                    "true");
  scoped_refptr<dom::Element> child = document()->CreateElement("div");
  live_region_element->AppendChild(child);
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_TRUE(live_region->IsAtomic(child));
  EXPECT_TRUE(live_region->IsAtomic(live_region_element));

  // Stop checking ancestors on the first aria-atomic attribute.
  live_region_element->SetAttribute(base::Tokens::aria_atomic().c_str(),
                                    "true");
  child->SetAttribute(base::Tokens::aria_atomic().c_str(), "false");
  live_region = LiveRegion::GetLiveRegionForNode(live_region_element);
  ASSERT_TRUE(live_region);
  EXPECT_FALSE(live_region->IsAtomic(child));
  EXPECT_TRUE(live_region->IsAtomic(live_region_element));
}
}  // namespace internal
}  // namespace accessibility
}  // namespace cobalt
