// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_STREAM_SOCKET_STREAM_JOB_MANAGER_H_
#define NET_SOCKET_STREAM_SOCKET_STREAM_JOB_MANAGER_H_
#pragma once

#include <map>
#include <string>

#include "net/socket_stream/socket_stream.h"
#include "net/socket_stream/socket_stream_job.h"

class GURL;

namespace net {

class SocketStreamJobManager {
 public:
  SocketStreamJobManager();
  ~SocketStreamJobManager();

  SocketStreamJob* CreateJob(
      const GURL& url, SocketStream::Delegate* delegate) const;

  SocketStreamJob::ProtocolFactory* RegisterProtocolFactory(
      const std::string& scheme, SocketStreamJob::ProtocolFactory* factory);

 private:
  typedef std::map<std::string, SocketStreamJob::ProtocolFactory*> FactoryMap;

  mutable Lock lock_;
  FactoryMap factories_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamJobManager);
};

}  // namespace net

#endif  // NET_SOCKET_STREAM_SOCKET_STREAM_JOB_MANAGER_H_
