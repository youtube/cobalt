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

#include "cobalt/dom/eme/media_keys.h"

#include "base/bind.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/eme/eme_helpers.h"
#include "cobalt/dom/eme/media_key_session.h"

namespace cobalt {
namespace dom {
namespace eme {

MediaKeys::MediaKeys(script::EnvironmentSettings* settings,
                     const scoped_refptr<media::DrmSystem>& drm_system,
                     script::ScriptValueFactory* script_value_factory)
    : dom_settings_(base::polymorphic_downcast<DOMSettings*>(settings)),
      script_value_factory_(script_value_factory),
      drm_system_(drm_system) {
  SB_DCHECK(drm_system_->is_valid())
      << "DrmSystem provided on initialization is invalid.";
}

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
      dom_settings_, drm_system_, script_value_factory_,
      base::Bind(&MediaKeys::OnSessionClosed, AsWeakPtr())));
  open_sessions_.push_back(session);
  return session;
}

MediaKeys::BoolPromiseHandle MediaKeys::SetServerCertificate(
    const BufferSource& server_certificate) {
  BoolPromiseHandle promise = script_value_factory_->CreateBasicPromise<bool>();

  // 1. If the Key System implementation represented by this object's cdm
  //    implementation value does not support server certificates, return a
  //    promise resolved with false.
  if (!drm_system_->IsServerCertificateUpdatable()) {
    promise->Resolve(false);
    return promise;
  }

  // 2. If serverCertificate is an empty array, return a promise rejected with a
  //    newly created TypeError.
  const uint8* server_certificate_buffer;
  int server_certificate_buffer_size = 0;
  GetBufferAndSize(server_certificate, &server_certificate_buffer,
                   &server_certificate_buffer_size);

  if (server_certificate_buffer_size == 0) {
    promise->Reject(script::kTypeError);
    return promise;
  }

  // 3. Let certificate be a copy of the contents of the serverCertificate
  //    parameter.
  // 4. Let promise be a new promise.
  // 5. Run the following steps in parallel:
  // 5.1 Let sanitized certificate be a validated and/or sanitized version of
  //     certificate.
  // 5.2 Use this object's cdm instance to process sanitized certificate.
  drm_system_->UpdateServerCertificate(
      server_certificate_buffer, server_certificate_buffer_size,
      base::Bind(&MediaKeys::OnServerCertificateUpdated, base::AsWeakPtr(this),
                 base::Owned(new BoolPromiseValue::Reference(this, promise))));

  // 5.3 and 5.4 are pending processing in OnServerCertificateUpdated().
  // 6. Return promise.
  return promise;
}

script::Handle<script::Uint8Array> MediaKeys::GetMetrics(
    script::ExceptionState* exception_state) {
  std::vector<uint8_t> metrics;
  if (drm_system_->GetMetrics(&metrics)) {
    return script::Uint8Array::New(dom_settings_->global_environment(),
                                   metrics.data(), metrics.size());
  }
  DOMException::Raise(DOMException::kNotSupportedErr, exception_state);
  return script::Handle<script::Uint8Array>();
}

void MediaKeys::OnSessionClosed(MediaKeySession* session) {
  // Erase-remove idiom, see
  // https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom.
  open_sessions_.erase(
      std::remove(open_sessions_.begin(), open_sessions_.end(), session),
      open_sessions_.end());
}

void MediaKeys::OnServerCertificateUpdated(
    BoolPromiseValue::Reference* promise_reference, SbDrmStatus status,
    const std::string& error_message) {
  // 5.3 If the preceding step failed, resolve promise with a new DOMException
  //     whose name is the appropriate error name.
  if (status != kSbDrmStatusSuccess) {
    RejectPromise(promise_reference, status, error_message);
    return;
  }
  // 5.4 Resolve promise with true.
  promise_reference->value().Resolve(true);
}

void MediaKeys::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(open_sessions_);
}

MediaKeys::~MediaKeys() {}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt
