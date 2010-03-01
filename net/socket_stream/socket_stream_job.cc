// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket_stream/socket_stream_job.h"

#include "net/socket_stream/socket_stream_job_manager.h"

namespace net {

static SocketStreamJobManager* GetJobManager() {
  return Singleton<SocketStreamJobManager>::get();
}

// static
SocketStreamJob::ProtocolFactory* SocketStreamJob::RegisterProtocolFactory(
    const std::string& scheme, ProtocolFactory* factory) {
  return GetJobManager()->RegisterProtocolFactory(scheme, factory);
}

// static
SocketStreamJob* SocketStreamJob::CreateSocketStreamJob(
    const GURL& url, SocketStream::Delegate* delegate) {
  return GetJobManager()->CreateJob(url, delegate);
}

}  // namespace net
