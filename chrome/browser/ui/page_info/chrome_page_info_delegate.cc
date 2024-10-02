// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/page_info/core/features.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/settings/ash/app_management/app_management_uma.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/lookalikes/safety_tip_ui_helper.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"
#include "ui/events/event.h"
#else
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

ChromePageInfoDelegate::ChromePageInfoDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  sentiment_service_ =
      TrustSafetySentimentServiceFactory::GetForProfile(GetProfile());
#endif
  base::UmaHistogramBoolean("Security.PageInfo.AboutThisSiteLanguageSupported",
                            page_info::IsAboutThisSiteFeatureEnabled(
                                g_browser_process->GetApplicationLocale()));
}

Profile* ChromePageInfoDelegate::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

permissions::ObjectPermissionContextBase*
ChromePageInfoDelegate::GetChooserContext(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::USB_CHOOSER_DATA:
      return UsbChooserContextFactory::GetForProfile(GetProfile());
    case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
      if (base::FeatureList::IsEnabled(
              features::kWebBluetoothNewPermissionsBackend)) {
        return BluetoothChooserContextFactory::GetForProfile(GetProfile());
      }
      return nullptr;
    case ContentSettingsType::SERIAL_CHOOSER_DATA:
#if !BUILDFLAG(IS_ANDROID)
      return SerialChooserContextFactory::GetForProfile(GetProfile());
#else
      NOTREACHED();
      return nullptr;
#endif
    case ContentSettingsType::HID_CHOOSER_DATA:
#if !BUILDFLAG(IS_ANDROID)
      return HidChooserContextFactory::GetForProfile(GetProfile());
#else
      NOTREACHED();
      return nullptr;
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
safe_browsing::ChromePasswordProtectionService*
ChromePageInfoDelegate::GetChromePasswordProtectionService() const {
  return safe_browsing::ChromePasswordProtectionService::
      GetPasswordProtectionService(GetProfile());
}

safe_browsing::PasswordProtectionService*
ChromePageInfoDelegate::GetPasswordProtectionService() const {
  return GetChromePasswordProtectionService();
}

void ChromePageInfoDelegate::OnUserActionOnPasswordUi(
    safe_browsing::WarningAction action) {
  auto* chrome_password_protection_service =
      GetChromePasswordProtectionService();
  DCHECK(chrome_password_protection_service);

  chrome_password_protection_service->OnUserAction(
      web_contents_,
      chrome_password_protection_service
          ->reused_password_account_type_for_last_shown_warning(),
      safe_browsing::RequestOutcome::UNKNOWN,
      safe_browsing::LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      /*verdict_token=*/"", safe_browsing::WarningUIType::PAGE_INFO, action);
}

std::u16string ChromePageInfoDelegate::GetWarningDetailText() {
  auto* chrome_password_protection_service =
      GetChromePasswordProtectionService();

  // |password_protection_service| may be null in test.
  return chrome_password_protection_service
             ? chrome_password_protection_service->GetWarningDetailText(
                   chrome_password_protection_service
                       ->reused_password_account_type_for_last_shown_warning())
             : std::u16string();
}
#endif

permissions::PermissionResult ChromePageInfoDelegate::GetPermissionResult(
    blink::PermissionType permission,
    const url::Origin& origin) {
  content::PermissionResult permission_result =
      GetProfile()
          ->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(permission, origin);
  return permissions::PermissionUtil::ToPermissionResult(permission_result);
}

#if !BUILDFLAG(IS_ANDROID)
void ChromePageInfoDelegate::FocusWebContents() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  browser->ActivateContents(web_contents_);
}

absl::optional<std::u16string> ChromePageInfoDelegate::GetFpsOwner(
    const GURL& site_url) {
  return PrivacySandboxServiceFactory::GetForProfile(GetProfile())
      ->GetFirstPartySetOwnerForDisplay(site_url);
}

bool ChromePageInfoDelegate::IsFpsManaged() {
  return PrivacySandboxServiceFactory::GetForProfile(GetProfile())
      ->IsFirstPartySetsDataAccessManaged();
}

bool ChromePageInfoDelegate::CreateInfoBarDelegate() {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents_);
  if (infobar_manager) {
    PageInfoInfoBarDelegate::Create(infobar_manager);
    return true;
  }
  return false;
}

std::unique_ptr<content_settings::CookieControlsController>
ChromePageInfoDelegate::CreateCookieControlsController() {
  Profile* profile = GetProfile();
  return std::make_unique<content_settings::CookieControlsController>(
      CookieSettingsFactory::GetForProfile(profile),
      profile->IsOffTheRecord()
          ? CookieSettingsFactory::GetForProfile(profile->GetOriginalProfile())
          : nullptr);
}

std::u16string ChromePageInfoDelegate::GetWebAppShortName() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);

  if (browser && web_app::AppBrowserController::IsWebApp(browser)) {
    return browser->app_controller()->GetAppShortName();
  }

  return u"";
}

void ChromePageInfoDelegate::ShowSiteSettings(const GURL& site_url) {
  if (web_app::HandleAppManagementLinkClickedInPageInfo(web_contents_))
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  chrome::ShowSiteSettings(browser, site_url);
}

void ChromePageInfoDelegate::ShowCookiesSettings() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  chrome::ShowSettingsSubPage(browser, chrome::kCookieSettingsSubPage);
}

void ChromePageInfoDelegate::ShowAllSitesSettingsFilteredByFpsOwner(
    const std::u16string& fps_owner) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  chrome::ShowAllSitesSettingsFilteredByFpsOwner(browser,
                                                 base::UTF16ToUTF8(fps_owner));
}

void ChromePageInfoDelegate::OpenCookiesDialog() {
  FocusWebContents();
  TabDialogs::FromWebContents(web_contents_)->ShowCollectedCookies();
}

void ChromePageInfoDelegate::OpenCertificateDialog(
    net::X509Certificate* certificate) {
  gfx::NativeWindow top_window = web_contents_->GetTopLevelNativeWindow();
  DCHECK(certificate);
  DCHECK(top_window);

  FocusWebContents();
  ShowCertificateViewer(web_contents_, top_window, certificate);
}

void ChromePageInfoDelegate::OpenConnectionHelpCenterPage(
    const ui::Event& event) {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(chrome::kPageInfoHelpCenterURL), content::Referrer(),
      ui::DispositionFromEventFlags(event.flags(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB),
      ui::PAGE_TRANSITION_LINK, false));
}

void ChromePageInfoDelegate::OpenSafetyTipHelpCenterPage() {
  OpenHelpCenterFromSafetyTip(web_contents_);
}

void ChromePageInfoDelegate::OpenContentSettingsExceptions(
    ContentSettingsType content_settings_type) {
  chrome::ShowContentSettingsExceptionsForProfile(GetProfile(),
                                                  content_settings_type);
}

void ChromePageInfoDelegate::OnPageInfoActionOccurred(
    PageInfo::PageInfoAction action) {
  if (sentiment_service_) {
    if (action == PageInfo::PAGE_INFO_OPENED)
      sentiment_service_->PageInfoOpened();
    else
      sentiment_service_->InteractedWithPageInfo();
  }
}

void ChromePageInfoDelegate::OnUIClosing() {
  if (sentiment_service_)
    sentiment_service_->PageInfoClosed();
}
#endif

permissions::PermissionDecisionAutoBlocker*
ChromePageInfoDelegate::GetPermissionDecisionAutoblocker() {
  return PermissionDecisionAutoBlockerFactory::GetForProfile(GetProfile());
}

StatefulSSLHostStateDelegate*
ChromePageInfoDelegate::GetStatefulSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetForProfile(GetProfile());
}

HostContentSettingsMap* ChromePageInfoDelegate::GetContentSettings() {
  return HostContentSettingsMapFactory::GetForProfile(GetProfile());
}

bool ChromePageInfoDelegate::IsSubresourceFilterActivated(
    const GURL& site_url) {
  subresource_filter::SubresourceFilterContentSettingsManager*
      settings_manager =
          SubresourceFilterProfileContextFactory::GetForProfile(GetProfile())
              ->settings_manager();

  return settings_manager->GetSiteActivationFromMetadata(site_url);
}

bool ChromePageInfoDelegate::IsContentDisplayedInVrHeadset() {
  return vr::VrTabHelper::IsContentDisplayedInHeadset(web_contents_);
}

security_state::SecurityLevel ChromePageInfoDelegate::GetSecurityLevel() {
  if (security_state_for_tests_set_)
    return security_level_for_tests_;

  // This is a no-op if a SecurityStateTabHelper already exists for
  // |web_contents|.
  SecurityStateTabHelper::CreateForWebContents(web_contents_);

  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  return helper->GetSecurityLevel();
}

security_state::VisibleSecurityState
ChromePageInfoDelegate::GetVisibleSecurityState() {
  if (security_state_for_tests_set_)
    return visible_security_state_for_tests_;

  // This is a no-op if a SecurityStateTabHelper already exists for
  // |web_contents|.
  SecurityStateTabHelper::CreateForWebContents(web_contents_);

  auto* helper = SecurityStateTabHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  return *helper->GetVisibleSecurityState();
}

std::unique_ptr<content_settings::PageSpecificContentSettings::Delegate>
ChromePageInfoDelegate::GetPageSpecificContentSettingsDelegate() {
  auto delegate = std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
      web_contents_);
  return std::move(delegate);
}

#if BUILDFLAG(IS_ANDROID)
const std::u16string ChromePageInfoDelegate::GetClientApplicationName() {
  return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
}
#endif

void ChromePageInfoDelegate::SetSecurityStateForTests(
    security_state::SecurityLevel security_level,
    security_state::VisibleSecurityState visible_security_state) {
  security_state_for_tests_set_ = true;
  security_level_for_tests_ = security_level;
  visible_security_state_for_tests_ = visible_security_state;
}
