// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/nss_cert_database.h"

#include <cert.h>
#include <certdb.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <secmod.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "net/base/cert_database.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"
#include "net/third_party/mozilla_security_manager/nsPKCS12Blob.h"

// In NSS 3.13, CERTDB_VALID_PEER was renamed CERTDB_TERMINAL_RECORD. So we use
// the new name of the macro.
#if !defined(CERTDB_TERMINAL_RECORD)
#define CERTDB_TERMINAL_RECORD CERTDB_VALID_PEER
#endif

// PSM = Mozilla's Personal Security Manager.
namespace psm = mozilla_security_manager;

namespace net {

NSSCertDatabase::ImportCertFailure::ImportCertFailure(
    X509Certificate* cert, int err)
    : certificate(cert),
      net_error(err) {}

NSSCertDatabase::ImportCertFailure::~ImportCertFailure() {}

// static
NSSCertDatabase* NSSCertDatabase::GetInstance() {
  return Singleton<NSSCertDatabase>::get();
}

NSSCertDatabase::NSSCertDatabase()
    : observer_list_(new ObserverListThreadSafe<Observer>) {
  crypto::EnsureNSSInit();
  psm::EnsurePKCS12Init();
}

NSSCertDatabase::~NSSCertDatabase() {}

void NSSCertDatabase::ListCerts(CertificateList* certs) {
  certs->clear();

  CERTCertList* cert_list = PK11_ListCerts(PK11CertListUnique, NULL);
  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    certs->push_back(X509Certificate::CreateFromHandle(
        node->cert, X509Certificate::OSCertHandles()));
  }
  CERT_DestroyCertList(cert_list);
}

CryptoModule* NSSCertDatabase::GetPublicModule() const {
  CryptoModule* module =
      CryptoModule::CreateFromHandle(crypto::GetPublicNSSKeySlot());
  // The module is already referenced when returned from
  // GetPublicNSSKeySlot, so we need to deref it once.
  PK11_FreeSlot(module->os_module_handle());

  return module;
}

CryptoModule* NSSCertDatabase::GetPrivateModule() const {
  CryptoModule* module =
      CryptoModule::CreateFromHandle(crypto::GetPrivateNSSKeySlot());
  // The module is already referenced when returned from
  // GetPrivateNSSKeySlot, so we need to deref it once.
  PK11_FreeSlot(module->os_module_handle());

  return module;
}

void NSSCertDatabase::ListModules(CryptoModuleList* modules,
                                  bool need_rw) const {
  modules->clear();

  PK11SlotList* slot_list = NULL;
  // The wincx arg is unused since we don't call PK11_SetIsLoggedInFunc.
  slot_list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
                                need_rw ? PR_TRUE : PR_FALSE,  // needRW
                                PR_TRUE,  // loadCerts (unused)
                                NULL);  // wincx
  if (!slot_list) {
    LOG(ERROR) << "PK11_GetAllTokens failed: " << PORT_GetError();
    return;
  }

  PK11SlotListElement* slot_element = PK11_GetFirstSafe(slot_list);
  while (slot_element) {
    modules->push_back(CryptoModule::CreateFromHandle(slot_element->slot));
    slot_element = PK11_GetNextSafe(slot_list, slot_element,
                                    PR_FALSE);  // restart
  }

  PK11_FreeSlotList(slot_list);
}

int NSSCertDatabase::ImportFromPKCS12(
    CryptoModule* module,
    const std::string& data,
    const string16& password,
    bool is_extractable,
    net::CertificateList* imported_certs) {
  int result = psm::nsPKCS12Blob_Import(module->os_module_handle(),
                                        data.data(), data.size(),
                                        password,
                                        is_extractable,
                                        imported_certs);
  if (result == net::OK)
    NotifyObserversOfCertAdded(NULL);

  return result;
}

int NSSCertDatabase::ExportToPKCS12(
    const CertificateList& certs,
    const string16& password,
    std::string* output) const {
  return psm::nsPKCS12Blob_Export(output, certs, password);
}

X509Certificate* NSSCertDatabase::FindRootInList(
    const CertificateList& certificates) const {
  DCHECK_GT(certificates.size(), 0U);

  if (certificates.size() == 1)
    return certificates[0].get();

  X509Certificate* cert0 = certificates[0];
  X509Certificate* cert1 = certificates[1];
  X509Certificate* certn_2 = certificates[certificates.size() - 2];
  X509Certificate* certn_1 = certificates[certificates.size() - 1];

  if (CERT_CompareName(&cert1->os_cert_handle()->issuer,
                       &cert0->os_cert_handle()->subject) == SECEqual)
    return cert0;
  if (CERT_CompareName(&certn_2->os_cert_handle()->issuer,
                       &certn_1->os_cert_handle()->subject) == SECEqual)
    return certn_1;

  VLOG(1) << "certificate list is not a hierarchy";
  return cert0;
}

bool NSSCertDatabase::ImportCACerts(const CertificateList& certificates,
                                    TrustBits trust_bits,
                                    ImportCertFailureList* not_imported) {
  X509Certificate* root = FindRootInList(certificates);
  bool success = psm::ImportCACerts(certificates, root, trust_bits,
                                    not_imported);
  if (success)
    NotifyObserversOfCertTrustChanged(NULL);

  return success;
}

bool NSSCertDatabase::ImportServerCert(const CertificateList& certificates,
                                       TrustBits trust_bits,
                                       ImportCertFailureList* not_imported) {
  return psm::ImportServerCert(certificates, trust_bits, not_imported);
}

NSSCertDatabase::TrustBits NSSCertDatabase::GetCertTrust(
    const X509Certificate* cert,
    CertType type) const {
  CERTCertTrust trust;
  SECStatus srv = CERT_GetCertTrust(cert->os_cert_handle(), &trust);
  if (srv != SECSuccess) {
    LOG(ERROR) << "CERT_GetCertTrust failed with error " << PORT_GetError();
    return TRUST_DEFAULT;
  }
  // We define our own more "friendly" TrustBits, which means we aren't able to
  // round-trip all possible NSS trust flag combinations.  We try to map them in
  // a sensible way.
  switch (type) {
    case CA_CERT: {
      const unsigned kTrustedCA = CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA;
      const unsigned kCAFlags = kTrustedCA | CERTDB_TERMINAL_RECORD;

      TrustBits trust_bits = TRUST_DEFAULT;
      if ((trust.sslFlags & kCAFlags) == CERTDB_TERMINAL_RECORD)
        trust_bits |= DISTRUSTED_SSL;
      else if (trust.sslFlags & kTrustedCA)
        trust_bits |= TRUSTED_SSL;

      if ((trust.emailFlags & kCAFlags) == CERTDB_TERMINAL_RECORD)
        trust_bits |= DISTRUSTED_EMAIL;
      else if (trust.emailFlags & kTrustedCA)
        trust_bits |= TRUSTED_EMAIL;

      if ((trust.objectSigningFlags & kCAFlags) == CERTDB_TERMINAL_RECORD)
        trust_bits |= DISTRUSTED_OBJ_SIGN;
      else if (trust.objectSigningFlags & kTrustedCA)
        trust_bits |= TRUSTED_OBJ_SIGN;

      return trust_bits;
    }
    case SERVER_CERT:
      if (trust.sslFlags & CERTDB_TERMINAL_RECORD) {
        if (trust.sslFlags & CERTDB_TRUSTED)
          return TRUSTED_SSL;
        return DISTRUSTED_SSL;
      }
      return TRUST_DEFAULT;
    default:
      return TRUST_DEFAULT;
  }
}

bool NSSCertDatabase::IsUntrusted(const X509Certificate* cert) const {
  CERTCertTrust nsstrust;
  SECStatus rv = CERT_GetCertTrust(cert->os_cert_handle(), &nsstrust);
  if (rv != SECSuccess) {
    LOG(ERROR) << "CERT_GetCertTrust failed with error " << PORT_GetError();
    return false;
  }

  // The CERTCertTrust structure contains three trust records:
  // sslFlags, emailFlags, and objectSigningFlags.  The three
  // trust records are independent of each other.
  //
  // If the CERTDB_TERMINAL_RECORD bit in a trust record is set,
  // then that trust record is a terminal record.  A terminal
  // record is used for explicit trust and distrust of an
  // end-entity or intermediate CA cert.
  //
  // In a terminal record, if neither CERTDB_TRUSTED_CA nor
  // CERTDB_TRUSTED is set, then the terminal record means
  // explicit distrust.  On the other hand, if the terminal
  // record has either CERTDB_TRUSTED_CA or CERTDB_TRUSTED bit
  // set, then the terminal record means explicit trust.
  //
  // For a root CA, the trust record does not have
  // the CERTDB_TERMINAL_RECORD bit set.

  static const unsigned int kTrusted = CERTDB_TRUSTED_CA | CERTDB_TRUSTED;
  if ((nsstrust.sslFlags & CERTDB_TERMINAL_RECORD) != 0 &&
      (nsstrust.sslFlags & kTrusted) == 0) {
    return true;
  }
  if ((nsstrust.emailFlags & CERTDB_TERMINAL_RECORD) != 0 &&
      (nsstrust.emailFlags & kTrusted) == 0) {
    return true;
  }
  if ((nsstrust.objectSigningFlags & CERTDB_TERMINAL_RECORD) != 0 &&
      (nsstrust.objectSigningFlags & kTrusted) == 0) {
    return true;
  }

  // Self-signed certificates that don't have any trust bits set are untrusted.
  // Other certificates that don't have any trust bits set may still be trusted
  // if they chain up to a trust anchor.
  if (CERT_CompareName(&cert->os_cert_handle()->issuer,
                       &cert->os_cert_handle()->subject) == SECEqual) {
    return (nsstrust.sslFlags & kTrusted) == 0 &&
           (nsstrust.emailFlags & kTrusted) == 0 &&
           (nsstrust.objectSigningFlags & kTrusted) == 0;
  }

  return false;
}

bool NSSCertDatabase::SetCertTrust(const X509Certificate* cert,
                                CertType type,
                                TrustBits trust_bits) {
  bool success = psm::SetCertTrust(cert, type, trust_bits);
  if (success)
    NotifyObserversOfCertTrustChanged(cert);

  return success;
}

bool NSSCertDatabase::DeleteCertAndKey(const X509Certificate* cert) {
  // For some reason, PK11_DeleteTokenCertAndKey only calls
  // SEC_DeletePermCertificate if the private key is found.  So, we check
  // whether a private key exists before deciding which function to call to
  // delete the cert.
  SECKEYPrivateKey *privKey = PK11_FindKeyByAnyCert(cert->os_cert_handle(),
                                                    NULL);
  if (privKey) {
    SECKEY_DestroyPrivateKey(privKey);
    if (PK11_DeleteTokenCertAndKey(cert->os_cert_handle(), NULL)) {
      LOG(ERROR) << "PK11_DeleteTokenCertAndKey failed: " << PORT_GetError();
      return false;
    }
  } else {
    if (SEC_DeletePermCertificate(cert->os_cert_handle())) {
      LOG(ERROR) << "SEC_DeletePermCertificate failed: " << PORT_GetError();
      return false;
    }
  }

  NotifyObserversOfCertRemoved(cert);

  return true;
}

bool NSSCertDatabase::IsReadOnly(const X509Certificate* cert) const {
  PK11SlotInfo* slot = cert->os_cert_handle()->slot;
  return slot && PK11_IsReadOnly(slot);
}

void NSSCertDatabase::AddObserver(Observer* observer) {
  observer_list_->AddObserver(observer);
}

void NSSCertDatabase::RemoveObserver(Observer* observer) {
  observer_list_->RemoveObserver(observer);
}

void NSSCertDatabase::NotifyObserversOfCertAdded(const X509Certificate* cert) {
  observer_list_->Notify(&Observer::OnCertAdded, make_scoped_refptr(cert));
}

void NSSCertDatabase::NotifyObserversOfCertRemoved(
    const X509Certificate* cert) {
  observer_list_->Notify(&Observer::OnCertRemoved, make_scoped_refptr(cert));
}

void NSSCertDatabase::NotifyObserversOfCertTrustChanged(
    const X509Certificate* cert) {
  observer_list_->Notify(
      &Observer::OnCertTrustChanged, make_scoped_refptr(cert));
}

}  // namespace net
