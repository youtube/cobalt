// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "net/android/keystore.h"
#include "net/cert/x509_certificate.h"
#include "net/net_test_jni_headers/AndroidKeyStoreTestUtil_jni.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

typedef base::android::ScopedJavaLocalRef<jobject> ScopedJava;

bool ReadTestFile(const char* filename, std::string* pkcs8) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  base::FilePath file_path = certs_dir.AppendASCII(filename);
  return base::ReadFileToString(file_path, pkcs8);
}

// Retrieve a JNI local ref from encoded PKCS#8 data.
ScopedJava GetPKCS8PrivateKeyJava(android::PrivateKeyType key_type,
                                  const std::string& pkcs8_key) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> bytes(
      base::android::ToJavaByteArray(
          env, reinterpret_cast<const uint8_t*>(pkcs8_key.data()),
          pkcs8_key.size()));

  ScopedJava key(Java_AndroidKeyStoreTestUtil_createPrivateKeyFromPKCS8(
      env, key_type, bytes));

  return key;
}

struct TestKey {
  const char* name;
  const char* cert_file;
  const char* key_file;
  int type;
  android::PrivateKeyType android_key_type;
};

const TestKey kTestKeys[] = {
    {"RSA", "client_1.pem", "client_1.pk8", EVP_PKEY_RSA,
     android::PRIVATE_KEY_TYPE_RSA},
    {"ECDSA_P256", "client_4.pem", "client_4.pk8", EVP_PKEY_EC,
     android::PRIVATE_KEY_TYPE_ECDSA},
    {"ECDSA_P384", "client_5.pem", "client_5.pk8", EVP_PKEY_EC,
     android::PRIVATE_KEY_TYPE_ECDSA},
    {"ECDSA_P521", "client_6.pem", "client_6.pk8", EVP_PKEY_EC,
     android::PRIVATE_KEY_TYPE_ECDSA},
};

std::string TestKeyToString(const testing::TestParamInfo<TestKey>& params) {
  return params.param.name;
}

}  // namespace

class SSLPlatformKeyAndroidTest : public testing::TestWithParam<TestKey>,
                                  public WithTaskEnvironment {};

TEST_P(SSLPlatformKeyAndroidTest, Matches) {
  const TestKey& test_key = GetParam();

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), test_key.cert_file);
  ASSERT_TRUE(cert);

  std::string key_bytes;
  ASSERT_TRUE(ReadTestFile(test_key.key_file, &key_bytes));
  ScopedJava java_key =
      GetPKCS8PrivateKeyJava(test_key.android_key_type, key_bytes);
  ASSERT_FALSE(java_key.is_null());

  scoped_refptr<SSLPrivateKey> key = WrapJavaPrivateKey(cert.get(), java_key);
  ASSERT_TRUE(key);

  EXPECT_EQ(SSLPrivateKey::DefaultAlgorithmPreferences(test_key.type,
                                                       true /* supports_pss */),
            key->GetAlgorithmPreferences());

  TestSSLPrivateKeyMatches(key.get(), key_bytes);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SSLPlatformKeyAndroidTest,
                         testing::ValuesIn(kTestKeys),
                         TestKeyToString);

}  // namespace net
