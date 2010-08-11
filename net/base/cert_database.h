// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_DATABASE_H_
#define NET_BASE_CERT_DATABASE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/ref_counted.h"

namespace net {

class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;

// This class provides functions to manipulate the local
// certificate store.

// TODO(gauravsh): This class could be augmented with methods
// for all operations that manipulate the underlying system
// certificate store.

class CertDatabase {
 public:
  CertDatabase();

  // Check whether this is a valid user cert that we have the private key for.
  // Returns OK or a network error code such as ERR_CERT_CONTAINS_ERRORS.
  int CheckUserCert(X509Certificate* cert);

  // Store user (client) certificate. Assumes CheckUserCert has already passed.
  // Returns OK, or ERR_ADD_USER_CERT_FAILED if there was a problem saving to
  // the platform cert database, or possibly other network error codes.
  int AddUserCert(X509Certificate* cert);

#if defined(USE_NSS)
  // Import certificates and private keys from PKCS #12 blob.
  // Returns OK or a network error code such as ERR_PKCS12_IMPORT_BAD_PASSWORD
  // or ERR_PKCS12_IMPORT_ERROR.
  int ImportFromPKCS12(const std::string& data, const string16& password);

  // Export the given certificates and private keys into a PKCS #12 blob,
  // storing into |output|.
  // Returns the number of certificates successfully exported.
  int ExportToPKCS12(const CertificateList& certs, const string16& password,
                     std::string* output);
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(CertDatabase);
};

}  // namespace net

#endif  // NET_BASE_CERT_DATABASE_H_
