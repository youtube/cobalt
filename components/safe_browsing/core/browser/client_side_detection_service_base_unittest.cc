// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/browser/client_side_phishing_model.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class MockClientSideDetectionServiceBase
    : public ClientSideDetectionServiceBase {
 public:
  explicit MockClientSideDetectionServiceBase(PrefService* prefs)
      : ClientSideDetectionServiceBase(prefs) {}

  void SendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> verdict,
      ClientReportPhishingRequestCallback callback,
      const std::string& access_token) override {}

  CSDModelType GetModelType() override { return CSDModelType::kNone; }

  base::ReadOnlySharedMemoryRegion GetModelSharedMemoryRegion() override {
    return base::ReadOnlySharedMemoryRegion();
  }

  base::CallbackListSubscription RegisterCallbackForModelUpdates(
      base::RepeatingClosure callback) override {
    return base::CallbackListSubscription();
  }

  using ClientSideDetectionServiceBase::AddCacheEntry;
  using ClientSideDetectionServiceBase::AddPhishingReport;
  using ClientSideDetectionServiceBase::GetPhishingNumReports;
  using ClientSideDetectionServiceBase::LoadPhishingReportTimesFromPrefs;
};

class ClientSideDetectionServiceBaseTest : public testing::Test {
 protected:
  void SetUp() override { RegisterProfilePrefs(prefs_.registry()); }

  size_t GetCacheSize(const ClientSideDetectionServiceBase& service) {
    return service.cache_.size();
  }
  bool HasCacheEntry(const ClientSideDetectionServiceBase& service,
                     const GURL& url) {
    return service.cache_.find(url) != service.cache_.end();
  }
  void UpdateCache(ClientSideDetectionServiceBase& service) {
    service.UpdateCache();
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(ClientSideDetectionServiceBaseTest, AtPhishingReportLimit) {
  MockClientSideDetectionServiceBase service(&prefs_);
  base::Time now = base::Time::Now();

  EXPECT_FALSE(service.AtPhishingReportLimit());

  for (int i = 0; i < ClientSideDetectionServiceBase::kMaxReportsPerInterval;
       ++i) {
    service.AddPhishingReport(now);
  }

  EXPECT_TRUE(service.AtPhishingReportLimit());
}

TEST_F(ClientSideDetectionServiceBaseTest, AtPhishingReportLimitESB) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["kMaxReportsPerIntervalESB"] = "10";
  feature_list.InitAndEnableFeatureWithParameters(
      kSafeBrowsingDailyPhishingReportsLimit, params);

  prefs_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  MockClientSideDetectionServiceBase service(&prefs_);
  base::Time now = base::Time::Now();

  for (int i = 0; i < 3; ++i) {
    service.AddPhishingReport(now);
  }

  // Should NOT be at limit yet for ESB (limit 10)
  EXPECT_FALSE(service.AtPhishingReportLimit());

  for (int i = 3; i < 10; ++i) {
    service.AddPhishingReport(now);
  }

  EXPECT_TRUE(service.AtPhishingReportLimit());
}

TEST_F(ClientSideDetectionServiceBaseTest, AddPhishingReport) {
  MockClientSideDetectionServiceBase service(&prefs_);
  base::Time now = base::Time::Now();

  EXPECT_TRUE(service.AddPhishingReport(now));
  EXPECT_EQ(1, service.GetPhishingNumReports());

  for (int i = 1; i < ClientSideDetectionServiceBase::kMaxReportsPerInterval;
       ++i) {
    EXPECT_TRUE(service.AddPhishingReport(now));
  }

  EXPECT_FALSE(service.AddPhishingReport(now));
  EXPECT_EQ(ClientSideDetectionServiceBase::kMaxReportsPerInterval,
            service.GetPhishingNumReports());
}

TEST_F(ClientSideDetectionServiceBaseTest, LoadPhishingReportTimesFromPrefs) {
  MockClientSideDetectionServiceBase service(&prefs_);
  base::Time now = base::Time::Now();

  service.AddPhishingReport(now);
  service.AddPhishingReport(now - base::Hours(1));
  service.AddPhishingReport(now - base::Days(2));  // Expired

  MockClientSideDetectionServiceBase service2(&prefs_);
  service2.LoadPhishingReportTimesFromPrefs();
  EXPECT_EQ(2, service2.GetPhishingNumReports());
}

TEST_F(ClientSideDetectionServiceBaseTest, CacheTest) {
  MockClientSideDetectionServiceBase service(&prefs_);
  GURL url("http://first.url.com/");
  bool is_phishing = false;

  EXPECT_FALSE(service.GetValidCachedResult(url, &is_phishing));

  base::Time now = base::Time::Now();

  // 1. Positive cache entry (valid: created now)
  service.AddCacheEntry(GURL("http://first.url.com/"), true, now);

  // 2. Expired negative cache entry (created 1 day + 1 hour ago)
  service.AddCacheEntry(
      GURL("http://second.url.com/"), false,
      now -
          base::Days(
              ClientSideDetectionServiceBase::kNegativeCacheIntervalDays) -
          base::Hours(1));

  // 3. Expired positive cache entry (created 35 minutes ago)
  service.AddCacheEntry(
      GURL("http://third.url.com/"), true,
      now -
          base::Minutes(
              ClientSideDetectionServiceBase::kPositiveCacheIntervalMinutes) -
          base::Minutes(5));

  // 4. Valid positive cache entry (created 25 minutes ago)
  service.AddCacheEntry(
      GURL("http://fourth.url.com/"), true,
      now -
          base::Minutes(
              ClientSideDetectionServiceBase::kPositiveCacheIntervalMinutes) +
          base::Minutes(5));

  // 5. Valid negative cache entry (created 23 hours 55 minutes ago)
  service.AddCacheEntry(
      GURL("http://fifth.url.com/"), false,
      now -
          base::Days(
              ClientSideDetectionServiceBase::kNegativeCacheIntervalDays) +
          base::Minutes(5));

  // Call UpdateCache
  UpdateCache(service);

  // The size should be 4 (first, third, fourth, fifth)
  EXPECT_EQ(4U, GetCacheSize(service));
  EXPECT_TRUE(HasCacheEntry(service, GURL("http://first.url.com/")));
  EXPECT_FALSE(HasCacheEntry(service, GURL("http://second.url.com/")));
  EXPECT_TRUE(HasCacheEntry(service, GURL("http://third.url.com/")));
  EXPECT_TRUE(HasCacheEntry(service, GURL("http://fourth.url.com/")));
  EXPECT_TRUE(HasCacheEntry(service, GURL("http://fifth.url.com/")));

  // Retrieve results
  EXPECT_TRUE(service.GetValidCachedResult(GURL("http://first.url.com/"),
                                           &is_phishing));
  EXPECT_TRUE(is_phishing);

  // Third URL is in cache but is expired (> 30 minutes)
  EXPECT_FALSE(service.GetValidCachedResult(GURL("http://third.url.com/"),
                                            &is_phishing));

  EXPECT_TRUE(service.GetValidCachedResult(GURL("http://fourth.url.com/"),
                                           &is_phishing));
  EXPECT_TRUE(is_phishing);

  EXPECT_TRUE(service.GetValidCachedResult(GURL("http://fifth.url.com/"),
                                           &is_phishing));
  EXPECT_FALSE(is_phishing);
}

TEST_F(ClientSideDetectionServiceBaseTest, IsPrivateIPAddress) {
  MockClientSideDetectionServiceBase service(&prefs_);

  net::IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral("10.1.2.3"));
  EXPECT_TRUE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("127.0.0.1"));
  EXPECT_TRUE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("172.24.3.4"));
  EXPECT_TRUE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("192.168.1.1"));
  EXPECT_TRUE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("fc00::"));
  EXPECT_TRUE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("fec0::"));
  EXPECT_TRUE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("fec0:1:2::3"));
  EXPECT_TRUE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("::1"));
  EXPECT_TRUE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("::ffff:192.168.1.1"));
  EXPECT_TRUE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("1.2.3.4"));
  EXPECT_FALSE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("200.1.1.1"));
  EXPECT_FALSE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("2001:0db8:ac10:fe01::"));
  EXPECT_FALSE(service.IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("::ffff:23c5:281b"));
  EXPECT_FALSE(service.IsPrivateIPAddress(address));
}

TEST_F(ClientSideDetectionServiceBaseTest, GetNumReportPruningTest) {
  MockClientSideDetectionServiceBase service(&prefs_);

  base::Time now = base::Time::Now();
  base::TimeDelta twenty_five_hours = base::Hours(25);

  // These two should be pruned because they are older than 24 hours
  EXPECT_TRUE(service.AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(service.AddPhishingReport(now - twenty_five_hours));

  // These two are within the 24 hour limit
  EXPECT_TRUE(service.AddPhishingReport(now));
  EXPECT_TRUE(service.AddPhishingReport(now));

  EXPECT_EQ(2, service.GetPhishingNumReports());
  EXPECT_FALSE(service.AtPhishingReportLimit());

  EXPECT_TRUE(service.AddPhishingReport(now));
  EXPECT_EQ(3, service.GetPhishingNumReports());
  EXPECT_TRUE(service.AtPhishingReportLimit());
}

}  // namespace safe_browsing
