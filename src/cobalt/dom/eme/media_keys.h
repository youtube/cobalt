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

#ifndef COBALT_DOM_EME_MEDIA_KEYS_H_
#define COBALT_DOM_EME_MEDIA_KEYS_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/dom/buffer_source.h"
#include "cobalt/dom/eme/media_key_session.h"
#include "cobalt/dom/eme/media_key_session_type.h"
#include "cobalt/media/base/drm_system.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {
namespace eme {

// Represents a set of keys that an associated HTMLMediaElement can use
// for decryption of media data during playback.
//   https://www.w3.org/TR/encrypted-media/#mediakeys-interface
class MediaKeys : public script::Wrappable,
                  public base::SupportsWeakPtr<MediaKeys> {
 public:
  // Custom, not in any spec.

  MediaKeys(const std::string& key_system,
            script::ScriptValueFactory* script_value_factory);

  media::DrmSystem* drm_system() const { return drm_system_.get(); }

  // Web API: MediaKeys.

  scoped_refptr<MediaKeySession> CreateSession(
      MediaKeySessionType session_type,
      script::ExceptionState* exception_state);

  DEFINE_WRAPPABLE_TYPE(MediaKeys);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~MediaKeys() override;

  void OnSessionClosed(MediaKeySession* session);

  script::ScriptValueFactory* script_value_factory_;
  scoped_refptr<media::DrmSystem> drm_system_;

  // A MediaKeySession object shall not be destroyed and shall continue
  // to receive events if it is not closed and the MediaKeys object that created
  // it remains accessible.
  //   https://www.w3.org/TR/encrypted-media/#mediakeysession-interface
  //
  // Number of sessions is expected to be very small, typically one.
  std::vector<scoped_refptr<MediaKeySession> > open_sessions_;

  DISALLOW_COPY_AND_ASSIGN(MediaKeys);
};

}  // namespace eme
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EME_MEDIA_KEYS_H_
