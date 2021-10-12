// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_CONTRIB_STADIA_X11_APPLICATION_STADIA_X11_H_
#define STARBOARD_CONTRIB_STADIA_X11_APPLICATION_STADIA_X11_H_

#include "starboard/shared/x11/application_x11.h"
#include "starboard/window.h"

namespace starboard {
namespace contrib {
namespace stadia {
namespace x11 {

// This application engine combines the generic queue with the X11 event queue.
class ApplicationStadiaX11 : public starboard::shared::x11::ApplicationX11 {
 public:
  SbWindow CreateWindow(const SbWindowOptions* options) override;
};

}  // namespace x11
}  // namespace stadia
}  // namespace contrib
}  // namespace starboard

#endif  // STARBOARD_CONTRIB_STADIA_X11_APPLICATION_STADIA_X11_H_
