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

#include "cobalt/dom/eme/media_key_system_access.h"

#include "cobalt/dom/eme/media_keys.h"
#include "cobalt/script/script_value_factory.h"

namespace cobalt {
namespace dom {
namespace eme {

MediaKeySystemAccess::MediaKeySystemAccess(
    const std::string& key_system,
    const MediaKeySystemConfiguration& configuration,
    script::ScriptValueFactory* script_value_factory)
    : key_system_(key_system),
      configuration_(configuration),
      script_value_factory_(script_value_factory) {}

// See
// https://www.w3.org/TR/encrypted-media/#dom-mediakeysystemaccess-createmediakeys.
script::Handle<MediaKeySystemAccess::InterfacePromise>
MediaKeySystemAccess::CreateMediaKeys() const {
  // 1. Let promise be a new promise.
  script::Handle<MediaKeySystemAccess::InterfacePromise> promise =
      script_value_factory_->CreateInterfacePromise<scoped_refptr<MediaKeys>>();

  // 2.10. Let media keys be a new MediaKeys object.
  scoped_refptr<MediaKeys> media_keys(
      new MediaKeys(key_system_, script_value_factory_));

  // 2.11. Resolve promise with media keys.
  promise->Resolve(media_keys);
  return promise;
}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt
