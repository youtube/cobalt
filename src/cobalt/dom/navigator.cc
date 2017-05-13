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

#include "cobalt/dom/navigator.h"

#include "cobalt/dom/dom_exception.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/dom/eme/media_key_system_access.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/script/script_value_factory.h"

using cobalt::media_session::MediaSession;

namespace cobalt {
namespace dom {

Navigator::Navigator(const std::string& user_agent, const std::string& language,
                     scoped_refptr<MediaSession> media_session,
                     script::ScriptValueFactory* script_value_factory)
    : user_agent_(user_agent),
      language_(language),
      mime_types_(new MimeTypeArray()),
      plugins_(new PluginArray()),
      media_session_(media_session),
      script_value_factory_(script_value_factory) {}

const std::string& Navigator::language() const { return language_; }

const std::string& Navigator::user_agent() const { return user_agent_; }

bool Navigator::java_enabled() const { return false; }

bool Navigator::cookie_enabled() const { return false; }

const scoped_refptr<MimeTypeArray>& Navigator::mime_types() const {
  return mime_types_;
}

const scoped_refptr<PluginArray>& Navigator::plugins() const {
  return plugins_;
}

const scoped_refptr<media_session::MediaSession>& Navigator::media_session()
    const {
  return media_session_;
}

#if defined(COBALT_MEDIA_SOURCE_2016)

namespace {

// TODO: Implement "3.1.1.1 Get Supported Configuration" using
//       SbMediaCanPlayMimeAndKeySystem().
//       https://www.w3.org/TR/encrypted-media/#get-supported-configuration
bool MaybeGetSupportedConfiguration(
    const std::string& /*key_system*/,
    const eme::MediaKeySystemConfiguration& candidate_configuration,
    eme::MediaKeySystemConfiguration* supported_configuration) {
  *supported_configuration = candidate_configuration;
  return true;
}

}  // namespace

// See
// https://www.w3.org/TR/encrypted-media/#dom-navigator-requestmediakeysystemaccess.
scoped_ptr<Navigator::InterfacePromiseValue>
Navigator::RequestMediaKeySystemAccess(
    const std::string& key_system,
    const script::Sequence<eme::MediaKeySystemConfiguration>&
        supported_configurations) {
  scoped_ptr<InterfacePromiseValue> promise =
      script_value_factory_
          ->CreateInterfacePromise<scoped_refptr<eme::MediaKeySystemAccess> >();
  InterfacePromiseValue::StrongReference promise_reference(*promise);

  // 1. If |keySystem| is the empty string, return a promise rejected
  //    with a newly created TypeError.
  // 2. If |supportedConfigurations| is empty, return a promise rejected
  //    with a newly created TypeError.
  if (key_system.empty() || supported_configurations.empty()) {
    promise_reference.value().Reject(script::kTypeError);
    return promise.Pass();
  }

  // 6.3. For each value in |supportedConfigurations|:
  for (size_t configuration_index = 0;
       configuration_index < supported_configurations.size();
       ++configuration_index) {
    // 6.3.3. If supported configuration is not NotSupported:
    eme::MediaKeySystemConfiguration supported_configuration;
    if (MaybeGetSupportedConfiguration(
            key_system, supported_configurations.at(configuration_index),
            &supported_configuration)) {
      // 6.3.3.1. Let access be a new MediaKeySystemAccess object.
      scoped_refptr<eme::MediaKeySystemAccess> media_key_system_access(
          new eme::MediaKeySystemAccess(key_system, supported_configuration,
                                        script_value_factory_));
      // 6.3.3.2. Resolve promise.
      promise_reference.value().Resolve(media_key_system_access);
      return promise.Pass();
    }
  }

  // 6.4. Reject promise with a NotSupportedError.
  promise_reference.value().Reject(
      new DOMException(DOMException::kNotSupportedErr));
  return promise.Pass();
}

#endif  // defined(COBALT_MEDIA_SOURCE_2016)

}  // namespace dom
}  // namespace cobalt
