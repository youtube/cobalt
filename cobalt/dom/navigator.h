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

#ifndef COBALT_DOM_NAVIGATOR_H_
#define COBALT_DOM_NAVIGATOR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/dom/captions/system_caption_settings.h"
#include "cobalt/dom/eme/media_key_system_configuration.h"
#include "cobalt/dom/mime_type_array.h"
#include "cobalt/dom/plugin_array.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/media_capture/media_devices.h"
#include "cobalt/media_session/media_session.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/navigator_base.h"
#include "cobalt/web/navigator_ua_data.h"

namespace cobalt {
namespace dom {

// The Navigator object represents the identity and state of the user agent (the
// client), and allows Web pages to register themselves as potential protocol
// and content handlers.
// https://www.w3.org/TR/html50/webappapis.html#navigator
class Navigator : public web::NavigatorBase {
 public:
  Navigator(script::EnvironmentSettings* settings,
            scoped_refptr<captions::SystemCaptionSettings> captions);
  Navigator(const Navigator&) = delete;
  Navigator& operator=(const Navigator&) = delete;

  uint64_t hardware_concurrency() const;

  // Web API: NavigatorLicenses
  const std::string licenses() const;

  // Web API: NavigatorStorageUtils
  bool java_enabled() const;

  // Web API: NavigatorPlugins
  bool cookie_enabled() const;

  // Web API: MediaDevices
  scoped_refptr<media_capture::MediaDevices> media_devices();

  const scoped_refptr<MimeTypeArray>& mime_types() const;
  const scoped_refptr<PluginArray>& plugins() const;

  const scoped_refptr<media_session::MediaSession>& media_session();

  // Set maybe freeze callback.
  void set_maybefreeze_callback(const base::Closure& maybe_freeze_callback) {
    maybe_freeze_callback_ = maybe_freeze_callback;
  }

  void set_media_player_factory(const media::WebMediaPlayerFactory* factory) {
    media_player_factory_ = factory;
  }

  // Web API: extension defined in Encrypted Media Extensions (16 March 2017).
  using InterfacePromise = script::Promise<scoped_refptr<script::Wrappable>>;
  script::Handle<InterfacePromise> RequestMediaKeySystemAccess(
      script::EnvironmentSettings* settings, const std::string& key_system,
      const script::Sequence<eme::MediaKeySystemConfiguration>&
          supported_configurations);

  const scoped_refptr<captions::SystemCaptionSettings>&
  system_caption_settings() const;

  DEFINE_WRAPPABLE_TYPE(Navigator);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~Navigator() override {}

  base::Optional<script::Sequence<eme::MediaKeySystemMediaCapability>>
  TryGetSupportedCapabilities(
      const media::CanPlayTypeHandler& can_play_type_handler,
      const std::string& key_system,
      const script::Sequence<eme::MediaKeySystemMediaCapability>&
          requested_media_capabilities);

  base::Optional<eme::MediaKeySystemConfiguration> TryGetSupportedConfiguration(
      const media::CanPlayTypeHandler& can_play_type_handler,
      const std::string& key_system,
      const eme::MediaKeySystemConfiguration& candidate_configuration);

  bool CanPlayWithCapability(
      const media::CanPlayTypeHandler& can_play_type_handler,
      const std::string& key_system,
      const eme::MediaKeySystemMediaCapability& media_capability);

  bool CanPlayWithoutAttributes(
      const media::CanPlayTypeHandler& can_play_type_handler,
      const std::string& content_type, const std::string& key_system,
      const std::string& encryption_scheme);

  bool CanPlayWithAttributes(
      const media::CanPlayTypeHandler& can_play_type_handler,
      const std::string& content_type, const std::string& key_system,
      const std::string& encryption_scheme);

  scoped_refptr<MimeTypeArray> mime_types_;
  scoped_refptr<PluginArray> plugins_;
  scoped_refptr<media_session::MediaSession> media_session_;
  scoped_refptr<media_capture::MediaDevices> media_devices_;
  scoped_refptr<captions::SystemCaptionSettings> system_caption_settings_;
  base::Optional<bool> key_system_with_attributes_supported_;

  base::Closure maybe_freeze_callback_;
  const media::WebMediaPlayerFactory* media_player_factory_ = nullptr;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NAVIGATOR_H_
