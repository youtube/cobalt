/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/renderer/backend/default_graphics_system.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/system_window/win/system_window.h"

namespace cobalt {
namespace renderer {
namespace backend {

scoped_ptr<GraphicsSystem> CreateDefaultGraphicsSystem() {
  return scoped_ptr<GraphicsSystem>(new GraphicsSystemEGL());
}

EGLNativeWindowType GetHandleFromSystemWindow(
    system_window::SystemWindow* system_window) {
  // The assumption here is that because we are in the Windows-specific
  // implementation of this function, the passed-in SystemWindow pointer will
  // be the Windows-specific subclass.
  system_window::SystemWindowWin* system_window_win =
      base::polymorphic_downcast<system_window::SystemWindowWin*>(
          system_window);
  return system_window_win->window_handle();
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
