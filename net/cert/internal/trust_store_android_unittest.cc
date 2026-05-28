// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/cert/internal/trust_store_android.h"

#include "base/memory/scoped_refptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {

namespace {

// Helper thread to query GetTrust concurrently.
class ConcurrentQueryThread : public base::SimpleThread {
 public:
  ConcurrentQueryThread(TrustStoreAndroid* trust_store,
                        const bssl::ParsedCertificate* cert)
      : base::SimpleThread("ConcurrentQueryThread"),
        trust_store_(trust_store),
        cert_(cert) {}

  void Run() override {
    bssl::CertificateTrust trust = trust_store_->GetTrust(cert_);
    (void)trust;

    bssl::ParsedCertificateList issuers;
    trust_store_->SyncGetIssuersOf(cert_, &issuers);
  }

 private:
  TrustStoreAndroid* const trust_store_;
  const bssl::ParsedCertificate* const cert_;
};

// Helper thread to trigger reloads continuously.
class ReloadThread : public base::SimpleThread {
 public:
  explicit ReloadThread(TrustStoreAndroid* trust_store)
      : base::SimpleThread("ReloadThread"), trust_store_(trust_store) {}

  void Run() override {
    for (int i = 0; i < 10; ++i) {
      trust_store_->OnTrustStoreChanged();
      base::PlatformThread::Sleep(base::Milliseconds(5));
    }
  }

 private:
  TrustStoreAndroid* const trust_store_;
};

// Helper thread to query continuously during reloads.
class ContinuousQueryThread : public base::SimpleThread {
 public:
  ContinuousQueryThread(TrustStoreAndroid* trust_store,
                        const bssl::ParsedCertificate* cert)
      : base::SimpleThread("ContinuousQueryThread"),
        trust_store_(trust_store),
        cert_(cert) {}

  void Run() override {
    for (int i = 0; i < 50; ++i) {
      bssl::CertificateTrust trust = trust_store_->GetTrust(cert_);
      (void)trust;
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
  }

 private:
  TrustStoreAndroid* const trust_store_;
  const bssl::ParsedCertificate* const cert_;
};

}  // namespace

// Test Case 1: Basic Concurrency & Stress Safety
// Verifies that 10 threads querying the store simultaneously during startup
// does not cause deadlocks, double-initialization crashes, or data races.
TEST(TrustStoreAndroidTest, ConcurrentInitializationSafety) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_GE(certs.size(), 1u);

  std::shared_ptr<const bssl::ParsedCertificate> test_cert =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(certs[0]->cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(test_cert);

  TrustStoreAndroid trust_store;

  const int kNumThreads = 10;
  std::vector<std::unique_ptr<ConcurrentQueryThread>> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    threads.push_back(std::make_unique<ConcurrentQueryThread>(&trust_store, test_cert.get()));
  }

  for (auto& thread : threads) {
    thread->Start();
  }
  for (auto& thread : threads) {
    thread->Join();
  }
}

// Test Case 2: Concurrent Reload Safety
// Verifies that triggering reloads (OnTrustStoreChanged) concurrently with
// continuous queries from multiple threads does not cause deadlocks, crashes,
// or data races.
TEST(TrustStoreAndroidTest, ConcurrentReloadSafety) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_GE(certs.size(), 1u);

  std::shared_ptr<const bssl::ParsedCertificate> test_cert =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(certs[0]->cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(test_cert);

  TrustStoreAndroid trust_store;

  // 1. First initialization.
  trust_store.Initialize();

  // 2. Start reload thread and multiple query threads.
  ReloadThread reload_thread(&trust_store);
  
  const int kNumQueryThreads = 5;
  std::vector<std::unique_ptr<ContinuousQueryThread>> query_threads;
  for (int i = 0; i < kNumQueryThreads; ++i) {
    query_threads.push_back(
        std::make_unique<ContinuousQueryThread>(&trust_store, test_cert.get()));
  }

  reload_thread.Start();
  for (auto& thread : query_threads) {
    thread->Start();
  }

  reload_thread.Join();
  for (auto& thread : query_threads) {
    thread->Join();
  }
}

}  // namespace net

