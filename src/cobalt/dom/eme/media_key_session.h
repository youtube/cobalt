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

#ifndef COBALT_DOM_EME_MEDIA_KEY_SESSION_H_
#define COBALT_DOM_EME_MEDIA_KEY_SESSION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/dom/buffer_source.h"
#include "cobalt/dom/eme/media_key_status.h"
#include "cobalt/dom/eme/media_key_status_map.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media/base/drm_system.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value_factory.h"
#include "starboard/drm.h"

namespace cobalt {
namespace dom {
namespace eme {

// A session provides a context for message exchange with the CDM as a result
// of which keys are made available to the CDM.
//   https://www.w3.org/TR/encrypted-media/#key-session
//
// Also see https://www.w3.org/TR/encrypted-media/#mediakeysession-interface.
class MediaKeySession : public EventTarget {
 public:
  typedef script::ScriptValue<script::Promise<void> > VoidPromiseValue;
  typedef base::Callback<void(MediaKeySession* session)> ClosedCallback;

  // Custom, not in any spec.
  MediaKeySession(const scoped_refptr<media::DrmSystem>& drm_system,
                  script::ScriptValueFactory* script_value_factory,
                  const ClosedCallback& closed_callback);

  // Web IDL: MediaKeySession.
  std::string session_id() const;
  const VoidPromiseValue* closed() const;
  const scoped_refptr<MediaKeyStatusMap>& key_statuses() const;
  const EventListenerScriptValue* onkeystatuseschange() const;
  void set_onkeystatuseschange(const EventListenerScriptValue& event_listener);
  const EventListenerScriptValue* onmessage() const;
  void set_onmessage(const EventListenerScriptValue& event_listener);
  script::Handle<script::Promise<void>> GenerateRequest(
      const std::string& init_data_type, const BufferSource& init_data);
  script::Handle<script::Promise<void>> Update(const BufferSource& response);
  script::Handle<script::Promise<void>> Close();

  DEFINE_WRAPPABLE_TYPE(MediaKeySession);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~MediaKeySession() override;

  void OnSessionUpdateRequestGenerated(
      VoidPromiseValue::Reference* promise_reference,
      scoped_array<uint8> message, int message_size);
  void OnSessionUpdateRequestDidNotGenerate(
      VoidPromiseValue::Reference* promise_reference);
  void OnSessionUpdated(VoidPromiseValue::Reference* promise_reference);
  void OnSessionDidNotUpdate(VoidPromiseValue::Reference* promise_reference);
  void OnSessionUpdateKeyStatuses(
      const std::vector<std::string>& key_ids,
      const std::vector<SbDrmKeyStatus>& key_statuses);
  void OnSessionClosed();

  EventQueue event_queue_;

  // Although it doesn't make much sense, it's possible to call session methods
  // when |MediaKeys| are destroyed. This behavior is underspecified but is
  // consistent with Chromium. For this reason we need to hold to |drm_system_|
  // from each session.
  const scoped_refptr<media::DrmSystem> drm_system_;
  scoped_ptr<media::DrmSystem::Session> drm_system_session_;
  script::ScriptValueFactory* const script_value_factory_;
  bool uninitialized_;
  bool callable_;
  scoped_refptr<MediaKeyStatusMap> key_status_map_;

  // TODO: Remove |closed_callback_| and change call sites to use closed()
  //       promise instead, once Cobalt switches to native SpiderMonkey
  //       promises.
  const ClosedCallback closed_callback_;
  const VoidPromiseValue::Reference closed_promise_reference_;

  bool initiated_by_generate_request_;

  DISALLOW_COPY_AND_ASSIGN(MediaKeySession);
};

}  // namespace eme
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EME_MEDIA_KEY_SESSION_H_
