// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WORKER_WORKER_SETTINGS_H_
#define COBALT_WORKER_WORKER_SETTINGS_H_

#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/message_port.h"
#include "cobalt/web/url_registry.h"

namespace cobalt {
namespace dom {
class MediaSourceAttachment;
}  // namespace dom
namespace worker {

// Worker Environment Settings object as described in
// https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#script-settings-for-workers

class WorkerSettings : public web::EnvironmentSettings {
 public:
  typedef web::UrlRegistry<dom::MediaSourceAttachment> MediaSourceRegistry;
  WorkerSettings();
  WorkerSettings(web::MessagePort* message_port,
                 MediaSourceRegistry* media_source_registry,
                 media::CanPlayTypeHandler* can_play_type_handler);

  web::MessagePort* message_port() const { return message_port_; }

  // From: script::EnvironmentSettings
  //
  const GURL& base_url() const override;

  // Return the origin of window's associated Document.
  //   https://html.spec.whatwg.org/#set-up-a-window-environment-settings-object
  void set_origin(const loader::Origin& origin) { origin_ = origin; }
  loader::Origin GetOrigin() const override;

  MediaSourceRegistry* media_source_registry() const {
    return media_source_registry_;
  }

  media::CanPlayTypeHandler* can_play_type_handler() const {
    return can_play_type_handler_;
  }

 private:
  // Outer message port.
  web::MessagePort* message_port_ = nullptr;

  loader::Origin origin_;

  MediaSourceRegistry* media_source_registry_;
  media::CanPlayTypeHandler* can_play_type_handler_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WORKER_SETTINGS_H_
