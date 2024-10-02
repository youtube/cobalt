// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_api.h"

#include <memory>
#include <set>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "components/value_store/leveldb_value_store.h"
#include "components/value_store/value_store.h"
#include "components/value_store/value_store_factory_impl.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"
#include "extensions/browser/api/storage/settings_test_util.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/manifest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace extensions {

namespace {

// Caller owns the returned object.
std::unique_ptr<KeyedService> CreateStorageFrontendForTesting(
    content::BrowserContext* context) {
  scoped_refptr<value_store::ValueStoreFactory> factory =
      new value_store::ValueStoreFactoryImpl(context->GetPath());
  return StorageFrontend::CreateForTesting(factory, context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<extensions::EventRouter>(context, nullptr);
}

}  // namespace

class StorageApiUnittest : public ApiUnitTest {
 public:
  StorageApiUnittest() {}
  ~StorageApiUnittest() override {}

 protected:
  void SetUp() override {
    ApiUnitTest::SetUp();

    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(), base::BindRepeating(&BuildEventRouter));

    // Ensure a StorageFrontend can be created on demand. The StorageFrontend
    // will be owned by the KeyedService system.
    StorageFrontend::GetFactoryInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating(&CreateStorageFrontendForTesting));

    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
  }

  void TearDown() override {
    render_process_host_.reset();
    ApiUnitTest::TearDown();
  }

  content::RenderProcessHost* render_process_host() const {
    return render_process_host_.get();
  }

  // Runs the storage.set() API function with local storage.
  void RunSetFunction(const std::string& key, const std::string& value) {
    RunFunction(
        new StorageStorageAreaSetFunction(),
        base::StringPrintf(
            "[\"local\", {\"%s\": \"%s\"}]", key.c_str(), value.c_str()));
  }

  // Runs the storage.get() API function with the local storage, and populates
  // |out_value| with the string result.
  testing::AssertionResult RunGetFunction(const std::string& key,
                                          std::string* out_value) {
    absl::optional<base::Value> result = RunFunctionAndReturnValue(
        new StorageStorageAreaGetFunction(),
        base::StringPrintf("[\"local\", \"%s\"]", key.c_str()));
    if (!result)
      return testing::AssertionFailure() << "No result";

    const base::Value::Dict* dict = result->GetIfDict();
    if (!dict) {
      return testing::AssertionFailure() << *result << " was not a dictionary.";
    }

    const std::string* dict_value = dict->FindString(key);
    if (!dict_value) {
      return testing::AssertionFailure() << " could not retrieve a string from"
          << dict << " at " << key;
    }
    *out_value = *dict_value;

    return testing::AssertionSuccess();
  }

  ExtensionsAPIClient extensions_api_client_;
  std::unique_ptr<content::RenderProcessHost> render_process_host_;
};

TEST_F(StorageApiUnittest, RestoreCorruptedStorage) {
  const char kKey[] = "key";
  const char kValue[] = "value";
  std::string result;

  // Do a simple set/get combo to make sure the API works.
  RunSetFunction(kKey, kValue);
  EXPECT_TRUE(RunGetFunction(kKey, &result));
  EXPECT_EQ(kValue, result);

  // Corrupt the store. This is not as pretty as ideal, because we use knowledge
  // of the underlying structure, but there's no real good way to corrupt a
  // store other than directly modifying the files.
  value_store::ValueStore* store =
      settings_test_util::GetStorage(extension_ref(), settings_namespace::LOCAL,
                                     StorageFrontend::Get(browser_context()));
  ASSERT_TRUE(store);
  // TODO(cmumford): Modify test as this requires that the factory always
  //                 creates instances of LeveldbValueStore.
  SettingsStorageQuotaEnforcer* quota_store =
      static_cast<SettingsStorageQuotaEnforcer*>(store);
  value_store::LeveldbValueStore* leveldb_store =
      static_cast<value_store::LeveldbValueStore*>(
          quota_store->get_delegate_for_test());
  leveldb::WriteBatch batch;
  batch.Put(kKey, "[{(.*+\"\'\\");
  EXPECT_TRUE(leveldb_store->WriteToDbForTest(&batch));
  EXPECT_TRUE(leveldb_store->Get(kKey).status().IsCorrupted());

  // Running another set should end up working (even though it will restore the
  // store behind the scenes).
  RunSetFunction(kKey, kValue);
  EXPECT_TRUE(RunGetFunction(kKey, &result));
  EXPECT_EQ(kValue, result);
}

TEST_F(StorageApiUnittest, StorageAreaOnChanged) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  EventRouter* event_router = EventRouter::Get(browser_context());
  event_router->AddEventListener(api::storage::OnChanged::kEventName,
                                 render_process_host(), extension()->id());
  event_router->AddEventListener("storage.local.onChanged",
                                 render_process_host(), extension()->id());

  RunSetFunction("key", "value");
  EXPECT_EQ(2u, event_observer.events().size());

  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::storage::OnChanged::kEventName));
  EXPECT_TRUE(
      base::Contains(event_observer.events(), "storage.local.onChanged"));
}

// Test that no event is dispatched if no listener is added.
TEST_F(StorageApiUnittest, StorageAreaOnChangedNoListener) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  RunSetFunction("key", "value");
  EXPECT_EQ(0u, event_observer.events().size());
}

// Test that no event is dispatched if a listener for a different extension is
// added.
TEST_F(StorageApiUnittest, StorageAreaOnChangedOtherListener) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  EventRouter* event_router = EventRouter::Get(browser_context());
  std::string other_listener_id =
      crx_file::id_util::GenerateId("other-listener");
  event_router->AddEventListener(api::storage::OnChanged::kEventName,
                                 render_process_host(), other_listener_id);
  event_router->AddEventListener("storage.local.onChanged",
                                 render_process_host(), other_listener_id);

  RunSetFunction("key", "value");
  EXPECT_EQ(0u, event_observer.events().size());
}

TEST_F(StorageApiUnittest, StorageAreaOnChangedOnlyOneListener) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  EventRouter* event_router = EventRouter::Get(browser_context());
  event_router->AddEventListener(api::storage::OnChanged::kEventName,
                                 render_process_host(), extension()->id());

  RunSetFunction("key", "value");
  EXPECT_EQ(1u, event_observer.events().size());

  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::storage::OnChanged::kEventName));
}

}  // namespace extensions
