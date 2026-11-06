// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_BASE_STARBOARD_URL_PLAYER_MEDIA_RESOURCE_H_
#define MEDIA_BASE_STARBOARD_URL_PLAYER_MEDIA_RESOURCE_H_

#include <cstdint>
#include <vector>

#include "base/time/time.h"
#include "media/base/demuxer.h"
#include "media/base/eme_constants.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace media {

// A media resource that is rendered by the platform URL player. It receives
// the events that only the platform player knows.
//
// UrlPlayerDemuxer implements it, and DemuxerManager owns the demuxer.
// StarboardRendererClient keeps a raw pointer obtained through
// MediaResource::AsUrlPlayerMediaResource(); like the MediaResource it comes
// from, it must outlive the renderer. The OnPlatform*() methods are called on
// the media thread.
class UrlPlayerMediaResource {
 public:
  virtual GURL GetMediaUrl() const = 0;

  virtual void OnPlatformMetadata(base::TimeDelta duration,
                                  const gfx::Size& natural_size) = 0;

  virtual void OnPlatformDurationChange(base::TimeDelta duration) = 0;
  virtual void OnPlatformBufferedRangesChange(base::TimeDelta start,
                                              base::TimeDelta length) = 0;
  virtual void OnPlatformEncryptedMediaInitData(
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data) = 0;

  // Sets the callback that fires the EME `encrypted` event.
  virtual void SetEncryptedMediaInitDataCB(
      Demuxer::EncryptedMediaInitDataCB cb) = 0;

 protected:
  virtual ~UrlPlayerMediaResource() = default;
};

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_URL_PLAYER_MEDIA_RESOURCE_H_
