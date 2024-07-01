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
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_
#define COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_

#include "base/memory/weak_ptr.h"
#include "cobalt/dom/audio_track_list.h"
#include "cobalt/dom/media_source_attachment.h"
#include "cobalt/dom/video_track_list.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace dom {

// Extension of the core MediaSourceAttachment interface. Includes extra
// interface methods used by concrete attachments to communicate with the media
// element.
class MediaSourceAttachmentSupplement
    : public MediaSourceAttachment,
      public base::SupportsWeakPtr<MediaSourceAttachmentSupplement> {
 public:
  MediaSourceAttachmentSupplement() = default;
  ~MediaSourceAttachmentSupplement() = default;

  // Communicates a change in the media resource duration to the attached media
  // element. In a same-thread attachment, communicates this information
  // synchronously.
  // Same-thread synchronous notification here is primarily to preserve
  // compliance of API behavior when not using MSE-in-Worker (setting
  // MediaSource.duration should be synchronously in agreement with subsequent
  // retrieval of MediaElement.duration, all on the main thread).
  virtual void NotifyDurationChanged(double duration) = 0;

  // Retrieves whether or not the media element has max video capabilities.
  // Implementations may chose to either directly, synchronously consult the
  // attached media element or rely on the element to correctly pump its
  // capabilities to this attachment (in a cross-thread implementation).
  virtual bool HasMaxVideoCapabilities() const = 0;

  // Retrieves the current (or a recent) media element time. Implementations may
  // choose to either directly, synchronously consult the attached media element
  // or rely on a "recent" time pumped by the attached element via the
  // MediaSourceAttachment interface (in a cross-thread implementation).
  virtual double GetRecentMediaTime() = 0;

  // Retrieves whether or not the media element currently has an error.
  // Implementations may choose to either directly, synchronously consult the
  // attached media element or rely on the element to correctly pump when it
  // has an error to this attachment (in a cross-thread implementation).
  virtual bool GetElementError() = 0;

  // Construct track lists for use by a SourceBuffer.
  virtual scoped_refptr<AudioTrackList> CreateAudioTrackList(
      script::EnvironmentSettings* settings) = 0;
  virtual scoped_refptr<VideoTrackList> CreateVideoTrackList(
      script::EnvironmentSettings* settings) = 0;

  // TODO: b/338425449 - Remove media_element method after rollout.
  // References to underlying objects are exposed for when the H5VCC
  // flag MediaElement.EnableUsingMediaSourceAttachmentMethods is disabled.
  virtual base::WeakPtr<HTMLMediaElement> media_element() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaSourceAttachmentSupplement);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_
