// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket_stream/socket_stream_job.h"

#include "base/memory/singleton.h"
#include "net/base/ssl_config_service.h"
#include "net/base/transport_security_state.h"
#include "net/socket_stream/socket_stream_job_manager.h"
#include "net/url_request/url_request_context.h"

namespace net {

// static
SocketStreamJob::ProtocolFactory* SocketStreamJob::RegisterProtocolFactory(
    const std::string& scheme, ProtocolFactory* factory) {
  return SocketStreamJobManager::GetInstance()->RegisterProtocolFactory(
      scheme, factory);
}

// static
SocketStreamJob* SocketStreamJob::CreateSocketStreamJob(
    const GURL& url,
    SocketStream::Delegate* delegate,
    TransportSecurityState* sts,
    SSLConfigService* ssl) {
  GURL socket_url(url);
  TransportSecurityState::DomainState domain_state;
  if (url.scheme() == "ws" && sts && sts->GetDomainState(
          &domain_state, url.host(), SSLConfigService::IsSNIAvailable(ssl)) &&
      domain_state.ShouldRedirectHTTPToHTTPS()) {
    url_canon::Replacements<char> replacements;
    static const char kNewScheme[] = "wss";
    replacements.SetScheme(kNewScheme,
                           url_parse::Component(0, strlen(kNewScheme)));
    socket_url = url.ReplaceComponents(replacements);
  }
  return SocketStreamJobManager::GetInstance()->CreateJob(socket_url, delegate);
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

void SocketStreamJob::RestartWithAuth(const AuthCredentials& credentials) {
  socket_->RestartWithAuth(credentials);
}

void SocketStreamJob::DetachDelegate() {
  socket_->DetachDelegate();
}

SocketStreamJob::~SocketStreamJob() {}

}  // namespace net
