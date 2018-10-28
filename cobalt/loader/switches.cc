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

// By default the image decoders produce output in whichever format that is
// considered to be efficient.  In most cases this means the output will be
// multi plane images, like kMultiPlaneImageFormatYUV3PlaneBT601FullRange.
// Pass the following command line argument to ensure that the decoding output
// is in single plane formats, like RGBA or BGRA.  This usually happens when
// there is no native multi plane image support on the platform, or when the
// multi plane image support is not efficient enough.
// Note that currently the decoding to single plane image is automatically
// enforced on blitter platforms.
const char kForceImageDecodingToSinglePlane[] =
    "force_image_decoding_to_single_plane";

}  // namespace switches
}  // namespace loader
}  // namespace cobalt
