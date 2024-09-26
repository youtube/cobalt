// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/local_network_requests_page_load_metrics_observer.h"

#include <vector>

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/test/navigation_simulator.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "url/gurl.h"

namespace internal {

typedef struct {
  const char* url;
  const char* host_ip;
  uint16_t port;
} PageAddressInfo;

typedef struct {
  internal::ResourceType resource_type;
  internal::PortType port_type;
  int success_count, failed_count;
} UkmMetricInfo;

static const PageAddressInfo
    kPublicPage = {"https://foo.com/", "216.58.195.78", 443},
    kPublicPageIPv6 = {"https://google.com/", "2607:f8b0:4005:809::200e", 443},
    kPrivatePage = {"http://test.local/", "192.168.10.123", 80},
    kLocalhostPage = {"http://localhost/", "127.0.0.1", 80},
    kLocalhostPageIPv6 = {"http://[::1]/", "::1", 80},
    kPublicRequest1 = {"http://bar.com/", "100.150.200.250", 80},
    kPublicRequest2 = {"https://www.baz.com/", "192.10.20.30", 443},
    kSameSubnetRequest1 = {"http://test2.local:9000/", "192.168.10.200", 9000},
    kSameSubnetRequest2 = {"http://test2.local:8000/index.html",
                           "192.168.10.200", 8000},
    kSameSubnetRequest3 = {"http://test2.local:8000/bar.html", "192.168.10.200",
                           8000},
    kDiffSubnetRequest1 = {"http://10.0.10.200/", "10.0.10.200", 80},
    kDiffSubnetRequest2 = {"http://172.16.0.85:8181/", "172.16.0.85", 8181},
    kDiffSubnetRequest3 = {"http://10.15.20.25:12345/", "10.15.20.25", 12345},
    kDiffSubnetRequest4 = {"http://172.31.100.20:515/", "172.31.100.20", 515},
    kLocalhostRequest1 = {"http://localhost:8080/", "127.0.0.1", 8080},  // WEB
    kLocalhostRequest2 = {"http://127.0.1.1:3306/", "127.0.1.1", 3306},  // DB
    kLocalhostRequest3 = {"http://localhost:515/", "127.0.2.1", 515},  // PRINT
    kLocalhostRequest4 = {"http://127.100.150.200:9000/", "127.100.150.200",
                          9000},  // DEV
    kLocalhostRequest5 = {"http://127.0.0.1:9876/", "127.0.0.1",
                          9876},  // OTHER
    kRouterRequest1 = {"http://10.0.0.1/", "10.0.0.1", 80},
    kRouterRequest2 = {"https://192.168.10.1/", "192.168.10.1", 443};

}  // namespace internal

class LocalNetworkRequestsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  LocalNetworkRequestsPageLoadMetricsObserverTest()
      : page_load_metrics::PageLoadMetricsObserverTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {}
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<LocalNetworkRequestsPageLoadMetricsObserver>());
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
  }

  void SimulateNavigateAndCommit(const internal::PageAddressInfo& page) {
    GURL url(page.url);
    net::IPAddress address;
    ASSERT_TRUE(address.AssignFromIPLiteral(page.host_ip));
    net::IPEndPoint remote_endpoint(address, page.port);

    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    navigation_simulator_->Start();
    navigation_simulator_->SetSocketAddress(remote_endpoint);
    navigation_simulator_->Commit();
  }

  void SimulateLoadedSuccessfulResource(
      const internal::PageAddressInfo& resource) {
    SimulateLoadedResourceWithNetError(resource, 0);
  }

  void SimulateLoadedFailedResource(const internal::PageAddressInfo& resource) {
    SimulateLoadedResourceWithNetError(resource, net::ERR_CONNECTION_REFUSED);
  }

  void SimulateLoadedResourceWithNetError(
      const internal::PageAddressInfo& resource,
      const int net_error) {
    net::IPAddress address;
    ASSERT_TRUE(address.AssignFromIPLiteral(resource.host_ip));
    page_load_metrics::ExtraRequestCompleteInfo request_info(
        url::SchemeHostPort(GURL(resource.url)),
        net::IPEndPoint(address, resource.port), -1 /* frame_tree_node_id */,
        !net_error /* was_cached */,
        (net_error ? 1024 * 20 : 0) /* raw_body_bytes */,
        0 /* original_network_content_length */,
        network::mojom::RequestDestination::kDocument, net_error,
        {} /* load_timing_info */);

    tester()->SimulateLoadedResource(
        request_info, navigation_simulator_->GetGlobalRequestID());
  }

  void NavigateToPageAndLoadResources(
      const internal::PageAddressInfo& page,
      const std::vector<std::pair<internal::PageAddressInfo, bool>>&
          resources_and_statuses) {
    SimulateNavigateAndCommit(page);
    for (size_t i = 0; i < resources_and_statuses.size(); ++i) {
      if (resources_and_statuses[i].second) {
        SimulateLoadedSuccessfulResource(resources_and_statuses[i].first);
      } else {
        SimulateLoadedFailedResource(resources_and_statuses[i].first);
      }
    }
    DeleteContents();
  }

  const content::GlobalRequestID GetGlobalRequestID() {
    DCHECK(navigation_simulator_);
    return navigation_simulator_->GetGlobalRequestID();
  }
  void ExpectUkmPageDomainMetric(const internal::PageAddressInfo& page,
                                 const internal::DomainType domain_type) {
    auto entries = tester()->test_ukm_recorder().GetEntriesByName(
        ukm::builders::PageDomainInfo::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                            GURL(page.url));
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entry, ukm::builders::PageDomainInfo::kDomainTypeName,
          static_cast<int>(domain_type));
    }
  }

  void ExpectMetrics(
      const internal::PageAddressInfo& page,
      const std::vector<internal::UkmMetricInfo>& expected_metrics) {
    using LocalNetworkRequests = ukm::builders::LocalNetworkRequests;
    auto entries = tester()->test_ukm_recorder().GetEntriesByName(
        LocalNetworkRequests::kEntryName);
    ASSERT_EQ(entries.size(), expected_metrics.size());
    for (size_t i = 0; i < entries.size() && i < expected_metrics.size(); i++) {
      tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entries[i],
                                                            GURL(page.url));
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entries[i], LocalNetworkRequests::kResourceTypeName,
          expected_metrics[i].resource_type);
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entries[i], LocalNetworkRequests::kCount_SuccessfulName,
          expected_metrics[i].success_count);
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entries[i], LocalNetworkRequests::kCount_FailedName,
          expected_metrics[i].failed_count);
      if (expected_metrics[i].resource_type ==
          internal::RESOURCE_TYPE_LOCALHOST) {
        tester()->test_ukm_recorder().ExpectEntryMetric(
            entries[i], LocalNetworkRequests::kPortTypeName,
            static_cast<int>(expected_metrics[i].port_type));
      }
    }
  }

 private:
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;
};

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest, NoMetrics) {
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().sources_count());
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().entries_count());
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PublicPageIPv6PublicRequests) {
  // Navigate to a public page and make only public resource requests.
  const internal::PageAddressInfo& page = internal::kPublicPageIPv6;
  NavigateToPageAndLoadResources(page, {{internal::kPublicRequest1, true},
                                        {internal::kPublicPageIPv6, true}});

  // Should generate only a domain type UKM entry and nothing else.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PUBLIC);
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PublicPagePublicRequests) {
  // Navigate to a public page and make only public resource requests.
  const internal::PageAddressInfo& page = internal::kPublicPage;
  NavigateToPageAndLoadResources(page, {{internal::kPublicRequest1, true},
                                        {internal::kPublicRequest2, true},
                                        {internal::kPublicPageIPv6, true}});

  // Should generate only a domain type UKM entry and nothing else.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PUBLIC);
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PrivatePageSelfRequests) {
  // Navigate to a private page and make resource requests only to the page
  // itself.
  const internal::PageAddressInfo& page = internal::kSameSubnetRequest1;
  NavigateToPageAndLoadResources(page, {{internal::kSameSubnetRequest2, true},
                                        {internal::kSameSubnetRequest3, false},
                                        {page, true}});

  // Should generate only a domain type UKM entry and nothing else.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PRIVATE);
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest, PrivatePageNoRequests) {
  // Navigate to a private page and make no resource requests.
  const internal::PageAddressInfo& page = internal::kPrivatePage;
  NavigateToPageAndLoadResources(
      page, std::vector<std::pair<internal::PageAddressInfo, bool>>{});

  // Should generate only a domain type UKM entry and nothing else.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PRIVATE);
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest, LocalhostPage) {
  // Navigate to a localhost page and make some arbitrary resource requests.
  const internal::PageAddressInfo& page = internal::kLocalhostPage;
  NavigateToPageAndLoadResources(page, {{internal::kPublicRequest1, true},
                                        {internal::kPublicRequest2, false},
                                        {internal::kSameSubnetRequest1, true},
                                        {internal::kLocalhostRequest5, true}});

  // Should generate only a domain type UKM entry and nothing else.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_LOCALHOST);
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest, LocalhostPageIPv6) {
  // Navigate to a localhost page with an IPv6 address and make some arbitrary
  // resource requests.
  const internal::PageAddressInfo& page = internal::kLocalhostPageIPv6;
  NavigateToPageAndLoadResources(page, {{internal::kPublicRequest1, false},
                                        {internal::kLocalhostRequest2, true},
                                        {internal::kDiffSubnetRequest1, false},
                                        {internal::kLocalhostRequest4, true}});

  // Should generate only a domain type UKM entry and nothing else.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_LOCALHOST);
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PublicPageAllSuccessfulRequests) {
  // Navigate to a public page and make successful resource requests to all
  // resource types.
  const internal::PageAddressInfo& page = internal::kPublicPage;
  NavigateToPageAndLoadResources(page, {{internal::kPublicPage, true},
                                        {internal::kPublicPageIPv6, true},
                                        {internal::kPrivatePage, true},
                                        {internal::kLocalhostPage, true},
                                        {internal::kLocalhostPageIPv6, true},
                                        {internal::kPublicRequest1, true},
                                        {internal::kPublicRequest2, true},
                                        {internal::kSameSubnetRequest1, true},
                                        {internal::kSameSubnetRequest2, true},
                                        {internal::kSameSubnetRequest3, true},
                                        {internal::kDiffSubnetRequest1, true},
                                        {internal::kDiffSubnetRequest2, true},
                                        {internal::kLocalhostRequest1, true},
                                        {internal::kLocalhostRequest2, true},
                                        {internal::kLocalhostRequest3, true},
                                        {internal::kLocalhostRequest4, true},
                                        {internal::kLocalhostRequest5, true},
                                        {internal::kRouterRequest1, true},
                                        {internal::kRouterRequest2, true}});

  // Should now have generated UKM entries for each of the types of resources
  // requested except for the public resources.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PUBLIC);

  // We should now see UKM entries and UMA histograms for each of the types of
  // resources requested except for public resources.
  ExpectMetrics(page,
                // List of expected UKM metric values.
                {
                    {internal::RESOURCE_TYPE_ROUTER, internal::PORT_TYPE_WEB, 1,
                     0},  // 10.0.0.1:80
                    {internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_WEB,
                     1, 0},  // 10.0.10.200:80
                    {internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_WEB,
                     1, 0},  // 172.16.0.85:8181
                    {internal::RESOURCE_TYPE_ROUTER, internal::PORT_TYPE_WEB, 1,
                     0},  // 192.168.10.1:443
                    {internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_WEB,
                     1, 0},  // 192.168.10.123:80
                    {internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_WEB,
                     3, 0},  // 192.168.10.200:8000
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_WEB,
                     2, 0},  // 127.0.0.1:80
                    {internal::RESOURCE_TYPE_LOCALHOST,
                     internal::PORT_TYPE_PRINT, 1, 0},  // 127.0.2.1:515
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_DB,
                     1, 0},  // 127.0.1.1:3306
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_WEB,
                     1, 0},  // 127.0.0.1:8080
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_DEV,
                     1, 0},  // 127.100.150.200:9000
                    {internal::RESOURCE_TYPE_LOCALHOST,
                     internal::PORT_TYPE_OTHER, 1, 0},  // 127.0.0.1:9876

                });
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PrivatePageAllSuccessfulRequests) {
  // Navigate to a private page and make successful resource requests to all
  // resource types.
  const internal::PageAddressInfo& page = internal::kPrivatePage;
  NavigateToPageAndLoadResources(page, {{internal::kPublicPage, true},
                                        {internal::kPublicPageIPv6, true},
                                        {internal::kPrivatePage, true},
                                        {internal::kLocalhostPage, true},
                                        {internal::kLocalhostPageIPv6, true},
                                        {internal::kPublicRequest1, true},
                                        {internal::kPublicRequest2, true},
                                        {internal::kSameSubnetRequest1, true},
                                        {internal::kSameSubnetRequest2, true},
                                        {internal::kSameSubnetRequest3, true},
                                        {internal::kDiffSubnetRequest1, true},
                                        {internal::kDiffSubnetRequest2, true},
                                        {internal::kLocalhostRequest1, true},
                                        {internal::kLocalhostRequest2, true},
                                        {internal::kLocalhostRequest3, true},
                                        {internal::kLocalhostRequest4, true},
                                        {internal::kLocalhostRequest5, true},
                                        {internal::kRouterRequest1, true},
                                        {internal::kRouterRequest2, true}});

  // Should now have generated UKM entries for each of the types of resources
  // requested except for the public resources.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PRIVATE);

  // We should now see UKM entries and UMA histograms for each of the types of
  // resources requested except for the request to the page itself.
  ExpectMetrics(page,
                // List of expected UKM metric values.
                {
                    {internal::RESOURCE_TYPE_LOCAL_DIFF_SUBNET,
                     internal::PORT_TYPE_WEB, 1, 0},  // 10.0.0.1:80
                    {internal::RESOURCE_TYPE_LOCAL_DIFF_SUBNET,
                     internal::PORT_TYPE_WEB, 1, 0},  // 10.0.10.200:80
                    {internal::RESOURCE_TYPE_PUBLIC, internal::PORT_TYPE_WEB, 1,
                     0},  // 100.150.200.250:80
                    {internal::RESOURCE_TYPE_LOCAL_DIFF_SUBNET,
                     internal::PORT_TYPE_WEB, 1, 0},  // 172.16.0.85:8181
                    {internal::RESOURCE_TYPE_PUBLIC, internal::PORT_TYPE_WEB, 1,
                     0},  // 192.10.20.30:443
                    {internal::RESOURCE_TYPE_LOCAL_SAME_SUBNET,
                     internal::PORT_TYPE_WEB, 1, 0},  // 192.168.10.1:443
                    {internal::RESOURCE_TYPE_LOCAL_SAME_SUBNET,
                     internal::PORT_TYPE_WEB, 3, 0},  // 192.168.10.200:8000
                    {internal::RESOURCE_TYPE_PUBLIC, internal::PORT_TYPE_WEB, 1,
                     0},  // 216.58.195.78:443
                    {internal::RESOURCE_TYPE_PUBLIC, internal::PORT_TYPE_WEB, 1,
                     0},  // [2607:f8b0:4005:809::200e]:443
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_WEB,
                     2, 0},  // 127.0.0.1:80
                    {internal::RESOURCE_TYPE_LOCALHOST,
                     internal::PORT_TYPE_PRINT, 1, 0},  // 127.0.2.1:515
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_DB,
                     1, 0},  // 127.0.1.1:3306
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_WEB,
                     1, 0},  // 127.0.0.1:8080
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_DEV,
                     1, 0},  // 127.100.150.200:9000
                    {internal::RESOURCE_TYPE_LOCALHOST,
                     internal::PORT_TYPE_OTHER, 1, 0},  // 127.0.0.1:9876
                });
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PrivatePageAllFailedRequests) {
  // Navigate to a private page and make successful resource requests to all
  // resource types.
  const internal::PageAddressInfo& page = internal::kPrivatePage;
  NavigateToPageAndLoadResources(page, {{internal::kPublicPage, false},
                                        {internal::kPublicPageIPv6, false},
                                        {internal::kPrivatePage, false},
                                        {internal::kLocalhostPage, false},
                                        {internal::kLocalhostPageIPv6, false},
                                        {internal::kPublicRequest1, false},
                                        {internal::kPublicRequest2, false},
                                        {internal::kSameSubnetRequest1, false},
                                        {internal::kSameSubnetRequest2, false},
                                        {internal::kSameSubnetRequest3, false},
                                        {internal::kDiffSubnetRequest1, false},
                                        {internal::kDiffSubnetRequest2, false},
                                        {internal::kLocalhostRequest1, false},
                                        {internal::kLocalhostRequest2, false},
                                        {internal::kLocalhostRequest3, false},
                                        {internal::kLocalhostRequest4, false},
                                        {internal::kLocalhostRequest5, false},
                                        {internal::kRouterRequest1, false},
                                        {internal::kRouterRequest2, false}});

  // Should now have generated UKM entries for each of the types of resources
  // requested except for the public resources.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PRIVATE);

  ExpectMetrics(page,
                // List of expected UKM metric values.
                {
                    {internal::RESOURCE_TYPE_LOCAL_DIFF_SUBNET,
                     internal::PORT_TYPE_WEB, 0, 1},  // 10.0.0.1:80
                    {internal::RESOURCE_TYPE_LOCAL_DIFF_SUBNET,
                     internal::PORT_TYPE_WEB, 0, 1},  // 10.0.10.200:80
                    {internal::RESOURCE_TYPE_PUBLIC, internal::PORT_TYPE_WEB, 0,
                     1},  // 100.150.200.250:80
                    {internal::RESOURCE_TYPE_LOCAL_DIFF_SUBNET,
                     internal::PORT_TYPE_WEB, 0, 1},  // 172.16.0.85:8181
                    {internal::RESOURCE_TYPE_PUBLIC, internal::PORT_TYPE_WEB, 0,
                     1},  // 192.10.20.30:443
                    {internal::RESOURCE_TYPE_LOCAL_SAME_SUBNET,
                     internal::PORT_TYPE_WEB, 0, 1},  // 192.168.10.1:443
                    {internal::RESOURCE_TYPE_LOCAL_SAME_SUBNET,
                     internal::PORT_TYPE_WEB, 0, 3},  // 192.168.10.200:8000
                    {internal::RESOURCE_TYPE_PUBLIC, internal::PORT_TYPE_WEB, 0,
                     1},  // 216.58.195.78:443
                    {internal::RESOURCE_TYPE_PUBLIC, internal::PORT_TYPE_WEB, 0,
                     1},  // [2607:f8b0:4005:809::200e]:443
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_WEB,
                     0, 2},  // 127.0.0.1:80
                    {internal::RESOURCE_TYPE_LOCALHOST,
                     internal::PORT_TYPE_PRINT, 0, 1},  // 127.0.2.1:515
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_DB,
                     0, 1},  // 127.0.1.1:3306
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_WEB,
                     0, 1},  // 127.0.0.1:8080
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_DEV,
                     0, 1},  // 127.100.150.200:9000
                    {internal::RESOURCE_TYPE_LOCALHOST,
                     internal::PORT_TYPE_OTHER, 0, 1},  // 127.0.0.1:9876
                });
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PublicPageMixedStatusRequests) {
  // Navigate to a public page and make mixed status resource requests.
  const internal::PageAddressInfo& page = internal::kPublicPage;
  NavigateToPageAndLoadResources(page, {{internal::kPublicRequest1, true},
                                        {internal::kSameSubnetRequest1, true},
                                        {internal::kLocalhostRequest2, false},
                                        {internal::kDiffSubnetRequest2, true},
                                        {internal::kLocalhostRequest5, false},
                                        {internal::kDiffSubnetRequest2, false},
                                        {internal::kRouterRequest1, true}});

  // Should now have generated UKM entries for each of the types of resources
  // requested except for the public resources.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PUBLIC);

  ExpectMetrics(page,
                // List of expected UKM metric values.
                {
                    {internal::RESOURCE_TYPE_ROUTER, internal::PORT_TYPE_WEB, 1,
                     0},  // 10.0.0.1:80
                    {internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_WEB,
                     1, 1},  // 172.16.0.85:8181
                    {internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_DEV,
                     1, 0},  // 192.168.10.200:8000
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_DB,
                     0, 1},  // 127.0.1.1:3306
                    {internal::RESOURCE_TYPE_LOCALHOST,
                     internal::PORT_TYPE_OTHER, 0, 1},  // 127.0.0.1:9876
                });
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PublicPageLargeNumberOfRequests) {
  // This test also verifies the sequence and timing of UKM metric generation.

  // Navigate to a public page with an IPv6 address.
  const internal::PageAddressInfo& page = internal::kPublicPageIPv6;
  SimulateNavigateAndCommit(page);

  // Should generate only a domain type UKM entry by this point.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PUBLIC);

  // Make 100 each of many different types of requests, with 1000 of a single
  // type.
  std::vector<std::pair<internal::PageAddressInfo, bool>> requests = {
      {internal::kPublicRequest1, true},
      {internal::kLocalhostPage, true},
      {internal::kLocalhostPageIPv6, false},
      {internal::kSameSubnetRequest1, false},
      {internal::kDiffSubnetRequest2, false},
  };
  for (auto request : requests) {
    for (int i = 0; i < 100; ++i) {
      SimulateLoadedResourceWithNetError(request.first,
                                         (request.second ? 0 : -1));
    }
  }
  for (int i = 0; i < 1000; ++i) {
    SimulateLoadedSuccessfulResource(internal::kDiffSubnetRequest1);
  }

  // At this point, we should still only see the domain type UKM entry.
  // Also history manipulation intervention will log a UKM for navigating away
  // from a page without user interaction.
  EXPECT_EQ(
      1ul,
      tester()->test_ukm_recorder().GetEntriesByName("PageDomainInfo").size());
  EXPECT_EQ(1ul, tester()
                     ->test_ukm_recorder()
                     .GetEntriesByName("HistoryManipulationIntervention")
                     .size());

  // Close the page.
  DeleteContents();

  ExpectMetrics(page,
                // List of expected UKM metric values.
                {
                    {internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_WEB,
                     1000, 0},  // 10.0.10.200:80
                    {internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_WEB,
                     0, 100},  // 172.16.0.85:8181
                    {internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_DEV,
                     0, 100},  // 192.168.10.200:9000
                    {internal::RESOURCE_TYPE_LOCALHOST, internal::PORT_TYPE_WEB,
                     100, 100},  // 127.0.0.1:80
                });
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PublicPageRequestIpInUrlOnly) {
  const internal::PageAddressInfo& page = internal::kPublicPage;
  SimulateNavigateAndCommit(page);

  // Load a resource that has the IP address in the URL but returned an empty
  // socket address for some reason.
  PageLoadMetricsObserverTestHarness::tester()->SimulateLoadedResource(
      {url::SchemeHostPort(GURL(internal::kDiffSubnetRequest2.url)),
       net::IPEndPoint(), -1 /* frame_tree_node_id */, true /* was_cached */,
       1024 * 20 /* raw_body_bytes */, 0 /* original_network_content_length */,
       network::mojom::RequestDestination::kDocument, 0,
       nullptr /* load_timing_info */},
      GetGlobalRequestID());
  DeleteContents();

  // We should still see a UKM entry and UMA histogram for the resource request.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PUBLIC);
  ExpectMetrics(
      page, {{internal::RESOURCE_TYPE_PRIVATE, internal::PORT_TYPE_WEB, 1, 0}});
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest,
       PublicPageRequestIpNotPresent) {
  const internal::PageAddressInfo& page = internal::kPublicPage;
  SimulateNavigateAndCommit(page);

  // Load a resource that doesn't have the IP address in the URL and returned an
  // empty socket address (e.g., failed DNS resolution).
  PageLoadMetricsObserverTestHarness::tester()->SimulateLoadedResource(
      {url::SchemeHostPort(GURL(internal::kPrivatePage.url)), net::IPEndPoint(),
       -1 /* frame_tree_node_id */, false /* was_cached */,
       0 /* raw_body_bytes */, 0 /* original_network_content_length */,
       network::mojom::RequestDestination::kDocument, -20,
       nullptr /* load_timing_info */},
      GetGlobalRequestID());
  DeleteContents();

  // We shouldn't see any UKM entries or UMA histograms this time.
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PUBLIC);
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest, PrivatePageSubnet10) {
  // Navigate to a private page on the 10.0.0.0/8 subnet and make requests to
  // other 10.0.0.0/8 subnet resources.
  const internal::PageAddressInfo& page = internal::kRouterRequest1;
  NavigateToPageAndLoadResources(page, {{internal::kDiffSubnetRequest1, false},
                                        {internal::kDiffSubnetRequest3, false},
                                        {internal::kRouterRequest2, false}});
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PRIVATE);

  // The first two requests should be on the same subnet and the last request
  // should be on a different subnet.
  ExpectMetrics(page,
                {
                    {internal::RESOURCE_TYPE_LOCAL_SAME_SUBNET,
                     internal::PORT_TYPE_WEB, 0, 1},  // 10.0.10.200:80
                    {internal::RESOURCE_TYPE_LOCAL_SAME_SUBNET,
                     internal::PORT_TYPE_OTHER, 0, 1},  // 10.15.20.25:12345
                    {internal::RESOURCE_TYPE_LOCAL_DIFF_SUBNET,
                     internal::PORT_TYPE_WEB, 0, 1},  // 192.168.10.1:443
                });
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest, PrivatePageSubnet172) {
  // Navigate to a private page on the 10.0.0.0/8 subnet and make requests to
  // other 10.0.0.0/8 subnet resources.
  const internal::PageAddressInfo& page = internal::kDiffSubnetRequest2;
  NavigateToPageAndLoadResources(page, {{internal::kDiffSubnetRequest4, false},
                                        {internal::kRouterRequest1, false}});
  ExpectUkmPageDomainMetric(page, internal::DOMAIN_TYPE_PRIVATE);

  // The first two requests should be on the same subnet and the last request
  // should be on a different subnet.
  ExpectMetrics(page,
                {
                    {internal::RESOURCE_TYPE_LOCAL_DIFF_SUBNET,
                     internal::PORT_TYPE_WEB, 0, 1},  // 10.0.10.200:80
                    {internal::RESOURCE_TYPE_LOCAL_SAME_SUBNET,
                     internal::PORT_TYPE_PRINT, 0, 1},  // 172.31.100.20:515
                });
}

TEST_F(LocalNetworkRequestsPageLoadMetricsObserverTest, PrivatePageFailedLoad) {
  GURL url(internal::kPrivatePage.url);
  auto navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation_simulator->Start();
  navigation_simulator->Fail(-20);
  navigation_simulator->CommitErrorPage();

  // Nothing should have been generated.
  // Note that the expected count is 1 because history manipulation intervention
  // will log a UKM for navigating away from a page without user interaction.
  EXPECT_EQ(1ul, tester()
                     ->test_ukm_recorder()
                     .GetEntriesByName("HistoryManipulationIntervention")
                     .size());
}
