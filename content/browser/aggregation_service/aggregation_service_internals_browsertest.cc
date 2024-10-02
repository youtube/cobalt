// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_internals_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_observer.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

using GetPendingReportsCallback = base::OnceCallback<void(
    std::vector<AggregationServiceStorage::RequestAndId>)>;

constexpr char kPrivateAggregationInternalsUrl[] =
    "chrome://private-aggregation-internals/";

const std::u16string kCompleteTitle = u"Complete";

class AggregationServiceInternalsWebUiBrowserTest : public ContentBrowserTest {
 public:
  AggregationServiceInternalsWebUiBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    auto aggregation_service = std::make_unique<MockAggregationService>();

    ON_CALL(*aggregation_service, GetPendingReportRequestsForWebUI)
        .WillByDefault([](GetPendingReportsCallback callback) {
          std::move(callback).Run(
              AggregatableReportRequestsAndIdsBuilder().Build());
        });

    static_cast<StoragePartitionImpl*>(shell()
                                           ->web_contents()
                                           ->GetBrowserContext()
                                           ->GetDefaultStoragePartition())
        ->OverrideAggregationServiceForTesting(std::move(aggregation_service));
  }

  // Executing javascript in the WebUI requires using an isolated world in which
  // to execute the script because WebUI has a default CSP policy denying
  // "eval()", which is what EvalJs uses under the hood.
  bool ExecJsInWebUI(const std::string& script) {
    return ExecJs(shell()->web_contents()->GetPrimaryMainFrame(), script,
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1);
  }

  // Registers a mutation observer that sets the window title to `title` when
  // the report table is empty.
  void SetTitleOnReportsTableEmpty(const std::u16string& title) {
    static constexpr char kObserveEmptyReportsTableScript[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 1 &&
            table.children[0].children[0].textContent === 'No sent or pending reports.') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(
        ExecJsInWebUI(JsReplace(kObserveEmptyReportsTableScript, title)));
  }

  void ClickRefreshButton() {
    EXPECT_TRUE(ExecJsInWebUI("document.getElementById('refresh').click();"));
  }

 protected:
  MockAggregationService& aggregation_service() {
    AggregationService* agg_service =
        static_cast<StoragePartitionImpl*>(shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition())
            ->GetAggregationService();
    return static_cast<MockAggregationService&>(*agg_service);
  }
};

IN_PROC_BROWSER_TEST_F(AggregationServiceInternalsWebUiBrowserTest,
                       NavigationUrl_ResolvedToWebUI) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         "document.body.innerHTML.includes('Private "
                         "Aggregation API Internals');",
                         EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

IN_PROC_BROWSER_TEST_F(AggregationServiceInternalsWebUiBrowserTest,
                       WebUIShownWithNoReports_NoReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  SetTitleOnReportsTableEmpty(kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AggregationServiceInternalsWebUiBrowserTest,
                       WebUIShownWithReports_ReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  base::Time now = base::Time::Now();

  ON_CALL(aggregation_service(), GetPendingReportRequestsForWebUI)
      .WillByDefault([now](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder()
                .AddRequestWithID(
                    aggregation_service::CreateExampleRequestWithReportTime(
                        now),
                    AggregationServiceStorage::RequestId(20))
                .Build());
      });

  aggregation_service::TestHpkeKey hpke_key =
      aggregation_service::GenerateKey("id123");

  AggregatableReportRequest request_1 =
      aggregation_service::CreateExampleRequest();
  absl::optional<AggregatableReport> report_1 =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          request_1, {hpke_key.public_key});

  aggregation_service().NotifyReportHandled(
      std::move(request_1), AggregationServiceStorage::RequestId(1),
      std::move(report_1), /*report_handled_time=*/now + base::Hours(1),
      AggregationServiceObserver::ReportStatus::kSent);

  AggregatableReportRequest request_2 =
      aggregation_service::CreateExampleRequest();
  aggregation_service().NotifyReportHandled(
      std::move(request_2), AggregationServiceStorage::RequestId(2),
      /*report=*/absl::nullopt,
      /*report_handled_time=*/now + base::Hours(2),
      AggregationServiceObserver::ReportStatus::kFailedToAssemble);

  AggregatableReportRequest request_3 =
      aggregation_service::CreateExampleRequest();
  absl::optional<AggregatableReport> report_3 =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          request_3, {hpke_key.public_key});

  aggregation_service().NotifyReportHandled(
      std::move(request_3), AggregationServiceStorage::RequestId(3),
      std::move(report_3),
      /*report_handled_time=*/now + base::Hours(3),
      AggregationServiceObserver::ReportStatus::kFailedToSend);

  {
    static constexpr char wait_script[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const cell = (a, b) => table.children[a]?.children[b]?.textContent;
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 4 &&
            cell(0, 1) === 'Pending' &&
            cell(0, 2) === 'https://reporting.example/example-path' &&
            cell(0, 3) === (new Date($2)).toLocaleString() &&
            cell(0, 4) === 'example-api' &&
            cell(0, 5) === '' &&
            cell(0, 6) === '[ {  "bucket": {   "high": "0",   "low": "123"  },  "value": 456 }]' &&
            cell(1, 1) === 'Sent' &&
            cell(2, 1) === 'Failed to assemble' &&
            cell(3, 1) === 'Failed to send') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(
        JsReplace(wait_script, kCompleteTitle, (now).ToJsTime())));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    ClickRefreshButton();
    EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char wait_script[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const cell = (a, b) => table.children[a]?.children[b]?.textContent;
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 4 &&
            cell(0, 1) === 'Failed to assemble' &&
            cell(1, 1) === 'Failed to send' &&
            cell(2, 1) === 'Pending' &&
            cell(3, 1) === 'Sent') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";

    const std::u16string kCompleteTitle2 = u"Complete2";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle2)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle2);
    // Sort by status ascending.
    EXPECT_TRUE(
        ExecJsInWebUI("document.querySelector('#reportTable')"
                      ".shadowRoot.querySelectorAll('th')[1].click();"));
    EXPECT_EQ(kCompleteTitle2, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char wait_script[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const cell = (a, b) => table.children[a]?.children[b]?.textContent;
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 4 &&
            cell(0, 1) === 'Sent' &&
            cell(1, 1) === 'Pending' &&
            cell(2, 1) === 'Failed to send' &&
            cell(3, 1) === 'Failed to assemble') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";

    const std::u16string kCompleteTitle3 = u"Complete3";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle3)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle3);
    // Sort by status descending.
    EXPECT_TRUE(
        ExecJsInWebUI("document.querySelector('#reportTable')"
                      ".shadowRoot.querySelectorAll('th')[1].click();"));
    EXPECT_EQ(kCompleteTitle3, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AggregationServiceInternalsWebUiBrowserTest,
                       WebUISendReports_ReportsRemoved) {
  EXPECT_CALL(aggregation_service(), GetPendingReportRequestsForWebUI)
      .WillOnce([](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder().Build());
      })  // on page loaded
      .WillOnce([](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder()
                .AddRequestWithID(aggregation_service::CreateExampleRequest(),
                                  AggregationServiceStorage::RequestId(5))
                .Build());
      })  // on page refresh
      .WillOnce([](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder().Build());
      });  // on page updated

  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  EXPECT_CALL(aggregation_service(),
              SendReportsForWebUI(
                  testing::ElementsAre(AggregationServiceStorage::RequestId(5)),
                  testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());

  static constexpr char wait_script[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 1 &&
            table.children[0].children[1].textContent === 'Pending') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the send reports button and expect that the report table is emptied.
  const std::u16string kSentTitle = u"Sent";
  TitleWatcher sent_title_watcher(shell()->web_contents(), kSentTitle);
  SetTitleOnReportsTableEmpty(kSentTitle);

  EXPECT_TRUE(ExecJsInWebUI(
      R"(document.querySelector('#reportTable')
         .shadowRoot.querySelector('input[type="checkbox"]').click();)"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.getElementById('send-reports').click();"));

  // The real aggregation service would do this itself, but the test aggregation
  // service requires manual triggering.
  aggregation_service().NotifyRequestStorageModified();

  EXPECT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AggregationServiceInternalsWebUiBrowserTest,
                       WebUIClearStorage_ReportsRemoved) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  ON_CALL(aggregation_service(), GetPendingReportRequestsForWebUI)
      .WillByDefault([](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder()
                .AddRequestWithID(aggregation_service::CreateExampleRequest(),
                                  AggregationServiceStorage::RequestId(5))
                .Build());
      });

  aggregation_service::TestHpkeKey hpke_key =
      aggregation_service::GenerateKey("id123");
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  absl::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          request, {hpke_key.public_key});

  aggregation_service().NotifyReportHandled(
      std::move(request), AggregationServiceStorage::RequestId(10),
      std::move(report),
      /*report_handled_time=*/base::Time::Now() + base::Hours(1),
      AggregationServiceObserver::ReportStatus::kSent);

  EXPECT_CALL(aggregation_service(), ClearData)
      .WillOnce(base::test::RunOnceCallback<3>());

  // Verify both rows get rendered.
  static constexpr char wait_script[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 2 &&
            table.children[0].children[1].textContent === 'Pending' &&
            table.children[1].children[1].textContent === 'Sent') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  // Wait for the table to be rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the send reports button and expect that the report table is emptied.
  const std::u16string kDeleteTitle = u"Delete";
  TitleWatcher delete_title_watcher(shell()->web_contents(), kDeleteTitle);
  SetTitleOnReportsTableEmpty(kDeleteTitle);

  EXPECT_TRUE(ExecJsInWebUI("document.getElementById('clear-data').click();"));

  EXPECT_EQ(kDeleteTitle, delete_title_watcher.WaitAndGetTitle());
}

}  // namespace

}  // namespace content
