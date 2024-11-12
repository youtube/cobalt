// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_SESSION_CLIENT_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_SESSION_CLIENT_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"

namespace google_apis {
class RequestSender;
}

namespace ash::boca {
class RemoveStudentRequest;
class CreateSessionRequest;
class GetSessionRequest;
class UpdateSessionRequest;
class UpdateStudentActivitiesRequest;
class UploadTokenRequest;
class JoinSessionRequest;

class SessionClientImpl {
 public:
  SessionClientImpl();
  explicit SessionClientImpl(
      std::unique_ptr<google_apis::RequestSender> sender);
  SessionClientImpl(const SessionClientImpl&) = delete;
  SessionClientImpl& operator=(const SessionClientImpl&) = delete;
  virtual ~SessionClientImpl();

  virtual std::unique_ptr<google_apis::RequestSender> CreateRequestSender();
  virtual void CreateSession(std::unique_ptr<CreateSessionRequest> request);
  virtual void GetSession(std::unique_ptr<GetSessionRequest> request);
  virtual void UploadToken(std::unique_ptr<UploadTokenRequest> request);
  virtual void UpdateSession(std::unique_ptr<UpdateSessionRequest> request);
  virtual void UpdateStudentActivity(
      std::unique_ptr<UpdateStudentActivitiesRequest> request);
  virtual void RemoveStudent(std::unique_ptr<RemoveStudentRequest> request);
  virtual void JoinSession(std::unique_ptr<JoinSessionRequest> request);
  google_apis::RequestSender* sender() { return sender_.get(); }

 private:
  std::unique_ptr<google_apis::RequestSender> sender_;
};
}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_SESSION_CLIENT_IMPL_H_
