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

#include "glimp/gles/unique_id_generator.h"

#include <algorithm>

#include "starboard/common/log.h"

namespace glimp {
namespace gles {

UniqueIdGenerator::UniqueIdGenerator() : largest_id_(0) {}

uint32_t UniqueIdGenerator::AcquireId() {
  if (reusable_ids_.empty()) {
    ++largest_id_;
    SB_DCHECK(largest_id_ != 0)
        << "An Id overflow occurred, too many Ids allocated.";
    return largest_id_;
  } else {
    uint32_t id = reusable_ids_.back();
    reusable_ids_.pop_back();
    return id;
  }
}

void UniqueIdGenerator::ReleaseId(uint32_t id) {
  SB_DCHECK(id <= largest_id_);
  SB_DCHECK(std::find(reusable_ids_.begin(), reusable_ids_.end(), id) ==
            reusable_ids_.end());
  reusable_ids_.push_back(id);
}

}  // namespace gles
}  // namespace glimp
