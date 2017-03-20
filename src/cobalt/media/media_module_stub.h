// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_MEDIA_MODULE_STUB_H_
#define COBALT_MEDIA_MEDIA_MODULE_STUB_H_

#include <string>

#include "cobalt/media/media_module.h"

namespace cobalt {
namespace media {

class MediaModuleStub : public MediaModule {
 public:
  MediaModuleStub() : MediaModule(math::Size(1920, 1080)) {}

  std::string CanPlayType(const std::string& mime_type,
                          const std::string& key_system) OVERRIDE;
  scoped_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      ::media::WebMediaPlayerClient* client) OVERRIDE;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_MEDIA_MODULE_STUB_H_
