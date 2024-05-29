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

#ifndef COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_
#define COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_

#include "base/memory/weak_ptr.h"
#include "cobalt/dom/audio_track_list.h"
#include "cobalt/dom/media_source_attachment.h"
#include "cobalt/dom/video_track_list.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace dom {

class MediaSourceAttachmentSupplement
    : public MediaSourceAttachment,
      public base::SupportsWeakPtr<MediaSourceAttachmentSupplement> {
 public:
  MediaSourceAttachmentSupplement() = default;
  ~MediaSourceAttachmentSupplement() = default;

  virtual void DurationChanged(double duration) = 0;
  virtual bool HasMaxVideoCapabilities() const = 0;
  virtual double GetRecentMediaTime() = 0;
  virtual bool GetElementError() = 0;
  virtual scoped_refptr<AudioTrackList> CreateAudioTrackList(
      script::EnvironmentSettings* settings) = 0;
  virtual scoped_refptr<VideoTrackList> CreateVideoTrackList(
      script::EnvironmentSettings* settings) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaSourceAttachmentSupplement);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_
