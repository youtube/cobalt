// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate_net_log_param.h"

#include <string>
#include <vector>

#include "base/values.h"
#include "net/base/x509_certificate.h"

namespace net {

base::Value* NetLogX509CertificateCallback(const X509Certificate* certificate,
                                           NetLog::LogLevel log_level) {
  DictionaryValue* dict = new DictionaryValue();
  ListValue* certs = new ListValue();
  std::vector<std::string> encoded_chain;
  certificate->GetPEMEncodedChain(&encoded_chain);
  for (size_t i = 0; i < encoded_chain.size(); ++i)
    certs->Append(base::Value::CreateStringValue(encoded_chain[i]));
  dict->Set("certificates", certs);
  return dict;
}

}  // namespace net
