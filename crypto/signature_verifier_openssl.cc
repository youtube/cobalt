// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_verifier.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "crypto/openssl_util.h"

namespace crypto {

struct SignatureVerifier::VerifyContext {
  ScopedOpenSSL<EVP_PKEY, EVP_PKEY_free> public_key;
  ScopedOpenSSL<EVP_MD_CTX, EVP_MD_CTX_destroy> ctx;
};

SignatureVerifier::SignatureVerifier()
    : verify_context_(NULL) {
}

SignatureVerifier::~SignatureVerifier() {
  Reset();
}

bool SignatureVerifier::VerifyInit(const uint8* signature_algorithm,
                                   int signature_algorithm_len,
                                   const uint8* signature,
                                   int signature_len,
                                   const uint8* public_key_info,
                                   int public_key_info_len) {
  DCHECK(!verify_context_);
  verify_context_ = new VerifyContext;
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ScopedOpenSSL<X509_ALGOR, X509_ALGOR_free> algorithm(
      d2i_X509_ALGOR(NULL, &signature_algorithm, signature_algorithm_len));
  if (!algorithm.get())
    return false;

  const EVP_MD* digest = EVP_get_digestbyobj(algorithm.get()->algorithm);
  DCHECK(digest);

  signature_.assign(signature, signature + signature_len);

  // BIO_new_mem_buf is not const aware, but it does not modify the buffer.
  char* data = reinterpret_cast<char*>(const_cast<uint8*>(public_key_info));
  ScopedOpenSSL<BIO, BIO_free_all> bio(BIO_new_mem_buf(data,
                                                       public_key_info_len));
  if (!bio.get())
    return false;

  verify_context_->public_key.reset(d2i_PUBKEY_bio(bio.get(), NULL));
  if (!verify_context_->public_key.get())
    return false;

  verify_context_->ctx.reset(EVP_MD_CTX_create());
  int rv = EVP_VerifyInit_ex(verify_context_->ctx.get(), digest, NULL);
  return rv == 1;
}

void SignatureVerifier::VerifyUpdate(const uint8* data_part,
                                     int data_part_len) {
  DCHECK(verify_context_);
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = EVP_VerifyUpdate(verify_context_->ctx.get(),
                            data_part, data_part_len);
  DCHECK_EQ(rv, 1);
}

bool SignatureVerifier::VerifyFinal() {
  DCHECK(verify_context_);
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = EVP_VerifyFinal(verify_context_->ctx.get(),
                           vector_as_array(&signature_), signature_.size(),
                           verify_context_->public_key.get());
  DCHECK_GE(rv, 0);
  Reset();
  return rv == 1;
}

void SignatureVerifier::Reset() {
  delete verify_context_;
  verify_context_ = NULL;
  signature_.clear();
}

}  // namespace crypto
