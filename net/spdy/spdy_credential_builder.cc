// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_credential_builder.h"

#include "base/logging.h"
#include "base/string_piece.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "net/base/asn1_util.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/net_errors.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_framer.h"

namespace net {

namespace {

std::vector<uint8> ToVector(base::StringPiece piece) {
  return std::vector<uint8>(piece.data(), piece.data() + piece.length());
}

}  // namespace

// static
int SpdyCredentialBuilder::Build(const std::string& tls_unique,
                                 SSLClientCertType type,
                                 const std::string& key,
                                 const std::string& cert,
                                 size_t slot,
                                 SpdyCredential* credential) {
  if (type != CLIENT_CERT_ECDSA_SIGN)
    return ERR_BAD_SSL_CLIENT_AUTH_CERT;

  std::string secret = SpdyCredentialBuilder::GetCredentialSecret(tls_unique);

  // Extract the SubjectPublicKeyInfo from the certificate.
  base::StringPiece public_key_info;
  if(!asn1::ExtractSPKIFromDERCert(cert, &public_key_info))
    return ERR_BAD_SSL_CLIENT_AUTH_CERT;

  // Next, extract the SubjectPublicKey data, which will actually
  // be stored in the cert field of the credential frame.
  base::StringPiece public_key;
  if (!asn1::ExtractSubjectPublicKeyFromSPKI(public_key_info, &public_key))
    return ERR_BAD_SSL_CLIENT_AUTH_CERT;
  // Drop one byte of padding bits count from the BIT STRING
  // (this will always be zero).  Drop one byte of X9.62 format specification
  // (this will always be 4 to indicated an uncompressed point).
  DCHECK_GT(public_key.length(), 2u);
  DCHECK_EQ(0, static_cast<int>(public_key[0]));
  DCHECK_EQ(4, static_cast<int>(public_key[1]));
  public_key = public_key.substr(2, public_key.length());

  // Convert the strings into a vector<unit8>
  std::vector<uint8> proof_vector;
  scoped_ptr<crypto::ECPrivateKey> private_key(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          ServerBoundCertService::kEPKIPassword,
          ToVector(key), ToVector(public_key_info)));
  scoped_ptr<crypto::ECSignatureCreator> creator(
      crypto::ECSignatureCreator::Create(private_key.get()));
  creator->Sign(reinterpret_cast<const unsigned char *>(secret.data()),
                secret.length(), &proof_vector);

  credential->slot = slot;
  credential->certs.push_back(public_key.as_string());
  credential->proof.assign(proof_vector.begin(), proof_vector.end());
  return OK;
}

// static
std::string SpdyCredentialBuilder::GetCredentialSecret(
    const std::string& tls_unique) {
  const char prefix[] = "SPDY CREDENTIAL ChannelID\0client -> server";
  std::string secret(prefix, arraysize(prefix));
  secret.append(tls_unique);

  return secret;
}

}  // namespace net
