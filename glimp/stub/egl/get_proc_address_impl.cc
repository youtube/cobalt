/*
 * Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "glimp/egl/get_proc_address_impl.h"

#include "glimp/egl/display_registry.h"
#include "glimp/egl/scoped_egl_lock.h"
#include "glimp/polymorphic_downcast.h"
#include "glimp/stub/egl/display_impl.h"
#include "starboard/common/string.h"

namespace glimp {
namespace egl {

MustCastToProperFunctionPointerType GetProcAddressImpl(const char* procname) {
  return NULL;
}

}  // namespace egl
}  // namespace glimp
