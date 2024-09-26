// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_MOCK_PLATFORM_KEYS_SERVICE_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_MOCK_PLATFORM_KEYS_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

namespace ash {
namespace platform_keys {

class MockPlatformKeysService : public PlatformKeysService {
 public:
  MockPlatformKeysService();
  MockPlatformKeysService(const MockPlatformKeysService&) = delete;
  MockPlatformKeysService& operator=(const MockPlatformKeysService&) = delete;
  ~MockPlatformKeysService() override;

  MOCK_METHOD(void,
              AddObserver,
              (PlatformKeysServiceObserver * observer),
              (override));

  MOCK_METHOD(void,
              RemoveObserver,
              (PlatformKeysServiceObserver * observer),
              (override));

  MOCK_METHOD(void,
              GenerateRSAKey,
              (chromeos::platform_keys::TokenId token_id,
               unsigned int modulus_length_bits,
               bool sw_backed,
               GenerateKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              GenerateECKey,
              (chromeos::platform_keys::TokenId token_id,
               const std::string& named_curve,
               GenerateKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              SignRSAPKCS1Digest,
              (absl::optional<chromeos::platform_keys::TokenId> token_id,
               std::vector<uint8_t> data,
               std::vector<uint8_t> public_key_spki_der,
               chromeos::platform_keys::HashAlgorithm hash_algorithm,
               SignCallback callback),
              (override));

  MOCK_METHOD(void,
              SignRSAPKCS1Raw,
              (absl::optional<chromeos::platform_keys::TokenId> token_id,
               std::vector<uint8_t> data,
               std::vector<uint8_t> public_key_spki_der,
               SignCallback callback),
              (override));

  MOCK_METHOD(void,
              SignECDSADigest,
              (absl::optional<chromeos::platform_keys::TokenId> token_id,
               std::vector<uint8_t> data,
               std::vector<uint8_t> public_key_spki_der,
               chromeos::platform_keys::HashAlgorithm hash_algorithm,
               SignCallback callback),
              (override));

  MOCK_METHOD(void,
              SelectClientCertificates,
              (std::vector<std::string> certificate_authorities,
               SelectCertificatesCallback callback),
              (override));

  MOCK_METHOD(void,
              GetCertificates,
              (chromeos::platform_keys::TokenId token_id,
               GetCertificatesCallback callback),
              (override));

  MOCK_METHOD(void,
              GetAllKeys,
              (chromeos::platform_keys::TokenId token_id,
               GetAllKeysCallback callback),
              (override));

  MOCK_METHOD(void,
              ImportCertificate,
              (chromeos::platform_keys::TokenId token_id,
               const scoped_refptr<net::X509Certificate>& certificate,
               ImportCertificateCallback callback),
              (override));

  MOCK_METHOD(void,
              RemoveCertificate,
              (chromeos::platform_keys::TokenId token_id,
               const scoped_refptr<net::X509Certificate>& certificate,
               RemoveCertificateCallback callback),
              (override));

  MOCK_METHOD(void,
              RemoveKey,
              (chromeos::platform_keys::TokenId token_id,
               std::vector<uint8_t> public_key_spki_der,
               RemoveKeyCallback callback),
              (override));

  MOCK_METHOD(void, GetTokens, (GetTokensCallback callback), (override));

  MOCK_METHOD(void,
              GetKeyLocations,
              (std::vector<uint8_t> public_key_spki_der,
               GetKeyLocationsCallback callback),
              (override));

  MOCK_METHOD(void,
              SetAttributeForKey,
              (chromeos::platform_keys::TokenId token_id,
               const std::string& public_key_spki_der,
               chromeos::platform_keys::KeyAttributeType attribute_type,
               std::vector<uint8_t> attribute_value,
               SetAttributeForKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              GetAttributeForKey,
              (chromeos::platform_keys::TokenId token_id,
               const std::string& public_key_spki_der,
               chromeos::platform_keys::KeyAttributeType attribute_type,
               GetAttributeForKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              IsKeyOnToken,
              (chromeos::platform_keys::TokenId token_id,
               std::vector<uint8_t> public_key_spki_der,
               IsKeyOnTokenCallback callback),
              (override));

  MOCK_METHOD(void,
              SetMapToSoftokenAttrsForTesting,
              (bool map_to_softoken_attrs_for_testing),
              (override));
};

std::unique_ptr<KeyedService> BuildMockPlatformKeysService(
    content::BrowserContext* context);

}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_MOCK_PLATFORM_KEYS_SERVICE_H_
