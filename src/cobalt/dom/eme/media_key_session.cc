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

#include "cobalt/dom/eme/media_key_session.h"

#include <type_traits>

#include "base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/eme/eme_helpers.h"
#include "cobalt/dom/eme/media_key_message_event.h"
#include "cobalt/dom/eme/media_key_message_event_init.h"
#include "cobalt/dom/eme/media_keys.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/script_value_factory.h"

// TODO: Remove this workaround.
#include "cobalt/network/network_module.h"

namespace cobalt {
namespace dom {
namespace eme {

// TODO: Remove this workaround.
namespace {

const char kIndividualizationUrlPrefix[] =
    "https://www.googleapis.com/certificateprovisioning/v1/devicecertificates/"
    "create?key=AIzaSyB-5OLKTx2iU5mko18DfdwK5611JIjbUhE&signedRequest=";

}  // namespace

MediaKeySession::IndividualizationFetcherDelegate::
    IndividualizationFetcherDelegate(MediaKeySession* media_key_session)
    : media_key_session_(media_key_session) {
  DCHECK(media_key_session_);
}

void MediaKeySession::IndividualizationFetcherDelegate::OnURLFetchDownloadData(
    const net::URLFetcher* source, scoped_ptr<std::string> download_data) {
  UNREFERENCED_PARAMETER(source);
  if (download_data) {
    response_.insert(response_.end(), download_data->begin(),
                     download_data->end());
  }
}

void MediaKeySession::IndividualizationFetcherDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  UNREFERENCED_PARAMETER(source);
  media_key_session_->OnIndividualizationResponse(response_);
}

void MediaKeySession::OnIndividualizationResponse(const std::string& response) {
  DCHECK(invidualization_fetcher_);
  invidualization_fetcher_.reset();

  if (response.empty()) {
    // TODO: Add error handling if we are going to keep this workaround.
    return;
  }

  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  drm_system_session_->Update(
      reinterpret_cast<const uint8*>(response.data()),
      static_cast<int>(response.size()),
      base::Bind(&MediaKeySession::OnSessionUpdated, base::AsWeakPtr(this),
                 base::Owned(new VoidPromiseValue::Reference(this, promise))),
      base::Bind(&MediaKeySession::OnSessionDidNotUpdate, base::AsWeakPtr(this),
                 base::Owned(new VoidPromiseValue::Reference(this, promise))));
}

// See step 3.1 of
// https://www.w3.org/TR/encrypted-media/#dom-mediakeys-createsession.
MediaKeySession::MediaKeySession(
    const scoped_refptr<media::DrmSystem>& drm_system,
    script::ScriptValueFactory* script_value_factory,
    const ClosedCallback& closed_callback)
    : // TODO: Remove this workaround.
      ALLOW_THIS_IN_INITIALIZER_LIST(invidualization_fetcher_delegate_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(event_queue_(this)),
      drm_system_(drm_system),
      drm_system_session_(drm_system->CreateSession(
#if SB_HAS(DRM_KEY_STATUSES)
          base::Bind(&MediaKeySession::OnSessionUpdateKeyStatuses,
                     base::AsWeakPtr(this))
#endif  // SB_HAS(DRM_KEY_STATUSES)
#if SB_HAS(DRM_SESSION_CLOSED)
          ,
          base::Bind(&MediaKeySession::OnSessionClosed,
                     base::AsWeakPtr(this))
#endif  // SB_HAS(DRM_SESSION_CLOSED)
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
script::Handle<script::Promise<void>> MediaKeySession::GenerateRequest(
    script::EnvironmentSettings* settings, const std::string& init_data_type,
    const BufferSource& init_data) {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();

  // 1. If this object is closed, return a promise rejected with
  //    an InvalidStateError.
  // 2. If this object's uninitialized value is false, return a promise rejected
  //    with an InvalidStateError.
  if (drm_system_session_->is_closed() || !uninitialized_) {
    promise->Reject(new DOMException(DOMException::kInvalidStateErr));
    return promise;
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
    promise->Reject(script::kTypeError);
    return promise;
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
                 base::AsWeakPtr(this), settings,
                 base::Owned(new VoidPromiseValue::Reference(this, promise))),
      base::Bind(&MediaKeySession::OnSessionUpdateRequestDidNotGenerate,
                 base::AsWeakPtr(this),
                 base::Owned(new VoidPromiseValue::Reference(this, promise))));

  // 11. Return promise.
  return promise;
}

// See https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-update.
script::Handle<script::Promise<void>> MediaKeySession::Update(
    const BufferSource& response) {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();

  // 1. If this object is closed, return a promise rejected with
  //    an InvalidStateError.
  // 2. If this object's callable value is false, return a promise rejected
  //    with an InvalidStateError.
  if (drm_system_session_->is_closed() || !callable_) {
    promise->Reject(new DOMException(DOMException::kInvalidStateErr));
    return promise;
  }

  const uint8* response_buffer;
  int response_buffer_size;
  GetBufferAndSize(response, &response_buffer, &response_buffer_size);

  // 3. If response is an empty array, return a promise rejected with a newly
  //    created TypeError.
  if (response_buffer_size == 0) {
    promise->Reject(script::kTypeError);
    return promise;
  }

  // 6.1. Let sanitized response be a validated and/or sanitized version of
  //      response copy.
  //
  // Sanitation is the responsibility of Starboard implementers.

  // 6.7. Use the CDM.
  drm_system_session_->Update(
      response_buffer, response_buffer_size,
      base::Bind(&MediaKeySession::OnSessionUpdated, base::AsWeakPtr(this),
                 base::Owned(new VoidPromiseValue::Reference(this, promise))),
      base::Bind(&MediaKeySession::OnSessionDidNotUpdate, base::AsWeakPtr(this),
                 base::Owned(new VoidPromiseValue::Reference(this, promise))));

  // 7. Return promise.
  return promise;
}

// See https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-close.
script::Handle<script::Promise<void>> MediaKeySession::Close() {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();

  // 2. If session is closed, return a resolved promise.
  if (drm_system_session_->is_closed()) {
    promise->Resolve();
    return promise;
  }

  // 3. If session's callable value is false, return a promise rejected with
  //    an InvalidStateError.
  if (!callable_) {
    promise->Reject(new DOMException(DOMException::kInvalidStateErr));
    return promise;
  }

  // 5.2. Use CDM to close the key session associated with session.
  drm_system_session_->Close();

  // Let |MediaKeys| know that the session should be removed from the list
  // of open sessions.
  closed_callback_.Run(this);

  // 5.3.1. Run the Session Closed algorithm on the session.
  OnSessionClosed();

  // 5.3.2. Resolve promise.
  promise->Resolve();
  return promise;
}

void MediaKeySession::TraceMembers(script::Tracer* tracer) {
  EventTarget::TraceMembers(tracer);

  tracer->Trace(event_queue_);
  tracer->Trace(key_status_map_);
}

// See
// https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-generaterequest.
void MediaKeySession::OnSessionUpdateRequestGenerated(
    script::EnvironmentSettings* settings,
    VoidPromiseValue::Reference* promise_reference,
    SbDrmSessionRequestType type, scoped_array<uint8> message,
    int message_size) {
  DCHECK(settings);
  DOMSettings* dom_settings =
      base::polymorphic_downcast<DOMSettings*>(settings);
  auto* global_environment = dom_settings->global_environment();
  DCHECK(global_environment);
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
  media_key_message_event_init.set_message(
      script::ArrayBuffer::New(global_environment, message.get(), message_size)
          .GetScriptValue());
  // 10.9.4.2. Let message type reflect the type of message, either
  //           "license-request" or "individualization-request".
  //
  // TODO: Introduce message type parameter to |SbDrmSessionUpdateRequestFunc|
  //       and stop pretending that all messages are license requests.
  switch (type) {
    case kSbDrmSessionRequestTypeLicenseRequest:
      media_key_message_event_init.set_message_type(
          kMediaKeyMessageTypeLicenseRequest);
      break;
    case kSbDrmSessionRequestTypeLicenseRenewal:
      media_key_message_event_init.set_message_type(
          kMediaKeyMessageTypeLicenseRenewal);
      break;
    case kSbDrmSessionRequestTypeLicenseRelease:
      media_key_message_event_init.set_message_type(
          kMediaKeyMessageTypeLicenseRelease);
      break;
    case kSbDrmSessionRequestTypeIndividualizationRequest:
      // TODO: Remove this workaround.
      // Note that |message_size| will never be 0, the if statement serves the
      // purpose to keep the statements after it uncommented without triggering
      // any build error.
      if (message_size != 0) {
        DCHECK(!invidualization_fetcher_);

        std::string request(
            reinterpret_cast<const char*>(message.get()),
            reinterpret_cast<const char*>(message.get()) + message_size);
        GURL request_url(kIndividualizationUrlPrefix + request);
        invidualization_fetcher_.reset(
            net::URLFetcher::Create(request_url, net::URLFetcher::POST,
                                    &invidualization_fetcher_delegate_));
        invidualization_fetcher_->SetRequestContext(
            dom_settings->network_module()->url_request_context_getter());
        // Don't cache the response, send it to us in OnURLFetchDownloadData().
        invidualization_fetcher_->DiscardResponse();
        // Handle redirect automatically.
        invidualization_fetcher_->SetAutomaticallyRetryOn5xx(true);
        invidualization_fetcher_->SetStopOnRedirect(false);
        // TODO: Handle CORS properly if we are going to keep this workaround.
        invidualization_fetcher_->Start();

        return;
      }
      media_key_message_event_init.set_message_type(
          kMediaKeyMessageTypeIndividualizationRequest);
      break;
  }

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
    VoidPromiseValue::Reference* promise_reference, SbDrmStatus status,
    const std::string& error_message) {
  // 10.10.1. If any of the preceding steps failed, reject promise with a new
  //          DOMException whose name is the appropriate error name.
  //
  RejectPromise(promise_reference, status, error_message);
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
    VoidPromiseValue::Reference* promise_reference, SbDrmStatus status,
    const std::string& error_message) {
  // 8.1.3. If any of the preceding steps failed, reject promise with a new
  //        DOMException whose name is the appropriate error name.
  //
  RejectPromise(promise_reference, status, error_message);
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
void MediaKeySession::OnSessionClosed() {
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
