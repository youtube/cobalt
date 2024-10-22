// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_STARBOARD_OVERLAY_MANAGER_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_OVERLAY_MANAGER_STARBOARD_H_

#include <memory>

#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {

class OverlayManagerStarboad : public OverlayManagerOzone {
 public:
  OverlayManagerStarboad();

  OverlayManagerStarboad(const OverlayManagerStarboad&) = delete;
  OverlayManagerStarboad& operator=(const OverlayManagerStarboad&) = delete;

  ~OverlayManagerStarboad() override;

  // OverlayManagerOzone:
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_OVERLAY_MANAGER_STARBOARD_H_
