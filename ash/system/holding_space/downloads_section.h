// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_DOWNLOADS_SECTION_H_
#define ASH_SYSTEM_HOLDING_SPACE_DOWNLOADS_SECTION_H_

#include <memory>

#include "ash/system/holding_space/holding_space_item_views_section.h"

namespace ash {

// Section for downloads in the `RecentFilesBubble`.
class DownloadsSection : public HoldingSpaceItemViewsSection {
 public:
  explicit DownloadsSection(HoldingSpaceViewDelegate* delegate);
  DownloadsSection(const DownloadsSection& other) = delete;
  DownloadsSection& operator=(const DownloadsSection& other) = delete;
  ~DownloadsSection() override;

  // HoldingSpaceItemViewsSection:
  const char* GetClassName() const override;
  std::unique_ptr<views::View> CreateHeader() override;
  std::unique_ptr<views::View> CreateContainer() override;
  std::unique_ptr<HoldingSpaceItemView> CreateView(
      const HoldingSpaceItem* item) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_DOWNLOADS_SECTION_H_
