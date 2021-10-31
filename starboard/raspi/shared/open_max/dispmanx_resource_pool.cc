// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/raspi/shared/open_max/dispmanx_resource_pool.h"

#include "starboard/configuration.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

DispmanxResourcePool::DispmanxResourcePool(size_t max_number_of_resources)
    : max_number_of_resources_(max_number_of_resources),
      number_of_resources_(0),
      last_frame_width_(0),
      last_frame_height_(0) {}

DispmanxResourcePool::~DispmanxResourcePool() {
  while (!free_resources_.empty()) {
    delete free_resources_.front();
    free_resources_.pop();
    --number_of_resources_;
  }
  SB_DCHECK(number_of_resources_ == 0) << number_of_resources_;
}

DispmanxYUV420Resource* DispmanxResourcePool::Alloc(int width,
                                                    int height,
                                                    int visible_width,
                                                    int visible_height) {
  ScopedLock scoped_lock(mutex_);

  if (last_frame_width_ != width || last_frame_height_ != height) {
    while (!free_resources_.empty()) {
      delete free_resources_.front();
      free_resources_.pop();
      --number_of_resources_;
    }
  }

  last_frame_width_ = width;
  last_frame_height_ = height;

  if (!free_resources_.empty()) {
    DispmanxYUV420Resource* resource = free_resources_.front();
    free_resources_.pop();
    return resource;
  }

  if (number_of_resources_ >= max_number_of_resources_) {
    return NULL;
  }

  ++number_of_resources_;
  return new DispmanxYUV420Resource(width, height, visible_width,
                                    visible_height);
}

void DispmanxResourcePool::Free(DispmanxYUV420Resource* resource) {
  ScopedLock scoped_lock(mutex_);
  if (resource->width() != last_frame_width_ ||
      resource->height() != last_frame_height_) {
    // The video has adapted, free the resource as it won't be reused any soon.
    delete resource;
    --number_of_resources_;
    return;
  }
  free_resources_.push(resource);
}

// static
void DispmanxResourcePool::DisposeDispmanxYUV420Resource(
    void* context,
    void* dispmanx_resource) {
  SB_DCHECK(context != NULL);
  SB_DCHECK(dispmanx_resource != NULL);
  DispmanxResourcePool* pool = reinterpret_cast<DispmanxResourcePool*>(context);
  pool->Free(reinterpret_cast<DispmanxYUV420Resource*>(dispmanx_resource));
  pool->Release();
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
