// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_CERT_NET_LOG_PARAM_H_
#define NET_BASE_X509_CERT_NET_LOG_PARAM_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "net/base/net_log.h"

namespace net {

class X509Certificate;

// NetLog parameter to describe an X509Certificate.
// Note: These parameters are memory intensive, due to PEM-encoding each
// certificate, thus should only be used when logging at NetLog::LOG_ALL.
class X509CertificateNetLogParam : public NetLog::EventParameters {
 public:
  explicit X509CertificateNetLogParam(X509Certificate* certificate);

  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~X509CertificateNetLogParam();

 private:
  std::vector<std::string> encoded_chain_;
};

}  // namespace net

#endif  // NET_BASE_X509_CERT_NET_LOG_PARAM_H_
