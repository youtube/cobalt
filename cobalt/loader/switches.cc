// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/switches.h"

namespace cobalt {
namespace loader {
namespace switches {

// Set this argument to "true" will allow image decoders produce output in multi
// plane images when decoding to multi plane image is more efficient.  Set it to
// "false" and the image decoders will always produce output in single plane
// RGBA format.
// It is default to "false" when the command line argument is not specified,
// because currently the renderer cannot render multi plane images as efficient
// as rendering single plane RGBA images.  This behavior may change in future.
const char kAllowImageDecodingToMultiPlane[] =
    "allow_image_decoding_to_multi_plane";

}  // namespace switches
}  // namespace loader
}  // namespace cobalt
