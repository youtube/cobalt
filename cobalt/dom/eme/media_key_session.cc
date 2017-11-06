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

#include "cobalt/dom/eme/media_key_session.h"

#include <type_traits>

#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/array_buffer_view.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/eme/media_key_message_event.h"
#include "cobalt/dom/eme/media_key_message_event_init.h"
#include "cobalt/dom/eme/media_keys.h"
#include "cobalt/script/script_value_factory.h"

namespace cobalt {
namespace dom {
namespace eme {

// See step 3.1 of
// https://www.w3.org/TR/encrypted-media/#dom-mediakeys-createsession.
MediaKeySession::MediaKeySession(
    const scoped_refptr<media::DrmSystem>& drm_system,
    script::ScriptValueFactory* script_value_factory,
    const ClosedCallback& closed_callback)
    : ALLOW_THIS_IN_INITIALIZER_LIST(event_queue_(this)),
      drm_system_(drm_system),
      drm_system_session_(drm_system->CreateSession(
#if SB_HAS(DRM_KEY_STATUSES)
          base::Bind(&MediaKeySession::OnSessionUpdateKeyStatuses,
                     base::AsWeakPtr(this))
#endif  // SB_HAS(DRM_KEY_STATUSES)
              )),  // NOLINT(whitespace/parens)
      script_value_factory_(script_value_factory),
      uninitialized_(true),
      callable_(false),
      key_status_map_(new MediaKeyStatusMap),
      closed_callback_(closed_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(closed_promise_reference_(
          this, script_value_factory->CreateBasicPromise<void>())),
      initiated_by_generate_request_(false) {
}

// According to the step 3.1 of
// https://www.w3.org/TR/encrypted-media/#dom-mediakeys-createsession,
// session ID should be empty until the first request is generated successfully.
std::string MediaKeySession::session_id() const {
  return drm_system_session_->id().value_or("");
}

MediaKeySession::~MediaKeySession() {}

const MediaKeySession::VoidPromiseValue* MediaKeySession::closed() const {
  return &closed_promise_reference_.referenced_value();
}

const scoped_refptr<MediaKeyStatusMap>& MediaKeySession::key_statuses() const {
  return key_status_map_;
}

const EventTarget::EventListenerScriptValue*
MediaKeySession::onkeystatuseschange() const {
  return GetAttributeEventListener(base::Tokens::keystatuseschange());
}

void MediaKeySession::set_onkeystatuseschange(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::keystatuseschange(), event_listener);
}

const EventTarget::EventListenerScriptValue* MediaKeySession::onmessage()
    const {
  return GetAttributeEventListener(base::Tokens::message());
}

void MediaKeySession::set_onmessage(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::message(), event_listener);
}

// See
// https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-generaterequest.
scoped_ptr<MediaKeySession::VoidPromiseValue> MediaKeySession::GenerateRequest(
    const std::string& init_data_type, const BufferSource& init_data) {
  scoped_ptr<VoidPromiseValue> promise =
      script_value_factory_->CreateBasicPromise<void>();
  VoidPromiseValue::StrongReference promise_reference(*promise);

  // 1. If this object is closed, return a promise rejected with
  //    an InvalidStateError.
  // 2. If this object's uninitialized value is false, return a promise rejected
  //    with an InvalidStateError.
  if (drm_system_session_->is_closed() || !uninitialized_) {
    promise_reference.value().Reject(
        new DOMException(DOMException::kInvalidStateErr));
    return promise.Pass();
  }

  // 3. Let this object's uninitialized value be false.
  uninitialized_ = false;

  const uint8* init_data_buffer;
  int init_data_buffer_size;
  GetBufferAndSize(init_data, &init_data_buffer, &init_data_buffer_size);

  // 4. If initDataType is the empty string, return a promise rejected with
  //    a newly created TypeError.
  // 5. If initData is an empty array, return a promise rejected with a newly
  //    created TypeError.
  if (init_data_type.empty() || init_data_buffer_size == 0) {
    promise_reference.value().Reject(script::kTypeError);
    return promise.Pass();
  }

  // 10.2. The user agent must thoroughly validate the initialization data
  //       before passing it to the CDM.
  //
  // Sanitation is the responsibility of Starboard implementers.

  // 10.9. Use the CDM.
  initiated_by_generate_request_ = true;
  drm_system_session_->GenerateUpdateRequest(
      init_data_type, init_data_buffer, init_data_buffer_size,
      base::Bind(&MediaKeySession::OnSessionUpdateRequestGenerated,
                 base::AsWeakPtr(this),
                 base::Owned(new VoidPromiseValue::Reference(this, *promise))),
      base::Bind(&MediaKeySession::OnSessionUpdateRequestDidNotGenerate,
                 base::AsWeakPtr(this),
                 base::Owned(new VoidPromiseValue::Reference(this, *promise))));

  // 11. Return promise.
  return promise.Pass();
}

// See https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-update.
scoped_ptr<MediaKeySession::VoidPromiseValue> MediaKeySession::Update(
    const BufferSource& response) {
  scoped_ptr<VoidPromiseValue> promise =
      script_value_factory_->CreateBasicPromise<void>();
  VoidPromiseValue::StrongReference promise_reference(*promise);

  // 1. If this object is closed, return a promise rejected with
  //    an InvalidStateError.
  // 2. If this object's callable value is false, return a promise rejected
  //    with an InvalidStateError.
  if (drm_system_session_->is_closed() || !callable_) {
    promise_reference.value().Reject(
        new DOMException(DOMException::kInvalidStateErr));
    return promise.Pass();
  }

  const uint8* response_buffer;
  int response_buffer_size;
  GetBufferAndSize(response, &response_buffer, &response_buffer_size);

  // 3. If response is an empty array, return a promise rejected with a newly
  //    created TypeError.
  if (response_buffer_size == 0) {
    promise_reference.value().Reject(script::kTypeError);
    return promise.Pass();
  }

  // 6.1. Let sanitized response be a validated and/or sanitized version of
  //      response copy.
  //
  // Sanitation is the responsibility of Starboard implementers.

  // 6.7. Use the CDM.
  drm_system_session_->Update(
      response_buffer, response_buffer_size,
      base::Bind(&MediaKeySession::OnSessionUpdated, base::AsWeakPtr(this),
                 base::Owned(new VoidPromiseValue::Reference(this, *promise))),
      base::Bind(&MediaKeySession::OnSessionDidNotUpdate, base::AsWeakPtr(this),
                 base::Owned(new VoidPromiseValue::Reference(this, *promise))));

  // 7. Return promise.
  return promise.Pass();
}

// See https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-close.
scoped_ptr<MediaKeySession::VoidPromiseValue> MediaKeySession::Close() {
  scoped_ptr<VoidPromiseValue> promise =
      script_value_factory_->CreateBasicPromise<void>();
  VoidPromiseValue::StrongReference promise_reference(*promise);

  // 2. If session is closed, return a resolved promise.
  if (drm_system_session_->is_closed()) {
    promise_reference.value().Resolve();
    return promise.Pass();
  }

  // 3. If session's callable value is false, return a promise rejected with
  //    an InvalidStateError.
  if (!callable_) {
    promise_reference.value().Reject(
        new DOMException(DOMException::kInvalidStateErr));
    return promise.Pass();
  }

  // 5.2. Use CDM to close the key session associated with session.
  drm_system_session_->Close();

  // Let |MediaKeys| know that the session should be removed from the list
  // of open sessions.
  closed_callback_.Run(this);

  // 5.3.1. Run the Session Closed algorithm on the session.
  OnClosed();

  // 5.3.2. Resolve promise.
  promise_reference.value().Resolve();
  return promise.Pass();
}

void MediaKeySession::TraceMembers(script::Tracer* tracer) {
  EventTarget::TraceMembers(tracer);

  tracer->Trace(key_status_map_);
  event_queue_.TraceMembers(tracer);
}

// See
// https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-generaterequest.
void MediaKeySession::OnSessionUpdateRequestGenerated(
    VoidPromiseValue::Reference* promise_reference, scoped_array<uint8> message,
    int message_size) {
  MediaKeyMessageEventInit media_key_message_event_init;
  // 10.9.4. If a license request for the requested license type can be
  //         generated based on the sanitized init data:
  // 10.9.4.1. Let message be a license request for the requested license type
  //           generated based on the sanitized init data interpreted
  //           per initDataType.
  // Otherwise:
  // 10.9.4.1. Let message be the request that needs to be processed before
  //           a license request request for the requested license type can be
  //           generated based on the sanitized init data.
  media_key_message_event_init.set_message(new ArrayBuffer(
      NULL, ArrayBuffer::kFromHeap, message.Pass(), message_size));
  // 10.9.4.2. Let message type reflect the type of message, either
  //           "license-request" or "individualization-request".
  //
  // TODO: Introduce message type parameter to |SbDrmSessionUpdateRequestFunc|
  //       and stop pretending that all messages are license requests.
  media_key_message_event_init.set_message_type(
      kMediaKeyMessageTypeLicenseRequest);

  // 10.3. Let this object's callable value be true.
  callable_ = true;

  // 10.4. Run the Queue a "message" Event algorithm on the session.
  //
  // TODO: Implement Event.isTrusted as per
  //       https://www.w3.org/TR/dom/#dom-event-istrusted and set it to true.
  event_queue_.Enqueue(
      new MediaKeyMessageEvent("message", media_key_message_event_init));

  // 10.5. Resolve promise.
  //
  // If the request was generated spontaneously by the underlying DRM system,
  // we shouldn't resolve the promise returned by |GenerateRequest|. The promise
  // was resolved in the first invocation of this method and now is simply
  // hanging around.
  if (initiated_by_generate_request_) {
    initiated_by_generate_request_ = false;
    promise_reference->value().Resolve();
  }
}

// See
// https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-generaterequest.
void MediaKeySession::OnSessionUpdateRequestDidNotGenerate(
    VoidPromiseValue::Reference* promise_reference) {
  // 10.10.1. If any of the preceding steps failed, reject promise with a new
  //          DOMException whose name is the appropriate error name.
  //
  // TODO: Introduce Starboard API that allows CDM to propagate error codes.
  promise_reference->value().Reject(new DOMException(DOMException::kNone));
}

// See https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-update.
void MediaKeySession::OnSessionUpdated(
    VoidPromiseValue::Reference* promise_reference) {
  // 8.1.1. If the set of keys known to the CDM for this object changed or
  //        the status of any key(s) changed, run the Update Key Statuses
  //        algorithm on the session.
  //
  // TODO: Implement key statuses.

  // 8.1.2. If the expiration time for the session changed, run the Update
  //        Expiration algorithm on the session.
  //
  // TODO: Implement expiration.

  // 8.2. Resolve promise.
  promise_reference->value().Resolve();
}

// See https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-update.
void MediaKeySession::OnSessionDidNotUpdate(
    VoidPromiseValue::Reference* promise_reference) {
  // 8.1.3. If any of the preceding steps failed, reject promise with a new
  //        DOMException whose name is the appropriate error name.
  //
  // TODO: Introduce Starboard API that allows CDM to propagate error codes.
  promise_reference->value().Reject(new DOMException(DOMException::kNone));
}

// See https://www.w3.org/TR/encrypted-media/#update-key-statuses.
void MediaKeySession::OnSessionUpdateKeyStatuses(
    const std::vector<std::string>& key_ids,
    const std::vector<SbDrmKeyStatus>& key_statuses) {
#define CHECK_KEY_STATUS_ENUM(starboard_value, dom_value)                  \
  static_assert(static_cast<MediaKeyStatus>(starboard_value) == dom_value, \
                "key status enum value mismatch");

  CHECK_KEY_STATUS_ENUM(kSbDrmKeyStatusUsable, kMediaKeyStatusUsable);
  CHECK_KEY_STATUS_ENUM(kSbDrmKeyStatusExpired, kMediaKeyStatusExpired);
  CHECK_KEY_STATUS_ENUM(kSbDrmKeyStatusReleased, kMediaKeyStatusReleased);
  CHECK_KEY_STATUS_ENUM(kSbDrmKeyStatusRestricted,
                        kMediaKeyStatusOutputRestricted);
  CHECK_KEY_STATUS_ENUM(kSbDrmKeyStatusDownscaled,
                        kMediaKeyStatusOutputDownscaled);
  CHECK_KEY_STATUS_ENUM(kSbDrmKeyStatusPending, kMediaKeyStatusStatusPending);
  CHECK_KEY_STATUS_ENUM(kSbDrmKeyStatusError, kMediaKeyStatusInternalError);

  DCHECK_EQ(key_ids.size(), key_statuses.size());

  // 1. Let the session be the associated MediaKeySession object.
  // 2. Let the input statuses be the sequence of pairs key ID and associated
  //    MediaKeyStatus pairs.
  // 3. Let the statuses be session's keyStatuses attribute.
  // 4. Run the following steps to replace the contents of statuses:
  // 4.1. Empty statuses.
  key_status_map_->Clear();

  // 4.2. For each pair in input statuses.
  for (size_t i = 0; i < key_ids.size(); ++i) {
    // 4.2.1. Let pair be the pair.
    // 4.2.2. Insert an entry for pair's key ID into statuses with the value of
    //        pair's MediaKeyStatus value.
    DCHECK_GE(key_statuses[i], kSbDrmKeyStatusUsable);
    DCHECK_LE(key_statuses[i], kSbDrmKeyStatusError);

    if (key_statuses[i] < kSbDrmKeyStatusUsable ||
        key_statuses[i] > kSbDrmKeyStatusError) {
      key_status_map_->Add(key_ids[i], kMediaKeyStatusInternalError);
    } else {
      key_status_map_->Add(key_ids[i],
                           static_cast<MediaKeyStatus>(key_statuses[i]));
    }
  }

  // 5. Queue a task to fire a simple event named keystatuseschange at the
  //    session.
  LOG(INFO) << "Fired 'keystatuseschange' event on MediaKeySession with "
            << key_status_map_->size() << " keys.";
  event_queue_.Enqueue(new Event(base::Tokens::keystatuseschange()));

  // 6. Queue a task to run the Attempt to Resume Playback If Necessary
  //    algorithm on each of the media element(s) whose mediaKeys attribute is
  //    the MediaKeys object that created the session.
}

// See https://www.w3.org/TR/encrypted-media/#session-closed.
void MediaKeySession::OnClosed() {
  // 2. Run the Update Key Statuses algorithm on the session, providing an empty
  //    sequence.
  //
  // TODO: Implement key statuses.

  // 3. Run the Update Expiration algorithm on the session, providing NaN.
  //
  // TODO: Implement expiration.

  // 4. Let promise be the closed attribute of the session.
  // 5. Resolve promise.
  closed_promise_reference_.value().Resolve();
}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt
