// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <pk11pub.h>
#include <secmod.h>
#include <ssl.h>
#include <nssb64.h>    // NSSBase64_EncodeItem()
#include <secder.h>    // DER_Encode()
#include <cryptohi.h>  // SEC_DerSignData()
#include <keyhi.h>     // SECKEY_CreateSubjectPublicKeyInfo()
#undef Lock

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/nss_init.h"

namespace net {

CertDatabase::CertDatabase() {
  Init();
}

bool CertDatabase::AddUserCert(const char* data, int len) {
  CERTCertificate* cert = NULL;
  PK11SlotInfo* slot = NULL;
  std::string nickname;
  bool is_success = true;

  // Make a copy of "data" since CERT_DecodeCertPackage
  // might modify it.
  char* data_copy = new char[len];
  memcpy(data_copy, data, len);

  // Parse into a certificate structure.
  cert = CERT_DecodeCertFromPackage(data_copy, len);
  delete [] data_copy;
  if (!cert) {
    LOG(ERROR) << "Couldn't create a temporary certificate";
    return false;
  }

  // Check if the private key corresponding to the certificate exist
  // We shouldn't accept any random client certificate sent by a CA.

  // Note: The NSS source documentation wrongly suggests that this
  // also imports the certificate if the private key exists. This
  // doesn't seem to be the case.

  slot = PK11_KeyForCertExists(cert, NULL, NULL);
  if (!slot) {
    LOG(ERROR) << "No corresponding private key in store";
    CERT_DestroyCertificate(cert);
    return false;
  }
  PK11_FreeSlot(slot);
  slot = NULL;

  // TODO(gauravsh): We also need to make sure another certificate
  // doesn't already exist for the same private key.

  // Create a nickname for this certificate.
  // We use the scheme used by Firefox:
  // --> <subject's common name>'s <issuer's common name> ID.
  //

  std::string username, ca_name;
  char* temp_username = CERT_GetCommonName(&cert->subject);
  char* temp_ca_name = CERT_GetCommonName(&cert->issuer);
  if (temp_username) {
    username = temp_username;
    PORT_Free(temp_username);
  }
  if (temp_ca_name) {
    ca_name = temp_ca_name;
    PORT_Free(temp_ca_name);
  }
  nickname = username + "'s " + ca_name + " ID";

  slot = PK11_ImportCertForKey(cert,
                               const_cast<char*>(nickname.c_str()),
                               NULL);
  if (slot) {
    PK11_FreeSlot(slot);
  } else {
    LOG(ERROR) << "Couldn't import user certificate.";
    is_success = false;
  }
  CERT_DestroyCertificate(cert);
  return is_success;
}

void CertDatabase::Init() {
  base::EnsureNSSInit();
}

}  // namespace net
