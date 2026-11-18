// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENSSL_HEADER_PEM_H
#define OPENSSL_HEADER_PEM_H

#include <openssl/base64.h>
#include <openssl/bio.h>
#include <openssl/cipher.h>
#include <openssl/digest.h>
#include <openssl/evp.h>
#include <openssl/pkcs7.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

// For compatibility with open-iscsi, which assumes that it can get
// `OPENSSL_malloc` from pem.h or err.h
#include <openssl/crypto.h>

#if defined(__cplusplus)
extern "C" {
#endif


// PEM.
//
// This library contains functions for reading and writing data encoded in PEM
// format. This format originated in Privacy-Enhanced Mail (RFC 1421). PEM
// consists of a series of PEM blocks, which are line-wrapped, base64-encoded
// data, wrapped in BEGIN and END lines, for example:
//
//   -----BEGIN PUBLIC KEY-----
//   MCowBQYDK2VwAyEAGb9ECWmEzf6FQbrBZ9w7lshQhqowtrbLDFw4rXAxZuE=
//   -----END PUBLIC KEY-----
//
// The BEGIN and END markers specify the type of data encoded. Multiple PEM
// blocks can be concatenated together. For example, it is common to represent a
// certificate chain as a series of PEM blocks of type CERTIFICATE. Owing to its
// email roots, PEM blocks can also be embedded in non-PEM data. The parsers in
// this library will generally skip over any non-PEM data, as well as any PEM
// blocks that are not of the expected type.
//
// PEM blocks can be encrypted with a password, specified in RFC 1423. PEM
// encryption is vulnerable to padding oracle attacks and should not be used as
// a load-bearing security measure. It is implemented for interoperability with
// legacy systems only.
//
// Callers should only use PEM for compatibility with legacy systems. PEM use
// should be limited to the components that directly interoperate with those
// systems, converting to and from more modern formats. PEM adds overhead to fit
// in email's 7-bit ASCII limitations, a constraint that is not relevant to most
// applications.


// API conventions.

// pem_password_cb is an application-supplied callback to supply a PEM password.
// On success, the callback should write the password into `out`, which has room
// for at most `max_out` bytes. On error, including if `max_out` is too small,
// it should return -1. `enc` is one if the password will be used for encrypting
// and zero if it is used for decrypting. `userdata` is the application-supplied
// parameter of the same name to the PEM function.
typedef int pem_password_cb(char *out, int max_out, int enc, void *userdata);

// PEM_def_callback is the default implementation of `pem_password_cb`. It
// interprets `userdata` as a NUL-terminated C string and outputs it according
// to `pem_password_cb`. If `userdata` is NULL, it returns -1.
//
// This differs from OpenSSL, which interactively prompts for a password by
// default.
OPENSSL_EXPORT int PEM_def_callback(char *buf, int size, int enc,
                                    void *userdata);

// The following sample functions document the calling conventions used by this
// library.

#if 0  // Sample functions

// PEM_read_bio_SAMPLE reads a PEM block from `bio`, skipping non-PEM data and
// unexpected block types. It then decodes the resulting PEM block and returns a
// newly-allocated `SAMPLE` object containing the parsed structure. If `out` is
// non-NULL, it additionally frees the previous value at `*out` and updates
// `*out` to the result.
//
// On decode or allocation error, or if EOF is reached before a matching PEM
// block is found, it returns NULL. In the error case, it will add
// `PEM_R_NO_START_LINE` to the error queue. Callers can check the error queue
// to distinguish these cases.
//
// If the PEM block is encrypted, `cb` will be called to look up the password.
// See `pem_password_cb` for details. If `cb` is NULL, `PEM_def_callback` is
// used and `userdata` should be a NUL-terminated C string containing the
// password. Set both `cb` and `userdata` to NULL to only handle plaintext
// blocks.
SAMPLE *PEM_read_bio_SAMPLE(BIO *bio, SAMPLE **out, pem_password_cb *cb,
                            void *userdata);

// PEM_write_bio_SAMPLE encodes `in` as a PEM block and writes it to `bio`. It
// returns one on success and zero on error.
//
// If `enc` is non-NULL, the PEM block is encrypted with the specified cipher
// and a password. If `pass` is non-NULL, `pass_len` bytes from `pass` is
// used as the password. Otherwise, `cb` is called. If `cb` is NULL,
// `PEM_def_callback` is used. PEM encryption is vulnerable to padding oracle
// attacks and should not be used.
//
// Some functions of this convention do not support encryption. In this case,
// the encryption-related parameters will be omitted.
//
// On error, the state of `bio` is undefined. It is possible a prefix of a PEM
// block was left in the `bio`.
int PEM_write_bio_SAMPLE(BIO *bio, const SAMPLE *in, const EVP_CIPHER *enc,
                         const uint8_t *pass, int pass_len, pem_password_cb *cb,
                         void *userdata);

#endif  // Sample functions


// Reading and writing objects as PEM.

// PEM_read_bio_X509 reads a PEM block of type "CERTIFICATE" or "X509
// CERTIFICATE", as described in `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT X509 *PEM_read_bio_X509(BIO *bio, X509 **out,
                                       pem_password_cb *cb, void *userdata);

// PEM_write_bio_X509 writes a PEM block of type "CERTIFICATE", as described in
// `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_X509(BIO *bio, const X509 *in);

// PEM_read_bio_X509_AUX reads a PEM block of type "CERTIFICATE", "X509
// CERTIFICATE", or "TRUSTED CERTIFICATE", as described in
// `PEM_read_bio_SAMPLE`.
//
// WARNING: This function parses auxiliary properties as in `d2i_X509_AUX`.
// Passing untrusted input to this function allows an attacker to influence
// those properties. See `d2i_X509_AUX` for details.
OPENSSL_EXPORT X509 *PEM_read_bio_X509_AUX(BIO *bio, X509 **out,
                                           pem_password_cb *cb, void *userdata);

// PEM_write_bio_X509_AUX writes a PEM block of type "TRUSTED CERTIFICATE", as
// described in `PEM_write_bio_SAMPLE`.
//
// WARNING: This function writes a custom OpenSSL-specific format that includes
// auxiliary properties. See `i2d_X509_AUX`.
OPENSSL_EXPORT int PEM_write_bio_X509_AUX(BIO *bio, const X509 *in);

// PEM_write_bio_X509_CRL writes a PEM block of type "X509 CRL", as described in
// `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_X509_CRL(BIO *bio, const X509_CRL *in);

// PEM_read_bio_X509_CRL reads a PEM block of type "X509 CRL", as described in
// `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT X509_CRL *PEM_read_bio_X509_CRL(BIO *bio, X509_CRL **out,
                                               pem_password_cb *cb,
                                               void *userdata);

// PEM_X509_INFO_read_bio reads PEM blocks from `bp` and decodes any
// certificates, CRLs, and private keys found. It returns a
// `STACK_OF(X509_INFO)` structure containing the results, or NULL on error.
//
// If `sk` is NULL, the result on success will be a newly-allocated
// `STACK_OF(X509_INFO)` structure which should be released with
// `sk_X509_INFO_pop_free` and `X509_INFO_free` when done.
//
// If `sk` is non-NULL, it appends the results to `sk` instead and returns `sk`
// on success. In this case, the caller retains ownership of `sk` in both
// success and failure.
//
// This function will decrypt any encrypted certificates in `bp`, using `cb`,
// but it will not decrypt encrypted private keys. Encrypted private keys are
// instead represented as placeholder `X509_INFO` objects with an empty `x_pkey`
// field. This allows this function to be used with inputs with unencrypted
// certificates, but encrypted passwords, without knowing the password. However,
// it also means that this function cannot be used to decrypt the private key
// when the password is known.
//
// WARNING: If the input contains "TRUSTED CERTIFICATE" PEM blocks, this
// function parses auxiliary properties as in `d2i_X509_AUX`. Passing untrusted
// input to this function allows an attacker to influence those properties. See
// `d2i_X509_AUX` for details.
OPENSSL_EXPORT STACK_OF(X509_INFO) *PEM_X509_INFO_read_bio(
    BIO *bp, STACK_OF(X509_INFO) *sk, pem_password_cb *cb, void *userdata);

// The following functions behave like corresponding `PEM_read_bio_*` function,
// but read from `fp`.
OPENSSL_EXPORT X509 *PEM_read_X509(FILE *fp, X509 **out, pem_password_cb *cb,
                                   void *userdata);
OPENSSL_EXPORT X509_CRL *PEM_read_X509_CRL(FILE *fp, X509_CRL **out,
                                           pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT X509 *PEM_read_X509_AUX(FILE *fp, X509 **out,
                                       pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT STACK_OF(X509_INFO) *PEM_X509_INFO_read(FILE *fp,
                                                       STACK_OF(X509_INFO) *sk,
                                                       pem_password_cb *cb,
                                                       void *userdata);

// The following functions behave like corresponding `PEM_write_bio_*` function,
// but write to `fp`.
OPENSSL_EXPORT int PEM_write_X509(FILE *fp, const X509 *x);
OPENSSL_EXPORT int PEM_write_X509_CRL(FILE *fp, const X509_CRL *in);
OPENSSL_EXPORT int PEM_write_X509_AUX(FILE *fp, const X509 *in);


// Not yet documented functions.
//
// TODO(crbug.com/42290574): Finish documenting and organizing this header.

#define PEM_BUFSIZE 1024

#define PEM_STRING_X509_OLD "X509 CERTIFICATE"
#define PEM_STRING_X509 "CERTIFICATE"
#define PEM_STRING_X509_PAIR "CERTIFICATE PAIR"
#define PEM_STRING_X509_TRUSTED "TRUSTED CERTIFICATE"
#define PEM_STRING_X509_REQ_OLD "NEW CERTIFICATE REQUEST"
#define PEM_STRING_X509_REQ "CERTIFICATE REQUEST"
#define PEM_STRING_X509_CRL "X509 CRL"
#define PEM_STRING_EVP_PKEY "ANY PRIVATE KEY"
#define PEM_STRING_PUBLIC "PUBLIC KEY"
#define PEM_STRING_RSA "RSA PRIVATE KEY"
#define PEM_STRING_RSA_PUBLIC "RSA PUBLIC KEY"
#define PEM_STRING_DSA "DSA PRIVATE KEY"
#define PEM_STRING_DSA_PUBLIC "DSA PUBLIC KEY"
#define PEM_STRING_EC "EC PRIVATE KEY"
#define PEM_STRING_PKCS7 "PKCS7"
#define PEM_STRING_PKCS7_SIGNED "PKCS #7 SIGNED DATA"
#define PEM_STRING_PKCS8 "ENCRYPTED PRIVATE KEY"
#define PEM_STRING_PKCS8INF "PRIVATE KEY"
#define PEM_STRING_DHPARAMS "DH PARAMETERS"
#define PEM_STRING_SSL_SESSION "SSL SESSION PARAMETERS"
#define PEM_STRING_DSAPARAMS "DSA PARAMETERS"
#define PEM_STRING_ECDSA_PUBLIC "ECDSA PUBLIC KEY"
#define PEM_STRING_ECPRIVATEKEY "EC PRIVATE KEY"
#define PEM_STRING_CMS "CMS"

#define PEM_TYPE_ENCRYPTED 10
#define PEM_TYPE_MIC_ONLY 20
#define PEM_TYPE_MIC_CLEAR 30
#define PEM_TYPE_CLEAR 40

#define DECLARE_PEM_read_fp(name, type)                      \
  OPENSSL_EXPORT type *PEM_read_##name(FILE *fp, type **out, \
                                       pem_password_cb *cb, void *userdata);

#define DECLARE_PEM_write_fp(name, type) \
  OPENSSL_EXPORT int PEM_write_##name(FILE *fp, const type *in);

#define DECLARE_PEM_write_cb_fp(name, type)                                    \
  OPENSSL_EXPORT int PEM_write_##name(FILE *fp, const type *in,                \
                                      const EVP_CIPHER *enc,                   \
                                      const unsigned char *pass, int pass_len, \
                                      pem_password_cb *cb, void *userdata);

#define DECLARE_PEM_read_bio(name, type)    \
  OPENSSL_EXPORT type *PEM_read_bio_##name( \
      BIO *bio, type **out, pem_password_cb *cb, void *userdata);

#define DECLARE_PEM_write_bio(name, type) \
  OPENSSL_EXPORT int PEM_write_bio_##name(BIO *bio, const type *in);

#define DECLARE_PEM_write_cb_bio(name, type)                        \
  OPENSSL_EXPORT int PEM_write_bio_##name(                          \
      BIO *bio, const type *in, const EVP_CIPHER *enc,              \
      const unsigned char *pass, int pass_len, pem_password_cb *cb, \
      void *userdata);

#define DECLARE_PEM_write(name, type) \
  DECLARE_PEM_write_bio(name, type)   \
  DECLARE_PEM_write_fp(name, type)

#define DECLARE_PEM_write_cb(name, type) \
  DECLARE_PEM_write_cb_bio(name, type)   \
  DECLARE_PEM_write_cb_fp(name, type)

#define DECLARE_PEM_read(name, type) \
  DECLARE_PEM_read_bio(name, type)   \
  DECLARE_PEM_read_fp(name, type)

#define DECLARE_PEM_rw(name, type) \
  DECLARE_PEM_read(name, type)     \
  DECLARE_PEM_write(name, type)

#define DECLARE_PEM_rw_cb(name, type) \
  DECLARE_PEM_read(name, type)        \
  DECLARE_PEM_write_cb(name, type)

// PEM_read_bio reads from `bp`, until the next PEM block. If one is found, it
// returns one and sets `*name`, `*header`, and `*data` to newly-allocated
// buffers containing the PEM type, the header block, and the decoded data,
// respectively. `*name` and `*header` are NUL-terminated C strings, while
// `*data` has `*len` bytes. The caller must release each of `*name`, `*header`,
// and `*data` with `OPENSSL_free` when done. If no PEM block is found, this
// function returns zero and pushes `PEM_R_NO_START_LINE` to the error queue. If
// one is found, but there is an error decoding it, it returns zero and pushes
// some other error to the error queue.
OPENSSL_EXPORT int PEM_read_bio(BIO *bp, char **name, char **header,
                                unsigned char **data, long *len);

// PEM_write_bio writes a PEM block to `bp`, containing `len` bytes from `data`
// as data. `name` and `hdr` are NUL-terminated C strings containing the PEM
// type and header block, respectively. This function returns zero on error and
// the number of bytes written on success.
OPENSSL_EXPORT int PEM_write_bio(BIO *bp, const char *name, const char *hdr,
                                 const unsigned char *data, long len);

OPENSSL_EXPORT int PEM_bytes_read_bio(unsigned char **pdata, long *plen,
                                      char **pnm, const char *expected_name,
                                      BIO *bp, pem_password_cb *cb, void *u);
OPENSSL_EXPORT void *PEM_ASN1_read_bio(d2i_of_void *d2i, const char *name,
                                       BIO *bp, void **x, pem_password_cb *cb,
                                       void *u);
OPENSSL_EXPORT int PEM_ASN1_write_bio(i2d_of_void *i2d, const char *name,
                                      BIO *bp, const void *x,
                                      const EVP_CIPHER *enc,
                                      const unsigned char *pass, int pass_len,
                                      pem_password_cb *cb, void *u);

OPENSSL_EXPORT int PEM_read(FILE *fp, char **name, char **header,
                            unsigned char **data, long *len);
OPENSSL_EXPORT int PEM_write(FILE *fp, const char *name, const char *hdr,
                             const unsigned char *data, long len);
OPENSSL_EXPORT void *PEM_ASN1_read(d2i_of_void *d2i, const char *name, FILE *fp,
                                   void **x, pem_password_cb *cb, void *u);
OPENSSL_EXPORT int PEM_ASN1_write(i2d_of_void *i2d, const char *name, FILE *fp,
                                  const void *x, const EVP_CIPHER *enc,
                                  const unsigned char *pass, int pass_len,
                                  pem_password_cb *callback, void *u);


DECLARE_PEM_rw(X509_REQ, X509_REQ)
DECLARE_PEM_write(X509_REQ_NEW, X509_REQ)


DECLARE_PEM_rw(PKCS7, PKCS7)
DECLARE_PEM_rw(PKCS8, X509_SIG)

DECLARE_PEM_rw(PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO)

DECLARE_PEM_rw_cb(RSAPrivateKey, RSA)

DECLARE_PEM_rw(RSAPublicKey, RSA)
DECLARE_PEM_rw(RSA_PUBKEY, RSA)

DECLARE_PEM_rw_cb(DSAPrivateKey, DSA)

DECLARE_PEM_rw(DSA_PUBKEY, DSA)

DECLARE_PEM_rw(DSAparams, DSA)

DECLARE_PEM_rw_cb(ECPrivateKey, EC_KEY)
DECLARE_PEM_rw(EC_PUBKEY, EC_KEY)


DECLARE_PEM_rw(DHparams, DH)


DECLARE_PEM_rw_cb(PrivateKey, EVP_PKEY)

DECLARE_PEM_rw(PUBKEY, EVP_PKEY)

OPENSSL_EXPORT int PEM_write_bio_PKCS8PrivateKey_nid(BIO *bp, const EVP_PKEY *x,
                                                     int nid, const char *pass,
                                                     int pass_len,
                                                     pem_password_cb *cb,
                                                     void *u);
OPENSSL_EXPORT int PEM_write_bio_PKCS8PrivateKey(BIO *bp, const EVP_PKEY *x,
                                                 const EVP_CIPHER *enc,
                                                 const char *pass, int pass_len,
                                                 pem_password_cb *cb, void *u);
OPENSSL_EXPORT int i2d_PKCS8PrivateKey_bio(BIO *bp, const EVP_PKEY *x,
                                           const EVP_CIPHER *enc,
                                           const char *pass, int pass_len,
                                           pem_password_cb *cb, void *u);
OPENSSL_EXPORT int i2d_PKCS8PrivateKey_nid_bio(BIO *bp, const EVP_PKEY *x,
                                               int nid, const char *pass,
                                               int pass_len,
                                               pem_password_cb *cb, void *u);
OPENSSL_EXPORT EVP_PKEY *d2i_PKCS8PrivateKey_bio(BIO *bp, EVP_PKEY **x,
                                                 pem_password_cb *cb, void *u);

OPENSSL_EXPORT int i2d_PKCS8PrivateKey_fp(FILE *fp, const EVP_PKEY *x,
                                          const EVP_CIPHER *enc,
                                          const char *pass, int pass_len,
                                          pem_password_cb *cb, void *u);
OPENSSL_EXPORT int i2d_PKCS8PrivateKey_nid_fp(FILE *fp, const EVP_PKEY *x,
                                              int nid, const char *pass,
                                              int pass_len, pem_password_cb *cb,
                                              void *u);
OPENSSL_EXPORT int PEM_write_PKCS8PrivateKey_nid(FILE *fp, const EVP_PKEY *x,
                                                 int nid, const char *pass,
                                                 int pass_len,
                                                 pem_password_cb *cb, void *u);

OPENSSL_EXPORT EVP_PKEY *d2i_PKCS8PrivateKey_fp(FILE *fp, EVP_PKEY **x,
                                                pem_password_cb *cb, void *u);

OPENSSL_EXPORT int PEM_write_PKCS8PrivateKey(FILE *fp, const EVP_PKEY *x,
                                             const EVP_CIPHER *enc,
                                             const char *pass, int pass_len,
                                             pem_password_cb *cd, void *u);


#if defined(__cplusplus)
}  // extern C
#endif

#define PEM_R_BAD_BASE64_DECODE 100
#define PEM_R_BAD_DECRYPT 101
#define PEM_R_BAD_END_LINE 102
#define PEM_R_BAD_IV_CHARS 103
#define PEM_R_BAD_PASSWORD_READ 104
#define PEM_R_CIPHER_IS_NULL 105
#define PEM_R_ERROR_CONVERTING_PRIVATE_KEY 106
#define PEM_R_NOT_DEK_INFO 107
#define PEM_R_NOT_ENCRYPTED 108
#define PEM_R_NOT_PROC_TYPE 109
#define PEM_R_NO_START_LINE 110
#define PEM_R_READ_KEY 111
#define PEM_R_SHORT_HEADER 112
#define PEM_R_UNSUPPORTED_CIPHER 113
#define PEM_R_UNSUPPORTED_ENCRYPTION 114
#define PEM_R_UNSUPPORTED_PROC_TYPE_VERSION 115
#define PEM_R_NO_DATA 116

#endif  // OPENSSL_HEADER_PEM_H
