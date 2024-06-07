// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
//
// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.cobalt.media;

import android.media.MediaFormat;
import java.nio.ByteBuffer;

class MediaFormatBuilder {
  public static void setCodecSpecificData(MediaFormat format, byte[][] csds) {
    // Codec Specific Data is set in the MediaFormat as ByteBuffer entries with keys csd-0,
    // csd-1, and so on. See:
    // http://developer.android.com/reference/android/media/MediaCodec.html for details.
    for (int i = 0; i < csds.length; ++i) {
      if (csds[i].length == 0) continue;
      String name = "csd-" + i;
      format.setByteBuffer(name, ByteBuffer.wrap(csds[i]));
    }
  }
}
;
