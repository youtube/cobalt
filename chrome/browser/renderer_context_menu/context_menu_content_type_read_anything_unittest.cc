// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type_read_anything.h"

#include <memory>

#include "components/renderer_context_menu/context_menu_content_type.h"
#include "content/public/browser/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class ContextMenuContentTypeReadAnythingTest : public testing::Test {
 public:
  ContextMenuContentTypeReadAnythingTest() = default;
  ~ContextMenuContentTypeReadAnythingTest() override = default;
};

TEST_F(ContextMenuContentTypeReadAnythingTest, SupportsGroup) {
  content::ContextMenuParams params;
  params.page_url =
      GURL("chrome-untrusted://read-anything-side-panel.top-chrome/");

  auto content_type =
      std::make_unique<ContextMenuContentTypeReadAnything>(params);

  // Disallowed groups should always return false.
  EXPECT_FALSE(
      content_type->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE));
  EXPECT_FALSE(
      content_type->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT));

  // ITEM_GROUP_COPY: Requires selection and not editable.
  params.selection_text = u"some text";
  params.is_editable = false;
  content_type = std::make_unique<ContextMenuContentTypeReadAnything>(params);
  EXPECT_TRUE(
      content_type->SupportsGroup(ContextMenuContentType::ITEM_GROUP_COPY));

  // ITEM_GROUP_LINK: Requires unfiltered_link_url.
  params.unfiltered_link_url = GURL("http://example.com");
  content_type = std::make_unique<ContextMenuContentTypeReadAnything>(params);
  EXPECT_TRUE(
      content_type->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK));

  // ITEM_GROUP_SEARCH_PROVIDER: Requires selection.
  params.selection_text = u"some text";
  content_type = std::make_unique<ContextMenuContentTypeReadAnything>(params);
  EXPECT_TRUE(content_type->SupportsGroup(
      ContextMenuContentType::ITEM_GROUP_SEARCH_PROVIDER));

  // ITEM_GROUP_DEVELOPER: Always true.
  EXPECT_TRUE(content_type->SupportsGroup(
      ContextMenuContentType::ITEM_GROUP_DEVELOPER));
}

}  // namespace
