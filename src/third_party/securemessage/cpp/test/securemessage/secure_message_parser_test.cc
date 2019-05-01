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

#include "securemessage/secure_message_parser.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

static const char* kVerificationKeyId = "test verification key id";
static const char* kDecryptionKeyId = "test decryption key id";
static const char* kIV = "test iv";
static const char* kPublicMetaData = "test metadata";
static const char* kSignature = "test signature";
static const char* kBody = "test body";
static const char* kAssociatedData = "test associated data";

using std::unique_ptr;

namespace securemessage {
namespace {

static SigScheme kSigScheme = HMAC_SHA256;
static EncScheme kEncScheme = AES_256_CBC;

class SecureMessageParserTest : public testing::Test {
 protected:
  unique_ptr<Header> header_;
  unique_ptr<HeaderAndBody> header_and_body_;
  unique_ptr<SecureMessage> secure_message_;

  virtual void SetUp() {
    header_.reset(new Header());
    header_->set_verification_key_id(kVerificationKeyId);
    header_->set_decryption_key_id(kDecryptionKeyId);
    header_->set_iv(kIV);
    header_->set_public_metadata(kPublicMetaData);
    header_->set_signature_scheme(kSigScheme);
    header_->set_encryption_scheme(kEncScheme);
    header_->set_associated_data_length(strlen(kAssociatedData));

    header_and_body_.reset(new HeaderAndBody());
    header_and_body_->set_allocated_header(new Header(*header_));
    header_and_body_->set_body(kBody);

    secure_message_.reset(new SecureMessage());
    secure_message_->set_header_and_body(header_and_body_->SerializeAsString());
    secure_message_->set_signature(kSignature);
  }

  virtual void TearDown() {
    header_->Clear();
    header_and_body_->Clear();
    secure_message_->Clear();
  }
};

TEST_F(SecureMessageParserTest, GetUnverifiedHeader) {
  std::unique_ptr<Header> test_header =
      SecureMessageParser::GetUnverifiedHeader(*secure_message_);

  EXPECT_NE(nullptr, test_header);
  EXPECT_EQ(kVerificationKeyId, test_header->verification_key_id());
  EXPECT_EQ(kDecryptionKeyId, test_header->decryption_key_id());
  EXPECT_EQ(kIV, test_header->iv());
  EXPECT_EQ(kPublicMetaData, test_header->public_metadata());
  EXPECT_EQ(HMAC_SHA256, test_header->signature_scheme());
  EXPECT_EQ(AES_256_CBC, test_header->encryption_scheme());
  EXPECT_EQ(strlen(kAssociatedData),
            static_cast<size_t>(test_header->associated_data_length()));
}

}  // namespace
}  // namespace securemessage
