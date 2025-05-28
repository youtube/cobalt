// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_MOCK_PAGE_ACTION_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_MOCK_PAGE_ACTION_MODEL_H_

#include <memory>

#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace page_actions {

class MockPageActionModel : public PageActionModelInterface {
 public:
  MockPageActionModel();
  ~MockPageActionModel() override;

  MOCK_METHOD(bool, GetVisible, (), (const, override));
  MOCK_METHOD(bool, GetShowSuggestionChip, (), (const, override));
  MOCK_METHOD(const std::u16string, GetText, (), (const, override));
  MOCK_METHOD(const std::u16string, GetTooltipText, (), (const, override));
  MOCK_METHOD(const ui::ImageModel&, GetImage, (), (const, override));
  MOCK_METHOD(void,
              AddObserver,
              (PageActionModelObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (PageActionModelObserver * observer),
              (override));
  MOCK_METHOD(void,
              SetActionItemProperties,
              (base::PassKey<PageActionController>,
               const actions::ActionItem* action_item),
              (override));
  MOCK_METHOD(void,
              SetShowRequested,
              (base::PassKey<PageActionController>, bool requested),
              (override));
  MOCK_METHOD(void,
              SetShowSuggestionChip,
              (base::PassKey<PageActionController>, bool show),
              (override));
  MOCK_METHOD(void,
              SetHasPinnedIcon,
              (base::PassKey<PageActionController>, bool has_pinned_icon),
              (override));
  MOCK_METHOD(void,
              SetTabActive,
              (base::PassKey<PageActionController>, bool is_active),
              (override));
  MOCK_METHOD(void,
              SetOverrideText,
              (base::PassKey<PageActionController>,
               const std::optional<std::u16string>& text),
              (override));
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_MOCK_PAGE_ACTION_MODEL_H_
