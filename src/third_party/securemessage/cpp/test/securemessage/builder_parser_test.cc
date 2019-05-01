/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "securemessage/byte_buffer.h"
#include "securemessage/public_key_proto_util.h"
#include "securemessage/secure_message_builder.h"
#include "securemessage/secure_message_parser.h"
#include "securemessage/secure_message_wrapper.h"


using std::unique_ptr;

namespace securemessage {
namespace {

const unsigned char kVerificationKeyId[2] = {0x00, 0x05};
const unsigned char kDecryptionKeyId[22] = {0, 1, 9, 8, 7, 6, 5, 4, 3, 2, 1,
                                            0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

class SecureMessageBuilderParserTest : public testing::Test {
 protected:
  unique_ptr<ByteBuffer> message_;
  unique_ptr<ByteBuffer> metadata_;

  // Key Ids
  unique_ptr<ByteBuffer> verification_key_id_;
  unique_ptr<ByteBuffer> decryption_key_id_;

  // Keys
  unique_ptr<CryptoOps::KeyPair> ec_key_;
  unique_ptr<CryptoOps::KeyPair> rsa_key_;
  unique_ptr<CryptoOps::SecretKey> aes_encryption_key_;
  unique_ptr<CryptoOps::SecretKey> hmac_key_;

  // Optional Associated Data
  string associated_data_;

  virtual void SetUp() {
    message_.reset(new ByteBuffer("Testing 1 2 3..."));
    metadata_.reset(new ByteBuffer("Test protocol"));

    verification_key_id_.reset(new ByteBuffer(kVerificationKeyId, 2));
    decryption_key_id_.reset(new ByteBuffer(kDecryptionKeyId, 22));
    associated_data_ = string();

    ec_key_ = CryptoOps::GenerateEcP256KeyPair();
    rsa_key_ = CryptoOps::GenerateRsa2048KeyPair();
    aes_encryption_key_ = CryptoOps::GenerateAes256SecretKey();
    hmac_key_ = CryptoOps::GenerateAes256SecretKey();
  }

  virtual void TearDown() {}

  // Produce a Signed SecureMessage
  unique_ptr<SecureMessage> Sign(CryptoOps::SigType sig_type) {
    unique_ptr<SecureMessageBuilder> builder = GetPreconfiguredBuilder();

    CryptoOps::Key* signing_key = GetSigningKeyFor(sig_type);

    if (signing_key == NULL) {
      return NULL;
    }

    return builder->BuildSignedCleartextMessage(*signing_key,
                                                sig_type,
                                                message_->String());
  }

  // Produce a Signed/Encrypted SecureMessage
  unique_ptr<SecureMessage> SignCrypt(CryptoOps::SigType sig_type,
                                      CryptoOps::EncType enc_type) {
    unique_ptr<SecureMessageBuilder> builder = GetPreconfiguredBuilder();
    CryptoOps::Key* signing_key = GetSigningKeyFor(sig_type);

    if (signing_key == NULL) {
      return nullptr;
    }

    return builder->BuildSignCryptedMessage(*signing_key,
                                            sig_type,
                                            *aes_encryption_key_,
                                            enc_type,
                                            message_->String());
  }

  void VerifyDecrypt(const SecureMessage& encrypted_and_signed,
                     CryptoOps::SigType sig_type,
                     CryptoOps::EncType enc_type,
                     bool should_fail) {
    CryptoOps::Key* verification_key = GetVerificationKeyFor(sig_type);

    unique_ptr<HeaderAndBody> header_and_body =
        SecureMessageParser::ParseSignCryptedMessage(
            encrypted_and_signed, *verification_key, sig_type,
            *aes_encryption_key_, enc_type, associated_data_);

    if (!should_fail) {
      ASSERT_NE(header_and_body, nullptr);
    } else {
      ASSERT_EQ(header_and_body, nullptr);
      return;
    }
    ConsistencyCheck(encrypted_and_signed, *header_and_body, sig_type,
                     enc_type);
  }

  // Returns if the header and body of a given secure message look sane
  void ConsistencyCheck(const SecureMessage& secure_message,
                        const HeaderAndBody& header_and_body,
                        CryptoOps::SigType sig_type,
                        CryptoOps::EncType enc_type) {
    unique_ptr<Header> header =
        SecureMessageParser::GetUnverifiedHeader(secure_message);

    ASSERT_TRUE(header != nullptr);
    CheckHeader(*header, sig_type, enc_type);
    CheckHeaderAndBody(*header, header_and_body);
  }

  // Returns if the header checks out or causes and error otherwise
  void CheckHeader(const Header& header, CryptoOps::SigType sig_type,
                     CryptoOps::EncType enc_type) {
    ASSERT_EQ(SecureMessageWrapper::GetSigScheme(sig_type),
              static_cast<int>(header.signature_scheme()));
    ASSERT_EQ(SecureMessageWrapper::GetEncScheme(enc_type),
              static_cast<int>(header.encryption_scheme()));
    CheckKeyIdsAndMetadata(verification_key_id_, decryption_key_id_, metadata_,
                           header);
  }

  // Returns if header and body check out, errors otherwise
  void CheckHeaderAndBody(const Header& header,
                          const HeaderAndBody& header_and_body) {
    ASSERT_EQ(header.SerializeAsString(),
              header_and_body.header().SerializeAsString());
    ASSERT_EQ(message_->String(), header_and_body.body());
  }

  // Returns if key ids and metadata check, errors otherwise
  void CheckKeyIdsAndMetadata(const unique_ptr<ByteBuffer>& verification_key_id,
                              const unique_ptr<ByteBuffer>& decryption_key_id,
                              const unique_ptr<ByteBuffer>& metadata,
                              const Header& header) {
    if (verification_key_id == nullptr) {
      ASSERT_FALSE(header.has_verification_key_id());
    } else {
      ASSERT_TRUE(verification_key_id->Equals(
          ByteBuffer(header.verification_key_id())));
    }
    if (decryption_key_id == nullptr) {
      ASSERT_FALSE(header.has_decryption_key_id());
    } else {
      ASSERT_TRUE(
          decryption_key_id->Equals(ByteBuffer(header.decryption_key_id())));
    }
    if (metadata == nullptr) {
      ASSERT_FALSE(header.has_public_metadata());
    } else {
      ASSERT_TRUE(metadata->Equals(ByteBuffer(header.public_metadata())));
    }
  }

  // Return the correct signing key for a signature type
  CryptoOps::Key* GetSigningKeyFor(CryptoOps::SigType sig_type) {
    if (sig_type == CryptoOps::SigType::ECDSA_P256_SHA256) {
      return ec_key_->private_key.get();
    }
    if (sig_type == CryptoOps::SigType::RSA2048_SHA256) {
      return rsa_key_->private_key.get();
    }
    if (sig_type == CryptoOps::SigType::HMAC_SHA256) {
      return hmac_key_.get();
    }
    return NULL;  // This should not happen
  }

  // Return the correct verification key for signature type
  CryptoOps::Key* GetVerificationKeyFor(CryptoOps::SigType sig_type) {
    if (CryptoOps::IsPublicKeyScheme(sig_type)) {
      if (sig_type == CryptoOps::SigType::ECDSA_P256_SHA256) {
        return ec_key_->public_key.get();
      } else if (sig_type == CryptoOps::SigType::RSA2048_SHA256) {
        return rsa_key_->public_key.get();
      } else {
        return NULL;  // This should not happen
      }
    }

    // For symmetric key schemes
    return GetSigningKeyFor(sig_type);
  }

  // Produce a preconfigured builder with several fields already set
  unique_ptr<SecureMessageBuilder> GetPreconfiguredBuilder() {
    unique_ptr<SecureMessageBuilder> builder(new SecureMessageBuilder());

    if (decryption_key_id_ != nullptr) {
      builder->SetDecryptionKeyId(decryption_key_id_->String());
    }
    if (verification_key_id_ != nullptr) {
      builder->SetVerificationKeyId(verification_key_id_->String());
    }
    if (metadata_ != nullptr) {
      builder->SetPublicMetadata(metadata_->String());
    }
    builder->SetAssociatedData(associated_data_);

    return builder;
  }
};

TEST_F(SecureMessageBuilderParserTest,
       TestEncryptedAndEcdsaSignedUsingPublicKeyProto) {
  // Safest usage of SignCryption is to set the VerificationKeyId to an actual
  // representation of the verification key.
  verification_key_id_.reset(
      new ByteBuffer(PublicKeyProtoUtil::EncodePublicKey(*(ec_key_->public_key))
                         ->SerializeAsString()));

  unique_ptr<SecureMessage> encrypted_and_signed = SignCrypt(
      CryptoOps::SigType::ECDSA_P256_SHA256, CryptoOps::EncType::AES_256_CBC);

  // Simulate extracting the verification key ID from the SecureMessage
  // (non-standard usage)
  string verification_id = SecureMessageParser::GetUnverifiedHeader(
                               *encrypted_and_signed)->verification_key_id();
  unique_ptr<GenericPublicKey> generic_key(new GenericPublicKey());
  generic_key->ParseFromString(verification_id);

  // Verify below is now forced to use the public key parsed out of the
  // verification id
  ec_key_->public_key = PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  VerifyDecrypt(*encrypted_and_signed, CryptoOps::SigType::ECDSA_P256_SHA256,
                CryptoOps::EncType::AES_256_CBC, false);
}

TEST_F(SecureMessageBuilderParserTest,
       TestEncryptedAndEcdsaSignedUsingPublicKeyProtoWithAssociatedData) {
  // Safest usage of SignCryption is to set the VerificationKeyId to an actual
  // representation of the verification key.
  verification_key_id_.reset(
      new ByteBuffer(PublicKeyProtoUtil::EncodePublicKey(*(ec_key_->public_key))
                         ->SerializeAsString()));
  associated_data_ = "test associated data";

  unique_ptr<SecureMessage> encrypted_and_signed = SignCrypt(
      CryptoOps::SigType::ECDSA_P256_SHA256, CryptoOps::EncType::AES_256_CBC);

  // Simulate extracting the verification key ID from the SecureMessage
  // (non-standard usage)
  string verification_id = SecureMessageParser::GetUnverifiedHeader(
                               *encrypted_and_signed)->verification_key_id();
  unique_ptr<GenericPublicKey> generic_key(new GenericPublicKey());
  generic_key->ParseFromString(verification_id);

  // Verify below is now forced to use the public key parsed out of the
  // verification id
  ec_key_->public_key = PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  VerifyDecrypt(*encrypted_and_signed, CryptoOps::SigType::ECDSA_P256_SHA256,
                CryptoOps::EncType::AES_256_CBC, false);

  // Same length associated data, but different content should fail
  associated_data_ = "test associated dat4";
  VerifyDecrypt(*encrypted_and_signed, CryptoOps::SigType::ECDSA_P256_SHA256,
                  CryptoOps::EncType::AES_256_CBC, true);

  // Blank associated data should also fail
  associated_data_ = "";
  VerifyDecrypt(*encrypted_and_signed, CryptoOps::SigType::ECDSA_P256_SHA256,
                CryptoOps::EncType::AES_256_CBC, true);
}

TEST_F(SecureMessageBuilderParserTest,
       TestEncryptedAndRsaSignedUsingPublicKeyProto) {
  verification_key_id_.reset(new ByteBuffer("replace me"));

  // TODO(aczeskis): Use this when EncodePublicKey() can handle RSA keys
  // verification_key_id_.reset(
  //    new ByteBuffer(PublicKeyProtoUtil::EncodePublicKey(
  //                       *(rsa_key_->public_key))->SerializeAsString()));

  unique_ptr<SecureMessage> encrypted_and_signed = SignCrypt(
      CryptoOps::SigType::RSA2048_SHA256, CryptoOps::EncType::AES_256_CBC);

  // TODO(aczeskis): Use this when EncodePublicKey and ParsePublicKey work
  // Simulate extracting the verification key ID from the SecureMessage
  // (non-standard usage)
  // string verification_id = SecureMessageParser::GetUnverifiedHeader(
  //                             *encrypted_and_signed)->verification_key_id();
  // unique_ptr<GenericPublicKey> generic_key(new GenericPublicKey());
  // generic_key->ParseFromString(verification_id);

  // TODO(aczeskis): Uncomment this when ParsePublicKey() can handle RSA keys
  // Verify below is now forced to use the public key parsed out of the
  // verification id
  // rsa_key_->public_key = PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  VerifyDecrypt(*encrypted_and_signed, CryptoOps::SigType::RSA2048_SHA256,
                CryptoOps::EncType::AES_256_CBC, false);
}


TEST_F(SecureMessageBuilderParserTest,
       TestEncryptedAndRsaSignedUsingPublicKeyProtoWithAssociatedData) {
  verification_key_id_.reset(new ByteBuffer("replace me"));

  // TODO(aczeskis): Use this when EncodePublicKey() can handle RSA keys
  // verification_key_id_.reset(
  //    new ByteBuffer(PublicKeyProtoUtil::EncodePublicKey(
  //                       *(rsa_key_->public_key))->SerializeAsString()));

  associated_data_ = "test associated data";

  unique_ptr<SecureMessage> encrypted_and_signed = SignCrypt(
      CryptoOps::SigType::RSA2048_SHA256, CryptoOps::EncType::AES_256_CBC);

  // TODO(aczeskis): Use this when EncodePublicKey and ParsePublicKey work
  // Simulate extracting the verification key ID from the SecureMessage
  // (non-standard usage)
  // string verification_id = SecureMessageParser::GetUnverifiedHeader(
  //                             *encrypted_and_signed)->verification_key_id();
  // unique_ptr<GenericPublicKey> generic_key(new GenericPublicKey());
  // generic_key->ParseFromString(verification_id);

  // TODO(aczeskis): Uncomment this when ParsePublicKey() can handle RSA keys
  // Verify below is now forced to use the public key parsed out of the
  // verification id
  // rsa_key_->public_key = PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  VerifyDecrypt(*encrypted_and_signed, CryptoOps::SigType::RSA2048_SHA256,
                CryptoOps::EncType::AES_256_CBC, false);

  // Same length associated data, but different content should fail
  associated_data_ = "test associated dat4";
  VerifyDecrypt(*encrypted_and_signed, CryptoOps::SigType::RSA2048_SHA256,
                  CryptoOps::EncType::AES_256_CBC, true);

  // Blank associated data should also fail
  associated_data_ = "";
  VerifyDecrypt(*encrypted_and_signed, CryptoOps::SigType::RSA2048_SHA256,
                  CryptoOps::EncType::AES_256_CBC, true);
}

}  // namespace
}  // namespace securemessage
