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

#ifndef STARBOARD_CONTRIB_STADIA_STADIA_INTERFACE_H_
#define STARBOARD_CONTRIB_STADIA_STADIA_INTERFACE_H_

#include "clients/vendor/public/stadia_lifecycle.h"
#include "clients/vendor/public/stadia_plugin.h"

namespace starboard {
namespace contrib {
namespace stadia {

struct StadiaInterface {
  StadiaPluginHasFunction StadiaPluginHas;
  StadiaPluginOpenFunction StadiaPluginOpen;
  StadiaPluginSendToFunction StadiaPluginSendTo;
  StadiaPluginCloseFunction StadiaPluginClose;

  StadiaInitializeFunction StadiaInitialize;

  StadiaInterface();
};

}  // namespace stadia
}  // namespace contrib
}  // namespace starboard

extern const starboard::contrib::stadia::StadiaInterface* g_stadia_interface;

#endif  // STARBOARD_CONTRIB_STADIA_STADIA_INTERFACE_H_
