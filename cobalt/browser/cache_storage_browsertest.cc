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

#include <optional>
#include <string>

#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/quota/quota_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

class CacheStorageBrowserTest : public content::ContentBrowserTest {
 public:
  CacheStorageBrowserTest() = default;
  ~CacheStorageBrowserTest() = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    content::StoragePartition* partition = shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition();
    auto* partition_impl =
        static_cast<content::StoragePartitionImpl*>(partition);
    if (partition_impl->HasSplitQuota()) {
      SetSplitQuotaSettings();
    } else {
      SetUnifiedQuotaSettings();
    }
  }

  void TearDownOnMainThread() override {
    content::StoragePartition::SetDefaultQuotaSettingsForTesting(nullptr);
    content::StoragePartition::SetDefaultCacheQuotaSettingsForTesting(nullptr);
  }

 private:
  // Simulates a configuration where persistent storage and cache are on
  // separate physical partitions with different sizes.
  void SetSplitQuotaSettings() {
    static storage::QuotaSettings quota_settings(
        storage::GetHardCodedSettings(1 * 1024 * 1024));
    content::StoragePartition::SetDefaultQuotaSettingsForTesting(
        &quota_settings);

    static storage::QuotaSettings cache_quota_settings(
        storage::GetHardCodedSettings(24 * 1024 * 1024));
    content::StoragePartition::SetDefaultCacheQuotaSettingsForTesting(
        &cache_quota_settings);
  }

  void SetUnifiedQuotaSettings() {
    static storage::QuotaSettings quota_settings(
        storage::GetHardCodedSettings(24 * 1024 * 1024));
    content::StoragePartition::SetDefaultQuotaSettingsForTesting(
        &quota_settings);
  }
};

// Verifies that the cache quota is applied to cache writes.
// Calls cache.put to write 2MiB of binary data to Cache Storage, then reads it
// back via cache.match to verify size and data integrity.
IN_PROC_BROWSER_TEST_F(CacheStorageBrowserTest, PutTwoMegabyteToCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  const char kScript[] = R"(
    (async () => {
      try {
        const cache = await caches.open('cobalt-test-2mb-cache');
        const kPayloadSize = 2 * 1024 * 1024;
        const payload = new Uint8Array(kPayloadSize);
        for (let i = 0; i < kPayloadSize; i++) {
          payload[i] = i % 256;
        }

        const request = new Request('/test-2mb-resource');
        const response = new Response(payload, {
          status: 200,
          statusText: 'OK',
          headers: {
            'Content-Type': 'application/octet-stream',
            'X-Custom-Header': 'Cobalt2MBTest'
          }
        });

        await cache.put(request, response);

        // Retrieve the cached response.
        const matched = await cache.match(request);
        if (!matched) {
          return { success: false, error: 'cache.match returned null' };
        }
        if (matched.status !== 200) {
          return { success: false, error: `Unexpected status code: ${matched.status}` };
        }
        if (matched.headers.get('X-Custom-Header') !== 'Cobalt2MBTest') {
          return { success: false, error: 'Custom header mismatch' };
        }
        if (matched.headers.get('Content-Type') !== 'application/octet-stream') {
          return { success: false, error: 'Content-Type header mismatch' };
        }

        // Verify that the retrieved payload size matches what was written.
        const buffer = await matched.arrayBuffer();
        if (buffer.byteLength !== kPayloadSize) {
          return {
            success: false,
            error: `Byte length mismatch: expected ${kPayloadSize}, got ${buffer.byteLength}`
          };
        }

        const resultView = new Uint8Array(buffer);
        for (let i = 0; i < kPayloadSize; i += 4096) {
          if (resultView[i] !== (i % 256)) {
            return {
              success: false,
              error: `Byte mismatch at offset ${i}: expected ${i % 256}, got ${resultView[i]}`
            };
          }
        }

        return { success: true };
      } catch (err) {
        return { success: false, error: err.toString() };
      }
    })()
  )";

  content::EvalJsResult eval_result =
      content::EvalJs(shell()->web_contents(), kScript);
  ASSERT_TRUE(eval_result.value.is_dict());
  const base::Value::Dict& dict = eval_result.value.GetDict();

  std::optional<bool> success = dict.FindBool("success");
  ASSERT_TRUE(success.has_value())
      << "Response dictionary missing 'success' field";

  if (!*success) {
    const std::string* error = dict.FindString("error");
    FAIL() << "JavaScript cache.put test failed: "
           << (error ? *error : "Unknown error");
  }
  EXPECT_TRUE(*success);
}

// Calls cache.put to write 1MB, then overwrites the entry with a new payload.
IN_PROC_BROWSER_TEST_F(CacheStorageBrowserTest, PutAndOverwriteOneMegabyte) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  const char kScript[] = R"(
    (async () => {
      try {
        const cache = await caches.open('cobalt-test-overwrite-cache');
        const kPayloadSize = 1024 * 1024;
        const request = new Request('/test-overwrite-1mb');

        // Initial 1MB payload (pattern A: i % 256)
        const payloadA = new Uint8Array(kPayloadSize);
        for (let i = 0; i < kPayloadSize; i++) {
          payloadA[i] = i % 256;
        }
        await cache.put(request, new Response(payloadA, {
          headers: { 'X-Version': 'A' }
        }));

        // Replacement 1MB payload (pattern B: (255 - (i % 256)))
        const payloadB = new Uint8Array(kPayloadSize);
        for (let i = 0; i < kPayloadSize; i++) {
          payloadB[i] = 255 - (i % 256);
        }
        await cache.put(request, new Response(payloadB, {
          headers: { 'X-Version': 'B' }
        }));

        // Verify the replaced entry
        const matched = await cache.match(request);
        if (!matched) {
          return { success: false, error: 'cache.match returned null after overwrite' };
        }
        if (matched.headers.get('X-Version') !== 'B') {
          return { success: false, error: 'Header did not update to version B' };
        }

        const buffer = await matched.arrayBuffer();
        if (buffer.byteLength !== kPayloadSize) {
          return { success: false, error: `Size mismatch after overwrite: ${buffer.byteLength}` };
        }

        const resultView = new Uint8Array(buffer);
        for (let i = 0; i < kPayloadSize; i += 4096) {
          const expectedByte = 255 - (i % 256);
          if (resultView[i] !== expectedByte) {
            return {
              success: false,
              error: `Byte mismatch in overwritten payload at ${i}: expected ${expectedByte}, got ${resultView[i]}`
            };
          }
        }

        return { success: true };
      } catch (err) {
        return { success: false, error: err.toString() };
      }
    })()
  )";

  content::EvalJsResult eval_result =
      content::EvalJs(shell()->web_contents(), kScript);
  ASSERT_TRUE(eval_result.value.is_dict());
  const base::Value::Dict& dict = eval_result.value.GetDict();

  std::optional<bool> success = dict.FindBool("success");
  ASSERT_TRUE(success.has_value())
      << "Response dictionary missing 'success' field";
  if (!*success) {
    const std::string* error = dict.FindString("error");
    FAIL() << "JavaScript cache.put overwrite test failed: "
           << (error ? *error : "Unknown error");
  }
  EXPECT_TRUE(*success);
}

// Verifies that the persistent storage quota is applied correctly.
// Attempts to write 2MB to persistent storage via IndexedDB. This write should
// fail since persistent storage quota is 1MB.
// If this is a platform that only supports unified quota, we explicitly lower
// the quota to 1MB here so we don't have to write a 24MB payload.
IN_PROC_BROWSER_TEST_F(CacheStorageBrowserTest,
                       FailToWriteMoreThanOneMegabyteToPersistentStorage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  static storage::QuotaSettings quota_settings(
      storage::GetHardCodedSettings(1024 * 1024));
  content::StoragePartition::SetDefaultQuotaSettingsForTesting(&quota_settings);

  const char kScript[] = R"(
    (async () => {
      try {
        const db = await new Promise((resolve, reject) => {
          const req = indexedDB.open('cobalt-test-quota-db', 1);
          req.onupgradeneeded = (e) => {
            e.target.result.createObjectStore('test-store', { keyPath: 'id' });
          };
          req.onsuccess = (e) => resolve(e.target.result);
          req.onerror = (e) => reject(req.error);
        });

        // Generate uncompressible data using XOR-shift PRNG so Snappy compression does not shrink it.
        // This usually shouldn't matter but is done just in case.
        const kChunkSize = 256 * 1024; // 256KB
        const chunk = new Uint8Array(kChunkSize);
        let state = 123456789;
        for (let i = 0; i < kChunkSize; i++) {
          state ^= (state << 13);
          state ^= (state >> 17);
          state ^= (state << 5);
          chunk[i] = state & 0xFF;
        }

        // Attempt to write up to 2MB in chunks (8 * 256KB = 2MB). This should fail.
        let writeFailedWithQuota = false;
        let errorDetail = '';

        for (let i = 0; i < 8; i++) {
          const writeResult = await new Promise((resolve) => {
            let settled = false;
            const tx = db.transaction(['test-store'], 'readwrite');
            const store = tx.objectStore('test-store');
            const putReq = store.put({ id: i, data: chunk });

            putReq.onerror = (e) => {
              e.preventDefault();
              e.stopPropagation();
            };

            tx.oncomplete = () => {
              if (!settled) {
                settled = true;
                resolve({ success: true });
              }
            };

            tx.onabort = () => {
              if (!settled) {
                settled = true;
                const err = tx.error ? tx.error.name : (putReq.error ? putReq.error.name : 'Aborted');
                resolve({ success: false, error: err });
              }
            };

            tx.onerror = (e) => {
              e.preventDefault();
              if (!settled) {
                settled = true;
                const err = tx.error ? tx.error.name : (putReq.error ? putReq.error.name : 'Error');
                resolve({ success: false, error: err });
              }
            };
          });

          if (!writeResult.success) {
            writeFailedWithQuota = true;
            errorDetail = writeResult.error;
            break;
          }
        }

        if (!writeFailedWithQuota) {
          return {
            failedAsExpected: false,
            error: 'Writing 2MB to persistent storage unexpectedly succeeded!'
          };
        }

        return {
          failedAsExpected: true,
          errorName: errorDetail
        };
      } catch (err) {
        return {
          failedAsExpected: true,
          errorName: err.toString()
        };
      }
    })()
  )";

  content::EvalJsResult eval_result =
      content::EvalJs(shell()->web_contents(), kScript);
  ASSERT_TRUE(eval_result.value.is_dict());
  const base::Value::Dict& dict = eval_result.value.GetDict();

  std::optional<bool> failed_as_expected = dict.FindBool("failedAsExpected");
  ASSERT_TRUE(failed_as_expected.has_value())
      << "Response dictionary missing 'failedAsExpected' field";

  if (!*failed_as_expected) {
    const std::string* error = dict.FindString("error");
    FAIL() << "Test failed: " << (error ? *error : "Unknown error");
  }
  EXPECT_TRUE(*failed_as_expected);
}

}  // namespace cobalt
