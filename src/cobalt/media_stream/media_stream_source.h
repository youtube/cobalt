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

#ifndef COBALT_MEDIA_STREAM_MEDIA_STREAM_SOURCE_H_
#define COBALT_MEDIA_STREAM_MEDIA_STREAM_SOURCE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cobalt/media_stream/media_stream_track.h"

namespace cobalt {
namespace media_stream {

class MediaStreamSource : public base::RefCounted<MediaStreamSource> {
 public:
  using ReadyState = ::cobalt::media_stream::MediaStreamTrack::ReadyState;

  MediaStreamSource() = default;
  virtual ~MediaStreamSource();

  void StopSource();
  // Only one call back can be set at a time.  Stopping a source will
  // call the |stop_callback| if set, and then clear it.
  void SetStopCallback(const base::Closure& stop_callback);

 protected:
  // Called from StopSource(). Derived classes can implement their own
  // logic here when StopSource is called.
  virtual void DoStopSource() = 0;
  // Calls |stop_callback| if it is set.
  void FinalizeStopSource();

 private:
  MediaStreamSource(const MediaStreamSource&) = delete;
  MediaStreamSource& operator=(const MediaStreamSource&) = delete;

  THREAD_CHECKER(thread_checker_);
  base::Closure stop_callback_;
};

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_MEDIA_STREAM_SOURCE_H_
