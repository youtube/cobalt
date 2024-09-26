// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing_hub/sharing_hub_model.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feed/web_feed_tab_helper.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/feed/feed_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/vector_icons.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace sharing_hub {

SharingHubAction::SharingHubAction(int command_id,
                                   std::u16string title,
                                   const gfx::VectorIcon* icon,
                                   std::string feature_name_for_metrics,
                                   int announcement_id)
    : command_id(command_id),
      title(title),
      icon(icon),
      feature_name_for_metrics(feature_name_for_metrics),
      announcement_id(announcement_id) {}

SharingHubAction::SharingHubAction(const SharingHubAction& src) = default;
SharingHubAction& SharingHubAction::operator=(const SharingHubAction& src) =
    default;
SharingHubAction::SharingHubAction(SharingHubAction&& src) = default;
SharingHubAction& SharingHubAction::operator=(SharingHubAction&& src) = default;

SharingHubModel::SharingHubModel(content::BrowserContext* context)
    : context_(context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PopulateFirstPartyActions();
}

SharingHubModel::~SharingHubModel() = default;

void SharingHubModel::GetFirstPartyActionList(
    content::WebContents* web_contents,
    std::vector<SharingHubAction>* list) {
  for (const auto& action : first_party_action_list_) {
    if (action.command_id == IDC_SEND_TAB_TO_SELF) {
      if (DoShowSendTabToSelfForWebContents(web_contents)) {
        list->push_back(action);
      }
    } else if (action.command_id == IDC_QRCODE_GENERATOR) {
      if (qrcode_generator::QRCodeGeneratorBubbleController::
              IsGeneratorAvailable(web_contents->GetLastCommittedURL())) {
        list->push_back(action);
      }
    } else if (action.command_id == IDC_FOLLOW) {
      TabWebFeedFollowState follow_state =
          feed::WebFeedTabHelper::GetFollowState(web_contents);
      if (follow_state == TabWebFeedFollowState::kNotFollowed)
        list->push_back(action);
    } else if (action.command_id == IDC_UNFOLLOW) {
      TabWebFeedFollowState follow_state =
          feed::WebFeedTabHelper::GetFollowState(web_contents);
      if (follow_state == TabWebFeedFollowState::kFollowed)
        list->push_back(action);
    } else if (action.command_id == IDC_SAVE_PAGE) {
      if (chrome::CanSavePage(
              chrome::FindBrowserWithWebContents(web_contents))) {
        list->push_back(action);
      }
    } else {
      list->push_back(action);
    }
  }
}

void SharingHubModel::PopulateFirstPartyActions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  first_party_action_list_.emplace_back(
      IDC_COPY_URL, l10n_util::GetStringUTF16(IDS_SHARING_HUB_COPY_LINK_LABEL),
      &kCopyIcon, "SharingHubDesktop.CopyURLSelected", IDS_LINK_COPIED);

  if (DesktopScreenshotsFeatureEnabled(context_)) {
    first_party_action_list_.emplace_back(
        IDC_SHARING_HUB_SCREENSHOT,
        l10n_util::GetStringUTF16(IDS_SHARING_HUB_SCREENSHOT_LABEL),
        &kSharingHubScreenshotIcon, "SharingHubDesktop.ScreenshotSelected", 0);
  }

  first_party_action_list_.emplace_back(
      IDC_SEND_TAB_TO_SELF,
      l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_SEND_TAB_TO_SELF),
      &kLaptopAndSmartphoneIcon, "SharingHubDesktop.SendTabToSelfSelected", 0);

  first_party_action_list_.emplace_back(
      IDC_QRCODE_GENERATOR,
      l10n_util::GetStringUTF16(IDS_SHARING_HUB_GENERATE_QR_CODE_LABEL),
      &kQrcodeGeneratorIcon, "SharingHubDesktop.QRCodeSelected", 0);

  if (media_router::MediaRouterEnabled(context_)) {
    first_party_action_list_.emplace_back(
        IDC_ROUTE_MEDIA,
        l10n_util::GetStringUTF16(IDS_SHARING_HUB_MEDIA_ROUTER_LABEL),
        &vector_icons::kMediaRouterIdleIcon, "SharingHubDesktop.CastSelected",
        0);
  }

  if (base::FeatureList::IsEnabled(feed::kWebUiFeed)) {
    first_party_action_list_.emplace_back(
        IDC_FOLLOW, l10n_util::GetStringUTF16(IDS_SHARING_HUB_FOLLOW_LABEL),
        &kAddIcon, "SharingHubDesktop.FollowSelected", 0);
    first_party_action_list_.emplace_back(
        IDC_UNFOLLOW,
        l10n_util::GetStringUTF16(IDS_SHARING_HUB_FOLLOWING_LABEL),
        &views::kMenuCheckIcon, "SharingHubDesktop.UnfollowSelected", 0);
  }

  first_party_action_list_.emplace_back(
      IDC_SAVE_PAGE, l10n_util::GetStringUTF16(IDS_SHARING_HUB_SAVE_PAGE_LABEL),
      &kSavePageIcon, "SharingHubDesktop.SavePageSelected", 0);
}

bool SharingHubModel::DoShowSendTabToSelfForWebContents(
    content::WebContents* web_contents) {
  return send_tab_to_self::ShouldDisplayEntryPoint(web_contents);
}

}  // namespace sharing_hub
