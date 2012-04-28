// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate_net_log_param.h"

#include "base/values.h"
#include "net/base/x509_certificate.h"

namespace net {

X509CertificateNetLogParam::X509CertificateNetLogParam(
    X509Certificate* certificate) {
  certificate->GetPEMEncodedChain(&encoded_chain_);
}

base::Value* X509CertificateNetLogParam::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  ListValue* certs = new ListValue();
  for (size_t i = 0; i < encoded_chain_.size(); ++i)
    certs->Append(base::Value::CreateStringValue(encoded_chain_[i]));
  dict->Set("certificates", certs);
  return dict;
}

X509CertificateNetLogParam::~X509CertificateNetLogParam() {}

}  // namespace net
