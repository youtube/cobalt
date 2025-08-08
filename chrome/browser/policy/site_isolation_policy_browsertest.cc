// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

class SiteIsolationPolicyBrowserTest : public PlatformBrowserTest {
 public:
  SiteIsolationPolicyBrowserTest(const SiteIsolationPolicyBrowserTest&) =
      delete;
  SiteIsolationPolicyBrowserTest& operator=(
      const SiteIsolationPolicyBrowserTest&) = delete;

 protected:
  SiteIsolationPolicyBrowserTest() = default;

  struct Expectations {
    const char* url;
    bool isolated;
  };

  void CheckExpectations(Expectations* expectations, size_t count) {
    content::BrowserContext* context = chrome_test_utils::GetProfile(this);
    for (size_t i = 0; i < count; ++i) {
      const GURL url(expectations[i].url);
      auto instance = content::SiteInstance::CreateForURL(context, url);
      EXPECT_EQ(expectations[i].isolated, instance->RequiresDedicatedProcess())
          << "; url = " << url;
    }
  }

  void CheckIsolatedOriginExpectations(Expectations* expectations,
                                       size_t count) {
    if (!content::AreAllSitesIsolatedForTesting())
      CheckExpectations(expectations, count);

    auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
    for (size_t i = 0; i < count; ++i) {
      const GURL url(expectations[i].url);
      const url::Origin origin = url::Origin::Create(url);
      EXPECT_EQ(expectations[i].isolated,
                policy->IsGloballyIsolatedOriginForTesting(origin))
          << "; origin = " << origin;
    }
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

template <bool policy_value>
class SitePerProcessPolicyBrowserTest : public SiteIsolationPolicyBrowserTest {
 public:
  SitePerProcessPolicyBrowserTest(const SitePerProcessPolicyBrowserTest&) =
      delete;
  SitePerProcessPolicyBrowserTest& operator=(
      const SitePerProcessPolicyBrowserTest&) = delete;

 protected:
  SitePerProcessPolicyBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    // We setup the policy here, because the policy must be 'live' before
    // the renderer is created, since the value for this policy is passed
    // to the renderer via a command-line. Setting the policy in the test
    // itself or in SetUpOnMainThread works for update-able policies, but
    // is too late for this one.
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;

#if BUILDFLAG(IS_ANDROID)
    const char* kPolicyName = policy::key::kSitePerProcessAndroid;
#else
    const char* kPolicyName = policy::key::kSitePerProcess;
#endif
    values.Set(kPolicyName, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(policy_value), nullptr);
    provider_.UpdateChromePolicy(values);
  }
};

typedef SitePerProcessPolicyBrowserTest<true>
    SitePerProcessPolicyBrowserTestEnabled;
typedef SitePerProcessPolicyBrowserTest<false>
    SitePerProcessPolicyBrowserTestDisabled;

// Ensure that --disable-site-isolation-trials and/or
// --disable-site-isolation-for-enterprise-policy do not override policies.
class NoOverrideSitePerProcessPolicyBrowserTest
    : public SitePerProcessPolicyBrowserTestEnabled {
 public:
  NoOverrideSitePerProcessPolicyBrowserTest(
      const NoOverrideSitePerProcessPolicyBrowserTest&) = delete;
  NoOverrideSitePerProcessPolicyBrowserTest& operator=(
      const NoOverrideSitePerProcessPolicyBrowserTest&) = delete;

 protected:
  NoOverrideSitePerProcessPolicyBrowserTest() = default;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
#if BUILDFLAG(IS_ANDROID)
    command_line->AppendSwitch(switches::kDisableSiteIsolationForPolicy);
#endif
  }
};

IN_PROC_BROWSER_TEST_F(SitePerProcessPolicyBrowserTestEnabled, Simple) {
  Expectations expectations[] = {
      {"https://foo.com/noodles.html", true},
      {"http://foo.com/", true},
      {"http://example.org/pumpkins.html", true},
  };
  CheckExpectations(expectations, std::size(expectations));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
// The policy is not supported on Android
class IsolateOriginsPolicyBrowserTest : public SiteIsolationPolicyBrowserTest {
 public:
  IsolateOriginsPolicyBrowserTest(const IsolateOriginsPolicyBrowserTest&) =
      delete;
  IsolateOriginsPolicyBrowserTest& operator=(
      const IsolateOriginsPolicyBrowserTest&) = delete;

 protected:
  IsolateOriginsPolicyBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    // We setup the policy here, because the policy must be 'live' before
    // the renderer is created, since the value for this policy is passed
    // to the renderer via a command-line. Setting the policy in the test
    // itself or in SetUpOnMainThread works for update-able policies, but
    // is too late for this one.
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    values.Set(
        policy::key::kIsolateOrigins, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value("https://policy1.example.org/,http://policy2.example.com"),
        nullptr);
    provider_.UpdateChromePolicy(values);
  }
};

IN_PROC_BROWSER_TEST_F(IsolateOriginsPolicyBrowserTest, Simple) {
  // Verify that the policy present at browser startup is correctly applied.
  Expectations expectations[] = {
      {"https://foo.com/noodles.html", false},
      {"http://foo.com/", false},
      {"https://policy1.example.org/pumpkins.html", true},
      {"http://policy2.example.com/index.php", true},
  };
  CheckIsolatedOriginExpectations(expectations, std::size(expectations));

  // Simulate updating the policy at "browser runtime".
  policy::PolicyMap values;
  values.Set(
      policy::key::kIsolateOrigins, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value("https://policy3.example.org/,http://policy4.example.com"),
      nullptr);
  provider_.UpdateChromePolicy(values);

  // Verify that the policy update above has taken effect:
  // - policy3 and policy4 origins should become isolated
  // - policy1 and policy2 origins will remain isolated, even though they were
  //   removed from the policy (this is an artifact caused by limitations of
  //   the current implementation, not something that is a hard requirement).
  Expectations expectations2[] = {
      {"https://foo.com/noodles.html", false},
      {"http://foo.com/", false},
      {"https://policy1.example.org/pumpkins.html", true},
      {"http://policy2.example.com/index.php", true},
      {"https://policy3.example.org/pumpkins.html", true},
      {"http://policy4.example.com/index.php", true},
  };
  CheckIsolatedOriginExpectations(expectations2, std::size(expectations2));
}
#endif

IN_PROC_BROWSER_TEST_F(NoOverrideSitePerProcessPolicyBrowserTest, Simple) {
  Expectations expectations[] = {
      {"https://foo.com/noodles.html", true},
      {"http://example.org/pumpkins.html", true},
  };
  CheckExpectations(expectations, std::size(expectations));
}

// After https://crbug.com/910273 was fixed, enterprise policy can only be used
// to disable Site Isolation on Android - the
// SitePerProcessPolicyBrowserTestFieldTrialTest tests should not be run on any
// other platform.  Note that browser_tests won't run on Android until
// https://crbug.com/611756 is fixed.
#if BUILDFLAG(IS_ANDROID)
class SitePerProcessPolicyBrowserTestFieldTrialTest
    : public SitePerProcessPolicyBrowserTestDisabled {
 public:
  SitePerProcessPolicyBrowserTestFieldTrialTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kSitePerProcess);
  }
  SitePerProcessPolicyBrowserTestFieldTrialTest(
      const SitePerProcessPolicyBrowserTestFieldTrialTest&) = delete;
  SitePerProcessPolicyBrowserTestFieldTrialTest& operator=(
      const SitePerProcessPolicyBrowserTestFieldTrialTest&) = delete;
  ~SitePerProcessPolicyBrowserTestFieldTrialTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SitePerProcessPolicyBrowserTestFieldTrialTest, Simple) {
  // Skip this test if the --site-per-process switch is present (e.g. on Site
  // Isolation Android chromium.fyi bot).  The test is still valid if
  // SitePerProcess is the default (e.g. via ContentBrowserClient's
  // ShouldEnableStrictSiteIsolation method) - don't skip the test in such case.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    return;
  }

  // Policy should inject kDisableSiteIsolationForPolicy rather than
  // kDisableSiteIsolation switch.
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolation));
  ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolationForPolicy));
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());

  Expectations expectations[] = {
      {"https://foo.com/noodles.html", false},
      {"http://example.org/pumpkins.html", false},
  };
  CheckExpectations(expectations, std::size(expectations));
}
#endif

IN_PROC_BROWSER_TEST_F(SiteIsolationPolicyBrowserTest, NoPolicyNoTrialsFlags) {
  // The switch to disable Site Isolation should be missing by default (i.e.
  // without an explicit enterprise policy).
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolation));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolationForPolicy));
  EXPECT_EQ(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites(),
            base::FeatureList::IsEnabled(features::kSitePerProcess));
#else
  EXPECT_TRUE(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
#endif  // BUILDFLAG(IS_ANDROID)
}
