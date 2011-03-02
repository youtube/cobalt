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

#include "net/third_party/mozilla_security_manager/nsPKCS12Blob.h"

#include <pk11pub.h>
#include <pkcs12.h>
#include <p12plcy.h>
#include <secerr.h>

#include "base/crypto/scoped_nss_types.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/nss_util_internal.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

namespace mozilla_security_manager {

namespace {

// unicodeToItem
//
// For the NSS PKCS#12 library, must convert PRUnichars (shorts) to
// a buffer of octets.  Must handle byte order correctly.
// TODO: Is there a mozilla way to do this?  In the string lib?
void unicodeToItem(const PRUnichar *uni, SECItem *item)
{
  int len = 0;
  while (uni[len++] != 0);
  SECITEM_AllocItem(NULL, item, sizeof(PRUnichar) * len);
#ifdef IS_LITTLE_ENDIAN
  int i = 0;
  for (i=0; i<len; i++) {
    item->data[2*i  ] = (unsigned char )(uni[i] << 8);
    item->data[2*i+1] = (unsigned char )(uni[i]);
  }
#else
  memcpy(item->data, uni, item->len);
#endif
}

// write_export_data
// write bytes to the exported PKCS#12 data buffer
void write_export_data(void* arg, const char* buf, unsigned long len) {
  std::string* dest = reinterpret_cast<std::string*>(arg);
  dest->append(buf, len);
}

// nickname_collision
// what to do when the nickname collides with one already in the db.
// Based on P12U_NicknameCollisionCallback from nss/cmd/pk12util/pk12util.c
SECItem* PR_CALLBACK
nickname_collision(SECItem *old_nick, PRBool *cancel, void *wincx)
{
  char           *nick     = NULL;
  SECItem        *ret_nick = NULL;
  CERTCertificate* cert    = (CERTCertificate*)wincx;

  if (!cancel || !cert) {
    // pk12util calls this error user cancelled?
    return NULL;
  }

  if (!old_nick)
    VLOG(1) << "no nickname for cert in PKCS12 file.";

  nick = CERT_MakeCANickname(cert);
  if (!nick) {
    return NULL;
  }

  if(old_nick && old_nick->data && old_nick->len &&
     PORT_Strlen(nick) == old_nick->len &&
     !PORT_Strncmp((char *)old_nick->data, nick, old_nick->len)) {
    PORT_Free(nick);
    PORT_SetError(SEC_ERROR_IO);
    return NULL;
  }

  VLOG(1) << "using nickname " << nick;
  ret_nick = PORT_ZNew(SECItem);
  if(ret_nick == NULL) {
    PORT_Free(nick);
    return NULL;
  }

  ret_nick->data = (unsigned char *)nick;
  ret_nick->len = PORT_Strlen(nick);

  return ret_nick;
}

// pip_ucs2_ascii_conversion_fn
// required to be set by NSS (to do PKCS#12), but since we've already got
// unicode make this a no-op.
PRBool
pip_ucs2_ascii_conversion_fn(PRBool toUnicode,
                             unsigned char *inBuf,
                             unsigned int inBufLen,
                             unsigned char *outBuf,
                             unsigned int maxOutBufLen,
                             unsigned int *outBufLen,
                             PRBool swapBytes)
{
  CHECK_GE(maxOutBufLen, inBufLen);
  // do a no-op, since I've already got unicode.  Hah!
  *outBufLen = inBufLen;
  memcpy(outBuf, inBuf, inBufLen);
  return PR_TRUE;
}

// Based on nsPKCS12Blob::ImportFromFileHelper.
int
nsPKCS12Blob_ImportHelper(const char* pkcs12_data,
                          size_t pkcs12_len,
                          const string16& password,
                          bool try_zero_length_secitem,
                          PK11SlotInfo *slot)
{
  DCHECK(pkcs12_data);
  DCHECK(slot);
  int import_result = net::ERR_PKCS12_IMPORT_FAILED;
  SECStatus srv = SECSuccess;
  SEC_PKCS12DecoderContext *dcx = NULL;
  SECItem unicodePw;
  unicodePw.type = siBuffer;
  unicodePw.len = 0;
  unicodePw.data = NULL;
  if (!try_zero_length_secitem) {
    unicodeToItem(password.c_str(), &unicodePw);
  }

  // initialize the decoder
  dcx = SEC_PKCS12DecoderStart(&unicodePw, slot,
                               // wincx
                               NULL,
                               // dOpen, dClose, dRead, dWrite, dArg: NULL
                               // specifies default impl using memory buffer.
                               NULL, NULL, NULL, NULL, NULL);
  if (!dcx) {
    srv = SECFailure;
    goto finish;
  }
  // feed input to the decoder
  srv = SEC_PKCS12DecoderUpdate(dcx,
                                (unsigned char*)pkcs12_data,
                                pkcs12_len);
  if (srv) goto finish;
  // verify the blob
  srv = SEC_PKCS12DecoderVerify(dcx);
  if (srv) goto finish;
  // validate bags
  srv = SEC_PKCS12DecoderValidateBags(dcx, nickname_collision);
  if (srv) goto finish;
  // import cert and key
  srv = SEC_PKCS12DecoderImportBags(dcx);
  if (srv) goto finish;
  import_result = net::OK;
finish:
  // If srv != SECSuccess, NSS probably set a specific error code.
  // We should use that error code instead of inventing a new one
  // for every error possible.
  if (srv != SECSuccess) {
    if (SEC_ERROR_BAD_PASSWORD == PORT_GetError()) {
      import_result = net::ERR_PKCS12_IMPORT_BAD_PASSWORD;
    }
    else
    {
      LOG(ERROR) << "PKCS#12 import failed with error " << PORT_GetError();
      import_result = net::ERR_PKCS12_IMPORT_FAILED;
    }
  }
  // finish the decoder
  if (dcx)
    SEC_PKCS12DecoderFinish(dcx);
  SECITEM_ZfreeItem(&unicodePw, PR_FALSE);
  return import_result;
}

PRBool
isExtractable(SECKEYPrivateKey *privKey)
{
  SECItem value;
  PRBool  isExtractable = PR_FALSE;
  SECStatus rv;

  rv=PK11_ReadRawAttribute(PK11_TypePrivKey, privKey, CKA_EXTRACTABLE, &value);
  if (rv != SECSuccess) {
    return PR_FALSE;
  }
  if ((value.len == 1) && (value.data != NULL)) {
    isExtractable = !!(*(CK_BBOOL*)value.data);
  }
  SECITEM_FreeItem(&value, PR_FALSE);
  return isExtractable;
}

class PKCS12InitSingleton {
 public:
  // From the PKCS#12 section of nsNSSComponent::InitializeNSS in
  // nsNSSComponent.cpp.
  PKCS12InitSingleton() {
    // Enable ciphers for PKCS#12
    SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
    SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
    SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
    SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);

    // Set no-op ascii-ucs2 conversion function to work around weird NSS
    // interface.  Thankfully, PKCS12 appears to be the only thing in NSS that
    // uses PORT_UCS2_ASCIIConversion, so this doesn't break anything else.
    PORT_SetUCS2_ASCIIConversionFunction(pip_ucs2_ascii_conversion_fn);
  }
};

static base::LazyInstance<PKCS12InitSingleton> g_pkcs12_init_singleton(
    base::LINKER_INITIALIZED);

}  // namespace

void EnsurePKCS12Init() {
  g_pkcs12_init_singleton.Get();
}

// Based on nsPKCS12Blob::ImportFromFile.
int nsPKCS12Blob_Import(PK11SlotInfo* slot,
                        const char* pkcs12_data,
                        size_t pkcs12_len,
                        const string16& password) {

  int rv = nsPKCS12Blob_ImportHelper(pkcs12_data, pkcs12_len, password, false,
                                     slot);

  // When the user entered a zero length password:
  //   An empty password should be represented as an empty
  //   string (a SECItem that contains a single terminating
  //   NULL UTF16 character), but some applications use a
  //   zero length SECItem.
  //   We try both variations, zero length item and empty string,
  //   without giving a user prompt when trying the different empty password flavors.
  if (rv == net::ERR_PKCS12_IMPORT_BAD_PASSWORD && password.empty()) {
    rv = nsPKCS12Blob_ImportHelper(pkcs12_data, pkcs12_len, password, true,
                                   slot);
  }
  return rv;
}

// Based on nsPKCS12Blob::ExportToFile
//
// Having already loaded the certs, form them into a blob (loading the keys
// also), encode the blob, and stuff it into the file.
//
// TODO: handle slots correctly
//       mirror "slotToUse" behavior from PSM 1.x
//       verify the cert array to start off with?
//       set appropriate error codes
int
nsPKCS12Blob_Export(std::string* output,
                    const net::CertificateList& certs,
                    const string16& password)
{
  int return_count = 0;
  SECStatus srv = SECSuccess;
  SEC_PKCS12ExportContext *ecx = NULL;
  SEC_PKCS12SafeInfo *certSafe = NULL, *keySafe = NULL;
  SECItem unicodePw;
  unicodePw.type = siBuffer;
  unicodePw.len = 0;
  unicodePw.data = NULL;

  int numCertsExported = 0;

  // get file password (unicode)
  unicodeToItem(password.c_str(), &unicodePw);

  // what about slotToUse in psm 1.x ???
  // create export context
  ecx = SEC_PKCS12CreateExportContext(NULL, NULL, NULL /*slot*/, NULL);
  if (!ecx) {
    srv = SECFailure;
    goto finish;
  }
  // add password integrity
  srv = SEC_PKCS12AddPasswordIntegrity(ecx, &unicodePw, SEC_OID_SHA1);
  if (srv) goto finish;

  for (size_t i=0; i<certs.size(); i++) {
    DCHECK(certs[i]);
    CERTCertificate* nssCert = certs[i]->os_cert_handle();
    DCHECK(nssCert);

    // We can only successfully export certs that are on 
    // internal token.  Most, if not all, smart card vendors
    // won't let you extract the private key (in any way
    // shape or form) from the card.  So let's punt if 
    // the cert is not in the internal db.
    if (nssCert->slot && !PK11_IsInternal(nssCert->slot)) {
      // we aren't the internal token, see if the key is extractable.
      SECKEYPrivateKey *privKey=PK11_FindKeyByDERCert(nssCert->slot,
                                                      nssCert,
                                                      NULL /* wincx */);

      if (privKey) {
        PRBool privKeyIsExtractable = isExtractable(privKey);

        SECKEY_DestroyPrivateKey(privKey);

        if (!privKeyIsExtractable) {
          LOG(ERROR) << "private key not extractable";
          // TODO(mattm): firefox has a notification dialog about trying to
          // export from a smartcard.  I don't think we support smartcards, so
          // we can ignore that for now.
          continue;
        }
      }
    }

    // XXX this is why, to verify the slot is the same
    // PK11_FindObjectForCert(nssCert, NULL, slot);
    // create the cert and key safes
    keySafe = SEC_PKCS12CreateUnencryptedSafe(ecx);
    if (!SEC_PKCS12IsEncryptionAllowed() || PK11_IsFIPS()) {
      certSafe = keySafe;
    } else {
      certSafe = SEC_PKCS12CreatePasswordPrivSafe(ecx, &unicodePw,
                           SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC);
    }
    if (!certSafe || !keySafe) {
      LOG(ERROR) << "!certSafe || !keySafe " << certSafe << " " << keySafe;
      srv = SECFailure;
      goto finish;
    }
    // add the cert and key to the blob
    srv = SEC_PKCS12AddCertAndKey(ecx, certSafe, NULL, nssCert,
                                  CERT_GetDefaultCertDB(),
                                  keySafe, NULL, PR_TRUE, &unicodePw,
                      SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC);
    if (srv) goto finish;
    ++numCertsExported;
  }

  if (!numCertsExported) goto finish;

  // encode and write
  srv = SEC_PKCS12Encode(ecx, write_export_data, output);
  if (srv) goto finish;
  return_count = numCertsExported;
finish:
  if (srv)
    LOG(ERROR) << "PKCS#12 export failed with error " << PORT_GetError();
  if (ecx)
    SEC_PKCS12DestroyExportContext(ecx);
  SECITEM_ZfreeItem(&unicodePw, PR_FALSE);
  return return_count;
}

}  // namespace mozilla_security_manager
