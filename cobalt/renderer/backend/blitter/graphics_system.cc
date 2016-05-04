/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/backend/blitter/graphics_system.h"

#include "cobalt/renderer/backend/blitter/texture_data.h"

#include "cobalt/renderer/backend/blitter/display.h"
#include "cobalt/renderer/backend/blitter/graphics_context.h"
#include "cobalt/renderer/backend/blitter/texture.h"

namespace cobalt {
namespace renderer {
namespace backend {

GraphicsSystemBlitter::GraphicsSystemBlitter() {
  device_ = SbBlitterCreateDefaultDevice();
}

GraphicsSystemBlitter::~GraphicsSystemBlitter() {
  SbBlitterDestroyDevice(device_);
}

scoped_ptr<Display> GraphicsSystemBlitter::CreateDisplay(
    system_window::SystemWindow* system_window) {
  return scoped_ptr<Display>(new DisplayBlitter(device_, system_window));
}

scoped_ptr<GraphicsContext> GraphicsSystemBlitter::CreateGraphicsContext() {
  return scoped_ptr<GraphicsContext>(new GraphicsContextBlitter(this));
}

scoped_ptr<TextureData> GraphicsSystemBlitter::AllocateTextureData(
    const SurfaceInfo& surface_info) {
  return scoped_ptr<TextureData>(
      new TextureDataBlitter(device_, surface_info.size, surface_info.format));
}

scoped_ptr<RawTextureMemory> GraphicsSystemBlitter::AllocateRawTextureMemory(
    size_t size_in_bytes, size_t alignment) {
  NOTREACHED();
  return scoped_ptr<RawTextureMemory>();
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
