// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_page_handler.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/side_panel/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/test_history_clusters_service.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#include <vector>

namespace {

class MockHistoryClustersTabHelper
    : public side_panel::HistoryClustersTabHelper {
 public:
  static MockHistoryClustersTabHelper* CreateForWebContents(
      content::WebContents* contents) {
    DCHECK(contents);
    DCHECK(!contents->GetUserData(UserDataKey()));
    contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new MockHistoryClustersTabHelper(contents)));
    return static_cast<MockHistoryClustersTabHelper*>(
        contents->GetUserData(UserDataKey()));
  }

  MOCK_METHOD(void, ShowJourneysSidePanel, (const std::string&), (override));

 private:
  explicit MockHistoryClustersTabHelper(content::WebContents* web_contents)
      : HistoryClustersTabHelper(web_contents) {}
};

class MockHistoryClustersModuleService : public HistoryClustersModuleService {
 public:
  MockHistoryClustersModuleService()
      : HistoryClustersModuleService(nullptr, nullptr, nullptr, nullptr) {}

  MOCK_METHOD1(
      GetClusters,
      std::unique_ptr<history_clusters::HistoryClustersServiceTask>(
          base::OnceCallback<void(
              std::vector<history::Cluster>,
              base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>
              callback));
};

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() : HistoryService() {}

  MOCK_METHOD1(ClearCachedDataForContextID,
               void(history::ContextID context_id));
  MOCK_METHOD3(HideVisits,
               base::CancelableTaskTracker::TaskId(
                   const std::vector<history::VisitID>& visit_ids,
                   base::OnceClosure callback,
                   base::CancelableTaskTracker* tracker));
};

class MockCartService : public CartService {
 public:
  explicit MockCartService(Profile* profile) : CartService(profile) {}

  MOCK_METHOD1(LoadAllActiveCarts, void(CartDB::LoadCallback callback));
};

constexpr char kSampleNonSearchUrl[] = "https://www.foo.com/";
constexpr char kSampleSearchUrl[] = "https://www.google.com/search?q=foo";

class HistoryClustersPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  HistoryClustersPageHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    mock_history_clusters_module_service_ =
        static_cast<MockHistoryClustersModuleService*>(
            HistoryClustersModuleServiceFactory::GetForProfile(profile()));
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    mock_history_clusters_tab_helper_ =
        MockHistoryClustersTabHelper::CreateForWebContents(web_contents_.get());
    mock_history_service_ =
        static_cast<MockHistoryService*>(HistoryServiceFactory::GetForProfile(
            profile(), ServiceAccessType::EXPLICIT_ACCESS));
    mock_cart_service_ = static_cast<MockCartService*>(
        CartServiceFactory::GetForProfile(profile()));
    handler_ = std::make_unique<HistoryClustersPageHandler>(
        mojo::PendingReceiver<ntp::history_clusters::mojom::PageHandler>(),
        web_contents_.get());
    ukm_source_id_ = web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId();
  }

  void TearDown() override {
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  MockHistoryClustersModuleService& mock_history_clusters_module_service() {
    return *mock_history_clusters_module_service_;
  }

  MockHistoryClustersTabHelper& mock_history_clusters_tab_helper() {
    return *mock_history_clusters_tab_helper_;
  }

  MockCartService& mock_cart_service() { return *mock_cart_service_; }

  MockHistoryService& mock_history_service() { return *mock_history_service_; }

  HistoryClustersPageHandler& handler() { return *handler_; }

  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }

  void ResetHandler() { handler_.reset(); }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{HistoryClustersModuleServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockHistoryClustersModuleService>();
             })},
            {HistoryServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockHistoryService>();
             })},
            {TemplateURLServiceFactory::GetInstance(),
             base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)},
            {CartServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockCartService>(
                   Profile::FromBrowserContext(context));
             })}};
  }

  raw_ptr<MockHistoryClustersModuleService>
      mock_history_clusters_module_service_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<MockHistoryClustersTabHelper> mock_history_clusters_tab_helper_;
  raw_ptr<MockHistoryService> mock_history_service_;
  raw_ptr<MockCartService> mock_cart_service_;
  std::unique_ptr<HistoryClustersPageHandler> handler_;
  ukm::SourceId ukm_source_id_;
};

history::ClusterVisit SampleVisitForURL(
    GURL url,
    bool has_url_keyed_image = true,
    const std::vector<std::string>& related_searches = {}) {
  history::VisitRow visit_row;
  visit_row.visit_id = 1;
  visit_row.visit_time = base::Time::Now();
  visit_row.is_known_to_sync = true;
  auto content_annotations = history::VisitContentAnnotations();
  content_annotations.has_url_keyed_image = has_url_keyed_image;
  content_annotations.related_searches = related_searches;
  history::AnnotatedVisit annotated_visit;
  annotated_visit.visit_row = std::move(visit_row);
  annotated_visit.content_annotations = std::move(content_annotations);
  std::string kSampleUrl = url.spec();
  history::ClusterVisit sample_visit;
  sample_visit.url_for_display = base::UTF8ToUTF16(kSampleUrl);
  sample_visit.normalized_url = url;
  sample_visit.annotated_visit = std::move(annotated_visit);
  sample_visit.score = 1.0f;
  return sample_visit;
}

history::Cluster SampleCluster(int id,
                               int srp_visits,
                               int non_srp_visits,
                               const std::vector<std::string> related_searches =
                                   {"fruits", "red fruits", "healthy fruits"}) {
  history::ClusterVisit sample_srp_visit =
      SampleVisitForURL(GURL(kSampleSearchUrl), false);
  history::ClusterVisit sample_non_srp_visit =
      SampleVisitForURL(GURL(kSampleNonSearchUrl), true, related_searches);

  std::vector<history::ClusterVisit> visits;
  visits.insert(visits.end(), srp_visits, sample_srp_visit);
  visits.insert(visits.end(), non_srp_visits, sample_non_srp_visit);

  std::string kSampleLabel = "LabelOne";
  return history::Cluster(id, std::move(visits),
                          {{u"apples", history::ClusterKeywordData()},
                           {u"Red Oranges", history::ClusterKeywordData()}},
                          /*should_show_on_prominent_ui_surfaces=*/true,
                          /*label=*/
                          l10n_util::GetStringFUTF16(
                              IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS,
                              base::UTF8ToUTF16(kSampleLabel)));
}

history::Cluster SampleCluster(int srp_visits,
                               int non_srp_visits,
                               const std::vector<std::string> related_searches =
                                   {"fruits", "red fruits", "healthy fruits"}) {
  return SampleCluster(1, srp_visits, non_srp_visits, related_searches);
}

TEST_F(HistoryClustersPageHandlerTest, GetClusters) {
  const int kSampleClusterCount = 3;
  std::vector<history::Cluster> sample_clusters;
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals;
  for (int i = 0; i < kSampleClusterCount; i++) {
    sample_clusters.push_back(
        SampleCluster(i, /*srp_visits=*/1, /*non_srp_visits=*/2));
    ranking_signals[i] = HistoryClustersModuleRankingSignals();
  }
  EXPECT_CALL(mock_history_clusters_module_service(), GetClusters(testing::_))
      .WillOnce(testing::Invoke(
          [&sample_clusters, &ranking_signals](
              base::OnceCallback<void(
                  std::vector<history::Cluster>,
                  base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>
                  callback)
              -> std::unique_ptr<history_clusters::HistoryClustersServiceTask> {
            std::move(callback).Run(sample_clusters, ranking_signals);
            return nullptr;
          }));

  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClustersCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&clusters_mojom](
              std::vector<history_clusters::mojom::ClusterPtr> clusters_arg) {
            clusters_mojom = std::move(clusters_arg);
          }));
  handler().GetClusters(callback.Get());

  ASSERT_EQ(3u, clusters_mojom.size());

  for (unsigned int i = 0; i < kSampleClusterCount; i++) {
    const auto& cluster_mojom = clusters_mojom[i];
    ASSERT_TRUE(cluster_mojom);
    ASSERT_EQ(i, cluster_mojom->id);
    ASSERT_EQ(3u, cluster_mojom->visits.size());
    for (size_t u = 1; u < cluster_mojom->visits.size(); u++) {
      ASSERT_EQ(kSampleNonSearchUrl, cluster_mojom->visits[u]->url_for_display);
    }

    // Just report them all shown so UKM can get recorded.
    handler().RecordLayoutTypeShown(
        ntp::history_clusters::mojom::LayoutType::kLayout1, cluster_mojom->id);
  }

  // Reset handler to make sure UKM is recorded.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ResetHandler();
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::NewTabPage_HistoryClusters::kEntryName);
  ASSERT_EQ(entries.size(), 3u);
}

TEST_F(HistoryClustersPageHandlerTest, GetFakeCluster) {
  const unsigned long kNumVisits = 2;
  const unsigned long kNumVisitsWithImages = 2;
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpHistoryClustersModule,
           {{ntp_features::kNtpHistoryClustersModuleDataParam,
             base::StringPrintf("%lu,%lu", kNumVisits, kNumVisitsWithImages)}}},
      },
      {});

  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClustersCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](
              std::vector<history_clusters::mojom::ClusterPtr> clusters_arg) {
            cluster_mojom = std::move(clusters_arg.front());
          }));
  handler().GetClusters(callback.Get());
  ASSERT_TRUE(cluster_mojom);
  ASSERT_EQ(0u, cluster_mojom->id);
  // The cluster visits should include an additional entry for the SRP visit.
  ASSERT_EQ(kNumVisits + 1, cluster_mojom->visits.size());
}

TEST_F(HistoryClustersPageHandlerTest,
       NoClusterReturnedForInvalidModuleDataParam) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpHistoryClustersModule,
           {{ntp_features::kNtpHistoryClustersModuleDataParam, "0"}}},
      },
      {});

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/1, /*non_srp_visits=*/2);
  EXPECT_CALL(mock_history_clusters_module_service(), GetClusters(testing::_))
      .Times(0);

  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClustersCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&clusters_mojom](
              std::vector<history_clusters::mojom::ClusterPtr> clusters_arg) {
            clusters_mojom = std::move(clusters_arg);
          }));
  handler().GetClusters(callback.Get());
  ASSERT_EQ(0u, clusters_mojom.size());
}

TEST_F(HistoryClustersPageHandlerTest, NoClusters) {
  EXPECT_CALL(mock_history_clusters_module_service(), GetClusters(testing::_))
      .WillOnce(testing::Invoke(
          [&](base::OnceCallback<void(
                  std::vector<history::Cluster>,
                  base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>
                  callback)
              -> std::unique_ptr<history_clusters::HistoryClustersServiceTask> {
            std::move(callback).Run({}, {});
            return nullptr;
          }));

  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClustersCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&clusters_mojom](
              std::vector<history_clusters::mojom::ClusterPtr> clusters_arg) {
            clusters_mojom = std::move(clusters_arg);
          }));
  handler().GetClusters(callback.Get());
  ASSERT_EQ(0u, clusters_mojom.size());
}

TEST_F(HistoryClustersPageHandlerTest, ShowJourneysSidePanel) {
  std::string kSampleQuery = "safest cars";
  std::string query;
  EXPECT_CALL(mock_history_clusters_tab_helper(), ShowJourneysSidePanel)
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&query)));

  handler().ShowJourneysSidePanel(kSampleQuery);

  EXPECT_EQ(kSampleQuery, query);
}

TEST_F(HistoryClustersPageHandlerTest, OpenUrlsInTabGroup) {
  const std::vector<GURL> urls = {GURL("http://www.google.com/search?q=foo"),
                                  GURL("http://foo/1"), GURL("http://foo/2")};
  handler().OpenUrlsInTabGroup(urls);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(urls.size(), static_cast<size_t>(tab_strip_model->GetTabCount()));
  for (size_t i = 0; i < urls.size(); i++) {
    ASSERT_EQ(urls[i], tab_strip_model->GetWebContentsAt(i)->GetURL());
  }

  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  ASSERT_EQ(1u, tab_group_model->ListTabGroups().size());
  ASSERT_EQ(1, tab_strip_model->GetIndexOfWebContents(
                   tab_strip_model->GetActiveWebContents()));
}

TEST_F(HistoryClustersPageHandlerTest, DismissCluster) {
  std::vector<history::VisitID> visit_ids;
  EXPECT_CALL(mock_history_service(), HideVisits)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&visit_ids](const std::vector<history::VisitID>& visit_ids_arg,
                       base::OnceClosure callback_arg,
                       base::CancelableTaskTracker* tracker_arg)
              -> base::CancelableTaskTracker::TaskId {
            visit_ids = visit_ids_arg;
            return 0;
          }));

  auto visit_mojom = history_clusters::mojom::URLVisit::New();
  visit_mojom->visit_id = 1;
  std::vector<history_clusters::mojom::URLVisitPtr> visits_mojom;
  visits_mojom.push_back(std::move(visit_mojom));

  handler().DismissCluster(std::move(visits_mojom));
  ASSERT_EQ(1u, visit_ids.size());
  ASSERT_EQ(1u, visit_ids.front());
}

TEST_F(HistoryClustersPageHandlerTest, RecordClick) {
  // Send down some clusters so we have a logger.
  int64_t cluster_id = 123;
  std::vector<history::Cluster> sample_clusters = {SampleCluster(0, 1, 2)};
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals =
      {{cluster_id, HistoryClustersModuleRankingSignals()}};
  EXPECT_CALL(mock_history_clusters_module_service(), GetClusters(testing::_))
      .WillOnce(testing::Invoke(
          [&sample_clusters, &ranking_signals](
              base::OnceCallback<void(
                  std::vector<history::Cluster>,
                  base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>
                  callback) {
            std::move(callback).Run(sample_clusters, ranking_signals);
            return nullptr;
          }));
  base::MockCallback<HistoryClustersPageHandler::GetClustersCallback> callback;
  EXPECT_CALL(callback, Run(testing::_));
  handler().GetClusters(callback.Get());

  // Simulate a click and layout type.
  handler().RecordLayoutTypeShown(
      ntp::history_clusters::mojom::LayoutType::kLayout1, cluster_id);
  handler().RecordClick(cluster_id);

  // Reset handler to make sure UKM is recorded.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ResetHandler();
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::NewTabPage_HistoryClusters::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::NewTabPage_HistoryClusters::kLayoutTypeShownName, 1);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::NewTabPage_HistoryClusters::kDidEngageWithModuleName, 1);
}

TEST_F(HistoryClustersPageHandlerTest, RecordLayoutTypeShown) {
  // Send down some clusters so we have a logger.
  int64_t cluster_id = 123;
  std::vector<history::Cluster> sample_clusters = {SampleCluster(0, 1, 2)};
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals =
      {{cluster_id, HistoryClustersModuleRankingSignals()}};
  EXPECT_CALL(mock_history_clusters_module_service(), GetClusters(testing::_))
      .WillOnce(testing::Invoke(
          [&sample_clusters, &ranking_signals](
              base::OnceCallback<void(
                  std::vector<history::Cluster>,
                  base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>
                  callback) {
            std::move(callback).Run(sample_clusters, ranking_signals);
            return nullptr;
          }));
  base::MockCallback<HistoryClustersPageHandler::GetClustersCallback> callback;
  EXPECT_CALL(callback, Run(testing::_));
  handler().GetClusters(callback.Get());

  // Simulate a layout type being chosen for the cluster.
  ntp::history_clusters::mojom::LayoutType layout_type =
      ntp::history_clusters::mojom::LayoutType::kLayout3;
  handler().RecordLayoutTypeShown(layout_type, cluster_id);

  // Reset handler to make sure UKM is recorded.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ResetHandler();
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::NewTabPage_HistoryClusters::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::NewTabPage_HistoryClusters::kLayoutTypeShownName, 3);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::NewTabPage_HistoryClusters::kDidEngageWithModuleName, 0);
}

TEST_F(HistoryClustersPageHandlerTest, NotLoadCartWithoutFeature) {
  history_clusters::mojom::ClusterPtr cluster_mojom;
  EXPECT_CALL(mock_cart_service(), LoadAllActiveCarts(testing::_)).Times(0);
  handler().GetCartForCluster(std::move(cluster_mojom), base::DoNothing());
}

class HistoryClustersPageHandlerCartInQuestTest
    : public HistoryClustersPageHandlerTest {
 public:
  HistoryClustersPageHandlerCartInQuestTest() {
    features_.InitAndEnableFeature(
        ntp_features::kNtpChromeCartInHistoryClusterModule);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(HistoryClustersPageHandlerCartInQuestTest, LoadCartWithFeature) {
  history_clusters::mojom::ClusterPtr cluster_mojom;
  EXPECT_CALL(mock_cart_service(), LoadAllActiveCarts(testing::_)).Times(1);
  handler().GetCartForCluster(std::move(cluster_mojom), base::DoNothing());
}

}  // namespace
