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

#include "cobalt/dom/eme/media_keys.h"

#include "base/bind.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/eme/media_key_session.h"

namespace cobalt {
namespace dom {
namespace eme {

MediaKeys::MediaKeys(const std::string& key_system,
                     script::ScriptValueFactory* script_value_factory)
    : script_value_factory_(script_value_factory),
      drm_system_(new media::DrmSystem(key_system.c_str())) {}

// See https://www.w3.org/TR/encrypted-media/#dom-mediakeys-createsession.
scoped_refptr<MediaKeySession> MediaKeys::CreateSession(
    MediaKeySessionType session_type, script::ExceptionState* exception_state) {
  // 1. If this object's supported session types value does not contain
  //    sessionType, throw a NotSupportedError.
  if (session_type != kMediaKeySessionTypeTemporary) {
    DOMException::Raise(DOMException::kNotSupportedErr, exception_state);
    return NULL;
  }

  // 3. Let session be a new MediaKeySession object.
  //
  // |MediaKeys| are passed to |MediaKeySession| as weak pointer because the
  // order of destruction is not guaranteed due to JavaScript memory management.
  scoped_refptr<MediaKeySession> session(new MediaKeySession(
      drm_system_, script_value_factory_,
      base::Bind(&MediaKeys::OnSessionClosed, AsWeakPtr())));
  open_sessions_.push_back(session);
  return session;
}

void MediaKeys::OnSessionClosed(MediaKeySession* session) {
  // Erase-remove idiom, see
  // https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom.
  open_sessions_.erase(
      std::remove(open_sessions_.begin(), open_sessions_.end(), session),
      open_sessions_.end());
}

void MediaKeys::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(open_sessions_);
}

MediaKeys::~MediaKeys() {}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt
