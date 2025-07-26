// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_metrics/navigation_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char* const kTestUrl = "http://www.example.com";
// http://ab.גדהוזח.ij/kl/mn/op.html in A-label form.
constexpr char kRtlUrl[] = "http://ab.xn--6dbcdefg.ij/kl/mn/op.html";
}  // namespace

namespace navigation_metrics {

TEST(NavigationMetrics, MainFrameSchemeDifferentDocument) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), false, false,
      profile_metrics::BrowserProfileType::kRegular);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPage, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 0);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 0);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kRegular, 1);
  test.ExpectTotalCount(kMainFrameProfileTypeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameProfileTypeDifferentPage,
                          profile_metrics::BrowserProfileType::kRegular, 1);
}

TEST(NavigationMetrics, MainFrameSchemeDifferentDocument_NonUniqueHostname) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL("http://site.test"), false, false,
      profile_metrics::BrowserProfileType::kRegular);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPage, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPageNonUniqueHostname,
                          1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 0);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kRegular, 1);
  test.ExpectTotalCount(kMainFrameProfileTypeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameProfileTypeDifferentPage,
                          profile_metrics::BrowserProfileType::kRegular, 1);
}

TEST(NavigationMetrics, MainFrameSchemeSameDocument) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), true, false,
      profile_metrics::BrowserProfileType::kRegular);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 0);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 0);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kRegular, 1);
  test.ExpectTotalCount(kMainFrameProfileTypeDifferentPage, 0);
}

TEST(NavigationMetrics, MainFrameSchemeDifferentDocumentOTR) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), false, true,
      profile_metrics::BrowserProfileType::kIncognito);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPage, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 0);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 1);
  test.ExpectUniqueSample(kMainFrameSchemeOTR, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPageOTR, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kIncognito, 1);
  test.ExpectTotalCount(kMainFrameProfileTypeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameProfileTypeDifferentPage,
                          profile_metrics::BrowserProfileType::kIncognito, 1);
}

TEST(NavigationMetrics, MainFrameSchemeSameDocumentOTR) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), true, true,
      profile_metrics::BrowserProfileType::kIncognito);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 0);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 1);
  test.ExpectUniqueSample(kMainFrameSchemeOTR, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 0);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kIncognito, 1);
  test.ExpectTotalCount(kMainFrameProfileTypeDifferentPage, 0);
}

TEST(NavigationMetrics, MainFrameDifferentDocumentHasRTLDomainFalse) {
  base::HistogramTester test;
  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), false, false,
      profile_metrics::BrowserProfileType::kRegular);
  test.ExpectTotalCount(kMainFrameHasRTLDomainDifferentPage, 1);
  test.ExpectTotalCount(kMainFrameHasRTLDomain, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomainDifferentPage, 0 /* false */,
                          1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomain, 0 /* false */, 1);
}

TEST(NavigationMetrics, MainFrameDifferentDocumentHasRTLDomainTrue) {
  base::HistogramTester test;
  RecordPrimaryMainFrameNavigation(
      GURL(kRtlUrl), false, false,
      profile_metrics::BrowserProfileType::kRegular);
  test.ExpectTotalCount(kMainFrameHasRTLDomainDifferentPage, 1);
  test.ExpectTotalCount(kMainFrameHasRTLDomain, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomainDifferentPage, 1 /* true */, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomain, 1 /* true */, 1);
}

TEST(NavigationMetrics, MainFrameSameDocumentHasRTLDomainFalse) {
  base::HistogramTester test;
  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), true, false,
      profile_metrics::BrowserProfileType::kRegular);
  test.ExpectTotalCount(kMainFrameHasRTLDomainDifferentPage, 0);
  test.ExpectTotalCount(kMainFrameHasRTLDomain, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomain, 0 /* false */, 1);
}

TEST(NavigationMetrics, MainFrameSameDocumentHasRTLDomainTrue) {
  base::HistogramTester test;
  RecordPrimaryMainFrameNavigation(
      GURL(kRtlUrl), true, false,
      profile_metrics::BrowserProfileType::kRegular);
  test.ExpectTotalCount(kMainFrameHasRTLDomainDifferentPage, 0);
  test.ExpectTotalCount(kMainFrameHasRTLDomain, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomain, 1 /* true */, 1);
}

}  // namespace navigation_metrics
