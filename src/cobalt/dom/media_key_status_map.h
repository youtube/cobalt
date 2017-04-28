// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_MEDIA_KEY_STATUS_MAP_H_
#define COBALT_DOM_MEDIA_KEY_STATUS_MAP_H_

#include "base/basictypes.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/array_buffer_view.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class MediaKeyStatusMap : public script::Wrappable {
 public:
  typedef script::UnionType2<scoped_refptr<ArrayBufferView>,
                             scoped_refptr<ArrayBuffer> > BufferSource;
  uint32_t size() const { return 0; }
  bool Has(BufferSource keyId) const { return false; }
  const script::ValueHandleHolder* Get(BufferSource keyId) const {
    return NULL;
  }

  DEFINE_WRAPPABLE_TYPE(MediaKeyStatusMap);

 private:
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_KEY_STATUS_MAP_H_
