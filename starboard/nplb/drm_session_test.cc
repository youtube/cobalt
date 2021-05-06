// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <string>
#include <vector>

#include "starboard/drm.h"

#include "starboard/common/log.h"
#include "starboard/common/queue.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

constexpr SbTimeMonotonic kDefaultWaitForCallbackTimeout = kSbTimeSecond;
constexpr char kWidevineKeySystem[] = "com.widevine.alpha";
constexpr char kCencInitDataType[] = "cenc";
constexpr int kInitialTicket = kSbDrmTicketInvalid + 1;

class SbDrmSessionTest : public ::testing::Test {
 protected:
  struct SessionUpdateRequestGeneratedCallbackEvent {
    SessionUpdateRequestGeneratedCallbackEvent() {}

    SessionUpdateRequestGeneratedCallbackEvent(
        SbDrmSystem drm_system,
        const int ticket,
        const SbDrmStatus status,
        const SbDrmSessionRequestType type,
        const std::string& error_message,
        const std::string& session_id,
        const std::vector<uint8_t>& content)
        : drm_system(drm_system),
          ticket(ticket),
          drm_status(status),
          type(type),
          error_message(error_message),
          session_id(session_id),
          content(content),
          is_valid(true) {}

    SbDrmSystem drm_system = kSbDrmSystemInvalid;
    int ticket = kSbDrmTicketInvalid;
    SbDrmStatus drm_status = kSbDrmStatusUnknownError;
    SbDrmSessionRequestType type = kSbDrmSessionRequestTypeLicenseRequest;
    std::string error_message;
    std::string session_id;
    std::vector<uint8_t> content;
    bool is_valid = false;
  };

  struct ServerCertificateUpdatedCallbackEvent {
    ServerCertificateUpdatedCallbackEvent() {}

    ServerCertificateUpdatedCallbackEvent(SbDrmSystem drm_system,
                                          const int ticket,
                                          const SbDrmStatus status,
                                          const std::string& error_message)
        : drm_system(drm_system),
          ticket(ticket),
          drm_status(status),
          error_message(error_message),
          is_valid(true) {}

    SbDrmSystem drm_system = kSbDrmSystemInvalid;
    int ticket = kSbDrmTicketInvalid;
    SbDrmStatus drm_status = kSbDrmStatusUnknownError;
    std::string error_message;
    bool is_valid = false;
  };

  struct SessionClosedCallbackEvent {
    SessionClosedCallbackEvent() {}

    SessionClosedCallbackEvent(SbDrmSystem drm_system,
                               const std::string& session_id)
        : drm_system(drm_system), session_id(session_id), is_valid(true) {}

    SbDrmSystem drm_system = kSbDrmSystemInvalid;
    std::string session_id;
    bool is_valid = false;
  };

  SbDrmSessionTest() {}

  void SetUp() override;
  void TearDown() override;

  // Checks callback parameters to determine if their respective SbDrm*
  // functions executed successfully.
  void CheckSessionUpdateRequestGeneratedCallback(
      const SessionUpdateRequestGeneratedCallbackEvent& event,
      const int ticket) const;
  void CheckServerCertificateUpdatedCallback(
      const ServerCertificateUpdatedCallbackEvent& event,
      const int ticket) const;
  void CheckSessionClosedCallback(const SessionClosedCallbackEvent& event,
                                  const std::string& session_id) const;

  void OnSessionUpdateRequestGenerated(SbDrmSystem drm_system,
                                       const int ticket,
                                       const SbDrmStatus status,
                                       const SbDrmSessionRequestType type,
                                       const std::string& error_message,
                                       const std::string& session_id,
                                       const std::vector<uint8_t>& content);
  void OnServerCertificateUpdated(SbDrmSystem drm_system,
                                  const int ticket,
                                  const SbDrmStatus status,
                                  const std::string& error_message);
  void OnSessionClosed(SbDrmSystem drm_system, const std::string& session_id);

  static void OnSessionUpdateRequestGeneratedFunc(SbDrmSystem drm_system,
                                                  void* context,
                                                  int ticket,
                                                  SbDrmStatus status,
                                                  SbDrmSessionRequestType type,
                                                  const char* error_message,
                                                  const void* session_id,
                                                  int session_id_size,
                                                  const void* content,
                                                  int content_size,
                                                  const char* url);
  static void OnServerCertificateUpdatedFunc(SbDrmSystem drm_system,
                                             void* context,
                                             int ticket,
                                             SbDrmStatus status,
                                             const char* error_message);
  static void OnSessionClosedFunc(SbDrmSystem drm_system,
                                  void* context,
                                  const void* session_id,
                                  int session_id_size);

  // Queue containing callback events for OnSessionUpdateRequestGeneratedFunc().
  Queue<SessionUpdateRequestGeneratedCallbackEvent>
      session_update_request_callback_event_queue_;
  // Queue containing callback events for OnServerCertificateUpdatedFunc().
  Queue<ServerCertificateUpdatedCallbackEvent>
      server_certificate_updated_callback_event_queue_;
  // Queue containing callback events for OnSessionClosedFunc().
  Queue<SessionClosedCallbackEvent> session_closed_callback_event_queue_;

  SbDrmSystem drm_system_ = kSbDrmSystemInvalid;
};

void SbDrmSessionTest::SetUp() {
  drm_system_ = SbDrmCreateSystem(
      kWidevineKeySystem, this, OnSessionUpdateRequestGeneratedFunc,
      DummySessionUpdatedFunc, DummySessionKeyStatusesChangedFunc,
      OnServerCertificateUpdatedFunc, OnSessionClosedFunc);
}

void SbDrmSessionTest::TearDown() {
  SbDrmDestroySystem(drm_system_);
}

void SbDrmSessionTest::CheckSessionUpdateRequestGeneratedCallback(
    const SessionUpdateRequestGeneratedCallbackEvent& event,
    const int ticket) const {
  ASSERT_TRUE(event.is_valid);
  ASSERT_EQ(event.drm_system, drm_system_);
  ASSERT_EQ(event.ticket, ticket);
  ASSERT_EQ(event.drm_status, kSbDrmStatusSuccess);
  ASSERT_TRUE(event.error_message.empty());
  ASSERT_FALSE(event.session_id.empty());
  ASSERT_FALSE(event.content.empty());
}

void SbDrmSessionTest::CheckServerCertificateUpdatedCallback(
    const ServerCertificateUpdatedCallbackEvent& event,
    const int ticket) const {
  ASSERT_TRUE(event.is_valid);
  ASSERT_EQ(event.drm_system, drm_system_);
  ASSERT_EQ(event.ticket, ticket);
  ASSERT_EQ(event.drm_status, kSbDrmStatusSuccess);
  ASSERT_TRUE(event.error_message.empty());
}

void SbDrmSessionTest::CheckSessionClosedCallback(
    const SessionClosedCallbackEvent& event,
    const std::string& session_id) const {
  ASSERT_TRUE(event.is_valid);
  ASSERT_EQ(event.drm_system, drm_system_);
  ASSERT_EQ(event.session_id, session_id);
}

void SbDrmSessionTest::OnSessionUpdateRequestGenerated(
    SbDrmSystem drm_system,
    const int ticket,
    const SbDrmStatus status,
    const SbDrmSessionRequestType type,
    const std::string& error_message,
    const std::string& session_id,
    const std::vector<uint8_t>& content) {
  session_update_request_callback_event_queue_.Put(
      SessionUpdateRequestGeneratedCallbackEvent(drm_system, ticket, status,
                                                 type, error_message,
                                                 session_id, content));
}

void SbDrmSessionTest::OnServerCertificateUpdated(
    SbDrmSystem drm_system,
    const int ticket,
    const SbDrmStatus status,
    const std::string& error_message) {
  server_certificate_updated_callback_event_queue_.Put(
      ServerCertificateUpdatedCallbackEvent(drm_system, ticket, status,
                                            error_message));
}

void SbDrmSessionTest::OnSessionClosed(SbDrmSystem drm_system,
                                       const std::string& session_id) {
  session_closed_callback_event_queue_.Put(
      SessionClosedCallbackEvent(drm_system, session_id));
}

// static.
void SbDrmSessionTest::OnSessionUpdateRequestGeneratedFunc(
    SbDrmSystem drm_system,
    void* context,
    int ticket,
    SbDrmStatus status,
    SbDrmSessionRequestType type,
    const char* error_message,
    const void* session_id,
    int session_id_size,
    const void* content,
    int content_size,
    const char* url) {
  SB_DCHECK(context);
  static_cast<SbDrmSessionTest*>(context)->OnSessionUpdateRequestGenerated(
      drm_system, ticket, status, type,
      error_message ? std::string(error_message) : "",
      session_id
          ? std::string(static_cast<const char*>(session_id),
                        static_cast<const char*>(session_id) + session_id_size)
          : "",
      content ? std::vector<uint8_t>(
                    static_cast<const uint8_t*>(content),
                    static_cast<const uint8_t*>(content) + content_size)
              : std::vector<uint8_t>());
}

// static.
void SbDrmSessionTest::OnServerCertificateUpdatedFunc(
    SbDrmSystem drm_system,
    void* context,
    int ticket,
    SbDrmStatus status,
    const char* error_message) {
  SB_DCHECK(context);
  static_cast<SbDrmSessionTest*>(context)->OnServerCertificateUpdated(
      drm_system, ticket, status,
      error_message ? std::string(error_message) : "");
}

// static.
void SbDrmSessionTest::OnSessionClosedFunc(SbDrmSystem drm_system,
                                           void* context,
                                           const void* session_id,
                                           int session_id_size) {
  SB_DCHECK(context);
  static_cast<SbDrmSessionTest*>(context)->OnSessionClosed(
      drm_system,
      session_id
          ? std::string(static_cast<const char*>(session_id),
                        static_cast<const char*>(session_id) + session_id_size)
          : "");
}

TEST_F(SbDrmSessionTest, SunnyDay) {
  if (!SbDrmSystemIsValid(drm_system_)) {
    SB_LOG(INFO) << "Skipping test, DRM system Widevine is not supported on "
                    "this platform.";
    return;
  }

  if (SbDrmIsServerCertificateUpdatable(drm_system_)) {
    SbDrmUpdateServerCertificate(drm_system_, kInitialTicket,
                                 kWidevineCertificate,
                                 SB_ARRAY_SIZE_INT(kWidevineCertificate));
    ASSERT_NO_FATAL_FAILURE(CheckServerCertificateUpdatedCallback(
        server_certificate_updated_callback_event_queue_.GetTimed(
            kDefaultWaitForCallbackTimeout),
        kInitialTicket));
  }

  SbDrmGenerateSessionUpdateRequest(drm_system_, kInitialTicket,
                                    kCencInitDataType, kCencInitData,
                                    SB_ARRAY_SIZE_INT(kCencInitData));
  auto session_update_request_generated_event =
      session_update_request_callback_event_queue_.GetTimed(
          kDefaultWaitForCallbackTimeout);
  ASSERT_NO_FATAL_FAILURE(CheckSessionUpdateRequestGeneratedCallback(
      session_update_request_generated_event, kInitialTicket));

  SbDrmCloseSession(drm_system_,
                    session_update_request_generated_event.session_id.c_str(),
                    session_update_request_generated_event.session_id.size());
  ASSERT_NO_FATAL_FAILURE(CheckSessionClosedCallback(
      session_closed_callback_event_queue_.GetTimed(
          kDefaultWaitForCallbackTimeout),
      session_update_request_generated_event.session_id));
}

TEST_F(SbDrmSessionTest, CloseDrmSessionBeforeUpdateSession) {
  if (!SbDrmSystemIsValid(drm_system_)) {
    SB_LOG(INFO) << "Skipping test, DRM system Widevine is not supported on "
                    "this platform.";
    return;
  }

  SbDrmGenerateSessionUpdateRequest(drm_system_, kInitialTicket,
                                    kCencInitDataType, kCencInitData,
                                    SB_ARRAY_SIZE_INT(kCencInitData));
  auto session_update_request_generated_event =
      session_update_request_callback_event_queue_.GetTimed(
          kDefaultWaitForCallbackTimeout);
  ASSERT_NO_FATAL_FAILURE(CheckSessionUpdateRequestGeneratedCallback(
      session_update_request_generated_event, kInitialTicket));

  // Some implementations may send a server certificate request with a fake
  // session ID before the underlying CDM session is established. This test
  // ensures that the fake session ID can be used to close the session, and that
  // the session closed callback is still called.
  SbDrmCloseSession(drm_system_,
                    session_update_request_generated_event.session_id.c_str(),
                    session_update_request_generated_event.session_id.size());
  ASSERT_NO_FATAL_FAILURE(CheckSessionClosedCallback(
      session_closed_callback_event_queue_.GetTimed(
          kDefaultWaitForCallbackTimeout),
      session_update_request_generated_event.session_id));
}

TEST_F(SbDrmSessionTest, InvalidSessionUpdateRequestParams) {
  if (!SbDrmSystemIsValid(drm_system_)) {
    SB_LOG(INFO) << "Skipping test, DRM system Widevine is not supported on "
                    "this platform.";
    return;
  }

  SbDrmGenerateSessionUpdateRequest(drm_system_, kSbDrmTicketInvalid,
                                    kCencInitDataType, kCencInitData,
                                    SB_ARRAY_SIZE_INT(kCencInitData));
  auto session_update_request_generated_event =
      session_update_request_callback_event_queue_.GetTimed(
          kDefaultWaitForCallbackTimeout);
  // Check that the session update request callback is not called when the
  // ticket is invalid.
  ASSERT_FALSE(session_update_request_generated_event.is_valid);

  SbDrmGenerateSessionUpdateRequest(kSbDrmSystemInvalid, kInitialTicket,
                                    kCencInitDataType, kCencInitData,
                                    SB_ARRAY_SIZE_INT(kCencInitData));
  session_update_request_generated_event =
      session_update_request_callback_event_queue_.GetTimed(
          kDefaultWaitForCallbackTimeout);
  // Check that the session update request callback is not called when the
  // DRM system is invalid.
  ASSERT_FALSE(session_update_request_generated_event.is_valid);

  constexpr int ticket = kInitialTicket + 1;

  if (SbDrmIsServerCertificateUpdatable(drm_system_)) {
    SbDrmUpdateServerCertificate(drm_system_, ticket, kWidevineCertificate,
                                 SB_ARRAY_SIZE_INT(kWidevineCertificate));
    ASSERT_NO_FATAL_FAILURE(CheckServerCertificateUpdatedCallback(
        server_certificate_updated_callback_event_queue_.GetTimed(
            kDefaultWaitForCallbackTimeout),
        ticket));
  }

  // Check that the session update request callback is still called when the
  // CENC initialization data is invalid.
  constexpr char kInvalidCencInitData[] = "invalidcencinitializationdata";
  SbDrmGenerateSessionUpdateRequest(
      drm_system_, ticket, kCencInitDataType, kInvalidCencInitData,
      SB_ARRAY_SIZE_INT(kInvalidCencInitData) - 1);
  session_update_request_generated_event =
      session_update_request_callback_event_queue_.GetTimed(
          kDefaultWaitForCallbackTimeout);
  ASSERT_TRUE(session_update_request_generated_event.is_valid);
  ASSERT_EQ(session_update_request_generated_event.ticket, ticket);
  if (session_update_request_generated_event.type ==
      kSbDrmSessionRequestTypeIndividualizationRequest) {
    SB_DLOG(ERROR) << "The session update request generated callback type is "
                      "|SbDrmSessionRequestType::"
                      "kSbDrmSessionRequestTypeIndividualizationRequest|. "
                      "The CDM is unprovisioned.";
  } else {
    ASSERT_NE(session_update_request_generated_event.drm_status,
              kSbDrmStatusSuccess);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
