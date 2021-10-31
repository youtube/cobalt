// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <memory>
#include <vector>

#include "base/optional.h"
#include "cobalt/dom/captions/system_caption_settings.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/eme/media_key_system_access.h"
#include "cobalt/media_capture/media_devices.h"
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/script/script_value_factory.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/media.h"

using cobalt::media_session::MediaSession;

namespace cobalt {
namespace dom {
namespace {

const char kLicensesRelativePath[] = "/licenses/licenses_cobalt.txt";

#if !defined(COBALT_BUILD_TYPE_GOLD)

std::string ToString(const std::string& str, int indent_level);
std::string ToString(const eme::MediaKeySystemMediaCapability& capability,
                     int indent_level);
std::string ToString(const eme::MediaKeySystemConfiguration& configuration,
                     int indent_level);

std::string GetIndent(int indent_level) {
  std::string indent;
  while (indent_level > 0) {
    indent += "  ";
    --indent_level;
  }
  return indent;
}

template <typename T>
std::string ToString(const script::Sequence<T>& sequence, int indent_level) {
  std::stringstream ss;

  ss << "{\n";

  for (auto iter = sequence.begin(); iter != sequence.end(); ++iter) {
    ss << ToString(*iter, indent_level + 1) << ",\n";
  }

  ss << GetIndent(indent_level) << "}";

  return ss.str();
}

std::string ToString(const std::string& str, int indent_level) {
  return GetIndent(indent_level) + str;
}

std::string ToString(const eme::MediaKeySystemMediaCapability& capability,
                     int indent_level) {
  return GetIndent(indent_level) + '\'' + capability.content_type() + "'/'" +
         capability.encryption_scheme().value_or("(null)") + '\'';
}

std::string ToString(const eme::MediaKeySystemConfiguration& configuration,
                     int indent_level) {
  DCHECK(configuration.has_label());

  std::stringstream ss;

  ss << GetIndent(indent_level) << "label:'" << configuration.label()
     << "': {\n";
  if (configuration.has_init_data_types()) {
    ss << GetIndent(indent_level + 1) << "init_data_types: "
       << ToString(configuration.init_data_types(), indent_level + 1) << ",\n";
  }
  if (configuration.has_audio_capabilities()) {
    ss << GetIndent(indent_level + 1) << "audio_capabilities: "
       << ToString(configuration.audio_capabilities(), indent_level + 1)
       << ",\n";
  }
  if (configuration.has_video_capabilities()) {
    ss << GetIndent(indent_level + 1) << "video_capabilities: "
       << ToString(configuration.video_capabilities(), indent_level + 1)
       << ",\n";
  }
  ss << GetIndent(indent_level) << "}";

  return ss.str();
}

#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

// This function is used when the underlying SbMediaCanPlayMimeAndKeySystem()
// implementation doesn't support extra attributes on |key_system|, it makes
// decision based on the key system itself.
bool IsEncryptionSchemeSupportedByDefault(
    const std::string& key_system, const std::string& encryption_scheme) {
  // 1. Playready only supports "cenc".
  if (key_system.find("playready") != key_system.npos) {
    return encryption_scheme == "cenc";
  }
  // 2. Fairplay only supports "cbcs" and "cbcs-1-9".
  if (key_system.find("fairplay") != key_system.npos) {
    return encryption_scheme == "cbcs" || encryption_scheme == "cbcs-1-9";
  }
  // 3. Widevine only supports "cenc", "cbcs" and "cbcs-1-9".
  if (key_system.find("widevine") != key_system.npos) {
    return encryption_scheme == "cenc" || encryption_scheme == "cbcs" ||
           encryption_scheme == "cbcs-1-9";
  }

  // The key system is unknown, assume only "cenc" is supported.
  return encryption_scheme == "cenc";
}

bool CanPlay(const media::CanPlayTypeHandler& can_play_type_handler,
             const std::string& content_type, const std::string& key_system) {
  auto can_play_result = can_play_type_handler.CanPlayAdaptive(
      content_type.c_str(), key_system.c_str());
  LOG_IF(INFO, can_play_result == kSbMediaSupportTypeMaybe)
      << "CanPlayAdaptive() returns \"maybe\".";
  return can_play_result == kSbMediaSupportTypeProbably ||
         can_play_result == kSbMediaSupportTypeMaybe;
}

}  // namespace

Navigator::Navigator(
    script::EnvironmentSettings* settings, const std::string& user_agent,
    UserAgentPlatformInfo* platform_info, const std::string& language,
    scoped_refptr<cobalt::dom::captions::SystemCaptionSettings> captions,
    script::ScriptValueFactory* script_value_factory)
    : user_agent_(user_agent),
      user_agent_data_(
          new NavigatorUAData(platform_info, script_value_factory)),
      language_(language),
      mime_types_(new MimeTypeArray()),
      plugins_(new PluginArray()),
      media_devices_(
          new media_capture::MediaDevices(settings, script_value_factory)),
      system_caption_settings_(captions),
      script_value_factory_(script_value_factory) {}

const std::string& Navigator::language() const { return language_; }

script::Sequence<std::string> Navigator::languages() const {
  script::Sequence<std::string> languages;
  languages.push_back(language_);
  return languages;
}

base::Optional<std::string> GetFilenameForLicenses() {
  const size_t kBufferSize = kSbFileMaxPath + 1;
  std::vector<char> buffer(kBufferSize, 0);
  bool got_path = SbSystemGetPath(kSbSystemPathContentDirectory, buffer.data(),
                                  static_cast<int>(kBufferSize));
  if (!got_path) {
    SB_DLOG(ERROR) << "Cannot get content path for licenses files.";
    return base::Optional<std::string>();
  }

  return std::string(buffer.data()).append(kLicensesRelativePath);
}

const std::string Navigator::licenses() const {
  base::Optional<std::string> filename = GetFilenameForLicenses();
  if (!filename) {
    return std::string();
  }

  SbFile file = SbFileOpen(filename->c_str(), kSbFileOpenOnly | kSbFileRead,
                           nullptr, nullptr);
  if (file == kSbFileInvalid) {
    SB_DLOG(WARNING) << "Cannot open licenses file: " << *filename;
    return std::string();
  }

  SbFileInfo info;
  bool success = SbFileGetInfo(file, &info);
  if (!success) {
    SB_DLOG(WARNING) << "Cannot get information for licenses file.";
    SbFileClose(file);
    return std::string();
  }
  // SbFileReadAll expects an int for the size argument. Assume that the file
  // is smaller than 2^32.
  int file_size = static_cast<int>(info.size);

  std::unique_ptr<char[]> buffer(new char[file_size]);
  SbFileReadAll(file, buffer.get(), file_size);
  const std::string file_contents = std::string(buffer.get(), file_size);
  SbFileClose(file);

  return file_contents;
}

const std::string& Navigator::user_agent() const { return user_agent_; }

const scoped_refptr<NavigatorUAData>& Navigator::user_agent_data() const {
  return user_agent_data_;
}

bool Navigator::java_enabled() const { return false; }

bool Navigator::cookie_enabled() const { return false; }

bool Navigator::on_line() const {
#if SB_API_VERSION >= 13
  return !SbSystemNetworkIsDisconnected();
#else
  return true;
#endif
}

scoped_refptr<media_capture::MediaDevices> Navigator::media_devices() {
  return media_devices_;
}

const scoped_refptr<MimeTypeArray>& Navigator::mime_types() const {
  return mime_types_;
}

const scoped_refptr<PluginArray>& Navigator::plugins() const {
  return plugins_;
}

const scoped_refptr<media_session::MediaSession>& Navigator::media_session() {
  if (media_session_ == nullptr) {
    media_session_ =
        scoped_refptr<media_session::MediaSession>(new MediaSession());

    if (media_player_factory_ != nullptr) {
      media_session_->EnsureMediaSessionClient();
      DCHECK(media_session_->media_session_client());
      media_session_->media_session_client()->SetMaybeFreezeCallback(
          maybe_freeze_callback_);
      media_session_->media_session_client()->SetMediaPlayerFactory(
          media_player_factory_);
    }
  }
  return media_session_;
}

// TODO: Move the following two functions to the bottom of the file, in sync
//       with the order of declaration.
// See
// https://www.w3.org/TR/encrypted-media/#get-supported-capabilities-for-audio-video-type.
base::Optional<script::Sequence<MediaKeySystemMediaCapability>>
Navigator::TryGetSupportedCapabilities(
    const media::CanPlayTypeHandler& can_play_type_handler,
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
    if (CanPlayWithCapability(can_play_type_handler, key_system,
                              requested_media_capability)) {
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
base::Optional<eme::MediaKeySystemConfiguration>
Navigator::TryGetSupportedConfiguration(
    const media::CanPlayTypeHandler& can_play_type_handler,
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
    base::Optional<script::Sequence<MediaKeySystemMediaCapability>>
        maybe_video_capabilities = TryGetSupportedCapabilities(
            can_play_type_handler, key_system,
            candidate_configuration.video_capabilities());
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
    base::Optional<script::Sequence<MediaKeySystemMediaCapability>>
        maybe_audio_capabilities = TryGetSupportedCapabilities(
            can_play_type_handler, key_system,
            candidate_configuration.audio_capabilities());
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

// See
// https://www.w3.org/TR/encrypted-media/#dom-navigator-requestmediakeysystemaccess.
script::Handle<Navigator::InterfacePromise>
Navigator::RequestMediaKeySystemAccess(
    script::EnvironmentSettings* settings, const std::string& key_system,
    const script::Sequence<eme::MediaKeySystemConfiguration>&
        supported_configurations) {
  DCHECK(settings);
  DOMSettings* dom_settings =
      base::polymorphic_downcast<DOMSettings*>(settings);
  DCHECK(dom_settings->can_play_type_handler());
  script::Handle<InterfacePromise> promise =
      script_value_factory_
          ->CreateInterfacePromise<scoped_refptr<eme::MediaKeySystemAccess>>();

#if !defined(COBALT_BUILD_TYPE_GOLD)
  LOG(INFO) << "Navigator.RequestMediaKeySystemAccess() called with '"
            << key_system << "', and\n"
            << ToString(supported_configurations, 0);
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

  // 1. If |keySystem| is the empty string, return a promise rejected
  //    with a newly created TypeError.
  // 2. If |supportedConfigurations| is empty, return a promise rejected
  //    with a newly created TypeError.
  if (key_system.empty() || supported_configurations.empty()) {
    promise->Reject(script::kTypeError);
    return promise;
  }

  // 6.3. For each value in |supportedConfigurations|:
  for (std::size_t configuration_index = 0;
       configuration_index < supported_configurations.size();
       ++configuration_index) {
    // 6.3.3. If supported configuration is not NotSupported:
    base::Optional<eme::MediaKeySystemConfiguration>
        maybe_supported_configuration = TryGetSupportedConfiguration(
            *dom_settings->can_play_type_handler(), key_system,
            supported_configurations.at(configuration_index));
    if (maybe_supported_configuration) {
      // 6.3.3.1. Let access be a new MediaKeySystemAccess object.
      scoped_refptr<eme::MediaKeySystemAccess> media_key_system_access(
          new eme::MediaKeySystemAccess(key_system,
                                        *maybe_supported_configuration,
                                        script_value_factory_));
#if !defined(COBALT_BUILD_TYPE_GOLD)
      LOG(INFO) << "Navigator.RequestMediaKeySystemAccess() resolved with '"
                << media_key_system_access->key_system() << "', and\n"
                << ToString(media_key_system_access->GetConfiguration(), 0);
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
      // 6.3.3.2. Resolve promise.
      promise->Resolve(media_key_system_access);
      return promise;
    }
  }

  // 6.4. Reject promise with a NotSupportedError.
  promise->Reject(new DOMException(DOMException::kNotSupportedErr));
  return promise;
}

const scoped_refptr<cobalt::dom::captions::SystemCaptionSettings>&
Navigator::system_caption_settings() const {
  return system_caption_settings_;
}

void Navigator::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(mime_types_);
  tracer->Trace(plugins_);
  tracer->Trace(media_session_);
  tracer->Trace(media_devices_);
  tracer->Trace(system_caption_settings_);
}

bool Navigator::CanPlayWithCapability(
    const media::CanPlayTypeHandler& can_play_type_handler,
    const std::string& key_system,
    const MediaKeySystemMediaCapability& media_capability) {
  const std::string& content_type = media_capability.content_type();

  // There is no encryption scheme specified, check directly.
  if (!media_capability.encryption_scheme().has_value()) {
    if (CanPlay(can_play_type_handler, content_type, key_system)) {
      LOG(INFO) << "Navigator::RequestMediaKeySystemAccess(" << content_type
                << ", " << key_system << ") -> supported";
      return true;
    }
    LOG(INFO) << "Navigator::RequestMediaKeySystemAccess(" << content_type
              << ", " << key_system << ") -> not supported";
    return false;
  }

  if (!key_system_with_attributes_supported_.has_value()) {
    if (!CanPlay(can_play_type_handler, content_type, key_system)) {
      // If the check on the basic key system fails, we don't even bother to
      // check if it supports key system with attributes.
      LOG(INFO) << "Navigator::RequestMediaKeySystemAccess(" << content_type
                << ", " << key_system << ") -> not supported";
      return false;
    }
    auto key_system_with_invalid_attribute =
        std::string(key_system) + "; invalid_attributes=\"value\"";
    // If an implementation supports attributes, it should ignore unknown
    // attributes and return true, as the key system has been verified to be
    // supported above.
    key_system_with_attributes_supported_ = CanPlay(
        can_play_type_handler, content_type, key_system_with_invalid_attribute);
    if (key_system_with_attributes_supported_.value()) {
      LOG(INFO) << "Navigator::RequestMediaKeySystemAccess() will use key"
                << " system with attributes.";
    } else {
      LOG(INFO) << "Navigator::RequestMediaKeySystemAccess() won't use key"
                << " system with attributes.";
    }
  }

  DCHECK(key_system_with_attributes_supported_.has_value());

  // As a key system with attributes support is optional, and the logic to
  // determine whether the encryptionScheme is supported can be quite different
  // depending on whether this is supported, we encapsulate the logic into two
  // different functions.
  if (key_system_with_attributes_supported_.value()) {
    return CanPlayWithAttributes(can_play_type_handler, content_type,
                                 key_system,
                                 media_capability.encryption_scheme().value());
  }

  return CanPlayWithoutAttributes(can_play_type_handler, content_type,
                                  key_system,
                                  media_capability.encryption_scheme().value());
}

bool Navigator::CanPlayWithoutAttributes(
    const media::CanPlayTypeHandler& can_play_type_handler,
    const std::string& content_type, const std::string& key_system,
    const std::string& encryption_scheme) {
  DCHECK(key_system_with_attributes_supported_.has_value());
  DCHECK(!key_system_with_attributes_supported_.value());

  if (!IsEncryptionSchemeSupportedByDefault(key_system, encryption_scheme)) {
    LOG(INFO) << "Navigator::RequestMediaKeySystemAccess() rejects "
              << key_system << " because encryptionScheme \""
              << encryption_scheme << "\" is not supported.";
    return false;
  }

  if (CanPlay(can_play_type_handler, content_type, key_system)) {
    LOG(INFO) << "Navigator::RequestMediaKeySystemAccess(" << content_type
              << ", " << key_system << ") with encryptionScheme \""
              << encryption_scheme << "\" -> supported";
    return true;
  }

  LOG(INFO) << "Navigator::RequestMediaKeySystemAccess(" << content_type << ", "
            << key_system << ") with encryptionScheme \"" << encryption_scheme
            << "\" -> not supported";
  return false;
}

bool Navigator::CanPlayWithAttributes(
    const media::CanPlayTypeHandler& can_play_type_handler,
    const std::string& content_type, const std::string& key_system,
    const std::string& encryption_scheme) {
  DCHECK(key_system_with_attributes_supported_.has_value());
  DCHECK(key_system_with_attributes_supported_.value());

  auto key_system_with_attributes =
      key_system + "; encryptionscheme=\"" + encryption_scheme + '"';

  if (CanPlay(can_play_type_handler, content_type,
              key_system_with_attributes)) {
    LOG(INFO) << "Navigator::RequestMediaKeySystemAccess(" << content_type
              << ", " << key_system << ") with encryptionScheme \""
              << encryption_scheme << "\" -> supported";
    return true;
  }

  LOG(INFO) << "Navigator::RequestMediaKeySystemAccess(" << content_type << ", "
            << key_system << ") with encryptionScheme \"" << encryption_scheme
            << "\" -> not supported";
  return false;
}

}  // namespace dom
}  // namespace cobalt
