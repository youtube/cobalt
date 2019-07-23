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

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_DISPMANX_RESOURCE_POOL_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_DISPMANX_RESOURCE_POOL_H_

#include <queue>

#include "starboard/common/mutex.h"
#include "starboard/common/ref_counted.h"
#include "starboard/raspi/shared/dispmanx_util.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

class DispmanxResourcePool : public RefCountedThreadSafe<DispmanxResourcePool> {
 public:
  explicit DispmanxResourcePool(size_t max_number_of_resources);
  ~DispmanxResourcePool();

  DispmanxYUV420Resource* Alloc(int width,
                                int height,
                                int visible_width,
                                int visible_height);
  void Free(DispmanxYUV420Resource* resource);

  static void DisposeDispmanxYUV420Resource(void* context,
                                            void* dispmanx_resource);

 private:
  typedef std::queue<DispmanxYUV420Resource*> ResourceQueue;

  const size_t max_number_of_resources_;

  Mutex mutex_;
  size_t number_of_resources_;
  int last_frame_width_;
  int last_frame_height_;
  ResourceQueue free_resources_;
};

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_DISPMANX_RESOURCE_POOL_H_
