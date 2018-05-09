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

#include "cobalt/media_capture/media_recorder.h"

namespace cobalt {
namespace media_capture {

void MediaRecorder::Start(int32 timeslice) {
  UNREFERENCED_PARAMETER(timeslice);
  NOTIMPLEMENTED();
}

void MediaRecorder::Stop() { NOTIMPLEMENTED(); }

bool MediaRecorder::IsTypeSupported(const base::StringPiece mime_type) {
  UNREFERENCED_PARAMETER(mime_type);
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media_capture
}  // namespace cobalt
