// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_SCREEN_CAPTURES_SECTION_H_
#define ASH_SYSTEM_HOLDING_SPACE_SCREEN_CAPTURES_SECTION_H_

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/system/holding_space/holding_space_item_views_section.h"

namespace ash {

// Section for screen captures in the `RecentFilesBubble`.
class ScreenCapturesSection : public HoldingSpaceItemViewsSection {
 public:
  explicit ScreenCapturesSection(HoldingSpaceViewDelegate* delegate);
  ScreenCapturesSection(const ScreenCapturesSection& other) = delete;
  ScreenCapturesSection& operator=(const ScreenCapturesSection& other) = delete;
  ~ScreenCapturesSection() override;

  // HoldingSpaceItemViewsSection:
  const char* GetClassName() const override;
  std::unique_ptr<views::View> CreateHeader() override;
  std::unique_ptr<views::View> CreateContainer() override;
  std::unique_ptr<HoldingSpaceItemView> CreateView(
      const HoldingSpaceItem* item) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_SCREEN_CAPTURES_SECTION_H_
