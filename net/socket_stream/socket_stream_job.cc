// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket_stream/socket_stream_job.h"

#include "base/singleton.h"
#include "net/socket_stream/socket_stream_job_manager.h"

namespace net {

// static
SocketStreamJob::ProtocolFactory* SocketStreamJob::RegisterProtocolFactory(
    const std::string& scheme, ProtocolFactory* factory) {
  return SocketStreamJobManager::GetInstance()->RegisterProtocolFactory(
      scheme, factory);
}

// static
SocketStreamJob* SocketStreamJob::CreateSocketStreamJob(
    const GURL& url, SocketStream::Delegate* delegate) {
  return SocketStreamJobManager::GetInstance()->CreateJob(url, delegate);
}

SocketStreamJob::SocketStreamJob() {}

SocketStream::UserData* SocketStreamJob::GetUserData(const void* key) const {
  return socket_->GetUserData(key);
}

void SocketStreamJob::SetUserData(const void* key,
                                  SocketStream::UserData* data) {
  socket_->SetUserData(key, data);
}

void SocketStreamJob::Connect() {
  socket_->Connect();
}

bool SocketStreamJob::SendData(const char* data, int len) {
  return socket_->SendData(data, len);
}

void SocketStreamJob::Close() {
  socket_->Close();
}

void SocketStreamJob::RestartWithAuth(const string16& username,
                                      const string16& password) {
  socket_->RestartWithAuth(username, password);
}

void SocketStreamJob::DetachDelegate() {
  socket_->DetachDelegate();
}

SocketStreamJob::~SocketStreamJob() {}

}  // namespace net
