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

#include "cobalt/media_stream/media_stream_source.h"

#include "base/callback_helpers.h"

namespace cobalt {
namespace media_stream {

MediaStreamSource::~MediaStreamSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(stop_callback_.is_null());
}

void MediaStreamSource::StopSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DoStopSource();
  FinalizeStopSource();
}

void MediaStreamSource::SetStopCallback(const base::Closure& stop_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(stop_callback_.is_null());
  stop_callback_ = stop_callback;
}

void MediaStreamSource::FinalizeStopSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ResetAndRunIfNotNull(&stop_callback_);
}

}  // namespace media_stream
}  // namespace cobalt
