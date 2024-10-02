// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_info/about_this_site_service_factory.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/page_info/page_info.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_test_util.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "ui/events/test/test_event.h"

namespace {

constexpr char kExpiredCertificateFile[] = "expired_cert.pem";
constexpr char kAboutThisSiteUrl[] = "a.test";
constexpr char kHistoryUrl[] = "b.test";
constexpr char kIsolatedWebAppUrl[] = "iwa.test";

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ui::test::TestEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Opens the Page Info bubble and retrieves the UI view identified by
// |view_id|.
views::View* GetView(Browser* browser, int view_id) {
  views::Widget* page_info_bubble =
      PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetWidget();
  EXPECT_TRUE(page_info_bubble);

  views::View* view = page_info_bubble->GetRootView()->GetViewByID(view_id);
  EXPECT_TRUE(view);
  return view;
}

}  // namespace

class PageInfoBubbleViewDialogBrowserTest : public DialogBrowserTest {
 public:
  PageInfoBubbleViewDialogBrowserTest() {
    // TODO(crbug.com/1344787): Clean up when PageSpecificSiteDataDialog is
    // launched. Disable features for the new version of "Cookies in use"
    // dialog. The new UI is covered by
    // PageInfoBubbleViewCookiesSubpageBrowserTest.
    feature_list_.InitWithFeatures(
        {}, {page_info::kPageSpecificSiteDataDialog,
             page_info::kPageInfoCookiesSubpage,
             // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having
             // to disable this feature.
             features::kHttpsUpgrades});
  }

  PageInfoBubbleViewDialogBrowserTest(
      const PageInfoBubbleViewDialogBrowserTest& test) = delete;
  PageInfoBubbleViewDialogBrowserTest& operator=(
      const PageInfoBubbleViewDialogBrowserTest& test) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name_with_param_suffix) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    const std::string& name =
        name_with_param_suffix.substr(0, name_with_param_suffix.find("/"));

    // All the possible test names.
    constexpr char kInsecure[] = "Insecure";
    constexpr char kInternal[] = "Internal";
    constexpr char kInternalExtension[] = "InternalExtension";
    constexpr char kInternalViewSource[] = "InternalViewSource";
    constexpr char kFile[] = "File";
    constexpr char kSecure[] = "Secure";
    constexpr char kSecureSubpage[] = "SecureSubpage";
    constexpr char kEvSecure[] = "EvSecure";
    constexpr char kEvSecureSubpage[] = "EvSecureSubpage";
    constexpr char kMalware[] = "Malware";
    constexpr char kDeceptive[] = "Deceptive";
    constexpr char kUnwantedSoftware[] = "UnwantedSoftware";
    constexpr char kSignInSyncPasswordReuse[] = "SignInSyncPasswordReuse";
    constexpr char kSignInNonSyncPasswordReuse[] = "SignInNonSyncPasswordReuse";
    constexpr char kEnterprisePasswordReuse[] = "EnterprisePasswordReuse";
    constexpr char kSavedPasswordReuse[] = "SavedPasswordReuse";
    constexpr char kMalwareAndBadCert[] = "MalwareAndBadCert";
    constexpr char kMixedContentForm[] = "MixedContentForm";
    constexpr char kMixedContent[] = "MixedContent";
    constexpr char kAllowAllPermissions[] = "AllowAllPermissions";
    constexpr char kBlockAllPermissions[] = "BlockAllPermissions";

    const GURL internal_url("chrome://settings");
    const GURL internal_extension_url("chrome-extension://example");
    const GURL file_url("file:///Users/homedirname/folder/file.pdf");
    // Note the following two URLs are not really necessary to get the different
    // versions of Page Info to appear, but are here to indicate the type of
    // URL each IdentityInfo type would normally be associated with.
    const GURL https_url("https://example.com");
    const GURL http_url("http://example.com");
    const std::string kSiteOrigin = "example.com";

    GURL url = http_url;
    if (name == kSecure || name == kEvSecure || name == kMixedContentForm ||
        name == kMixedContent || name == kAllowAllPermissions ||
        name == kBlockAllPermissions || name == kMalwareAndBadCert) {
      url = https_url;
    }
    if (name == kInternal) {
      url = internal_url;
    } else if (name == kInternalExtension) {
      url = internal_extension_url;
    } else if (name == kInternalViewSource) {
      constexpr char kTestHtml[] = "/viewsource/test.html";
      ASSERT_TRUE(embedded_test_server()->Start());
      url = GURL(content::kViewSourceScheme + std::string(":") +
                 embedded_test_server()->GetURL(kTestHtml).spec());
    } else if (name == kFile) {
      url = file_url;
    }

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    OpenPageInfoBubble(browser());

    safe_browsing::ReusedPasswordAccountType reused_password_account_type;
    PageInfoUI::IdentityInfo identity;
    if (name == kInsecure) {
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_NO_CERT;
    } else if (name == kSecure || name == kAllowAllPermissions ||
               name == kBlockAllPermissions || name == kSecureSubpage) {
      // Generate a valid mock HTTPS identity, with a certificate.
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_CERT;
      constexpr char kGoodCertificateFile[] = "ok_cert.pem";
      identity.certificate = net::ImportCertFromFile(
          net::GetTestCertsDirectory(), kGoodCertificateFile);
    } else if (name == kEvSecure || name == kEvSecureSubpage) {
      // Generate a valid mock EV HTTPS identity, with an EV certificate. Must
      // match conditions in PageInfoBubbleView::SetIdentityInfo() for setting
      // the certificate button subtitle.
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_EV_CERT;
      identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
      scoped_refptr<net::X509Certificate> ev_cert =
          net::X509Certificate::CreateFromBytes(thawte_der);
      ASSERT_TRUE(ev_cert);
      identity.certificate = ev_cert;
    } else if (name == kMalware) {
      identity.safe_browsing_status = PageInfo::SAFE_BROWSING_STATUS_MALWARE;
    } else if (name == kDeceptive) {
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SOCIAL_ENGINEERING;
    } else if (name == kUnwantedSoftware) {
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_UNWANTED_SOFTWARE;
    } else if (name == kSignInSyncPasswordReuse) {
      reused_password_account_type.set_account_type(
          safe_browsing::ReusedPasswordAccountType::GSUITE);
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE;
      identity.show_change_password_buttons = true;
    } else if (name == kSignInNonSyncPasswordReuse) {
      reused_password_account_type.set_account_type(
          safe_browsing::ReusedPasswordAccountType::GMAIL);
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE;
      identity.show_change_password_buttons = true;
    } else if (name == kEnterprisePasswordReuse) {
      reused_password_account_type.set_account_type(
          safe_browsing::ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE;
      identity.show_change_password_buttons = true;
    } else if (name == kSavedPasswordReuse) {
      reused_password_account_type.set_account_type(
          safe_browsing::ReusedPasswordAccountType::SAVED_PASSWORD);
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE;
      identity.show_change_password_buttons = true;
    } else if (name == kMalwareAndBadCert) {
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_ERROR;
      identity.certificate = net::ImportCertFromFile(
          net::GetTestCertsDirectory(), kExpiredCertificateFile);
      identity.safe_browsing_status = PageInfo::SAFE_BROWSING_STATUS_MALWARE;
    } else if (name == kMixedContentForm) {
      identity.identity_status =
          PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT;
      identity.connection_status =
          PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION;
    } else if (name == kMixedContent) {
      identity.identity_status =
          PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT;
      identity.connection_status =
          PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE;
    }

    if (name == kAllowAllPermissions || name == kBlockAllPermissions) {
      // Generate a |PermissionInfoList| with every permission allowed/blocked.
      PermissionInfoList permissions_list;
      for (ContentSettingsType content_type :
           PageInfo::GetAllPermissionsForTesting()) {
        PageInfo::PermissionInfo info;
        info.type = content_type;
        info.setting = (name == kAllowAllPermissions) ? CONTENT_SETTING_ALLOW
                                                      : CONTENT_SETTING_BLOCK;
        info.default_setting =
            content_settings::ContentSettingsRegistry::GetInstance()
                ->Get(info.type)
                ->GetInitialDefaultSetting();
        info.source = content_settings::SettingSource::SETTING_SOURCE_USER;
        permissions_list.push_back(info);
      }

      ChosenObjectInfoList chosen_object_list;
      PageInfo* presenter = GetPresenter();
      EXPECT_TRUE(presenter);
      EXPECT_TRUE(presenter->ui_for_testing());
      auto* current_ui = presenter->ui_for_testing();
      views::View* bubble_view =
          PageInfoBubbleView::GetPageInfoBubbleForTesting();
      // Normally |PageInfoBubbleView| doesn't update the permissions already
      // shown if they change while it's still open. For this test, manually
      // force an update by clearing the existing permission views here.
      bubble_view->GetFocusManager()->SetFocusedView(nullptr);

        auto* main_page = static_cast<PageInfoMainView*>(current_ui);
        main_page->toggle_rows_.clear();
        main_page->permissions_view_->RemoveAllChildViews();

      current_ui->SetPermissionInfo(permissions_list,
                                    std::move(chosen_object_list));
    }

    if (name == kSignInSyncPasswordReuse ||
        name == kSignInNonSyncPasswordReuse ||
        name == kEnterprisePasswordReuse || name == kSavedPasswordReuse) {
      safe_browsing::ChromePasswordProtectionService* service =
          safe_browsing::ChromePasswordProtectionService::
              GetPasswordProtectionService(browser()->profile());
      service->set_reused_password_account_type_for_last_shown_warning(
          reused_password_account_type);
      identity.safe_browsing_details = service->GetWarningDetailText(
          service->reused_password_account_type_for_last_shown_warning());
    }

    if (name == kSecureSubpage || name == kEvSecureSubpage) {
      PageInfoBubbleView* bubble_view = static_cast<PageInfoBubbleView*>(
          PageInfoBubbleView::GetPageInfoBubbleForTesting());
      bubble_view->OpenSecurityPage();
    }

    if (name != kInsecure && name.find(kInternal) == std::string::npos &&
        name != kFile) {
      identity.site_identity = kSiteOrigin;
      // The bubble may be PageInfoBubbleView or InternalPageInfoBubbleView. The
      // latter is only used for |kInternal|, so it is safe to static_cast here.
      PageInfo* presenter = GetPresenter();
      EXPECT_TRUE(presenter);
      EXPECT_TRUE(presenter->ui_for_testing());
      presenter->ui_for_testing()->SetIdentityInfo(identity);
    }
  }

  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi())
      return false;
    // Check that each expected View is present in the Page Info bubble.
    views::View* page_info_bubble_view =
        PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetContentsView();
    for (auto id : expected_identifiers_) {
      views::View* view = GetView(browser(), id);
      if (!page_info_bubble_view->Contains(view))
        return false;
    }
    return true;
  }

  PageInfo* GetPresenter() {
    return static_cast<PageInfoBubbleView*>(
               PageInfoBubbleView::GetPageInfoBubbleForTesting())
        ->presenter_for_testing();
  }

 private:
  std::vector<PageInfoViewFactory::PageInfoViewID> expected_identifiers_;
  base::test::ScopedFeatureList feature_list_;
};

// Shows the Page Info bubble for a HTTP page (specifically, about:blank).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_Insecure) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a HTTPS page.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_Secure) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a HTTPS page.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_SecureSubpage) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_EvSecure) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_EvSecureSubpage) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an internal page, e.g. chrome://settings.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_Internal) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an extensions page.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_InternalExtension) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a chrome page that displays the source HTML.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_InternalViewSource) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a file:// URL.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_File) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for malware by Safe Browsing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_Malware) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for social engineering by Safe
// Browsing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_Deceptive) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for distributing unwanted
// software by Safe Browsing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_UnwantedSoftware) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for malware that also has a bad
// certificate.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_MalwareAndBadCert) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an admin-provided cert when the page is
// secure, but has a form that submits to an insecure url.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_MixedContentForm) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an admin-provided cert when the page is
// secure, but it uses insecure resources (e.g. images).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_MixedContent) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble with all the permissions displayed with 'Allow'
// set. All permissions will show regardless of its factory default value.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_AllowAllPermissions) {
  // Last updated in crrev.com/c/4150491.
  set_baseline("4150491");
  ShowAndVerifyUi();
}

// Shows the Page Info bubble with all the permissions displayed with 'Block'
// set. All permissions will show regardless of its factory default value.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_BlockAllPermissions) {
  // Last updated in crrev.com/c/4150491.
  set_baseline("4150491");
  ShowAndVerifyUi();
}

// Shows the Page Info bubble Safe Browsing warning after detecting the user has
// re-used an existing password on a site, e.g. due to phishing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_SavedPasswordReuse) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble Safe Browsing warning after detecting the
// signed-in syncing user has re-used an existing password on a site, e.g. due
// to phishing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_SignInSyncPasswordReuse) {
  ShowAndVerifyUi();
}
// Shows the Page Info bubble Safe Browsing warning after detecting the
// signed-in not syncing user has re-used an existing password on a site, e.g.
// due to phishing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_SignInNonSyncPasswordReuse) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble Safe Browsing warning after detecting the
// enterprise user has re-used an existing password on a site, e.g. due to
// phishing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_EnterprisePasswordReuse) {
  ShowAndVerifyUi();
}

class PageInfoBubbleViewAboutThisSiteDialogBrowserTest
    : public DialogBrowserTest {
 public:
  PageInfoBubbleViewAboutThisSiteDialogBrowserTest() {
    // TODO(crbug.com/1344787): Clean up when PageSpecificSiteDataDialog is
    // launched.
    feature_list_.InitWithFeatures({page_info::kPageInfoAboutThisSiteEn,
                                    page_info::kPageInfoAboutThisSiteNonEn},
                                   {page_info::kPageSpecificSiteDataDialog,
                                    page_info::kPageInfoCookiesSubpage});
  }

  void SetUpOnMainThread() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    host_resolver()->AddRule("*", "127.0.0.1");

    optimization_guide::OptimizationMetadata optimization_metadata;
    page_info::proto::AboutThisSiteMetadata metadata;
    auto* site_info = metadata.mutable_site_info();

    auto* description = site_info->mutable_description();
    description->set_description(
        "A domain used in illustrative examples in documents");
    description->set_lang("en_US");
    description->set_name("Example");
    description->mutable_source()->set_url("https://example.com");
    description->mutable_source()->set_label("Example source");

    auto* more_about = site_info->mutable_more_about();
    more_about->set_url("https://example.com/moreinfo");

    optimization_metadata.SetAnyMetadataForTesting(metadata);

    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide_decider->AddHintForTesting(
        GetUrl(kAboutThisSiteUrl), optimization_guide::proto::ABOUT_THIS_SITE,
        optimization_metadata);
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableCheckingUserPermissionsForTesting);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetUrl(kAboutThisSiteUrl)));
    OpenPageInfoBubble(browser());

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    std::u16string site_name = u"Example site";
    bubble_view->presenter_for_testing()->SetSiteNameForTesting(site_name);
    ASSERT_EQ(
        bubble_view->presenter_for_testing()->GetSiteNameOrAppNameToDisplay(),
        site_name);

    if (name == "AboutThisSite") {
      // No further action needed, default case.
    } else {
      NOTREACHED();
    }
  }

  GURL GetUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewAboutThisSiteDialogBrowserTest,
                       InvokeUi_AboutThisSite) {
  ShowAndVerifyUi();
}

class PageInfoBubbleViewPrivacySandboxDialogBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PageInfoBubbleViewPrivacySandboxDialogBrowserTest() {
    // TODO(crbug.com/1344787): Clean up when PageSpecificSiteDataDialog is
    // launched.
    feature_list_.InitWithFeatures(
        {GetParam() ? privacy_sandbox::kPrivacySandboxSettings4
                    : privacy_sandbox::kPrivacySandboxSettings3},
        {page_info::kPageSpecificSiteDataDialog,
         page_info::kPageInfoCookiesSubpage});
  }

  void SetUpOnMainThread() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name_with_param_suffix) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    const std::string& name =
        name_with_param_suffix.substr(0, name_with_param_suffix.find("/"));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl("a.test")));

    // TODO(crbug.com/1286276): It would be better to actually access the
    // topic through Javascript for an end-to-end test when the API is ready.
    auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame());

    pscs->OnTopicAccessed(
        url::Origin::Create(GURL("https://a.test")), false,
        privacy_sandbox::CanonicalTopic(
            browsing_topics::Topic(1),
            privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY));

    OpenPageInfoBubble(browser());

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    std::u16string site_name = u"Example site";
    bubble_view->presenter_for_testing()->SetSiteNameForTesting(site_name);
    ASSERT_EQ(
        bubble_view->presenter_for_testing()->GetSiteNameOrAppNameToDisplay(),
        site_name);

    if (name == "PrivacySandboxMain") {
      // No further action needed, default case.
    } else {
      CHECK_EQ(name, "PrivacySandboxSubpage");
      bubble_view->OpenAdPersonalizationPage();
    }
  }

  GURL GetUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_P(PageInfoBubbleViewPrivacySandboxDialogBrowserTest,
                       InvokeUi_PrivacySandboxMain) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PageInfoBubbleViewPrivacySandboxDialogBrowserTest,
                       InvokeUi_PrivacySandboxSubpage) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageInfoBubbleViewPrivacySandboxDialogBrowserTest,
                         testing::Bool());

class PageInfoBubbleViewHistoryDialogBrowserTest : public DialogBrowserTest {
 public:
  PageInfoBubbleViewHistoryDialogBrowserTest() {
    // TODO(crbug.com/1344787): Clean up when PageSpecificSiteDataDialog is
    // launched.
    feature_list_.InitWithFeatures({page_info::kPageInfoHistoryDesktop},
                                   {page_info::kPageSpecificSiteDataDialog,
                                    page_info::kPageInfoCookiesSubpage});
  }

  void SetUpOnMainThread() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    host_resolver()->AddRule("*", "127.0.0.1");

    base::Time yesterday = base::Time::Now() - base::Days(1);
    auto* history_service = HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    history_service->AddPage(GetUrl(kHistoryUrl), yesterday,
                             history::SOURCE_BROWSED);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kHistoryUrl)));
    OpenPageInfoBubble(browser());

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    std::u16string site_name = u"Example site";
    bubble_view->presenter_for_testing()->SetSiteNameForTesting(site_name);
    ASSERT_EQ(
        bubble_view->presenter_for_testing()->GetSiteNameOrAppNameToDisplay(),
        site_name);
  }

  GURL GetUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewHistoryDialogBrowserTest,
                       InvokeUi_History) {
  ShowAndVerifyUi();
}

class PageInfoBubbleViewCookiesSubpageBrowserTest : public DialogBrowserTest {
 public:
  PageInfoBubbleViewCookiesSubpageBrowserTest() {
    feature_list_.InitWithFeatures(
        {page_info::kPageSpecificSiteDataDialog,
         page_info::kPageInfoCookiesSubpage,
         privacy_sandbox::kPrivacySandboxFirstPartySetsUI},
        {});
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name_with_param_suffix) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    const std::string& name =
        name_with_param_suffix.substr(0, name_with_param_suffix.find("/"));

    constexpr char kCookiesSubpageFpsBlocked3pcAllowed[] =
        "CookiesSubpageFpsBlocked3pcAllowed";
    constexpr char kCookiesSubpageFpsAllowed3pcBlocked[] =
        "CookiesSubpageFpsAllowed3pcBlocked";
    constexpr char kCookiesSubpageFpsBlocked3pcBlocked[] =
        "CookiesSubpageFpsBlocked3pcBlocked";
    constexpr char kCookiesSubpageFpsAllowed3pcAllowed[] =
        "CookiesSubpageFpsAllowed3pcAllowed";
    constexpr char kCookiesSubpageFpsAllowed3pcEnforcedByPolicy[] =
        "CookiesSubpageFpsAllowed3pcEnforcedByPolicy";
    constexpr char kCookiesSubpageFpsAllowed3pcEnforcedByExtension[] =
        "CookiesSubpageFpsAllowed3pcEnforcedByExtension";
    constexpr char kCookiesSubpageFpsAllowed3pcEnforcedByCookieSetting[] =
        "CookiesSubpageFpsAllowed3pcEnforcedByCookieSetting";
    constexpr char kCookiesSubpageFpsManaged3pcAllowed[] =
        "CookiesSubpageFpsManaged3pcAllowed";

    const int blocked_sites_count = 8;
    const int allowed_sites_count = 9;
    const std::u16string kSiteOrigin = u"example.com";

    PageInfoUI::CookiesNewInfo cookie_info;
    cookie_info.allowed_sites_count = allowed_sites_count;
    cookie_info.blocked_sites_count = blocked_sites_count;
    cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
    cookie_info.status = CookieControlsStatus::kEnabled;
    // TODO(crbug.com/1346305): Add fps enforcement info when finished
    // implementing it.
    if (name == kCookiesSubpageFpsAllowed3pcAllowed ||
        name == kCookiesSubpageFpsAllowed3pcBlocked ||
        name == kCookiesSubpageFpsAllowed3pcEnforcedByPolicy ||
        name == kCookiesSubpageFpsAllowed3pcEnforcedByExtension ||
        name == kCookiesSubpageFpsAllowed3pcEnforcedByCookieSetting ||
        name == kCookiesSubpageFpsManaged3pcAllowed) {
      cookie_info.fps_info = {PageInfoUI::CookiesFpsInfo(kSiteOrigin)};

      // Otherwise it's by default false
      if (name == kCookiesSubpageFpsManaged3pcAllowed)
        cookie_info.fps_info->is_managed = true;

    }  // Otherwise by default it's null

    if (name == kCookiesSubpageFpsAllowed3pcBlocked ||
        name == kCookiesSubpageFpsBlocked3pcBlocked ||
        name == kCookiesSubpageFpsAllowed3pcEnforcedByPolicy ||
        name == kCookiesSubpageFpsAllowed3pcEnforcedByExtension ||
        name == kCookiesSubpageFpsAllowed3pcEnforcedByCookieSetting) {
      cookie_info.status = CookieControlsStatus::kEnabled;
      if (name == kCookiesSubpageFpsAllowed3pcEnforcedByPolicy) {
        cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
      } else if (name == kCookiesSubpageFpsAllowed3pcEnforcedByExtension) {
        cookie_info.enforcement =
            CookieControlsEnforcement::kEnforcedByExtension;
      } else if (name == kCookiesSubpageFpsAllowed3pcEnforcedByCookieSetting) {
        cookie_info.enforcement =
            CookieControlsEnforcement::kEnforcedByCookieSetting;
      }
    } else if (name == kCookiesSubpageFpsAllowed3pcAllowed ||
               name == kCookiesSubpageFpsBlocked3pcAllowed ||
               name == kCookiesSubpageFpsManaged3pcAllowed) {
      cookie_info.status = CookieControlsStatus::kDisabled;
    }

    // Open Page Info and wait for it to be fully initialized.
    base::RunLoop run_loop;
    GetPageInfoDialogCreatedCallbackForTesting() = run_loop.QuitClosure();
    OpenPageInfoBubble(browser());
    run_loop.Run();

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    auto* presenter = bubble_view->presenter_for_testing();
    EXPECT_TRUE(presenter);
    EXPECT_TRUE(presenter->ui_for_testing());

    // Open Cookies Subpage and wait for it to be fully initialized.
    base::RunLoop run_loop2;
    bubble_view->OpenCookiesPage();
    auto* cookies_subpage_content = static_cast<PageInfoCookiesContentView*>(
        bubble_view
            ->GetViewByID(PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW)
            ->children()[1]);
    cookies_subpage_content->SetInitializedCallbackForTesting(
        run_loop2.QuitClosure());
    run_loop2.Run();

    presenter->ui_for_testing()->SetCookieInfo(cookie_info);

    // Removing the focus as with tests run in parallel it causes different
    // outputs.
    bubble_view->GetFocusManager()->SetFocusedView(nullptr);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Show different sets of buttons in cookies subpage with different
// enforcements:

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_CookiesSubpageFpsBlocked3pcAllowed) {
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_CookiesSubpageFpsAllowed3pcBlocked) {
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_CookiesSubpageFpsBlocked3pcBlocked) {
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_CookiesSubpageFpsAllowed3pcAllowed) {
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_CookiesSubpageFpsAllowed3pcEnforcedByPolicy) {
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(
    PageInfoBubbleViewCookiesSubpageBrowserTest,
    InvokeUi_CookiesSubpageFpsAllowed3pcEnforcedByExtension) {
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(
    PageInfoBubbleViewCookiesSubpageBrowserTest,
    InvokeUi_CookiesSubpageFpsAllowed3pcEnforcedByCookieSetting) {
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_CookiesSubpageFpsManaged3pcAllowed) {
  ShowAndVerifyUi();
}

class PageInfoBubbleViewIsolatedWebAppBrowserTest : public DialogBrowserTest {
 public:
  PageInfoBubbleViewIsolatedWebAppBrowserTest() {
    // TODO(crbug.com/1344787): Clean up when PageSpecificSiteDataDialog is
    // launched.
    feature_list_.InitWithFeatures({}, {page_info::kPageSpecificSiteDataDialog,
                                        page_info::kPageInfoCookiesSubpage});
  }

  void SetUpOnMainThread() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetUrl(kIsolatedWebAppUrl)));
    OpenPageInfoBubble(browser());

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    std::u16string app_name = u"Google IWA";
    bubble_view->presenter_for_testing()->SetIsolatedWebAppNameForTesting(
        app_name);
    bubble_view->presenter_for_testing()->UpdateSecurityState();
    // For Isolated Web Apps, normal site name gets overridden by app name.
    ASSERT_EQ(
        bubble_view->presenter_for_testing()->GetSiteNameOrAppNameToDisplay(),
        app_name);
  }

  GURL GetUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Test renamed, as currently Skia Gold doesn't support resetting test
// expectation for tests run on windows.
// crbug.com/1403038
IN_PROC_BROWSER_TEST_F(
    PageInfoBubbleViewIsolatedWebAppBrowserTest,
    InvokeUi_AppNameIsDisplayedInsteadOfOriginForIsolatedWebApps_REV1) {
  ShowAndVerifyUi();
}
