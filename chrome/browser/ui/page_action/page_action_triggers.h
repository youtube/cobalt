// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_TRIGGERS_H_
#define CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_TRIGGERS_H_

#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/base/class_property.h"

namespace page_actions {
using PageActionTrigger = ::toolbar_ui_api::mojom::PageActionTrigger;

constexpr std::underlying_type_t<page_actions::PageActionTrigger>
    kInvalidPageActionTrigger = -1;

enum class PageActionEntryPoint {
  kSuggestionChip = 0,
  kAnchoredMessage = 1,
};

constexpr std::underlying_type_t<page_actions::PageActionEntryPoint>
    kInvalidPageActionEntryPoint = -1;

extern const ui::ClassProperty<std::underlying_type_t<PageActionTrigger>>* const
    kPageActionTriggerKey;
extern const ui::ClassProperty<
    std::underlying_type_t<PageActionEntryPoint>>* const
    kPageActionEntryPointKey;

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_TRIGGERS_H_
