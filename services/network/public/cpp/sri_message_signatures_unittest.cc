// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/sri_message_signatures.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/sri_message_signature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

using Parameters = mojom::SRIMessageSignatureComponent::Parameter;

namespace {

// Exciting test constants, leaning on test data from the RFC.
//
// Base64 encoded Ed25519 Test Keys, pulled from the RFC at
// https://datatracker.ietf.org/doc/html/rfc9421#appendix-B.1.4
const char* kPublicKey = "JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";

// Another base64 encoded Ed25519 key, randomly generated:
//
// {
//   "crv": "Ed25519",
//   "d": "MTodZiTA9CBsuIvSfO679TThkG3b7ce6R3sq_CdyVp4",
//   "ext": true,
//   "kty": "OKP",
//   "x": "xDnP380zcL4rJ76rXYjeHlfMyPZEOqpJYjsjEppbuXE"
// }
const char* kPublicKey2 = "xDnP380zcL4rJ76rXYjeHlfMyPZEOqpJYjsjEppbuXE=";

// The following constants are extracted from this known-good response that
// matches the constraints described in
// https://wicg.github.io/signature-based-sri/#verification-requirements-for-sri
//
// ```
// HTTP/1.1 200 OK
// Date: Tue, 20 Apr 2021 02:07:56 GMT
// Content-Type: application/json
// Unencoded-Digest: sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:
// Content-Length: 18
// Signature-Input: signature=("unencoded-digest";sf); \
//                  keyid="JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs="; \
//                  tag="sri"
// Signature: signature=:gHim9e5Pk2H7c9BStOmxSmkyc8+ioZgoxynu3d4INAT4dwfj \
//                       5LhvaV9DFnEQ9p7C0hzW4o4Qpkm5aApd6WLLCw==:
//
// {"hello": "world"}
// ```
const char* kSignature =
    "gHim9e5Pk2H7c9BStOmxSmkyc8+"
    "ioZgoxynu3d4INAT4dwfj5LhvaV9DFnEQ9p7C0hzW4o4Qpkm5aApd6WLLCw==";

const char* kValidDigestHeader =
    "sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
const char* kValidDigestHeader512 =
    "sha-512=:WZDPaVn/7XgHaAy8pmojAkGWoRx2UFChF41A2svX+TaPm+AbwAgBWnrIiYllu7BNN"
    "yealdVLvRwEmTHWXvJwew==:";

// A basic signature header set with no expiration.
const char* kValidSignatureInputHeader =
    "signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
    "89iXES9+vFgrIy29clF9CC/"
    "oPPsw3c5D0bs=\";tag=\"sri\"";
const char* kUnusedSignatureInputHeader =
    "unused-signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
    "89iXES9+vFgrIy29"
    "clF9CC/oPPsw3c5D0bs=\";tag=\"sri\"";
const char* kValidSignatureHeader =
    "signature=:gHim9e5Pk2H7c9BStOmxSmkyc8+ioZgoxynu3d4INAT4dwfj5LhvaV9DFnEQ9p7"
    "C0hzW4o4Qpkm5aApd6WLLCw==:";
const char* kUnusedSignatureHeader =
    "unused-input=:gHim9e5Pk2H7c9BStOmxSmkyc8+ioZgoxynu3d4INAT4dwfj5LhvaV9DFnEQ"
    "9p7C0hzW4o4Qpkm5aApd6WLLCw==:";

// The following signature was generated using test-key-ed25519 from RFC 9421
// (https://datatracker.ietf.org/doc/html/rfc9421#appendix-B.1.4),
// the same key used for generating the constants above.
//
// A valid signature header set with expiration in the future (2142-12-30).
const char* kValidExpiringSignatureInputHeader =
    "signature=(\"unencoded-digest\";sf);expires=5459212800;"
    "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\"";
const char* kValidExpiringSignatureHeader =
    "signature=:dHodKM9puo7Q9D6W1ELELGLlRcQPa+Tdu6uQ93ajVGsY/gzDbTXqEBb650PEgXM"
    "xv+fHGIy8QGb7stkMgbphCQ==:";
const int64_t kValidExpiringSignatureExpiresAt = 5459212800;

constexpr std::string_view kAcceptSignature = "accept-signature";

const GURL kExampleURL = GURL("https://example.test/");

}  // namespace

class SRIMessageSignatureParserTest : public testing::Test {
 protected:
  SRIMessageSignatureParserTest() = default;

  scoped_refptr<net::HttpResponseHeaders> GetHeaders(const char* signature,
                                                     const char* input) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    if (signature) {
      builder.AddHeader("Signature", signature);
    }
    if (input) {
      builder.AddHeader("Signature-Input", input);
    }
    return builder.Build();
  }

  const GURL& url() { return kExampleURL; }

  void ValidateBasicTestHeader(const mojom::SRIMessageSignaturePtr& sig) {
    EXPECT_EQ("signature", sig->label);
    EXPECT_EQ(std::nullopt, sig->created);
    EXPECT_EQ(std::nullopt, sig->expires);
    EXPECT_EQ(kPublicKey, sig->keyid);
    EXPECT_EQ(std::nullopt, sig->nonce);
    EXPECT_EQ("sri", sig->tag);
    EXPECT_EQ(kSignature, base::Base64Encode(sig->signature));

    ASSERT_EQ(1u, sig->components.size());
    EXPECT_EQ("unencoded-digest", sig->components[0]->name);
    ASSERT_EQ(1u, sig->components[0]->params.size());
    EXPECT_EQ(mojom::SRIMessageSignatureComponent::Parameter::
                  kStrictStructuredFieldSerialization,
              sig->components[0]->params[0]);
  }
};

TEST_F(SRIMessageSignatureParserTest, NoHeaders) {
  auto headers = GetHeaders(/*signature=*/nullptr, /*input=*/nullptr);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);
  EXPECT_EQ(0u, result->signatures.size());
  EXPECT_EQ(0u, result->errors.size());
}

TEST_F(SRIMessageSignatureParserTest, NoSignatureHeader) {
  auto headers = GetHeaders(/*signature=*/nullptr, kValidSignatureInputHeader);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);
  EXPECT_EQ(0u, result->signatures.size());
  ASSERT_EQ(1u, result->errors.size());
  EXPECT_EQ(mojom::SRIMessageSignatureError::kMissingSignatureHeader,
            result->errors[0]);
}

TEST_F(SRIMessageSignatureParserTest, NoSignatureInputHeader) {
  auto headers = GetHeaders(kValidSignatureHeader, /*input=*/nullptr);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);
  EXPECT_EQ(0u, result->signatures.size());
  ASSERT_EQ(1u, result->errors.size());
  EXPECT_EQ(mojom::SRIMessageSignatureError::kMissingSignatureInputHeader,
            result->errors[0]);
}

TEST_F(SRIMessageSignatureParserTest, ValidHeaders) {
  auto headers = GetHeaders(kValidSignatureHeader, kValidSignatureInputHeader);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);

  EXPECT_EQ(1u, result->signatures.size());
  EXPECT_EQ(0u, result->errors.size());
  ValidateBasicTestHeader(result->signatures[0]);
}

TEST_F(SRIMessageSignatureParserTest, UnmatchedLabelsInAdditionToValidHeaders) {
  // kValidSignatureInputHeader defines inputs for the `signature` label. The
  // following header will define a signature for that label, as well as another
  // signature with an unused label.
  //
  // We're currently ignoring this mismatched signature, and therefore treating
  // the header as valid.
  std::string two_signatures =
      base::StrCat({kUnusedSignatureHeader, ",", kValidSignatureHeader});
  std::string two_inputs = base::StrCat(
      {kUnusedSignatureInputHeader, ",", kValidSignatureInputHeader});

  // Too many signatures:
  {
    auto headers =
        GetHeaders(two_signatures.c_str(), kValidSignatureInputHeader);
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);
    EXPECT_EQ(1u, result->signatures.size());
    EXPECT_EQ(1u, result->errors.size());
    EXPECT_EQ(
        mojom::SRIMessageSignatureError::kSignatureInputHeaderMissingLabel,
        result->errors[0]);
    ValidateBasicTestHeader(result->signatures[0]);
  }

  // Too many inputs:
  {
    auto headers = GetHeaders(kValidSignatureHeader, two_inputs.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);
    EXPECT_EQ(1u, result->signatures.size());
    // TODO(crbug.com/381044049): We should probably have a parsing error here.
    EXPECT_EQ(0u, result->errors.size());
    ValidateBasicTestHeader(result->signatures[0]);
  }

  // Too many everythings!
  {
    auto headers = GetHeaders(two_signatures.c_str(), two_inputs.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);
    EXPECT_EQ(1u, result->signatures.size());
    EXPECT_EQ(1u, result->errors.size());
    EXPECT_EQ(
        mojom::SRIMessageSignatureError::kSignatureInputHeaderMissingLabel,
        result->errors[0]);
    ValidateBasicTestHeader(result->signatures[0]);
  }
}

TEST_F(SRIMessageSignatureParserTest, MalformedSignatureHeader) {
  const char* cases[] = {
      // Non-dictionaries
      "",
      "1",
      "1.1",
      "\"string\"",
      "token",
      ":lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
      "?0",
      "@12345",
      "%\"display\"",
      "A, list, of, tokens",
      "(inner list)",

      // Dictionaries with non-byte-sequence values.
      "signature=",
      "signature=1",
      "signature=1.1",
      "signature=\"string\"",
      "signature=token",
      "signature=?0",
      "signature=@12345",
      "signature=%\"display\"",
      "signature=(inner list of tokens)",

      // Dictionaries with byte-sequence values of the wrong length:
      "signature=:YQ==:",

      // Parameterized, but otherwise correct byte-sequence values:
      ("signature=:amDAmvl9bsfIcfA/bIJsBuBvInjJAaxxNIlLOzNI3FkrnG2k52UxXJprz89"
       "+2aOwEAz3w6KjjZuGkdrOUwxhBQ==:;param=1"),
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test << "`");
    auto headers = GetHeaders(test, kValidSignatureInputHeader);
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    // As these are all malformed, we expect parsing to return no headers.
    EXPECT_EQ(0u, result->signatures.size());
    EXPECT_EQ(1u, result->errors.size());
  }
}

TEST_F(SRIMessageSignatureParserTest, MalformedSignatureInputComponents) {
  struct {
    const char* value;
    mojom::SRIMessageSignatureError error;
  } cases[] = {
      // Non-dictionaries:
      {"", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"1", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"1.1", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"\"string\"",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {":lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"?0", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"A, list, of, tokens",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"(inner list)",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"@12345", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"%\"display\"",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},

      // Dictionaries with non-inner-list values:
      {"signature",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=1",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=1.1",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=\"string\"",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=token",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=?0",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=:badbeef:",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      // We don't yet support dates or display strings, so these are invalid
      // headers, not invalid types.
      {"signature=@12345",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=%\"display\"",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},

      // Dictionaries with inner-list values that contain non-strings:
      {"signature=()", mojom::SRIMessageSignatureError::
                           kSignatureInputHeaderValueMissingComponents},
      {"signature=(1)", mojom::SRIMessageSignatureError::
                            kSignatureInputHeaderInvalidComponentType},
      {"signature=(1.1)", mojom::SRIMessageSignatureError::
                              kSignatureInputHeaderInvalidComponentType},
      {"signature=(token)", mojom::SRIMessageSignatureError::
                                kSignatureInputHeaderInvalidComponentType},
      {"signature=(:lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentType},
      {"signature=(?0)", mojom::SRIMessageSignatureError::
                             kSignatureInputHeaderInvalidComponentType},
      {"signature=(A list of tokens)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentType},
      // We don't yet support dates or display strings, so these are invalid
      // headers, not invalid types.
      {"signature=(@12345)",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=(%\"display\")",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},

      // Components that are valid per-spec, but aren't quite constrained enough
      // for SRI's initial implementation. We'll eventually treat these as valid
      // headers, but they're parse errors for now.
      {"signature=(\"invalid header names\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"@unknown-derived-components\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"not-unencoded-digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"Unencoded-Digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"UNENCODED-DIGEST\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"unencoded-digest\" \"and-something-else\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"something-else\" \"unencoded-digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},

      // Invalid component params:
      {"signature=(\"unencoded-digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=)",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=(\"unencoded-digest\";sf=1)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=1.1)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=\"string\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=token)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=?0)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=:badbeef:)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf;not-sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      // We don't yet support dates or display strings, so these are invalid
      // headers, not invalid types.
      {"signature=(\"unencoded-digest\";sf=@12345)",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=(\"unencoded-digest\";sf=%\"display\")",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},

      // One valid, one invalid component:
      {"signature=(\"unencoded-digest\";sf \"unknown\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"unknown\" \"unencoded-digest\";sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"unencoded-digest\";sf \"@path\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidDerivedComponentParameter},
      {"signature=(\"@path\" \"unencoded-digest\";sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidDerivedComponentParameter},
      {"signature=(\"unencoded-digest\";sf token;req)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentType},
      {"signature=(token;req \"unencoded-digest\";sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentType},

      // Valid component without valid `unencoded-digest`:
      {"signature=(\"unencoded-digest\" \"@path\";req)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"@path\";req \"unencoded-digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"@path\";req \"not-unencoded-digest\";sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.value << "`");

    // Tack valid parameters onto the test string so that we're actually
    // just testing the component parsing.
    std::string test_with_params =
        base::StrCat({test.value, ";keyid=\"", kPublicKey, "\";tag=\"sri\""});
    auto headers = GetHeaders(kValidSignatureHeader, test_with_params.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    // As these are all malformed, we expect parsing to return no headers.
    EXPECT_EQ(0u, result->signatures.size());
    ASSERT_GT(result->errors.size(), 0u);
    EXPECT_THAT(result->errors, testing::Contains(test.error));
  }
}

TEST_F(SRIMessageSignatureParserTest, MalformedSignatureInputParameters) {
  const char* cases[] = {
      // Missing a required parameter:
      "keyid=\"[KEY]\"",
      "tag=\"sri\"",

      // Duplication (insofar as the invalid value comes last):
      "keyid=\"[KEY]\";keyid=\"not-[KEY]\";tag=\"sri\"",
      "keyid=\"[KEY]\";tag=\"sri\";tag=\"not-sri\"",

      // Unknown parameter:
      "keyid=\"[KEY]\";tag=\"sri\";unknown=1",

      // Alg is present:
      "alg=;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=1;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=1.1;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=token;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=?0;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=@12345;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=%\"display\";keyid=\"[KEY]\";tag=\"sri\"",
      "alg=:badbeef:;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=\"sri\"",

      // Invalid `created`:
      //
      // - Types:
      "created=;keyid=\"[KEY]\";tag=\"sri\"",
      "created=1.1;keyid=\"[KEY]\";tag=\"sri\"",
      "created=\"string\";keyid=\"[KEY]\";tag=\"sri\"",
      "created=token;keyid=\"[KEY]\";tag=\"sri\"",
      "created=?0;keyid=\"[KEY]\";tag=\"sri\"",
      "created=@12345;keyid=\"[KEY]\";tag=\"sri\"",
      "created=%\"display\";keyid=\"[KEY]\";tag=\"sri\"",
      "created=:badbeef:;keyid=\"[KEY]\";tag=\"sri\"",
      // - Values
      "created=-1;keyid=\"[KEY]\";tag=\"sri\"",

      // Invalid `expires`:
      //
      // - Types:
      "expires=;keyid=\"[KEY]\";tag=\"sri\"",
      "expires=1.1;keyid=\"[KEY]\";tag=\"sri\"",
      "expires=\"string\";keyid=\"[KEY]\";tag=\"sri\"",
      "expires=token;keyid=\"[KEY]\";tag=\"sri\"",
      "expires=?0;keyid=\"[KEY]\";tag=\"sri\"",
      "expires=@12345;keyid=\"[KEY]\";tag=\"sri\"",
      "expires=%\"display\";keyid=\"[KEY]\";tag=\"sri\"",
      "expires=:badbeef:;keyid=\"[KEY]\";tag=\"sri\"",
      // - Values
      "expires=-1;keyid=\"[KEY]\";tag=\"sri\"",

      // Invalid `keyid`:
      //
      // - Types
      "keyid=;tag=\"sri\"",
      "keyid=1;tag=\"sri\"",
      "keyid=1.1;tag=\"sri\"",
      "keyid=token;tag=\"sri\"",
      "keyid=?0;tag=\"sri\"",
      "keyid=@12345;tag=\"sri\"",
      "keyid=%\"display\";tag=\"sri\"",
      "keyid=:badbeef:;tag=\"sri\"",
      // - Values
      "keyid=\"not a base64-encoded key\";tag=\"sri\"",

      // Invalid `nonce`:
      //
      // - Types
      "keyid=\"[KEY]\";nonce=;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=1;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=1.1;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=token;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=?0;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=@12345;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=%\"display\";tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=:badbeef:;tag=\"not-sri\"",

      // Invalid `tag`:
      //
      // - Types
      "keyid=\"[KEY]\";tag=",
      "keyid=\"[KEY]\";tag=1",
      "keyid=\"[KEY]\";tag=1.1",
      "keyid=\"[KEY]\";tag=token",
      "keyid=\"[KEY]\";tag=?0",
      "keyid=\"[KEY]\";tag=@12345",
      "keyid=\"[KEY]\";tag=%\"display\"",
      "keyid=\"[KEY]\";tag=:badbeef:",
      // - Values
      "keyid=\"[KEY]\";tag=\"not-sri\"",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test << "`");
    std::string processed_input =
        base::StrCat({"signature=(\"unencoded-digest\";sf);", test});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    // As these are all malformed, we expect parsing to return no headers.
    EXPECT_EQ(0u, result->signatures.size());
  }
}

TEST_F(SRIMessageSignatureParserTest, ValidComponents) {
  struct {
    std::string_view components;
    std::vector<std::string_view> expected_names;
  } cases[] = {
      {"\"unencoded-digest\";sf", {"unencoded-digest"}},
      {"\"unencoded-digest\";sf \"@path\";req", {"unencoded-digest", "@path"}},
      {"\"@path\";req \"unencoded-digest\";sf", {"@path", "unencoded-digest"}}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "Component value: `" << test.components << "`");

    // Tack valid parameters onto the test string so that we're actually
    // just testing the component parsing.
    std::string test_with_params =
        base::StrCat({"signature=(", test.components, ");keyid=\"", kPublicKey,
                      "\";tag=\"sri\""});
    auto headers = GetHeaders(kValidSignatureHeader, test_with_params.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->errors.size());
    ASSERT_EQ(test.expected_names.size(),
              result->signatures[0]->components.size());
    for (size_t i = 0; i < test.expected_names.size(); i++) {
      EXPECT_EQ(test.expected_names[i],
                result->signatures[0]->components[i]->name);
    }
  }
}

TEST_F(SRIMessageSignatureParserTest, Created) {
  const char* cases[] = {
      "0",
      "1",
      "999999999999999",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Created value: `" << test << "`");

    // Build the header.
    std::string processed_input =
        base::StrCat({"signature=(\"unencoded-digest\";sf);created=", test,
                      ";keyid=\"[KEY]\";tag=\"sri\""});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->errors.size());
    ASSERT_TRUE(result->signatures[0]->created.has_value());

    int64_t expected_int;
    base::StringToInt64(test, &expected_int);
    EXPECT_EQ(expected_int, result->signatures[0]->created.value());
  }
}

TEST_F(SRIMessageSignatureParserTest, Expires) {
  const char* cases[] = {
      "0",
      "1",
      "999999999999999",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Expires value: `" << test << "`");

    // Build the header.
    std::string processed_input =
        base::StrCat({"signature=(\"unencoded-digest\";sf);expires=", test,
                      ";keyid=\"[KEY]\";tag=\"sri\""});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->errors.size());
    ASSERT_TRUE(result->signatures[0]->expires.has_value());

    int64_t expected_int;
    base::StringToInt64(test, &expected_int);
    EXPECT_EQ(expected_int, result->signatures[0]->expires.value());
  }
}

TEST_F(SRIMessageSignatureParserTest, Nonce) {
  const char* cases[] = {
      "valid",
      "also valid",
      "999999999999999",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Nonce value: `" << test << "`");

    // Build the header.
    std::string processed_input =
        base::StrCat({"signature=(\"unencoded-digest\";sf);keyid=\"[KEY]\";",
                      "nonce=\"", test, "\";tag=\"sri\""});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->errors.size());
    ASSERT_TRUE(result->signatures[0]->nonce.has_value());
    EXPECT_EQ(test, result->signatures[0]->nonce.value());
  }
}

TEST_F(SRIMessageSignatureParserTest, ParameterSorting) {
  std::vector<const char*> params = {
      "created=12345",
      "expires=12345",
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\"",
      "nonce=\"n\"",
      "tag=\"sri\""};

  do {
    std::stringstream header;
    header << "signature=(\"unencoded-digest\";sf)";
    for (const char* param : params) {
      header << ';' << param;
    }
    SCOPED_TRACE(header.str());
    auto headers = GetHeaders(kValidSignatureHeader, header.str().c_str());
    auto result = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->errors.size());
  } while (std::next_permutation(params.begin(), params.end()));
}

//
// "Signature Base" Creation Tests
//
class SRIMessageSignatureBaseTest : public testing::Test {
 protected:
  SRIMessageSignatureBaseTest() {}

  const GURL& url() { return kExampleURL; }

  scoped_refptr<net::HttpResponseHeaders> ValidHeadersPlusInput(
      const char* input) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    builder.AddHeader("Unencoded-Digest", kValidDigestHeader);
    builder.AddHeader("Signature", kValidSignatureHeader);
    if (input) {
      builder.AddHeader("Signature-Input", input);
    }
    return builder.Build();
  }
};

TEST_F(SRIMessageSignatureBaseTest, NoSignaturesNoBase) {
  auto headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200").Build();
  mojom::SRIMessageSignaturePtr signature;

  std::optional<std::string> result =
      ConstructSignatureBase(signature, this->url(), *headers);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SRIMessageSignatureBaseTest, ValidHeadersValidBase) {
  auto headers = ValidHeadersPlusInput(kValidSignatureInputHeader);
  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->errors.size());

  std::optional<std::string> result =
      ConstructSignatureBase(parsed->signatures[0], this->url(), *headers);
  ASSERT_TRUE(result.has_value());
  std::string expected_base =
      base::StrCat({"\"unencoded-digest\";sf: ", kValidDigestHeader,
                    "\n\"@signature-params\": "
                    "(\"unencoded-digest\";sf);keyid=\"",
                    kPublicKey, "\";tag=\"sri\""});
  EXPECT_EQ(expected_base, result.value());
}

TEST_F(SRIMessageSignatureBaseTest, ValidHeadersStrictlySerializedBase) {
  // Regardless of (valid) whitespace, the signature base is strictly
  // serialized.
  const char* cases[] = {
      // Base
      ("signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Leading space.
      (" signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Space before inner-list item.
      ("signature=( \"unencoded-digest\";sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Space after `;` in a param.
      ("signature=(\"unencoded-digest\"; sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Space after inner-list item.
      ("signature=(\"unencoded-digest\";sf );keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Trailing space.
      ("signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\" "),
      // All valid spaces.
      (" signature=( \"unencoded-digest\"; sf );  keyid="
       "\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\"; tag=\"sri\"  ")};

  for (auto* const test : cases) {
    SCOPED_TRACE(test);
    auto headers = ValidHeadersPlusInput(test);
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->errors.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], this->url(), *headers);
    ASSERT_TRUE(result.has_value());
    std::string expected_base =
        base::StrCat({"\"unencoded-digest\";sf: ", kValidDigestHeader,
                      "\n\"@signature-params\": "
                      "(\"unencoded-digest\";sf);keyid=\"",
                      kPublicKey, "\";tag=\"sri\""});
    EXPECT_EQ(expected_base, result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, PathComponent) {
  struct {
    std::string_view url;
    std::string_view path;
  } cases[] = {
      {"https://url.test/", "/"},
      {"https://url.test:8080/", "/"},
      {"https://user:pass@url.test/", "/"},
      {"https://url.test/?a=b", "/"},
      {"https://url.test/#anchor", "/"},
      {"https://url.test/path", "/path"},
      {"https://url.test/path/", "/path/"},
      {"https://url.test/pAtH", "/pAtH"},
      {"https://url.test/%0Apath", "/%0Apath"},
      {"https://url.test/%0apath", "/%0apath"},
      {"https://url.test/path/../", "/"},
      {"https://url.test/ü", "/%C3%BC"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);

    std::string input_header =
        base::StrCat({"signature=(\"unencoded-digest\";sf \"@path\";req);",
                      "keyid=\"", kPublicKey, "\";tag=\"sri\""});

    std::stringstream expected_base;
    expected_base
        << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
        << "\"@path\";req: " << test.path << '\n'
        << "\"@signature-params\": (\"unencoded-digest\";sf \"@path\";req);"
        << "keyid=\"" << kPublicKey << "\";tag=\"sri\"";

    auto headers = ValidHeadersPlusInput(input_header.c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->errors.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], GURL(test.url), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, ValidHeaderParams) {
  struct {
    int64_t created;
    int64_t expires;
    std::string nonce;
  } cases[] = {{0, 0, ""},
               {0, 1, ""},
               {0, 0, "noncy-nonce"},
               {0, 1, "noncy-nonce"},
               {1, 0, ""},
               {1, 1, ""},
               {1, 0, "noncy-nonce"},
               {1, 1, "noncy-nonce"},
               {999999999999999, 999999999999999, "noncy-nonce"}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "Test case:\n- Created: `" << test.created
                 << "`\n- Expires: `" << test.expires << "`\n- Nonce:  `"
                 << test.nonce << '`');

    // Construct the header and the expectations based on the test case:
    std::stringstream input_header;
    input_header << "signature=(\"unencoded-digest\";sf)";

    std::stringstream expected_base;
    expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                  << "\"@signature-params\": (\"unencoded-digest\";sf)";
    if (test.created) {
      input_header << ";created=" << test.created;
      expected_base << ";created=" << test.created;
    }
    if (test.expires) {
      input_header << ";expires=" << test.expires;
      expected_base << ";expires=" << test.expires;
    }
    input_header << ";keyid=\"" << kPublicKey << '"';
    expected_base << ";keyid=\"" << kPublicKey << '"';
    if (!test.nonce.empty()) {
      input_header << ";nonce=\"" << test.nonce << '"';
      expected_base << ";nonce=\"" << test.nonce << '"';
    }
    input_header << ";tag=\"sri\"";
    expected_base << ";tag=\"sri\"";

    auto headers = ValidHeadersPlusInput(input_header.str().c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->errors.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], this->url(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, ParameterSorting) {
  std::vector<const char*> params = {
      "created=12345",
      "expires=12345",
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\"",
      "nonce=\"n\"",
      "tag=\"sri\""};

  do {
    std::stringstream input_header;
    input_header << "signature=(\"unencoded-digest\";sf)";

    std::stringstream expected_base;
    expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                  << "\"@signature-params\": (\"unencoded-digest\";sf)";
    for (const char* param : params) {
      input_header << ';' << param;
      expected_base << ';' << param;
    }

    SCOPED_TRACE(input_header.str());
    auto headers = ValidHeadersPlusInput(input_header.str().c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->errors.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], this->url(), *headers);
    EXPECT_THAT(result, testing::Optional(expected_base.str()));
  } while (std::next_permutation(params.begin(), params.end()));
}

//
// Validation Tests
//
class SRIMessageSignatureValidationTest : public testing::Test {
 protected:
  SRIMessageSignatureValidationTest() {}

  scoped_refptr<net::HttpResponseHeaders> Headers(std::string_view digest,
                                                  std::string_view signature,
                                                  std::string_view input) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    if (!digest.empty()) {
      builder.AddHeader("Unencoded-Digest", digest);
    }
    if (!signature.empty()) {
      builder.AddHeader("Signature", signature);
    }
    if (!input.empty()) {
      builder.AddHeader("Signature-Input", input);
    }
    return builder.Build();
  }

  const GURL& url() { return kExampleURL; }

  scoped_refptr<net::HttpResponseHeaders> ValidHeaders() {
    return Headers(kValidDigestHeader, kValidSignatureHeader,
                   kValidSignatureInputHeader);
  }

  std::string SignatureInputHeader(std::string_view name,
                                   std::string_view keyid) {
    return base::StrCat({name,
                         "=(\"unencoded-digest\";sf);"
                         "keyid=\"",
                         keyid, "\";tag=\"sri\""});
  }

  std::string SignatureHeader(std::string name, std::string_view sig) {
    return base::StrCat({name, "=:", sig, ":"});
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SRIMessageSignatureValidationTest, NoSignatures) {
  auto headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200").Build();
  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(0u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->errors.size());

  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, this->url(), *headers));
  EXPECT_EQ(0u, parsed->errors.size());
}

TEST_F(SRIMessageSignatureValidationTest, ValidSignature) {
  auto headers = ValidHeaders();
  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->errors.size());

  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, this->url(), *headers));
  EXPECT_EQ(0u, parsed->errors.size());
}

TEST_F(SRIMessageSignatureValidationTest, ValidPlusInvalidSignature) {
  const char* wrong_key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  const char* wrong_signature =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAA==";

  std::string signature_header =
      base::StrCat({SignatureHeader("signature", kSignature), ",",
                    SignatureHeader("bad-signature", wrong_signature)});
  std::string input_header =
      base::StrCat({SignatureInputHeader("signature", kPublicKey), ",",
                    SignatureInputHeader("bad-signature", wrong_key)});
  auto headers = Headers(kValidDigestHeader, signature_header, input_header);

  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(2u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->errors.size());

  EXPECT_FALSE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, this->url(), *headers));
  ASSERT_EQ(1u, parsed->errors.size());
  EXPECT_EQ(mojom::SRIMessageSignatureError::kValidationFailedSignatureMismatch,
            parsed->errors[0]);
}

TEST_F(SRIMessageSignatureValidationTest, MultipleValidSignatures) {
  std::string signature_header =
      base::StrCat({SignatureHeader("signature", kSignature), ",",
                    SignatureHeader("bad-signature", kSignature)});
  std::string input_header =
      base::StrCat({SignatureInputHeader("signature", kPublicKey), ",",
                    SignatureInputHeader("bad-signature", kPublicKey)});
  auto headers = Headers(kValidDigestHeader, signature_header, input_header);

  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(2u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->errors.size());

  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, this->url(), *headers));
  EXPECT_EQ(0u, parsed->errors.size());
}

TEST_F(SRIMessageSignatureValidationTest, ValidSignatureExpires) {
  auto headers = Headers(kValidDigestHeader, kValidExpiringSignatureHeader,
                         kValidExpiringSignatureInputHeader);
  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->errors.size());

  // Signature should validate at the moment before and of expiration.
  auto diff = kValidExpiringSignatureExpiresAt -
              base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000 - 1;
  task_environment_.AdvanceClock(base::Seconds(diff));
  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, this->url(), *headers));
  ASSERT_EQ(0u, parsed->errors.size());

  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, this->url(), *headers));
  ASSERT_EQ(0u, parsed->errors.size());

  // ...but not after expiration.
  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_FALSE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, this->url(), *headers));
  ASSERT_EQ(1u, parsed->errors.size());
  EXPECT_EQ(mojom::SRIMessageSignatureError::kValidationFailedSignatureExpired,
            parsed->errors[0]);
}

TEST_F(SRIMessageSignatureValidationTest, ValidSignatureDigestHeaderMismatch) {
  const char* cases[] = {
      "",
      "sha-256=:YQ==:",
      kValidDigestHeader512,
  };

  for (auto* test : cases) {
    SCOPED_TRACE(testing::Message() << "Test case: `" << test << '`');

    auto headers =
        Headers(test, kValidSignatureHeader, kValidSignatureInputHeader);
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->errors.size());

    EXPECT_FALSE(
        ValidateSRIMessageSignaturesOverHeaders(parsed, this->url(), *headers));
    EXPECT_EQ(1u, parsed->errors.size());
    EXPECT_EQ(
        mojom::SRIMessageSignatureError::kValidationFailedSignatureMismatch,
        parsed->errors[0]);
  }
}

class SRIMessageSignatureEnforcementTest
    : public SRIMessageSignatureValidationTest,
      public testing::WithParamInterface<bool> {
 protected:
  SRIMessageSignatureEnforcementTest() {}

  mojom::URLResponseHeadPtr ResponseHead(std::string_view digest,
                                         std::string_view signature,
                                         std::string_view input) {
    auto head = mojom::URLResponseHead::New();
    head->headers = Headers(digest, signature, input);
    return head;
  }
};

INSTANTIATE_TEST_SUITE_P(FeatureFlag,
                         SRIMessageSignatureEnforcementTest,
                         testing::Values(true, false));

TEST_P(SRIMessageSignatureEnforcementTest, NoHeaders) {
  bool feature_flag_enabled = GetParam();

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      features::kSRIMessageSignatureEnforcement, feature_flag_enabled);

  auto head = ResponseHead("", "", "");
  auto result = MaybeBlockResponseForSRIMessageSignature(
      this->url(), *head, /*checks_forced_by_initiator=*/false);
  EXPECT_FALSE(result.has_value());
}

TEST_P(SRIMessageSignatureEnforcementTest, ValidHeaders) {
  bool feature_flag_enabled = GetParam();

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      features::kSRIMessageSignatureEnforcement, feature_flag_enabled);

  auto head = ResponseHead(kValidDigestHeader, kValidSignatureHeader,
                           kValidSignatureInputHeader);
  auto result = MaybeBlockResponseForSRIMessageSignature(
      this->url(), *head, /*checks_forced_by_initiator=*/false);
  EXPECT_FALSE(result.has_value());
}

TEST_P(SRIMessageSignatureEnforcementTest, MismatchedHeaders) {
  bool feature_flag_enabled = GetParam();

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      features::kSRIMessageSignatureEnforcement, feature_flag_enabled);

  const char* wrong_key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  const char* wrong_signature =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAA==";

  auto head = ResponseHead(kValidDigestHeader,
                           SignatureHeader("bad-signature", wrong_signature),
                           SignatureInputHeader("bad-signature", wrong_key));
  auto result = MaybeBlockResponseForSRIMessageSignature(
      this->url(), *head, /*checks_forced_by_initiator=*/false);
  if (feature_flag_enabled) {
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch,
              result.value());
  } else {
    EXPECT_FALSE(result.has_value());
  }
}

TEST_P(SRIMessageSignatureEnforcementTest, MismatchedHeadersAndForcedChecks) {
  // Same test as `MismatchedHeaders`, but forces integrity checks, which means
  // that the result will not depend upon whether or not the feature flag is
  // enabled: this test should consistently fail validation.
  bool feature_flag_enabled = GetParam();

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      features::kSRIMessageSignatureEnforcement, feature_flag_enabled);

  const char* wrong_key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  const char* wrong_signature =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAA==";

  auto head = ResponseHead(kValidDigestHeader,
                           SignatureHeader("bad-signature", wrong_signature),
                           SignatureInputHeader("bad-signature", wrong_key));
  auto result = MaybeBlockResponseForSRIMessageSignature(
      this->url(), *head, /*checks_forced_by_initiator=*/true);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch,
            result.value());
}

class SRIMessageSignatureRequestHeaderTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  SRIMessageSignatureRequestHeaderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        context_(net::CreateTestURLRequestContextBuilder()->Build()),
        url_request_(context_->CreateRequest(kExampleURL,
                                             net::DEFAULT_PRIORITY,
                                             /*delegate=*/nullptr,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kSRIMessageSignatureEnforcement, GetParam());
  }

  net::URLRequest* url_request() const { return url_request_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> url_request_;
};

INSTANTIATE_TEST_SUITE_P(FeatureFlag,
                         SRIMessageSignatureRequestHeaderTest,
                         testing::Values(true, false));

TEST_P(SRIMessageSignatureRequestHeaderTest, NoSignaturesNoHeader) {
  MaybeSetAcceptSignatureHeader(url_request(), {});
  EXPECT_FALSE(url_request()
                   ->extra_request_headers()
                   .GetHeader(kAcceptSignature)
                   .has_value());
}

TEST_P(SRIMessageSignatureRequestHeaderTest, InvalidSignatures) {
  const std::vector<std::string> cases[] = {
      // Not base64:
      {"invalid"},
      {"also\rinvalid"},
      {"also\ninvalid"},
      {"also\r\ninvalid"},
      {"also\"invalid"},
      // Incorrect padding:
      {"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs"},
      // base64url:
      {"JrQLj5P_89iXES9-vFgrIy29clF9CC_oPPsw3c5D0bs="},
      // Incorrect length:
      {"YQ=="},
      // Prefixed:
      {"ed25519-JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs="},
      // Multiple invalid:
      {"invalid", "and also invalid"},
      {"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs",
       "JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs"},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(base::JoinString(test, ", "));
    MaybeSetAcceptSignatureHeader(url_request(), test);
    EXPECT_FALSE(url_request()
                     ->extra_request_headers()
                     .GetHeader(kAcceptSignature)
                     .has_value());
  }
}

TEST_P(SRIMessageSignatureRequestHeaderTest, ValidSignature) {
  const std::vector<std::string> cases[] = {
      {kPublicKey},
      // Valid, invalid => valid
      {kPublicKey, "invalid"},
      // Invalid, valid => valid
      {"invalid", kPublicKey},
      // Invalid, valid, invalid => valid
      {"invalid", kPublicKey, "invalid"},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(base::JoinString(test, ", "));
    MaybeSetAcceptSignatureHeader(url_request(), test);

    auto result =
        url_request()->extra_request_headers().GetHeader(kAcceptSignature);

    // The result does not depend on the feature flag: we rely on the caller to
    // give us expected signatures iff they should be delivered.
    std::string expected =
        base::StrCat({"sig0=(\"unencoded-digest\";sf);keyid=\"", kPublicKey,
                      "\";tag=\"sri\""});
    EXPECT_THAT(result, testing::Optional(expected));
  }
}

TEST_P(SRIMessageSignatureRequestHeaderTest, ValidSignatures) {
  const std::vector<std::string> cases[] = {
      {kPublicKey, kPublicKey2},
      // Valid, invalid => valid
      {kPublicKey, kPublicKey2, "invalid"},
      // Invalid, valid => valid
      {"invalid", kPublicKey, kPublicKey2},
      // Invalid, valid, invalid => valid
      {"invalid", kPublicKey, kPublicKey2, "invalid"},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(base::JoinString(test, ", "));
    MaybeSetAcceptSignatureHeader(url_request(), test);

    auto result =
        url_request()->extra_request_headers().GetHeader(kAcceptSignature);
    // The result does not depend on the feature flag: we rely on the caller to
    // give us expected signatures iff they should be delivered.
    std::string expected = base::StrCat(
        {"sig0=(\"unencoded-digest\";sf);keyid=\"", kPublicKey,
         "\";tag=\"sri\", ", "sig1=(\"unencoded-digest\";sf);keyid=\"",
         kPublicKey2, "\";tag=\"sri\""});
    EXPECT_THAT(result, testing::Optional(expected));
  }
}

}  // namespace network
