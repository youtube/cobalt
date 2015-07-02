/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "media/filters/shell_ffmpeg.h"

namespace media {

namespace {
bool ffmpeg_initialized = false;
}  // namespace

void EnsureFfmpegInitialized() {
  if (!ffmpeg_initialized) {
    av_register_all();
    ffmpeg_initialized = true;
  }
}

}  // namespace media
