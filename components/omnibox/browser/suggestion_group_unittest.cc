// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group_util.h"

#include "testing/gtest/include/gtest/gtest.h"

// Ensures that accessing unset fields is safe and verifies the default values.
// https://developers.google.com/protocol-buffers/docs/reference/cpp-generated
TEST(SuggestionGroupTest, DefaultValuesForUnsetFields) {
  omnibox::GroupConfig group_config;

  ASSERT_FALSE(group_config.has_header_text());
  ASSERT_EQ("", group_config.header_text());

  ASSERT_FALSE(group_config.has_side_type());
  ASSERT_EQ(omnibox::GroupConfig_SideType_DEFAULT_PRIMARY,
            group_config.side_type());

  ASSERT_FALSE(group_config.has_render_type());
  ASSERT_EQ(omnibox::GroupConfig_RenderType_DEFAULT_VERTICAL,
            group_config.render_type());

  ASSERT_FALSE(group_config.has_visibility());
  ASSERT_EQ(omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
            group_config.visibility());

  ASSERT_FALSE(group_config.has_why_this_result_reason());
  ASSERT_EQ(0U, group_config.why_this_result_reason());

  ASSERT_FALSE(group_config.has_section());
  ASSERT_EQ(omnibox::SECTION_DEFAULT, group_config.section());
}

// Ensures that omnibox::GroupIdForNumber() returns the omnibox::GroupId enum
// object corresponding to the given value; and returns omnibox::GROUP_INVALID
// when there is no corresponding enum object.
TEST(SuggestionGroupTest, GroupIdForNumber) {
  ASSERT_EQ(omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
            omnibox::GroupIdForNumber(40000));
  ASSERT_EQ(omnibox::GROUP_INVALID, omnibox::GroupIdForNumber(-1));
  ASSERT_EQ(omnibox::GROUP_INVALID, omnibox::GroupIdForNumber(0));
  ASSERT_EQ(omnibox::GROUP_INVALID, omnibox::GroupIdForNumber(123456789));
}
