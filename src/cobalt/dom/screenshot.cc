// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/screenshot.h"

#include "base/string_piece.h"
#include "third_party/modp_b64/modp_b64.h"

namespace cobalt {
namespace dom {

Screenshot::Screenshot(scoped_refptr<ArrayBuffer> pixel_data)
    : pixel_data_(pixel_data) {
  DCHECK(pixel_data);
}

}  // namespace dom
}  // namespace cobalt
