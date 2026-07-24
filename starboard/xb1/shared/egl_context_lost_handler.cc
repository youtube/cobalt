// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/xb1/shared/egl_context_lost_handler.h"

#include "starboard/extension/egl_context_lost_handler.h"
#include "starboard/shared/uwp/extended_resources_manager.h"

using starboard::shared::uwp::ExtendedResourcesManager;

namespace starboard {
namespace xb1 {
namespace shared {

namespace {

void HandleEglContextLost() {
  ExtendedResourcesManager::GetInstance()->OnDeviceLost();
}

const CobaltExtensionEglContextLostHandlerApi kEglContextLostHandlerApi = {
    kCobaltExtensionEglContextLostHandlerName,
    1,
    &HandleEglContextLost,
};

}  // namespace

const void* GetEglContextLostHandlerApi() {
  return &kEglContextLostHandlerApi;
}

}  // namespace shared
}  // namespace xb1
}  // namespace starboard
