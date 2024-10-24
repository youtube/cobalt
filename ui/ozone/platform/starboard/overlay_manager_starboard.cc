// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/starboard/overlay_manager_starboard.h"

#include <memory>

#include "base/logging.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {
namespace {

class OverlayCandidatesStarboad : public OverlayCandidatesOzone {
 public:
  OverlayCandidatesStarboad() {}

  void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces) override {}
};

}  // namespace

OverlayManagerStarboad::OverlayManagerStarboad() {
  LOG(INFO) << "OverlayManagerStarboad::OverlayManagerStarboad";
}

OverlayManagerStarboad::~OverlayManagerStarboad() {
}

std::unique_ptr<OverlayCandidatesOzone>
OverlayManagerStarboad::CreateOverlayCandidates(gfx::AcceleratedWidget w) {
  LOG(INFO) << "OverlayManagerStarboad::CreateOverlayCandidates";
  return std::make_unique<OverlayCandidatesStarboad>();
}

}  // namespace ui
