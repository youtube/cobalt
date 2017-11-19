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

#include "base/optional.h"
#include "cobalt/dom/dom_exception.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/dom/eme/media_key_system_access.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media_capture/media_devices.h"
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/script/script_value_factory.h"
#include "starboard/media.h"

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
      media_devices_(new media_capture::MediaDevices(script_value_factory)),
      script_value_factory_(script_value_factory) {}

const std::string& Navigator::language() const { return language_; }

const std::string& Navigator::user_agent() const { return user_agent_; }

bool Navigator::java_enabled() const { return false; }

bool Navigator::cookie_enabled() const { return false; }

scoped_refptr<media_capture::MediaDevices> Navigator::media_devices() {
  return media_devices_;
}

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

// See
// https://www.w3.org/TR/encrypted-media/#get-supported-capabilities-for-audio-video-type.
base::optional<script::Sequence<MediaKeySystemMediaCapability> >
TryGetSupportedCapabilities(
    const std::string& key_system,
    const script::Sequence<MediaKeySystemMediaCapability>&
        requested_media_capabilities) {
  // 2. Let supported media capabilities be an empty sequence of
  //    MediaKeySystemMediaCapability dictionaries.
  script::Sequence<MediaKeySystemMediaCapability> supported_media_capabilities;
  // 3. For each requested media capability in requested media capabilities:
  for (std::size_t media_capability_index = 0;
       media_capability_index < requested_media_capabilities.size();
       ++media_capability_index) {
    const MediaKeySystemMediaCapability& requested_media_capability =
        requested_media_capabilities.at(media_capability_index);
    // 3.1. Let content type be requested media capability's contentType member.
    const std::string& content_type = requested_media_capability.content_type();
    // 3.3. If content type is the empty string, return null.
    if (content_type.empty()) {
      return base::nullopt;
    }
    // 3.13. If the user agent and [CDM] implementation definitely support
    //       playback of encrypted media data for the combination of container,
    //       media types [...]:
    if (SbMediaCanPlayMimeAndKeySystem(content_type.c_str(),
                                       key_system.c_str()) ==
        kSbMediaSupportTypeProbably) {
      // 3.13.1. Add requested media capability to supported media capabilities.
      supported_media_capabilities.push_back(requested_media_capability);
    }
  }
  // 4. If supported media capabilities is empty, return null.
  if (supported_media_capabilities.empty()) {
    return base::nullopt;
  }
  // 5. Return supported media capabilities.
  return supported_media_capabilities;
}

// Technically, a user agent is supposed to implement "3.1.1.1 Get Supported
// Configuration" which requests the user consent until it's given. But since
// Cobalt never interacts with the user directly, we'll assume that the consent
// is always given and go straight to "3.1.1.2 Get Supported Configuration and
// Consent". See
// https://www.w3.org/TR/encrypted-media/#get-supported-configuration-and-consent.
base::optional<eme::MediaKeySystemConfiguration> TryGetSupportedConfiguration(
    const std::string& key_system,
    const eme::MediaKeySystemConfiguration& candidate_configuration) {
  // 1. Let accumulated configuration be a new MediaKeySystemConfiguration
  //    dictionary.
  eme::MediaKeySystemConfiguration accumulated_configuration;

  // 2. Set the label member of accumulated configuration to equal the label
  //    member of candidate configuration.
  accumulated_configuration.set_label(candidate_configuration.label());

  // For now, copy initialization data types into accumulated configuration.
  //
  // TODO: Implement step 3 after introducing a Starboard API for detecting
  //       supported initialization data type.
  // TODO: Checking has_init_data_types() won't be needed once Cobalt supports
  //       default values for IDL sequences.
  accumulated_configuration.set_init_data_types(
      candidate_configuration.has_init_data_types()
          ? candidate_configuration.init_data_types()
          : script::Sequence<std::string>());

  // TODO: Reject distinctive identifiers, persistent state, and persistent
  //       sessions.

  // 15. If the videoCapabilities and audioCapabilities members in candidate
  //     configuration are both empty, return NotSupported.
  //
  // TODO: Checking has_video_capabilities() and has_audio_capabilities() won't
  //       be needed once Cobalt supports default values for IDL sequences.
  if ((!candidate_configuration.has_video_capabilities() ||
       candidate_configuration.video_capabilities().empty()) &&
      (!candidate_configuration.has_audio_capabilities() ||
       candidate_configuration.audio_capabilities().empty())) {
    return base::nullopt;
  }

  // 16. If the videoCapabilities member in candidate configuration is
  //     non-empty:
  if (candidate_configuration.has_video_capabilities() &&
      !candidate_configuration.video_capabilities().empty()) {
    // 16.1. Let video capabilities be the result of executing the "Get
    //       Supported Capabilities for Audio/Video Type" algorithm.
    base::optional<script::Sequence<MediaKeySystemMediaCapability> >
        maybe_video_capabilities = TryGetSupportedCapabilities(
            key_system, candidate_configuration.video_capabilities());
    // 16.2. If video capabilities is null, return NotSupported.
    if (!maybe_video_capabilities) {
      return base::nullopt;
    }
    // 16.3. Set the videoCapabilities member of accumulated configuration to
    //       video capabilities.
    accumulated_configuration.set_video_capabilities(*maybe_video_capabilities);
  } else {
    // Otherwise: set the videoCapabilities member of accumulated configuration
    // to an empty sequence.
    accumulated_configuration.set_video_capabilities(
        script::Sequence<MediaKeySystemMediaCapability>());
  }

  // 17. If the audioCapabilities member in candidate configuration is
  //     non-empty:
  if (candidate_configuration.has_audio_capabilities() &&
      !candidate_configuration.audio_capabilities().empty()) {
    // 17.1. Let audio capabilities be the result of executing the "Get
    //       Supported Capabilities for Audio/Video Type" algorithm.
    base::optional<script::Sequence<MediaKeySystemMediaCapability> >
        maybe_audio_capabilities = TryGetSupportedCapabilities(
            key_system, candidate_configuration.audio_capabilities());
    // 17.2. If audio capabilities is null, return NotSupported.
    if (!maybe_audio_capabilities) {
      return base::nullopt;
    }
    // 17.3. Set the audioCapabilities member of accumulated configuration to
    //       audio capabilities.
    accumulated_configuration.set_audio_capabilities(*maybe_audio_capabilities);
  } else {
    // Otherwise: set the audioCapabilities member of accumulated configuration
    // to an empty sequence.
    accumulated_configuration.set_audio_capabilities(
        script::Sequence<MediaKeySystemMediaCapability>());
  }

  // 23. Return accumulated configuration.
  return accumulated_configuration;
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
  for (std::size_t configuration_index = 0;
       configuration_index < supported_configurations.size();
       ++configuration_index) {
    // 6.3.3. If supported configuration is not NotSupported:
    base::optional<eme::MediaKeySystemConfiguration>
        maybe_supported_configuration = TryGetSupportedConfiguration(
            key_system, supported_configurations.at(configuration_index));
    if (maybe_supported_configuration) {
      // 6.3.3.1. Let access be a new MediaKeySystemAccess object.
      scoped_refptr<eme::MediaKeySystemAccess> media_key_system_access(
          new eme::MediaKeySystemAccess(key_system,
                                        *maybe_supported_configuration,
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

void Navigator::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(mime_types_);
  tracer->Trace(plugins_);
  tracer->Trace(media_session_);
  tracer->Trace(media_devices_);
}

}  // namespace dom
}  // namespace cobalt
