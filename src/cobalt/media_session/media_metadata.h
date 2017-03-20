// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SESSION_MEDIA_METADATA_H_
#define COBALT_MEDIA_SESSION_MEDIA_METADATA_H_

#include <string>

#include "cobalt/media_session/media_metadata_init.h"

namespace cobalt {
namespace media_session {

class MediaMetadata : public script::Wrappable {
 public:
  MediaMetadata() {}
  explicit MediaMetadata(const MediaMetadataInit& init) {
    if (init.has_title()) {
      title_ = init.title();
    }

    if (init.has_artist()) {
      artist_ = init.artist();
    }

    if (init.has_album()) {
      album_ = init.album();
    }
  }

  scoped_refptr<MediaMetadata> metadata() {
    return scoped_refptr<MediaMetadata>();
  }

  std::string title() const { return title_; }

  void set_title(const std::string& value) { title_ = value; }

  std::string artist() const { return artist_; }

  void set_artist(const std::string& value) { artist_ = value; }

  std::string album() const { return album_; }

  void set_album(const std::string& value) { album_ = value; }

  DEFINE_WRAPPABLE_TYPE(MediaMetadata);

 private:
  std::string title_;
  std::string artist_;
  std::string album_;

  DISALLOW_COPY_AND_ASSIGN(MediaMetadata);
};

}  // namespace media_session
}  // namespace cobalt
#endif  // COBALT_MEDIA_SESSION_MEDIA_METADATA_H_
