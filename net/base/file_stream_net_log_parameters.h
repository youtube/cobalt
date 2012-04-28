// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File stream error reporting.

#ifndef NET_BASE_FILE_STREAM_NET_LOG_PARAMETERS_H_
#define NET_BASE_FILE_STREAM_NET_LOG_PARAMETERS_H_
#pragma once

#include <string>

#include "net/base/net_errors.h"
#include "net/base/net_log.h"

namespace net {

// NetLog parameters when a FileStream has an error.
class FileStreamErrorParameters : public net::NetLog::EventParameters {
 public:
  FileStreamErrorParameters(const std::string& operation,
                            int os_error,
                            net::Error net_error);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~FileStreamErrorParameters();

 private:
  std::string operation_;
  int os_error_;
  net::Error net_error_;

  DISALLOW_COPY_AND_ASSIGN(FileStreamErrorParameters);
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_NET_LOG_PARAMETERS_H_
