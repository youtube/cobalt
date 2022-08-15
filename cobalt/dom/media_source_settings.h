// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_MEDIA_SOURCE_SETTINGS_H_
#define COBALT_DOM_MEDIA_SOURCE_SETTINGS_H_

#include <string>

#include "base/optional.h"
#include "base/synchronization/lock.h"

namespace cobalt {
namespace dom {

// Holds WebModule wide settings for MediaSource related objects.
class MediaSourceSettings {
 public:
  // The return value will be used in `SourceBuffer::EvictCodedFrames()` to
  // allow it to evict extra data from the SourceBuffer, so it can reduce the
  // overall memory used by the underlying Demuxer implementation.
  virtual base::Optional<int> GetSourceBufferEvictExtraInBytes() const = 0;

 protected:
  MediaSourceSettings() = default;
  ~MediaSourceSettings() = default;

  MediaSourceSettings(const MediaSourceSettings&) = delete;
  MediaSourceSettings& operator=(const MediaSourceSettings&) = delete;
};

// Allows setting the values of MediaSource settings via a name and an int
// value.
// This class is thread safe.
class MediaSourceSettingsImpl : public MediaSourceSettings {
 public:
  base::Optional<int> GetSourceBufferEvictExtraInBytes() const override {
    base::AutoLock auto_lock(lock_);
    return source_buffer_evict_extra_in_bytes_;
  }

  // Returns true when the setting associated with `name` is set to `value`.
  // Returns false when `name` is not associated with any settings, or if
  // `value` contains an invalid value.
  bool Set(const std::string& name, int value);

 private:
  mutable base::Lock lock_;
  base::Optional<int> source_buffer_evict_extra_in_bytes_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_SETTINGS_H_
