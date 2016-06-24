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

#ifndef GLIMP_GLES_UNIQUE_ID_GENERATOR_H_
#define GLIMP_GLES_UNIQUE_ID_GENERATOR_H_

#include <KHR/khrplatform.h>

#include <vector>

namespace glimp {
namespace gles {

// A helper class to make it easy to obtain a unique Id (that we can assign
// to GL resources).  In order to aid with debugging, it will never return
// 0 as a valid Id.
class UniqueIdGenerator {
 public:
  UniqueIdGenerator();
  uint32_t AcquireId();
  void ReleaseId(uint32_t id);

 private:
  std::vector<uint32_t> reusable_ids_;
  uint32_t largest_id_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_UNIQUE_ID_GENERATOR_H_
