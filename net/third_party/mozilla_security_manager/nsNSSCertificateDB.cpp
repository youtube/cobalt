 /* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"

#include <cert.h>
#include <pk11pub.h>
#include <secerr.h>

#include "base/crypto/scoped_nss_types.h"
#include "base/logging.h"
#include "base/nss_util_internal.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertTrust.h"

namespace mozilla_security_manager {

// Based on nsNSSCertificateDB::handleCACertDownload, minus the UI bits.
bool ImportCACerts(const net::CertificateList& certificates,
                   net::X509Certificate* root,
                   unsigned int trustBits,
                   net::CertDatabase::ImportCertFailureList* not_imported) {
  base::ScopedPK11Slot slot(base::GetDefaultNSSKeySlot());
  if (!slot.get()) {
    LOG(ERROR) << "Couldn't get internal key slot!";
    return false;
  }

  // Mozilla had some code here to check if a perm version of the cert exists
  // already and use that, but CERT_NewTempCertificate actually does that
  // itself, so we skip it here.

  if (!CERT_IsCACert(root->os_cert_handle(), NULL)) {
    not_imported->push_back(net::CertDatabase::ImportCertFailure(
        root, net::ERR_IMPORT_CA_CERT_NOT_CA));
  } else if (root->os_cert_handle()->isperm) {
    // Mozilla just returns here, but we continue in case there are other certs
    // in the list which aren't already imported.
    // TODO(mattm): should we set/add trust if it differs from the present
    // settings?
    not_imported->push_back(net::CertDatabase::ImportCertFailure(
        root, net::ERR_IMPORT_CERT_ALREADY_EXISTS));
  } else {
    // Mozilla uses CERT_AddTempCertToPerm, however it is privately exported,
    // and it doesn't take the slot as an argument either.  Instead, we use
    // PK11_ImportCert and CERT_ChangeCertTrust.
    char* nickname = CERT_MakeCANickname(root->os_cert_handle());
    if (!nickname)
      return false;
    SECStatus srv = PK11_ImportCert(slot.get(), root->os_cert_handle(),
                                    CK_INVALID_HANDLE,
                                    nickname,
                                    PR_FALSE /* includeTrust (unused) */);
    PORT_Free(nickname);
    if (srv != SECSuccess) {
      LOG(ERROR) << "PK11_ImportCert failed with error " << PORT_GetError();
      return false;
    }
    if (!SetCertTrust(root, net::CA_CERT, trustBits))
      return false;
  }

  PRTime now = PR_Now();
  // Import additional delivered certificates that can be verified.
  // This is sort of merged in from Mozilla's ImportValidCACertsInList.  Mozilla
  // uses CERT_FilterCertListByUsage to filter out non-ca certs, but we want to
  // keep using X509Certificates, so that we can use them to build the
  // |not_imported| result.  So, we keep using our net::CertificateList and
  // filter it ourself.
  for (size_t i = 0; i < certificates.size(); i++) {
    const scoped_refptr<net::X509Certificate>& cert = certificates[i];
    if (cert == root) {
      // we already processed that one
      continue;
    }

    // Mozilla uses CERT_FilterCertListByUsage(certList, certUsageAnyCA,
    // PR_TRUE).  Afaict, checking !CERT_IsCACert on each cert is equivalent.
    if (!CERT_IsCACert(cert->os_cert_handle(), NULL)) {
      not_imported->push_back(net::CertDatabase::ImportCertFailure(
          cert, net::ERR_IMPORT_CA_CERT_NOT_CA));
      VLOG(1) << "skipping cert (non-ca)";
      continue;
    }

    if (cert->os_cert_handle()->isperm) {
      not_imported->push_back(net::CertDatabase::ImportCertFailure(
          cert, net::ERR_IMPORT_CERT_ALREADY_EXISTS));
      VLOG(1) << "skipping cert (perm)";
      continue;
    }

    if (CERT_VerifyCert(CERT_GetDefaultCertDB(), cert->os_cert_handle(),
        PR_TRUE, certUsageVerifyCA, now, NULL, NULL) != SECSuccess) {
      // TODO(mattm): use better error code (map PORT_GetError to an appropriate
      // error value).  (maybe make MapSecurityError or MapCertErrorToCertStatus
      // public.)
      not_imported->push_back(net::CertDatabase::ImportCertFailure(
          cert, net::ERR_FAILED));
      VLOG(1) << "skipping cert (verify) " << PORT_GetError();
      continue;
    }

    // Mozilla uses CERT_ImportCerts, which doesn't take a slot arg.  We use
    // PK11_ImportCert instead.
    char* nickname = CERT_MakeCANickname(cert->os_cert_handle());
    if (!nickname)
      return false;
    SECStatus srv = PK11_ImportCert(slot.get(), cert->os_cert_handle(),
                                    CK_INVALID_HANDLE,
                                    nickname,
                                    PR_FALSE /* includeTrust (unused) */);
    PORT_Free(nickname);
    if (srv != SECSuccess) {
      LOG(ERROR) << "PK11_ImportCert failed with error " << PORT_GetError();
      // TODO(mattm): Should we bail or continue on error here?  Mozilla doesn't
      // check error code at all.
      not_imported->push_back(net::CertDatabase::ImportCertFailure(
          cert, net::ERR_IMPORT_CA_CERT_FAILED));
    }
  }

  // Any errors importing individual certs will be in listed in |not_imported|.
  return true;
}

// Based on nsNSSCertificateDB::ImportServerCertificate.
bool ImportServerCert(const net::CertificateList& certificates,
                      net::CertDatabase::ImportCertFailureList* not_imported) {
  base::ScopedPK11Slot slot(base::GetDefaultNSSKeySlot());
  if (!slot.get()) {
    LOG(ERROR) << "Couldn't get internal key slot!";
    return false;
  }

  for (size_t i = 0; i < certificates.size(); ++i) {
    const scoped_refptr<net::X509Certificate>& cert = certificates[i];

    // Mozilla uses CERT_ImportCerts, which doesn't take a slot arg.  We use
    // PK11_ImportCert instead.
    SECStatus srv = PK11_ImportCert(slot.get(), cert->os_cert_handle(),
                                    CK_INVALID_HANDLE,
                                    cert->subject().GetDisplayName().c_str(),
                                    PR_FALSE /* includeTrust (unused) */);
    if (srv != SECSuccess) {
      LOG(ERROR) << "PK11_ImportCert failed with error " << PORT_GetError();
      not_imported->push_back(net::CertDatabase::ImportCertFailure(
          cert, net::ERR_IMPORT_SERVER_CERT_FAILED));
      continue;
    }
  }

  // Set as valid peer, but without any extra trust.
  SetCertTrust(certificates[0].get(), net::SERVER_CERT,
               net::CertDatabase::UNTRUSTED);
  // TODO(mattm): Report SetCertTrust result?  Putting in not_imported
  // wouldn't quite match up since it was imported...

  // Any errors importing individual certs will be in listed in |not_imported|.
  return true;
}

// Based on nsNSSCertificateDB::SetCertTrust.
bool
SetCertTrust(const net::X509Certificate* cert,
             net::CertType type,
             unsigned int trusted)
{
  SECStatus srv;
  nsNSSCertTrust trust;
  CERTCertificate *nsscert = cert->os_cert_handle();
  if (type == net::CA_CERT) {
    // always start with untrusted and move up
    trust.SetValidCA();
    trust.AddCATrust(trusted & net::CertDatabase::TRUSTED_SSL,
                     trusted & net::CertDatabase::TRUSTED_EMAIL,
                     trusted & net::CertDatabase::TRUSTED_OBJ_SIGN);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(),
                               nsscert,
                               trust.GetTrust());
  } else if (type == net::SERVER_CERT) {
    // always start with untrusted and move up
    trust.SetValidPeer();
    trust.AddPeerTrust(trusted & net::CertDatabase::TRUSTED_SSL, 0, 0);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(),
                               nsscert,
                               trust.GetTrust());
  } else if (type == net::EMAIL_CERT) {
    // always start with untrusted and move up
    trust.SetValidPeer();
    trust.AddPeerTrust(0, trusted & net::CertDatabase::TRUSTED_EMAIL, 0);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(),
                               nsscert,
                               trust.GetTrust());
  } else {
    // ignore user certs
    return true;
  }
  if (srv != SECSuccess)
    LOG(ERROR) << "SetCertTrust failed with error " << PORT_GetError();
  return srv == SECSuccess;
}

}  // namespace mozilla_security_manager
