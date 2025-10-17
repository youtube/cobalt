// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

PageInfoSecurityContentView::PageInfoSecurityContentView(
    PageInfo* presenter,
    bool is_standalone_page)
    : presenter_(presenter) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  security_view_ = AddChildView(std::make_unique<SecurityInformationView>(
      ChromeLayoutProvider::Get()
          ->GetInsetsMetric(views::INSETS_DIALOG)
          .left()));

  if (is_standalone_page) {
    const int bottom_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
    // The last view in the subpage is a RichHoverButton, which overrides the
    // bottom dialog inset in favor of its own.
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, bottom_margin, 0));

    presenter_->InitializeUiState(this, base::DoNothing());
  }
}

PageInfoSecurityContentView::~PageInfoSecurityContentView() = default;

void PageInfoSecurityContentView::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
      GetSecurityDescription(identity_info);
  security_description_type_ = security_description->type;

  const int icon_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);
  if (security_description->summary_style == SecuritySummaryColor::RED) {
    if (identity_info.safe_browsing_status ==
            PageInfo::SAFE_BROWSING_STATUS_MALWARE ||
        identity_info.safe_browsing_status ==
            PageInfo::SAFE_BROWSING_STATUS_SOCIAL_ENGINEERING ||
        identity_info.safe_browsing_status ==
            PageInfo::SAFE_BROWSING_STATUS_UNWANTED_SOFTWARE) {
      security_view_->SetIcon(ui::ImageModel::FromVectorIcon(
          vector_icons::kDangerousIcon, ui::kColorAlertHighSeverity,
          icon_size));
    } else {
      security_view_->SetIcon(ui::ImageModel::FromVectorIcon(
          vector_icons::kNotSecureWarningIcon, ui::kColorAlertHighSeverity,
          icon_size));
    }
    security_view_->SetSummary(security_description->summary, STYLE_RED);
  } else if (security_description->summary_style ==
                 SecuritySummaryColor::ENTERPRISE &&
             (identity_info.safe_browsing_status ==
                  PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_WARN ||
              identity_info.safe_browsing_status ==
                  PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_BLOCK)) {
    security_view_->SetIcon(
        PageInfoViewFactory::GetImageModel(vector_icons::kBusinessIcon));
    security_view_->SetSummary(security_description->summary,
                               views::style::STYLE_BODY_3_MEDIUM);
  } else {
    security_view_->SetIcon(PageInfoViewFactory::GetConnectionSecureIcon());
    security_view_->SetSummary(security_description->summary,
                               views::style::STYLE_BODY_3_MEDIUM);
  }
  security_view_->SetDetailsWithLearnMore(
      security_description->details,
      base::BindRepeating(&PageInfoSecurityContentView::SecurityDetailsClicked,
                          base::Unretained(this)));

  // The "re-enable warnings" button is shown if the user has bypassed SSL
  // error interstitials or HTTP warning interstitials (with HTTPS-First Mode
  // enabled) on this site.
  if (identity_info.show_ssl_decision_revoke_button) {
    security_view_->AddResetDecisionsLabel(
        base::BindRepeating(&PageInfoSecurityContentView::ResetDecisionsClicked,
                            base::Unretained(this)));
  }

  if (identity_info.certificate) {
    certificate_ = identity_info.certificate;

    // Show information about the page's certificate.
    // The text of link to the Certificate Viewer varies depending on the
    // validity of the Certificate.
    const bool valid_identity =
        (identity_info.identity_status != PageInfo::SITE_IDENTITY_STATUS_ERROR);
    std::u16string tooltip;
    if (valid_identity) {
      tooltip = l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_CERTIFICATE_VALID_LINK_TOOLTIP,
          base::UTF8ToUTF16(certificate_->issuer().GetDisplayName()));
    } else {
      tooltip = l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_CERTIFICATE_INVALID_LINK_TOOLTIP);
    }

    // Add the Certificate Section.
    const ui::ImageModel icon =
        base::FeatureList::IsEnabled(net::features::kVerifyQWACs)
            ? PageInfoViewFactory::GetImageModel(vector_icons::kStickyNote2Icon)
            : (valid_identity ? PageInfoViewFactory::GetImageModel(
                                    vector_icons::kCertificateIcon)
                              : PageInfoViewFactory::GetImageModel(
                                    vector_icons::kCertificateOffIcon));
    const int title_id = (valid_identity && !base::FeatureList::IsEnabled(
                                                net::features::kVerifyQWACs))
                             ? IDS_PAGE_INFO_CERTIFICATE_IS_VALID
                             : IDS_PAGE_INFO_CERTIFICATE_DETAILS;

    std::u16string subtitle_text;
    // Only show the EV certificate details if there are no errors or mixed
    // content.
    if (identity_info.identity_status ==
            PageInfo::SITE_IDENTITY_STATUS_EV_CERT &&
        identity_info.connection_status ==
            PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED) {
      // An EV cert is required to have an organization name and a country.
      if (!certificate_->subject().organization_names.empty() &&
          !certificate_->subject().country_name.empty()) {
        subtitle_text = l10n_util::GetStringFUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
            base::UTF8ToUTF16(certificate_->subject().organization_names[0]),
            base::UTF8ToUTF16(certificate_->subject().country_name));
      }
    }

    if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
      // If QWAC info line has been added previously, remove the old one before
      // recreating it. Re-adding it bumps it to the bottom of the
      // container, but its unlikely that the user will notice, since other
      // things are changing too.
      if (one_qwac_view_) {
        RemoveChildViewT(one_qwac_view_.get());
      }
      if (identity_info.identity_status ==
              PageInfo::SITE_IDENTITY_STATUS_1QWAC_CERT &&
          identity_info.connection_status ==
              PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED) {
        one_qwac_view_ = AddChildView(std::make_unique<SecurityInformationView>(
            ChromeLayoutProvider::Get()
                ->GetInsetsMetric(views::INSETS_DIALOG)
                .left()));
        one_qwac_view_->SetSummary(
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_QWAC_STATUS_TITLE),
            views::style::STYLE_BODY_3_MEDIUM);
        // TODO(crbug.com/392934324): import & use correct icon.
        one_qwac_view_->SetIcon(
            PageInfoViewFactory::GetImageModel(vector_icons::kCertificateIcon));
        if (certificate_ &&
            !certificate_->subject().organization_names.empty() &&
            !certificate_->subject().country_name.empty()) {
          one_qwac_view_->SetDetails(l10n_util::GetStringFUTF16(
              IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
              base::UTF8ToUTF16(certificate_->subject().organization_names[0]),
              base::UTF8ToUTF16(certificate_->subject().country_name)));
        }
      }
    }

    // If the certificate button has been added previously, remove the old one
    // before recreating it. Re-adding it bumps it to the bottom of the
    // container, but its unlikely that the user will notice, since other
    // things are changing too.
    if (certificate_button_) {
      RemoveChildViewT(certificate_button_.get());
    }
    certificate_button_ =
        AddChildViewRaw(std::make_unique<RichHoverButton>(
                            base::BindRepeating(
                                [](PageInfoSecurityContentView* view) {
                                  view->presenter_->OpenCertificateDialog(
                                      view->certificate_.get());
                                },
                                this),
                            icon, l10n_util::GetStringUTF16(title_id),
                            subtitle_text, PageInfoViewFactory::GetLaunchIcon())
                            .release());
    certificate_button_->SetID(
        PageInfoViewFactory::
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER);
    certificate_button_->SetTooltipText(tooltip);
    certificate_button_->SetTitleTextStyleAndColor(
        views::style::STYLE_BODY_3_MEDIUM, kColorPageInfoForeground);
    certificate_button_->SetSubtitleTextStyleAndColor(
        views::style::STYLE_BODY_4, kColorPageInfoSubtitleForeground);
  }

  if (identity_info.show_change_password_buttons) {
    security_view_->AddPasswordReuseButtons(
        identity_info.safe_browsing_status,
        base::BindRepeating(
            [](PageInfoSecurityContentView* view) {
              view->presenter_->OnChangePasswordButtonPressed();
            },
            this),
        base::BindRepeating(
            [](PageInfoSecurityContentView* view) {
              view->GetWidget()->Close();
              view->presenter_->OnAllowlistPasswordReuseButtonPressed();
            },
            this));
  }
  PreferredSizeChanged();
}

void PageInfoSecurityContentView::ResetDecisionsClicked() {
  presenter_->OnRevokeSSLErrorBypassButtonPressed();
  GetWidget()->Close();
}

void PageInfoSecurityContentView::SecurityDetailsClicked(
    const ui::Event& event) {
  if (security_description_type_ == SecurityDescriptionType::SAFETY_TIP) {
    presenter_->OpenSafetyTipHelpCenterPage();
  } else if (security_description_type_ ==
             SecurityDescriptionType::SAFE_BROWSING) {
    presenter_->OpenSafeBrowsingHelpCenterPage(event);
  } else {
    presenter_->OpenConnectionHelpCenterPage(event);
  }
}

BEGIN_METADATA(PageInfoSecurityContentView)
END_METADATA
