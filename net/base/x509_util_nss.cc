// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_util.h"
#include "net/base/x509_util_nss.h"

#include <cert.h>
#include <cryptohi.h>
#include <pk11pub.h>
#include <prerror.h>
#include <secmod.h>
#include <secport.h>

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_nss_types.h"

namespace {

class ObCertOIDWrapper {
 public:
  static ObCertOIDWrapper* GetInstance() {
    // Instantiated as a leaky singleton to allow the singleton to be
    // constructed on a worker thead that is not joined when a process
    // shuts down.
    return Singleton<ObCertOIDWrapper,
                     LeakySingletonTraits<ObCertOIDWrapper> >::get();
  }

  SECOidTag ob_cert_oid_tag() const {
    return ob_cert_oid_tag_;
  }

 private:
  friend struct DefaultSingletonTraits<ObCertOIDWrapper>;

  ObCertOIDWrapper();

  SECOidTag ob_cert_oid_tag_;

  DISALLOW_COPY_AND_ASSIGN(ObCertOIDWrapper);
};

ObCertOIDWrapper::ObCertOIDWrapper(): ob_cert_oid_tag_(SEC_OID_UNKNOWN) {
  // 1.3.6.1.4.1.11129.2.1.6
  // (iso.org.dod.internet.private.enterprises.google.googleSecurity.
  //  certificateExtensions.originBoundCertificate)
  static const uint8 kObCertOID[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x01, 0x06
  };
  SECOidData oid_data;
  memset(&oid_data, 0, sizeof(oid_data));
  oid_data.oid.data = const_cast<uint8*>(kObCertOID);
  oid_data.oid.len = sizeof(kObCertOID);
  oid_data.offset = SEC_OID_UNKNOWN;
  oid_data.desc = "Origin Bound Certificate";
  oid_data.mechanism = CKM_INVALID_MECHANISM;
  oid_data.supportedExtension = SUPPORTED_CERT_EXTENSION;
  ob_cert_oid_tag_ = SECOID_AddEntry(&oid_data);
  if (ob_cert_oid_tag_ == SEC_OID_UNKNOWN)
    LOG(ERROR) << "OB_CERT OID tag creation failed";
}

// Creates a Certificate object that may be passed to the SignCertificate
// method to generate an X509 certificate.
// Returns NULL if an error is encountered in the certificate creation
// process.
// Caller responsible for freeing returned certificate object.
CERTCertificate* CreateCertificate(
    SECKEYPublicKey* public_key,
    const std::string& subject,
    uint32 serial_number,
    base::TimeDelta valid_duration) {
  // Create info about public key.
  CERTSubjectPublicKeyInfo* spki =
      SECKEY_CreateSubjectPublicKeyInfo(public_key);
  if (!spki)
    return NULL;

  // Create the certificate request.
  CERTName* subject_name =
      CERT_AsciiToName(const_cast<char*>(subject.c_str()));
  CERTCertificateRequest* cert_request =
      CERT_CreateCertificateRequest(subject_name, spki, NULL);
  SECKEY_DestroySubjectPublicKeyInfo(spki);

  if (!cert_request) {
    PRErrorCode prerr = PR_GetError();
    LOG(ERROR) << "Failed to create certificate request: " << prerr;
    CERT_DestroyName(subject_name);
    return NULL;
  }

  PRTime now = PR_Now();
  PRTime not_after = now + valid_duration.InMicroseconds();

  // Note that the time is now in micro-second unit.
  CERTValidity* validity = CERT_CreateValidity(now, not_after);
  CERTCertificate* cert = CERT_CreateCertificate(serial_number, subject_name,
                                                 validity, cert_request);
  if (!cert) {
    PRErrorCode prerr = PR_GetError();
    LOG(ERROR) << "Failed to create certificate: " << prerr;
  }

  // Cleanup for resources used to generate the cert.
  CERT_DestroyName(subject_name);
  CERT_DestroyValidity(validity);
  CERT_DestroyCertificateRequest(cert_request);

  return cert;
}

// Signs a certificate object, with |key| generating a new X509Certificate
// and destroying the passed certificate object (even when NULL is returned).
// The logic of this method references SignCert() in NSS utility certutil:
// http://mxr.mozilla.org/security/ident?i=SignCert.
// Returns true on success or false if an error is encountered in the
// certificate signing process.
bool SignCertificate(
    CERTCertificate* cert,
    SECKEYPrivateKey* key) {
  // |arena| is used to encode the cert.
  PLArenaPool* arena = cert->arena;
  SECOidTag algo_id = SEC_GetSignatureAlgorithmOidTag(key->keyType,
                                                      SEC_OID_SHA1);
  if (algo_id == SEC_OID_UNKNOWN)
    return false;

  SECStatus rv = SECOID_SetAlgorithmID(arena, &cert->signature, algo_id, 0);
  if (rv != SECSuccess)
    return false;

  // Generate a cert of version 3.
  *(cert->version.data) = 2;
  cert->version.len = 1;

  SECItem der;
  der.len = 0;
  der.data = NULL;

  // Use ASN1 DER to encode the cert.
  void* encode_result = SEC_ASN1EncodeItem(
      arena, &der, cert, SEC_ASN1_GET(CERT_CertificateTemplate));
  if (!encode_result)
    return false;

  // Allocate space to contain the signed cert.
  SECItem* result = SECITEM_AllocItem(arena, NULL, 0);
  if (!result)
    return false;

  // Sign the ASN1 encoded cert and save it to |result|.
  rv = SEC_DerSignData(arena, result, der.data, der.len, key, algo_id);
  if (rv != SECSuccess)
    return false;

  // Save the signed result to the cert.
  cert->derCert = *result;

  return true;
}

}  // namespace

namespace net {

namespace x509_util {

CERTCertificate* CreateSelfSignedCert(
    SECKEYPublicKey* public_key,
    SECKEYPrivateKey* private_key,
    const std::string& subject,
    uint32 serial_number,
    base::TimeDelta valid_duration) {
  CERTCertificate* cert = CreateCertificate(public_key,
                                            subject,
                                            serial_number,
                                            valid_duration);
  if (!cert)
    return NULL;

  if (!SignCertificate(cert, private_key)) {
    CERT_DestroyCertificate(cert);
    return NULL;
  }

  return cert;
}

bool CreateOriginBoundCert(
    crypto::RSAPrivateKey* key,
    const std::string& origin,
    uint32 serial_number,
    base::TimeDelta valid_duration,
    std::string* der_cert) {
  DCHECK(key);

  SECKEYPublicKey* public_key;
  SECKEYPrivateKey* private_key;
#if defined(USE_NSS)
  public_key = key->public_key();
  private_key = key->key();
#else
  crypto::ScopedSECKEYPublicKey scoped_public_key;
  crypto::ScopedSECKEYPrivateKey scoped_private_key;
  {
    // Based on the NSS RSAPrivateKey::CreateFromPrivateKeyInfoWithParams.
    // This method currently leaks some memory.
    // See http://crbug.com/34742.
    ANNOTATE_SCOPED_MEMORY_LEAK;
    crypto::EnsureNSSInit();

    std::vector<uint8> key_data;
    key->ExportPrivateKey(&key_data);

    crypto::ScopedPK11Slot slot(crypto::GetPrivateNSSKeySlot());
    if (!slot.get())
      return NULL;

    SECItem der_private_key_info;
    der_private_key_info.data = const_cast<unsigned char*>(&key_data[0]);
    der_private_key_info.len = key_data.size();
    // Allow the private key to be used for key unwrapping, data decryption,
    // and signature generation.
    const unsigned int key_usage = KU_KEY_ENCIPHERMENT | KU_DATA_ENCIPHERMENT |
                                   KU_DIGITAL_SIGNATURE;
    SECStatus rv =  PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &der_private_key_info, NULL, NULL, PR_FALSE, PR_FALSE,
        key_usage, &private_key, NULL);
    scoped_private_key.reset(private_key);
    if (rv != SECSuccess) {
      NOTREACHED();
      return NULL;
    }

    public_key = SECKEY_ConvertToPublicKey(private_key);
    if (!public_key) {
      NOTREACHED();
      return NULL;
    }
    scoped_public_key.reset(public_key);
  }
#endif

  CERTCertificate* cert = CreateCertificate(public_key,
                                            "CN=anonymous.invalid",
                                            serial_number,
                                            valid_duration);

  if (!cert)
    return false;

  // Create opaque handle used to add extensions later.
  void* cert_handle;
  if ((cert_handle = CERT_StartCertExtensions(cert)) == NULL) {
    LOG(ERROR) << "Unable to get opaque handle for adding extensions";
    CERT_DestroyCertificate(cert);
    return false;
  }

  // Create SECItem for IA5String encoding.
  SECItem origin_string_item = {
    siAsciiString,
    (unsigned char*)origin.data(),
    origin.size()
  };

  // IA5Encode and arena allocate SECItem
  SECItem* asn1_origin_string = SEC_ASN1EncodeItem(
      cert->arena, NULL, &origin_string_item,
      SEC_ASN1_GET(SEC_IA5StringTemplate));
  if (asn1_origin_string == NULL) {
    LOG(ERROR) << "Unable to get ASN1 encoding for origin in ob_cert extension";
    CERT_DestroyCertificate(cert);
    return false;
  }

  // Add the extension to the opaque handle
  if (CERT_AddExtension(cert_handle,
                        ObCertOIDWrapper::GetInstance()->ob_cert_oid_tag(),
                        asn1_origin_string,
                        PR_TRUE, PR_TRUE) != SECSuccess){
    LOG(ERROR) << "Unable to add origin bound cert extension to opaque handle";
    CERT_DestroyCertificate(cert);
    return false;
  }

  // Copy extension into x509 cert
  if (CERT_FinishExtensions(cert_handle) != SECSuccess){
    LOG(ERROR) << "Unable to copy extension to X509 cert";
    CERT_DestroyCertificate(cert);
    return false;
  }

  if (!SignCertificate(cert, private_key)) {
    CERT_DestroyCertificate(cert);
    return false;
  }

  DCHECK(cert->derCert.len);
  // XXX copied from X509Certificate::GetDEREncoded
  der_cert->clear();
  der_cert->append(reinterpret_cast<char*>(cert->derCert.data),
                   cert->derCert.len);
  CERT_DestroyCertificate(cert);
  return true;
}

} // namespace x509_util

} // namespace net
