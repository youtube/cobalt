// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/eme/media_keys.h"
#include "cobalt/media/base/drm_system.h"
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
MediaKeySystemAccess::CreateMediaKeys(
    script::EnvironmentSettings* settings) const {
  // 1. Let promise be a new promise.
  script::Handle<MediaKeySystemAccess::InterfacePromise> promise =
      script_value_factory_->CreateInterfacePromise<scoped_refptr<MediaKeys>>();

  // 2.5. Let instance be a new instance of the Key System implementation
  // represented by this object's cdm implementation value.
  scoped_refptr<media::DrmSystem> drm_system(
      new media::DrmSystem(key_system_.c_str()));

  // 2.9. If any of the preceding steps failed, reject promise with a new
  // DOMException whose name is the appropriate error name.
  if (!drm_system->is_valid()) {
    drm_system.reset();
    promise->Reject(new DOMException(
        DOMException::kNotSupportedErr,
        "Failed to load and initialize the Key System implementation."));
    return promise;
  }

  // 2.10. Let media keys be a new MediaKeys object.
  // 2.10.5. Let the cdm instance value be instance.
  scoped_refptr<MediaKeys> media_keys(
      new MediaKeys(settings, drm_system, script_value_factory_));

  // 2.11. Resolve promise with media keys.
  promise->Resolve(media_keys);
  return promise;
}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt
