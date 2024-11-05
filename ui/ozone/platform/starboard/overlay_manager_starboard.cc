// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

OverlayManagerStarboad::OverlayManagerStarboad() {}

OverlayManagerStarboad::~OverlayManagerStarboad() {}

std::unique_ptr<OverlayCandidatesOzone>
OverlayManagerStarboad::CreateOverlayCandidates(gfx::AcceleratedWidget widget) {
  return std::make_unique<OverlayCandidatesStarboad>();
}

}  // namespace ui
