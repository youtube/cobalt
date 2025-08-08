// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/cookies_tree_model.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mock_settings_observer.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/mock_browsing_data_quota_helper.h"
#include "components/browsing_data/content/mock_cache_storage_helper.h"
#include "components/browsing_data/content/mock_cookie_helper.h"
#include "components/browsing_data/content/mock_database_helper.h"
#include "components/browsing_data/content/mock_file_system_helper.h"
#include "components/browsing_data/content/mock_indexed_db_helper.h"
#include "components/browsing_data/content/mock_local_storage_helper.h"
#include "components/browsing_data/content/mock_service_worker_helper.h"
#include "components/browsing_data/content/mock_shared_worker_helper.h"
#include "components/browsing_data/core/features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#endif

using content::BrowserThread;
using ::testing::_;

namespace {

class CookiesTreeModelTest : public testing::Test {
 public:
  ~CookiesTreeModelTest() override {
    // Avoid memory leaks.
#if BUILDFLAG(ENABLE_EXTENSIONS)
    special_storage_policy_ = nullptr;
#endif
    // TODO(arthursonzogni): Consider removing this line, or at least explain
    // why it is needed.
    base::RunLoop().RunUntilIdle();
    profile_.reset();
    // TODO(arthursonzogni): Consider removing this line, or at least explain
    // why it is needed.
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    if (base::FeatureList::IsEnabled(
            browsing_data::features::kDeprecateCookiesTreeModel)) {
      GTEST_SKIP() << "kDeprecateCookiesTreeModel is enabled skipping "
                      "CookiesTreeModel tests";
    }

    profile_ = std::make_unique<TestingProfile>();
    auto* storage_partition = profile_->GetDefaultStoragePartition();
    mock_browsing_data_cookie_helper_ =
        base::MakeRefCounted<browsing_data::MockCookieHelper>(
            storage_partition);
    mock_browsing_data_database_helper_ =
        base::MakeRefCounted<browsing_data::MockDatabaseHelper>(
            storage_partition);
    mock_browsing_data_local_storage_helper_ =
        base::MakeRefCounted<browsing_data::MockLocalStorageHelper>(
            storage_partition);
    mock_browsing_data_session_storage_helper_ =
        base::MakeRefCounted<browsing_data::MockLocalStorageHelper>(
            storage_partition);
    mock_browsing_data_indexed_db_helper_ =
        base::MakeRefCounted<browsing_data::MockIndexedDBHelper>(
            storage_partition);
    mock_browsing_data_file_system_helper_ =
        base::MakeRefCounted<browsing_data::MockFileSystemHelper>(
            storage_partition);
    mock_browsing_data_quota_helper_ =
        base::MakeRefCounted<MockBrowsingDataQuotaHelper>();
    mock_browsing_data_service_worker_helper_ =
        base::MakeRefCounted<browsing_data::MockServiceWorkerHelper>(
            storage_partition);
    mock_browsing_data_shared_worker_helper_ =
        base::MakeRefCounted<browsing_data::MockSharedWorkerHelper>(
            storage_partition);
    mock_browsing_data_cache_storage_helper_ =
        base::MakeRefCounted<browsing_data::MockCacheStorageHelper>(
            storage_partition);

    const char kExtensionScheme[] = "extensionscheme";
    auto cookie_settings =
        base::MakeRefCounted<content_settings::CookieSettings>(
            HostContentSettingsMapFactory::GetForProfile(profile_.get()),
            profile_->GetPrefs(),
            TrackingProtectionSettingsFactory::GetForProfile(profile_.get()),
            profile_->IsIncognitoProfile(), kExtensionScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    special_storage_policy_ =
        base::MakeRefCounted<ExtensionSpecialStoragePolicy>(
            cookie_settings.get());
#endif
  }

  void TearDown() override {
    mock_browsing_data_cache_storage_helper_ = nullptr;
    mock_browsing_data_shared_worker_helper_ = nullptr;
    mock_browsing_data_service_worker_helper_ = nullptr;
    mock_browsing_data_quota_helper_ = nullptr;
    mock_browsing_data_file_system_helper_ = nullptr;
    mock_browsing_data_indexed_db_helper_ = nullptr;
    mock_browsing_data_session_storage_helper_ = nullptr;
    mock_browsing_data_local_storage_helper_ = nullptr;
    mock_browsing_data_database_helper_ = nullptr;
    mock_browsing_data_cookie_helper_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<CookiesTreeModel> CreateCookiesTreeModelWithInitialSample() {
    auto container = std::make_unique<LocalDataContainer>(
        mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
        mock_browsing_data_local_storage_helper_,
        mock_browsing_data_session_storage_helper_,
        mock_browsing_data_indexed_db_helper_,
        mock_browsing_data_file_system_helper_,
        mock_browsing_data_quota_helper_,
        mock_browsing_data_service_worker_helper_,
        mock_browsing_data_shared_worker_helper_,
        mock_browsing_data_cache_storage_helper_);

    auto cookies_model = std::make_unique<CookiesTreeModel>(
        std::move(container), special_storage_policy());
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo1"), "A=1");
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo2"), "B=1");
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo3"), "C=1");
    mock_browsing_data_cookie_helper_->Notify();
    mock_browsing_data_database_helper_->AddDatabaseSamples();
    mock_browsing_data_database_helper_->Notify();
    mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_local_storage_helper_->Notify();
    mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_session_storage_helper_->Notify();
    mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
    mock_browsing_data_indexed_db_helper_->Notify();
    mock_browsing_data_file_system_helper_->AddFileSystemSamples();
    mock_browsing_data_file_system_helper_->Notify();
    mock_browsing_data_quota_helper_->AddQuotaSamples();
    mock_browsing_data_quota_helper_->Notify();
    mock_browsing_data_service_worker_helper_->AddServiceWorkerSamples();
    mock_browsing_data_service_worker_helper_->Notify();
    mock_browsing_data_shared_worker_helper_->AddSharedWorkerSamples();
    mock_browsing_data_shared_worker_helper_->Notify();
    mock_browsing_data_cache_storage_helper_->AddCacheStorageSamples();
    mock_browsing_data_cache_storage_helper_->Notify();

    {
      SCOPED_TRACE(
          "Initial State 3 cookies, 2 databases, 2 local storages, "
          "2 session storages, 2 indexed DBs, 3 filesystems, "
          "2 quotas, 2 service workers, 2 shared workers,"
          "2 cache storages");
      // 63 because there's the root, then
      // cshost1 -> cache storage -> https://cshost1:1/
      // cshost2 -> cache storage -> https://cshost2:2/
      // foo1 -> cookies -> a,
      // foo2 -> cookies -> b,
      // foo3 -> cookies -> c,
      // gdbhost1 -> database -> http://gdbhost1:1/,
      // gdbhost2 -> database -> http://gdbhost2:2/,
      // host1 -> localstorage -> http://host1:1/,
      //       -> sessionstorage -> http://host1:1/,
      // host2 -> localstorage -> http://host2:2/.
      //       -> sessionstorage -> http://host2:2/,
      // idbhost1 -> indexeddb -> http://idbhost1:1/,
      // idbhost2 -> indexeddb -> http://idbhost2:2/,
      // fshost1 -> filesystem -> http://fshost1:1/,
      // fshost2 -> filesystem -> http://fshost2:1/,
      // fshost3 -> filesystem -> http://fshost3:1/,
      // quotahost1 -> quotahost1,
      // quotahost2 -> quotahost2,
      // swhost1 -> service worker -> https://swhost1:1
      // swhost2 -> service worker -> https://swhost1:2
      // sharedworkerhost1 -> shared worker -> https://sharedworkerhost1:1,
      // sharedworkerhost2 -> shared worker -> https://sharedworkerhost2:2
      EXPECT_EQ(63u, cookies_model->GetRoot()->GetTotalNodeCount());
      EXPECT_EQ("A,B,C", GetDisplayedCookies(cookies_model.get()));
      EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
                GetDisplayedDatabases(cookies_model.get()));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedLocalStorages(cookies_model.get()));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedSessionStorages(cookies_model.get()));
      EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
                GetDisplayedIndexedDBs(cookies_model.get()));
      EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
                GetDisplayedFileSystems(cookies_model.get()));
      EXPECT_EQ("quotahost1,quotahost2",
                GetDisplayedQuotas(cookies_model.get()));
      EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
                GetDisplayedServiceWorkers(cookies_model.get()));
      EXPECT_EQ(
          "https://sharedworkerhost1:1/app/worker.js,"
          "https://sharedworkerhost2:2/worker.js",
          GetDisplayedSharedWorkers(cookies_model.get()));
      EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
                GetDisplayedCacheStorages(cookies_model.get()));
    }
    return cookies_model;
  }

  // Checks that, when setting content settings for host nodes in the
  // cookie tree, the content settings are applied to the expected URL.
  void CheckContentSettingsUrlForHostNodes(
      const CookieTreeNode* node,
      CookieTreeNode::DetailedInfo::NodeType node_type,
      content_settings::CookieSettings* cookie_settings,
      const GURL& expected_url) {
    for (const auto& child : node->children()) {
      CheckContentSettingsUrlForHostNodes(child.get(),
                                          child->GetDetailedInfo().node_type,
                                          cookie_settings, expected_url);
    }

    ASSERT_EQ(node_type, node->GetDetailedInfo().node_type);

    if (node_type == CookieTreeNode::DetailedInfo::TYPE_HOST) {
      const CookieTreeHostNode* host =
          static_cast<const CookieTreeHostNode*>(node);

      if (expected_url.SchemeIsFile()) {
        EXPECT_FALSE(host->CanCreateContentException());
      } else {
        cookie_settings->ResetCookieSetting(expected_url);
        EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(expected_url));

        host->CreateContentException(cookie_settings,
                                     CONTENT_SETTING_SESSION_ONLY);
        EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(expected_url));
      }
    }
  }

  std::string GetNodesOfChildren(
      const CookieTreeNode* node,
      CookieTreeNode::DetailedInfo::NodeType node_type) {
    if (!node->children().empty()) {
      std::string retval;
      for (const auto& child : node->children())
        retval += GetNodesOfChildren(child.get(), node_type);
      return retval;
    }

    if (node->GetDetailedInfo().node_type != node_type)
      return std::string();

    // TODO: GetURL().spec() is used instead of Serialize() for backwards
    // compatibility with tests. The tests should be updated once all
    // appropriate parts have been migrated to url::Origin.
    switch (node_type) {
      case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
        return node->GetDetailedInfo().cookie->Name() + ",";
      case CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE:
      case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
      case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
      case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
      case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER:
      case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
        return node->GetDetailedInfo()
                   .usage_info->storage_key.origin()
                   .GetURL()
                   .spec() +
               ",";
      case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM:
        return node->GetDetailedInfo()
                   .file_system_info->origin.GetURL()
                   .spec() +
               ",";
      case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
        return node->GetDetailedInfo().quota_info->storage_key.origin().host() +
               ",";
      case CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER:
        return node->GetDetailedInfo().shared_worker_info->worker.spec() + ",";
      default:
        return std::string();
    }
  }

  // Get the nodes names displayed in the view (if we had one) in the order
  // they are displayed, as a comma seperated string.
  // Ex: EXPECT_STREQ("X,Y", GetDisplayedNodes(cookies_view, type).c_str());
  std::string GetDisplayedNodes(CookiesTreeModel* cookies_model,
                                CookieTreeNode::DetailedInfo::NodeType type) {
    CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(
        cookies_model->GetRoot());
    std::string retval = GetNodesOfChildren(root, type);
    if (!retval.empty() && retval.back() == ',')
      retval.erase(retval.length() - 1);
    return retval;
  }

  std::string GetDisplayedCookies(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_COOKIE);
  }

  std::string GetDisplayedDatabases(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_DATABASE);
  }

  std::string GetDisplayedLocalStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE);
  }

  std::string GetDisplayedSessionStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(
        cookies_model, CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE);
  }

  std::string GetDisplayedIndexedDBs(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB);
  }

  std::string GetDisplayedFileSystems(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM);
  }

  std::string GetDisplayedQuotas(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_QUOTA);
  }

  std::string GetDisplayedServiceWorkers(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER);
  }

  std::string GetDisplayedSharedWorkers(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER);
  }

  std::string GetDisplayedCacheStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE);
  }

  // Do not call on the root.
  void DeleteStoredObjects(CookieTreeNode* node) {
    node->DeleteStoredObjects();
    CookieTreeNode* parent_node = node->parent();
    DCHECK(parent_node);
    parent_node->GetModel()->Remove(parent_node, node);
  }

 protected:
  ExtensionSpecialStoragePolicy* special_storage_policy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return special_storage_policy_.get();
#else
    return nullptr;
#endif
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<browsing_data::MockCookieHelper>
      mock_browsing_data_cookie_helper_;
  scoped_refptr<browsing_data::MockDatabaseHelper>
      mock_browsing_data_database_helper_;
  scoped_refptr<browsing_data::MockLocalStorageHelper>
      mock_browsing_data_local_storage_helper_;
  scoped_refptr<browsing_data::MockLocalStorageHelper>
      mock_browsing_data_session_storage_helper_;
  scoped_refptr<browsing_data::MockIndexedDBHelper>
      mock_browsing_data_indexed_db_helper_;
  scoped_refptr<browsing_data::MockFileSystemHelper>
      mock_browsing_data_file_system_helper_;
  scoped_refptr<MockBrowsingDataQuotaHelper>
      mock_browsing_data_quota_helper_;
  scoped_refptr<browsing_data::MockServiceWorkerHelper>
      mock_browsing_data_service_worker_helper_;
  scoped_refptr<browsing_data::MockSharedWorkerHelper>
      mock_browsing_data_shared_worker_helper_;
  scoped_refptr<browsing_data::MockCacheStorageHelper>
      mock_browsing_data_cache_storage_helper_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy> special_storage_policy_;
#endif
};

TEST_F(CookiesTreeModelTest, RemoveAll) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // Reset the selection of the first row.
  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ("A,B,C",
              GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
  }

  mock_browsing_data_cookie_helper_->Reset();
  mock_browsing_data_database_helper_->Reset();
  mock_browsing_data_local_storage_helper_->Reset();
  mock_browsing_data_session_storage_helper_->Reset();
  mock_browsing_data_indexed_db_helper_->Reset();
  mock_browsing_data_service_worker_helper_->Reset();
  mock_browsing_data_shared_worker_helper_->Reset();
  mock_browsing_data_cache_storage_helper_->Reset();
  mock_browsing_data_file_system_helper_->Reset();

  cookies_model->DeleteAllStoredObjects();

  // Make sure the nodes are also deleted from the model's cache.
  // http://crbug.com/43249
  cookies_model->UpdateSearchResults(std::u16string());

  {
    // 2 nodes - root and app
    SCOPED_TRACE("After removing");
    EXPECT_EQ(1u, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ(0u, cookies_model->GetRoot()->children().size());
    EXPECT_EQ(std::string(), GetDisplayedCookies(cookies_model.get()));
    EXPECT_TRUE(mock_browsing_data_cookie_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_database_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_local_storage_helper_->AllDeleted());
    EXPECT_FALSE(mock_browsing_data_session_storage_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_indexed_db_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_file_system_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_service_worker_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_shared_worker_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_cache_storage_helper_->AllDeleted());
  }
}

TEST_F(CookiesTreeModelTest, Remove) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // Children start out arranged as follows:
  //
  // 0. `cshost1`
  // 1. `cshost2`
  // 2. `foo1`
  // 3. `foo2`
  // 4. `foo3`
  // 5. `fshost1`
  // 6. `fshost2`
  // 7. `fshost3`
  // 8. `gdbhost1`
  // 9. `gdbhost2`
  // 10. `host1`
  // 11. `host2`
  // 12. `idbhost1`
  // 13. `idbhost2`
  // 14. `quotahost1`
  // 15. `quotahost2`
  // 16. `sharedworkerhost1`
  // 17. `sharedworkerhost2`
  // 18. `swhost1`
  // 19. `swhost2`
  //
  // Here, we'll remove them one by one, starting from the end, and
  // check that the state makes sense. Initially there are 63 total nodes.

  // swhost2 -> service worker -> https://swhost1:2 (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[19].get());
  {
    SCOPED_TRACE("`swhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(60u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // swhost1 -> service worker -> https://swhost1:1 (3 nodes)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[18].get());
  {
    SCOPED_TRACE("`swhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(57u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // sharedworkerhost2 -> shared worker -> https://sharedworkerhost2:2 (3
  // objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[17].get());
  {
    SCOPED_TRACE("`sharedworkerhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("https://sharedworkerhost1:1/app/worker.js",
              GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(54u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // sharedworkerhost1 -> shared worker -> https://sharedworkerhost1:1 (3 nodes)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[16].get());
  {
    SCOPED_TRACE("`sharedworkerhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(51u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // quotahost2 -> quotahost2 (2 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[15].get());
  {
    SCOPED_TRACE("`quotahost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(49u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // quotahost1 -> quotahost1 (2 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[14].get());
  {
    SCOPED_TRACE("`quotahost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(47u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // idbhost2 -> indexeddb -> http://idbhost2:2/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[13].get());
  {
    SCOPED_TRACE("`idbhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(44u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // idbhost1 -> indexeddb -> http://idbhost1:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[12].get());
  {
    SCOPED_TRACE("`idbhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(41u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // host2 -> localstorage -> http://host2:2/,
  //       -> sessionstorage -> http://host2:2/ (5 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[11].get());
  {
    SCOPED_TRACE("`host2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(36u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // host1 -> localstorage -> http://host1:1/,
  //       -> sessionstorage -> http://host1:1/ (5 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[10].get());
  {
    SCOPED_TRACE("`host1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(31u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // gdbhost2 -> database -> http://gdbhost2:2/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[9].get());
  {
    SCOPED_TRACE("`gdbhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(28u, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  // gdbhost1 -> database -> http://gdbhost1:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[8].get());
  {
    SCOPED_TRACE("`gdbhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(25u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // fshost3 -> filesystem -> http://fshost3:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[7].get());
  {
    SCOPED_TRACE("`fshost3` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(22u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // fshost2 -> filesystem -> http://fshost2:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[6].get());
  {
    SCOPED_TRACE("`fshost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(19u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // fshost1 -> filesystem -> http://fshost1:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[5].get());
  {
    SCOPED_TRACE("`fshost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(16u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // foo3 -> cookies -> c (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[4].get());
  {
    SCOPED_TRACE("`foo3` removed.");
    EXPECT_STREQ("A,B", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(13u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // foo2 -> cookies -> b (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[3].get());
  {
    SCOPED_TRACE("`foo2` removed.");
    EXPECT_STREQ("A", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(10u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // foo1 -> cookies -> a (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[2].get());
  {
    SCOPED_TRACE("`foo1` removed.");
    EXPECT_STREQ("", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(7u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // cshost2 -> cache storage -> https://cshost2:2/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[1].get());
  {
    SCOPED_TRACE("`cshost2` removed.");
    EXPECT_STREQ("", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(4u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // cshost1 -> cache storage -> https://cshost1:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[0].get());
  {
    SCOPED_TRACE("`cshost1` removed.");
    EXPECT_STREQ("", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(1u, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookiesNode) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[2]->children()[0].get());
  {
    SCOPED_TRACE("First cookies origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    // 61 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted.
    EXPECT_EQ(61u, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[8]->children()[0].get());
  {
    SCOPED_TRACE("First database origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost2:2/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(59u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[10]->children()[0].get());
  {
    SCOPED_TRACE("First local storage origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost2:2/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(57u, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookieNode) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[3]->children()[0].get());
  {
    SCOPED_TRACE("Second origin COOKIES node removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    // 61 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted.
    EXPECT_EQ(61u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[8]->children()[0].get());
  {
    SCOPED_TRACE("First database origin removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost2:2/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(59u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[10]->children()[0].get());
  {
    SCOPED_TRACE("First local storage origin removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost2:2/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ(57u, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNode) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->Notify();
  mock_browsing_data_database_helper_->AddDatabaseSamples();
  mock_browsing_data_database_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
  mock_browsing_data_indexed_db_helper_->Notify();
  mock_browsing_data_file_system_helper_->AddFileSystemSamples();
  mock_browsing_data_file_system_helper_->Notify();
  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();
  mock_browsing_data_service_worker_helper_->AddServiceWorkerSamples();
  mock_browsing_data_service_worker_helper_->Notify();
  mock_browsing_data_shared_worker_helper_->AddSharedWorkerSamples();
  mock_browsing_data_shared_worker_helper_->Notify();
  mock_browsing_data_cache_storage_helper_->AddCacheStorageSamples();
  mock_browsing_data_cache_storage_helper_->Notify();

  {
    SCOPED_TRACE(
        "Initial State 4 cookies, 2 databases, 2 local storages, "
        "2 session storages, 2 indexed DBs, 3 file systems, "
        "2 quotas, 2 service workers, 2 shared workers, 2 caches.");
    // 58 because there's the root, then
    // cshost1 -> cache storage -> https://cshost1:1/
    // cshost2 -> cache storage -> https://cshost2:2/
    // foo1 -> cookies -> a,
    // foo2 -> cookies -> b,
    // foo3 -> cookies -> c,d
    // dbhost1 -> database -> http://gdbhost1:1/,
    // dbhost2 -> database -> http://gdbhost2:2/,
    // host1 -> localstorage -> http://host1:1/,
    //       -> sessionstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/,
    //       -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/,
    // fshost1 -> filesystem -> http://fshost1:1/,
    // fshost2 -> filesystem -> http://fshost2:1/,
    // fshost3 -> filesystem -> http://fshost3:1/,
    // quotahost1 -> quotahost1,
    // quotahost2 -> quotahost2.
    // swhost1 -> service worker -> https://swhost1:1
    // swhost2 -> service worker -> https://swhost1:2
    // sharedworkerhost1 -> shared worker -> https://sharedworkerhost1:1
    // sharedworkerhost2 -> shared worker -> https://sharedworkerhost2:2
    EXPECT_EQ(64u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(&cookies_model));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(&cookies_model));
  }
  DeleteStoredObjects(cookies_model.GetRoot()->children()[4].get());
  {
    SCOPED_TRACE("Third cookie origin removed");
    EXPECT_STREQ("A,B", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(&cookies_model));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(&cookies_model));
    EXPECT_EQ(60u, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNodeOf3) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "E=1");
  mock_browsing_data_cookie_helper_->Notify();
  mock_browsing_data_database_helper_->AddDatabaseSamples();
  mock_browsing_data_database_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
  mock_browsing_data_indexed_db_helper_->Notify();
  mock_browsing_data_file_system_helper_->AddFileSystemSamples();
  mock_browsing_data_file_system_helper_->Notify();
  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();
  mock_browsing_data_service_worker_helper_->AddServiceWorkerSamples();
  mock_browsing_data_service_worker_helper_->Notify();
  mock_browsing_data_shared_worker_helper_->AddSharedWorkerSamples();
  mock_browsing_data_shared_worker_helper_->Notify();
  mock_browsing_data_cache_storage_helper_->AddCacheStorageSamples();
  mock_browsing_data_cache_storage_helper_->Notify();

  {
    SCOPED_TRACE(
        "Initial State 5 cookies, 2 databases, 2 local storages, "
        "2 session storages, 2 indexed DBs, 3 filesystems, "
        "2 quotas, 2 service workers, 2 shared workers, 2 caches.");
    // 59 because there's the root, then
    // cshost1 -> cache storage -> https://cshost1:1/
    // cshost2 -> cache storage -> https://cshost2:2/
    // foo1 -> cookies -> a,
    // foo2 -> cookies -> b,
    // foo3 -> cookies -> c,d,e
    // dbhost1 -> database -> http://gdbhost1:1/,
    // dbhost2 -> database -> http://gdbhost2:2/,
    // host1 -> localstorage -> http://host1:1/,
    //       -> sessionstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/,
    //       -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/,
    // fshost1 -> filesystem -> http://fshost1:1/,
    // fshost2 -> filesystem -> http://fshost2:1/,
    // fshost3 -> filesystem -> http://fshost3:1/,
    // quotahost1 -> quotahost1,
    // quotahost2 -> quotahost2.
    // swhost1 -> service worker -> https://swhost1:1
    // swhost2 -> service worker -> https://swhost1:2
    // sharedworkerhost1 -> shared worker -> https://sharedworkerhost1:1
    // sharedworkerhost2 -> shared worker -> https://sharedworkerhost2:2
    EXPECT_EQ(65u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(&cookies_model));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(&cookies_model));
  }
  DeleteStoredObjects(cookies_model.GetRoot()
                          ->children()[4]
                          ->children()[0]
                          ->children()[1]
                          .get());
  {
    SCOPED_TRACE("Middle cookie in third cookie origin removed");
    EXPECT_STREQ("A,B,C,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ(64u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(&cookies_model));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(&cookies_model));
  }
}

TEST_F(CookiesTreeModelTest, RemoveSecondOrigin) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "E=1");
  mock_browsing_data_cookie_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 5 cookies");
    // 12 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d,e
    EXPECT_EQ(12u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteStoredObjects(cookies_model.GetRoot()->children()[1].get());
  {
    SCOPED_TRACE("Second origin removed");
    EXPECT_STREQ("A,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    // Left with root -> foo1 -> cookies -> a, foo3 -> cookies -> c,d,e
    EXPECT_EQ(9u, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, OriginOrdering) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://a.foo2.com"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2.com"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://b.foo1.com"), "C=1");
  // Leading dot on the foo4
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://foo4.com"), "D=1; domain=.foo4.com; path=/;");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://a.foo1.com"), "E=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1.com"), "F=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3.com"), "G=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo4.com"), "H=1");
  mock_browsing_data_cookie_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 8 cookies");
    EXPECT_EQ(23u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("F,E,C,B,A,G,D,H",
        GetDisplayedCookies(&cookies_model).c_str());
  }
  // Delete "E"
  DeleteStoredObjects(cookies_model.GetRoot()->children()[1].get());
  {
    EXPECT_STREQ("F,C,B,A,G,D,H", GetDisplayedCookies(&cookies_model).c_str());
  }
}

TEST_F(CookiesTreeModelTest, ContentSettings) {
  GURL host("http://xyz.com/");
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->AddCookieSamples(host, "A=1");
  mock_browsing_data_cookie_helper_->Notify();

  TestingProfile profile;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();
  MockSettingsObserver observer(content_settings);

  CookieTreeRootNode* root =
      static_cast<CookieTreeRootNode*>(cookies_model.GetRoot());
  CookieTreeHostNode* origin =
      root->GetOrCreateHostNode(host);

  EXPECT_EQ(1u, origin->children().size());
  EXPECT_TRUE(origin->CanCreateContentException());
  EXPECT_CALL(observer, OnContentSettingsChanged(
                            content_settings, ContentSettingsType::COOKIES,
                            false, ContentSettingsPattern::FromURL(host),
                            ContentSettingsPattern::Wildcard(), false))
      .Times(2);
  origin->CreateContentException(
      cookie_settings, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(cookie_settings->IsFullCookieAccessAllowed(
      host, net::SiteForCookies::FromUrl(host), url::Origin::Create(host),
      net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(host));
}

TEST_F(CookiesTreeModelTest, FileSystemFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(u"fshost1");
  EXPECT_EQ("http://fshost1:1/",
            GetDisplayedFileSystems(cookies_model.get()));

  cookies_model->UpdateSearchResults(u"fshost2");
  EXPECT_EQ("http://fshost2:2/",
            GetDisplayedFileSystems(cookies_model.get()));

  cookies_model->UpdateSearchResults(u"fshost3");
  EXPECT_EQ("http://fshost3:3/",
            GetDisplayedFileSystems(cookies_model.get()));

  cookies_model->UpdateSearchResults(std::u16string());
  EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
            GetDisplayedFileSystems(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, ServiceWorkerFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(u"swhost1");
  EXPECT_EQ("https://swhost1:1/",
            GetDisplayedServiceWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(u"swhost2");
  EXPECT_EQ("https://swhost2:2/",
            GetDisplayedServiceWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(u"swhost3");
  EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(std::u16string());
  EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
            GetDisplayedServiceWorkers(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, SharedWorkerFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(u"sharedworkerhost1");
  EXPECT_EQ("https://sharedworkerhost1:1/app/worker.js",
            GetDisplayedSharedWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(u"sharedworkerhost2");
  EXPECT_EQ("https://sharedworkerhost2:2/worker.js",
            GetDisplayedSharedWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(u"sharedworkerhost3");
  EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(std::u16string());
  EXPECT_EQ(
      "https://sharedworkerhost1:1/app/worker.js,"
      "https://sharedworkerhost2:2/worker.js",
      GetDisplayedSharedWorkers(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, CacheStorageFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(u"cshost1");
  EXPECT_EQ("https://cshost1:1/",
            GetDisplayedCacheStorages(cookies_model.get()));

  cookies_model->UpdateSearchResults(u"cshost2");
  EXPECT_EQ("https://cshost2:2/",
            GetDisplayedCacheStorages(cookies_model.get()));

  cookies_model->UpdateSearchResults(u"cshost3");
  EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));

  cookies_model->UpdateSearchResults(std::u16string());
  EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
            GetDisplayedCacheStorages(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, CookiesFilter) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://123.com"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1.com"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2.com"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3.com"), "D=1");
  mock_browsing_data_cookie_helper_->Notify();
  EXPECT_EQ("A,B,C,D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(std::u16string(u"foo"));
  EXPECT_EQ("B,C,D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(std::u16string(u"2"));
  EXPECT_EQ("A,C", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(std::u16string(u"foo3"));
  EXPECT_EQ("D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(std::u16string());
  EXPECT_EQ("A,B,C,D", GetDisplayedCookies(&cookies_model));
}

// Tests that cookie source URLs are stored correctly in the cookies
// tree model.
TEST_F(CookiesTreeModelTest, CanonicalizeCookieSource) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("file:///tmp/test.html"), "A=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com"), "B=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/"), "C=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/test"), "D=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com:1234/"), "E=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("https://example.com/"), "F=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://user:pwd@example.com/"), "G=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/test?foo"), "H=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/test#foo"), "I=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("https://example2.com/test#foo"), "J=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example3.com:1234/test#foo"), "K=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://user:pwd@example4.com/test?foo"), "L=1");
  mock_browsing_data_cookie_helper_->Notify();

  // Check that all the above example.com cookies go on the example.com
  // host node.
  cookies_model.UpdateSearchResults(std::u16string(u"example.com"));
  EXPECT_EQ("B,C,D,E,F,G,H,I", GetDisplayedCookies(&cookies_model));

  TestingProfile profile;
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();

  // Check that content settings for different URLs get applied to the
  // correct URL. That is, setting a cookie on https://example2.com
  // should create a host node for https://example2.com and thus content
  // settings set on that host node should apply to https://example2.com.

  cookies_model.UpdateSearchResults(std::u16string(u"file://"));
  EXPECT_EQ("", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("file:///test/tmp.html"));

  cookies_model.UpdateSearchResults(std::u16string(u"example2.com"));
  EXPECT_EQ("J", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("https://example2.com"));

  cookies_model.UpdateSearchResults(std::u16string(u"example3.com"));
  EXPECT_EQ("K", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("http://example3.com"));

  cookies_model.UpdateSearchResults(std::u16string(u"example4.com"));
  EXPECT_EQ("L", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("http://example4.com"));
}

TEST_F(CookiesTreeModelTest, CookieDeletionFilterNormalUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      supervised_user::kClearingCookiesKeepsSupervisedUsersSignedIn);
  auto callback =
      CookiesTreeModel::GetCookieDeletionDisabledCallback(profile_.get());
  EXPECT_FALSE(callback);
}

TEST_F(CookiesTreeModelTest, CookieDeletionFilterIncognitoProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      supervised_user::kClearingCookiesKeepsSupervisedUsersSignedIn);
  auto* incognito_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(), true);
  ASSERT_TRUE(incognito_profile->IsOffTheRecord());
  auto callback =
      CookiesTreeModel::GetCookieDeletionDisabledCallback(incognito_profile);
  EXPECT_TRUE(callback);
  EXPECT_FALSE(callback.Run(GURL("https://google.com")));
  EXPECT_FALSE(callback.Run(GURL("https://example.com")));
  EXPECT_FALSE(callback.Run(GURL("http://youtube.com")));
  EXPECT_FALSE(callback.Run(GURL("https://youtube.com")));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(CookiesTreeModelTest, CookieDeletionFilterChildUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      supervised_user::kClearingCookiesKeepsSupervisedUsersSignedIn);
  profile_->SetIsSupervisedProfile();
  auto callback =
      CookiesTreeModel::GetCookieDeletionDisabledCallback(profile_.get());

  EXPECT_TRUE(callback);
  EXPECT_FALSE(callback.Run(GURL("https://google.com")));
  EXPECT_FALSE(callback.Run(GURL("https://example.com")));
  EXPECT_TRUE(callback.Run(GURL("http://youtube.com")));
  EXPECT_TRUE(callback.Run(GURL("https://youtube.com")));
}
#endif

TEST_F(
    CookiesTreeModelTest,
    CookieDeletionFilterNormalUserWithClearingCookiesEnabledForSupervisedUsers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      supervised_user::kClearingCookiesKeepsSupervisedUsersSignedIn);
  auto callback =
      CookiesTreeModel::GetCookieDeletionDisabledCallback(profile_.get());
  EXPECT_TRUE(callback);
  EXPECT_FALSE(callback.Run(GURL("https://google.com")));
  EXPECT_FALSE(callback.Run(GURL("https://google.com")));
  EXPECT_FALSE(callback.Run(GURL("https://example.com")));
  EXPECT_FALSE(callback.Run(GURL("http://youtube.com")));
  EXPECT_FALSE(callback.Run(GURL("https://youtube.com")));
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(
    CookiesTreeModelTest,
    CookieDeletionFilterChildUserWithClearingCookiesEnabledForSupervisedUsers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      supervised_user::kClearingCookiesKeepsSupervisedUsersSignedIn);
  profile_->SetIsSupervisedProfile();
  auto callback =
      CookiesTreeModel::GetCookieDeletionDisabledCallback(profile_.get());

  EXPECT_TRUE(callback);
  EXPECT_FALSE(callback.Run(GURL("https://google.com")));
  EXPECT_FALSE(callback.Run(GURL("https://example.com")));
  EXPECT_TRUE(callback.Run(GURL("http://youtube.com")));
  EXPECT_TRUE(callback.Run(GURL("https://youtube.com")));
}
#endif

TEST_F(CookiesTreeModelTest, InclusiveSize) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // The root node doesn't have a concept of inclusive size, and so we must look
  // at the host nodes.
  auto& host_nodes = cookies_model->GetRoot()->children();
  uint64_t total =
      std::accumulate(host_nodes.cbegin(), host_nodes.cend(), int64_t{0},
                      [](int64_t total, const auto& child) {
                        return total + child->InclusiveSize();
                      });
  EXPECT_EQ(51u, total);
}

}  // namespace
