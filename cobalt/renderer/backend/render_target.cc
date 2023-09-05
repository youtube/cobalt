// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/backend/render_target.h"

#include <stdatomic.h>

namespace cobalt {
namespace renderer {
namespace backend {

atomic_int_least32_t RenderTarget::serial_counter_ = 1;

RenderTarget::RenderTarget() {
  serial_number_ = atomic_fetch_add(&serial_counter_, 1);
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
