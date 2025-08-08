// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_BUBBLE_H_
#define ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_BUBBLE_H_

#include <memory>
#include <vector>

#include "ash/system/holding_space/holding_space_tray_child_bubble.h"

namespace ash {

// Child bubble of `HoldingSpaceTrayBubble` for recent files.
class RecentFilesBubble : public HoldingSpaceTrayChildBubble {
 public:
  explicit RecentFilesBubble(HoldingSpaceViewDelegate* delegate);
  RecentFilesBubble(const RecentFilesBubble& other) = delete;
  RecentFilesBubble& operator=(const RecentFilesBubble& other) = delete;
  ~RecentFilesBubble() override;

  // HoldingSpaceTrayChildBubble:
  const char* GetClassName() const override;
  std::unique_ptr<views::View> CreatePlaceholder() override;
  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> CreateSections()
      override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_BUBBLE_H_
