// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_DATABASE_H_
#define NET_BASE_CERT_DATABASE_H_

#include "net/base/x509_certificate.h"

namespace net {

// This class provides functions to manipulate the local
// certificate store.

// TODO(gauravsh): This class could be augmented with methods
// for all operations that manipulate the underlying system
// certificate store.

class CertDatabase {
 public:
  CertDatabase();

  // Extract and Store User (Client) Certificate from a data blob.
  // Return true if successful.
  bool AddUserCert(const char* data, int len);

 private:
  void Init();
  DISALLOW_COPY_AND_ASSIGN(CertDatabase);
};

}  // namespace net

#endif  // NET_BASE_CERT_DATABASE_H_
