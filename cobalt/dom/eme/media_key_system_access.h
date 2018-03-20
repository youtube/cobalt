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

#ifndef COBALT_DOM_EME_MEDIA_KEY_SYSTEM_ACCESS_H_
#define COBALT_DOM_EME_MEDIA_KEY_SYSTEM_ACCESS_H_

#include <string>

#include "cobalt/dom/eme/media_key_system_configuration.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {
namespace eme {

// Provides access to a key system.
//   https://www.w3.org/TR/encrypted-media/#mediakeysystemaccess-interface
class MediaKeySystemAccess : public script::Wrappable {
 public:
  using InterfacePromise = script::Promise<scoped_refptr<script::Wrappable>>;

  // Custom, not in any spec.
  MediaKeySystemAccess(const std::string& key_system,
                       const MediaKeySystemConfiguration& configuration,
                       script::ScriptValueFactory* script_value_factory);

  // Web API: MediaKeySystemAccess.
  const std::string& key_system() const { return key_system_; }
  const MediaKeySystemConfiguration& GetConfiguration() const {
    return configuration_;
  }
  script::Handle<InterfacePromise> CreateMediaKeys() const;

  DEFINE_WRAPPABLE_TYPE(MediaKeySystemAccess);

 private:
  ~MediaKeySystemAccess() override {}

  const std::string key_system_;
  const MediaKeySystemConfiguration configuration_;
  script::ScriptValueFactory* script_value_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaKeySystemAccess);
};

}  // namespace eme
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EME_MEDIA_KEY_SYSTEM_ACCESS_H_
