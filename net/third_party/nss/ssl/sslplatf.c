/*
 * Platform specific crypto wrappers
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ryan Sleevi <ryan.sleevi@gmail.com>
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
/* $Id$ */
#include "ssl.h"
#include "certt.h"
#include "keythi.h"
#include "sslimpl.h"
#include "cryptohi.h"
#include "secitem.h"

#ifdef NSS_PLATFORM_CLIENT_AUTH
CERTCertificateList*
hack_NewCertificateListFromCertList(CERTCertList* list)
{
    CERTCertificateList * chain = NULL;
    PRArenaPool * arena = NULL;
    CERTCertListNode * node;
    int len;

    if (CERT_LIST_EMPTY(list))
        goto loser;

    arena = PORT_NewArena(4096);
    if (arena == NULL)
        goto loser;

    for (len = 0, node = CERT_LIST_HEAD(list); !CERT_LIST_END(node, list);
        len++, node = CERT_LIST_NEXT(node)) {
    }

    chain = PORT_ArenaNew(arena, CERTCertificateList);
    if (chain == NULL)
        goto loser;

    chain->certs = PORT_ArenaNewArray(arena, SECItem, len);
    if (!chain->certs)
        goto loser;
    chain->len = len;

    for (len = 0, node = CERT_LIST_HEAD(list); !CERT_LIST_END(node, list);
        len++, node = CERT_LIST_NEXT(node)) {
        // Check to see if the last cert to be sent is a self-signed cert,
        // and if so, omit it from the list of certificates. However, if
        // there is only one cert (len == 0), include the cert, as it means
        // the EE cert is self-signed.
        if (len > 0 && (len == chain->len - 1) && node->cert->isRoot) {
            chain->len = len;
            break;
        }
        SECITEM_CopyItem(arena, &chain->certs[len], &node->cert->derCert);
    }

    chain->arena = arena;
    return chain;

loser:
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    return NULL;
}

#if defined(XP_WIN32)
void
ssl_FreePlatformKey(PlatformKey key)
{
    if (key) {
        if (key->dwKeySpec != CERT_NCRYPT_KEY_SPEC)
            CryptReleaseContext(key->hCryptProv, 0);
        /* FIXME(rsleevi): Close CNG keys. */
        PORT_Free(key);
    }
}

void
ssl_FreePlatformAuthInfo(PlatformAuthInfo* info)
{
    if (info->provider != NULL) {
        PORT_Free(info->provider);
        info->provider = NULL;
    }
    if (info->container != NULL) {
        PORT_Free(info->container);
        info->container = NULL;
    }
    info->provType = 0;
}

void
ssl_InitPlatformAuthInfo(PlatformAuthInfo* info)
{
    info->provider  = NULL;
    info->container = NULL;
    info->provType  = 0;
}

PRBool
ssl_PlatformAuthTokenPresent(PlatformAuthInfo *info)
{
    HCRYPTPROV prov = 0;

    if (!info || !info->provider || !info->container)
        return PR_FALSE;

    if (!CryptAcquireContextA(&prov, info->container, info->provider,
                              info->provType, 0))
        return PR_FALSE;

    CryptReleaseContext(prov, 0);
    return PR_TRUE;
}

void
ssl_GetPlatformAuthInfoForKey(PlatformKey key,
                              PlatformAuthInfo *info)
{
    DWORD bytesNeeded = 0;
    ssl_InitPlatformAuthInfo(info);
    if (!key || key->dwKeySpec == CERT_NCRYPT_KEY_SPEC)
        goto error;

    bytesNeeded = sizeof(info->provType);
    if (!CryptGetProvParam(key->hCryptProv, PP_PROVTYPE,
                           (BYTE*)&info->provType, &bytesNeeded, 0))
        goto error;

    bytesNeeded = 0;
    if (!CryptGetProvParam(key->hCryptProv, PP_CONTAINER, NULL, &bytesNeeded,
                           0))
        goto error;
    info->container = (char*)PORT_Alloc(bytesNeeded);
    if (info->container == NULL)
        goto error;
    if (!CryptGetProvParam(key->hCryptProv, PP_CONTAINER,
                           (BYTE*)info->container, &bytesNeeded, 0))
        goto error;

    bytesNeeded = 0;
    if (!CryptGetProvParam(key->hCryptProv, PP_NAME, NULL, &bytesNeeded, 0))
        goto error;
    info->provider = (char*)PORT_Alloc(bytesNeeded);
    if (info->provider == NULL)
        goto error;
    if (!CryptGetProvParam(key->hCryptProv, PP_NAME, (BYTE*)info->provider,
                           &bytesNeeded, 0))
        goto error;

    goto done;
error:
    ssl_FreePlatformAuthInfo(info);

done:
    return;
}

SECStatus
ssl3_PlatformSignHashes(SSL3Hashes *hash, PlatformKey key, SECItem *buf, 
                        PRBool isTLS)
{
    SECStatus    rv             = SECFailure;
    PRBool       doDerEncode    = PR_FALSE;
    SECItem      hashItem;
    HCRYPTKEY    hKey           = 0;
    DWORD        argLen         = 0;
    ALG_ID       keyAlg         = 0;
    DWORD        signatureLen   = 0;
    ALG_ID       hashAlg        = 0;
    HCRYPTHASH   hHash          = 0;
    DWORD        hashLen        = 0;
    unsigned int i              = 0;

    buf->data = NULL;
    if (!CryptGetUserKey(key->hCryptProv, key->dwKeySpec, &hKey)) {
        if (GetLastError() == NTE_NO_KEY) {
            PORT_SetError(SEC_ERROR_NO_KEY);
        } else {
            PORT_SetError(SEC_ERROR_INVALID_KEY);
        }
        goto done;
    }

    argLen = sizeof(keyAlg);
    if (!CryptGetKeyParam(hKey, KP_ALGID, (BYTE*)&keyAlg, &argLen, 0)) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        goto done;
    }

    switch (keyAlg) {
        case CALG_RSA_KEYX:
        case CALG_RSA_SIGN:
            hashAlg       = CALG_SSL3_SHAMD5;
            hashItem.data = hash->md5;
            hashItem.len  = sizeof(SSL3Hashes);
            break;
        case CALG_DSS_SIGN:
        case CALG_ECDSA:
            if (keyAlg == CALG_ECDSA) {
                doDerEncode = PR_TRUE;
            } else {
                doDerEncode = isTLS;
            }
            hashAlg       = CALG_SHA1;
            hashItem.data = hash->sha;
            hashItem.len  = sizeof(hash->sha);
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_KEY);
            goto done;
    }
    PRINT_BUF(60, (NULL, "hash(es) to be signed", hashItem.data, hashItem.len));

    if (!CryptCreateHash(key->hCryptProv, hashAlg, 0, 0, &hHash)) {
        PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
        goto done;    
    }
    argLen = sizeof(hashLen);
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &argLen, 0)) {
        PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
        goto done;
    }
    if (hashLen != hashItem.len) {
        PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
        goto done;
    }
    if (!CryptSetHashParam(hHash, HP_HASHVAL, (BYTE*)hashItem.data, 0)) {
        PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
        goto done;
    }
    if (!CryptSignHash(hHash, key->dwKeySpec, NULL, 0,
                       NULL, &signatureLen) || signatureLen == 0) {
        PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
        goto done;
    }
    buf->data = (unsigned char *)PORT_Alloc(signatureLen);
    if (!buf->data)
        goto done;    /* error code was set. */

    if (!CryptSignHash(hHash, key->dwKeySpec, NULL, 0,
                       (BYTE*)buf->data, &signatureLen)) {
        PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
        goto done;
    }
    buf->len = signatureLen;

    /* CryptoAPI signs in little-endian, so reverse */
    for (i = 0; i < buf->len / 2; ++i) {
        unsigned char tmp = buf->data[i];
        buf->data[i] = buf->data[buf->len - 1 - i];
        buf->data[buf->len - 1 - i] = tmp;
    }
    if (doDerEncode) {
        SECItem   derSig = {siBuffer, NULL, 0};

        /* This also works for an ECDSA signature */
        rv = DSAU_EncodeDerSigWithLen(&derSig, buf, buf->len);
        if (rv == SECSuccess) {
            PORT_Free(buf->data);     /* discard unencoded signature. */
            *buf = derSig;            /* give caller encoded signature. */
        } else if (derSig.data) {
            PORT_Free(derSig.data);
        }
    } else {
        rv = SECSuccess;
    }

    PRINT_BUF(60, (NULL, "signed hashes", buf->data, buf->len));
done:
    if (hHash)
        CryptDestroyHash(hHash);
    if (hKey)
        CryptDestroyKey(hKey);
    if (rv != SECSuccess && buf->data) {
        PORT_Free(buf->data);
        buf->data = NULL;
    }
    return rv;
}

#elif defined(XP_MACOSX)
#include <Security/cssm.h>

/*
 * In Mac OS X 10.5, these two functions are private but implemented, and
 * in Mac OS X 10.6, these are exposed publicly.  To compile with the 10.5
 * SDK, we declare them here.
 */
OSStatus SecKeychainItemCreatePersistentReference(SecKeychainItemRef itemRef, CFDataRef *persistentItemRef);
OSStatus SecKeychainItemCopyFromPersistentReference(CFDataRef persistentItemRef, SecKeychainItemRef *itemRef);

void
ssl_FreePlatformKey(PlatformKey key)
{
    CFRelease(key);
}

void
ssl_FreePlatformAuthInfo(PlatformAuthInfo* info)
{
    if (info->keychain != NULL) {
        CFRelease(info->keychain);
        info->keychain = NULL;
    }
    if (info->persistentKey != NULL) {
        CFRelease(info->persistentKey);
        info->persistentKey = NULL;
    }
}

void
ssl_InitPlatformAuthInfo(PlatformAuthInfo* info)
{
    info->keychain = NULL;
    info->persistentKey = NULL;
}

PRBool
ssl_PlatformAuthTokenPresent(PlatformAuthInfo* info)
{
    if (!info || !info->keychain || !info->persistentKey)
        return PR_FALSE;

    // Not actually interested in the status, but it can be used to make sure
    // that the keychain still exists (as smart card ejection will remove
    // the keychain)
    SecKeychainStatus keychainStatus;
    OSStatus rv = SecKeychainGetStatus(info->keychain, &keychainStatus);
    if (rv != noErr)
        return PR_FALSE;

    // Make sure the individual key still exists within the keychain, if
    // the keychain is present
    SecKeychainItemRef keychainItem;
    rv = SecKeychainItemCopyFromPersistentReference(info->persistentKey,
                                                    &keychainItem);
    if (rv != noErr)
        return PR_FALSE;

    CFRelease(keychainItem);
    return PR_TRUE;
}

void
ssl_GetPlatformAuthInfoForKey(PlatformKey key,
                              PlatformAuthInfo *info)
{
    SecKeychainItemRef keychainItem = (SecKeychainItemRef)key;
    OSStatus rv = SecKeychainItemCopyKeychain(keychainItem, &info->keychain);
    if (rv == noErr) {
        rv = SecKeychainItemCreatePersistentReference(keychainItem,
                                                      &info->persistentKey);
    }
    if (rv != noErr) {
        ssl_FreePlatformAuthInfo(info);
    }
    return;
}

SECStatus
ssl3_PlatformSignHashes(SSL3Hashes *hash, PlatformKey key, SECItem *buf, 
                        PRBool isTLS)
{
    SECStatus       rv                  = SECFailure;
    PRBool          doDerEncode         = PR_FALSE;
    unsigned int    signatureLen;
    OSStatus        status              = noErr;
    CSSM_CSP_HANDLE cspHandle           = 0;
    const CSSM_KEY *cssmKey             = NULL;
    CSSM_ALGORITHMS sigAlg;
    const CSSM_ACCESS_CREDENTIALS * cssmCreds = NULL;
    CSSM_RETURN     cssmRv;
    CSSM_DATA       hashData;
    CSSM_DATA       signatureData;
    CSSM_CC_HANDLE  cssmSignature       = 0;

    buf->data = NULL;

    status = SecKeyGetCSPHandle(key, &cspHandle);
    if (status != noErr) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        goto done;
    }

    status = SecKeyGetCSSMKey(key, &cssmKey);
    if (status != noErr || !cssmKey) {
        PORT_SetError(SEC_ERROR_NO_KEY);
        goto done;
    }

    /* SecKeyGetBlockSize wasn't addeded until OS X 10.6 - but the
     * needed information is readily available on the key itself.
     */
    signatureLen = (cssmKey->KeyHeader.LogicalKeySizeInBits + 7) / 8;
    
    if (signatureLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        goto done;
    }

    buf->data = (unsigned char *)PORT_Alloc(signatureLen);
    if (!buf->data)
        goto done;    /* error code was set. */

    sigAlg = cssmKey->KeyHeader.AlgorithmId;
    switch (sigAlg) {
        case CSSM_ALGID_RSA:
            hashData.Data   = hash->md5;
            hashData.Length = sizeof(SSL3Hashes);
            break;
        case CSSM_ALGID_ECDSA:
        case CSSM_ALGID_DSA:
            if (sigAlg == CSSM_ALGID_ECDSA) {
                doDerEncode = PR_TRUE;
            } else {
                doDerEncode = isTLS;
            }
            hashData.Data   = hash->sha;
            hashData.Length = sizeof(hash->sha);
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_KEY);
            goto done;
    }
    PRINT_BUF(60, (NULL, "hash(es) to be signed", hashData.Data, hashData.Length));

    /* TODO(rsleevi): Should it be kSecCredentialTypeNoUI? In Win32, at least,
     * you can prevent the UI by setting the provider handle on the
     * certificate to be opened with CRYPT_SILENT, but is there an equivalent?
     */
    status = SecKeyGetCredentials(key, CSSM_ACL_AUTHORIZATION_SIGN,
                                  kSecCredentialTypeDefault, &cssmCreds);
    if (status != noErr) {
        PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
        goto done;
    }

    signatureData.Length = signatureLen;
    signatureData.Data   = (uint8*)buf->data;
    
    cssmRv = CSSM_CSP_CreateSignatureContext(cspHandle, sigAlg, cssmCreds,
                                             cssmKey, &cssmSignature);
    if (cssmRv) {
        PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
        goto done;
    }

    /* See "Apple Cryptographic Service Provider Functional Specification" */
    if (cssmKey->KeyHeader.AlgorithmId == CSSM_ALGID_RSA) {
        /* To set RSA blinding for RSA keys */
        CSSM_CONTEXT_ATTRIBUTE blindingAttr;
        blindingAttr.AttributeType   = CSSM_ATTRIBUTE_RSA_BLINDING;
        blindingAttr.AttributeLength = sizeof(uint32);
        blindingAttr.Attribute.Uint32 = 1;
        cssmRv = CSSM_UpdateContextAttributes(cssmSignature, 1, &blindingAttr);
        if (cssmRv) {
            PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
            goto done;
        }
    }

    cssmRv = CSSM_SignData(cssmSignature, &hashData, 1, CSSM_ALGID_NONE,
                           &signatureData);
    if (cssmRv) {
        PORT_SetError(SSL_ERROR_SIGN_HASHES_FAILURE);
        goto done;
    }
    buf->len = signatureData.Length;

    if (doDerEncode) {
        SECItem derSig = {siBuffer, NULL, 0};

        /* This also works for an ECDSA signature */
        rv = DSAU_EncodeDerSigWithLen(&derSig, buf, buf->len);
        if (rv == SECSuccess) {
            PORT_Free(buf->data);     /* discard unencoded signature. */
            *buf = derSig;            /* give caller encoded signature. */
        } else if (derSig.data) {
            PORT_Free(derSig.data);
        }
    } else {
        rv = SECSuccess;
    }

    PRINT_BUF(60, (NULL, "signed hashes", buf->data, buf->len));
done:
    /* cspHandle, cssmKey, and cssmCreds are owned by the SecKeyRef and
     * should not be freed. When the PlatformKey is freed, they will be
     * released.
     */
    if (cssmSignature)
        CSSM_DeleteContext(cssmSignature);

    if (rv != SECSuccess && buf->data) {
        PORT_Free(buf->data);
        buf->data = NULL;
    }
    return rv;
}
#else
void
ssl_FreePlatformKey(PlatformKey key)
{
}

void
ssl_FreePlatformAuthInfo(PlatformAuthInfo *info)
{
}

void
ssl_InitPlatformAuthInfo(PlatformAuthInfo *info)
{
}

PRBool
ssl_PlatformAuthTokenPresent(PlatformAuthInfo *info)
{
    return PR_FALSE;
}

void
ssl_GetPlatformAuthInfoForKey(PlatformKey key, PlatformAuthInfo *info)
{
}

SECStatus
ssl3_PlatformSignHashes(SSL3Hashes *hash, PlatformKey key, SECItem *buf,
                        PRBool isTLS)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}
#endif

#endif /* NSS_PLATFORM_CLIENT_AUTH */
