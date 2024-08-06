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

#ifndef COBALT_MEDIA_BASE_MEDIA_LOG_HOLDER_H_
#define COBALT_MEDIA_BASE_MEDIA_LOG_HOLDER_H_

#include "media/base/media_log.h"
#include "third_party/chromium/media/base/media_log.h"

namespace cobalt {
namespace media {

class MediaLogHolder {
 public:
  MediaLogHolder() = default;

  template <::media::MediaLogEvent media_event_log>
  void AddEvent() {}

  template <::media::MediaLogEvent media_event_log, typename EventParam>
  void AddEvent(const EventParam&) {}

  template <typename ErrorType>
  void NotifyError(ErrorType error) {}

  template <typename MediaLogType>
  MediaLogType As();

  template <>
  ::media_m96::MediaLog* As<::media_m96::MediaLog*>() {
    return &media_log_m96_;
  }

  template <>
  ::media::MediaLog* As<::media::MediaLog*>() {
    return &media_log_m114_;
  }

 private:
  ::media_m96::MediaLog media_log_m96_;
  ::media::MediaLog media_log_m114_;

  DISALLOW_COPY_AND_ASSIGN(MediaLogHolder);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_MEDIA_LOG_HOLDER_H_
