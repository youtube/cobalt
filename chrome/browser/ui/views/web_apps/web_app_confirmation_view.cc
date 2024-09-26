// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_confirmation_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/widget/widget.h"

namespace {

bool g_auto_accept_web_app_for_testing = false;
bool g_auto_check_open_in_window_for_testing = false;
const char* g_title_to_use_for_app = nullptr;

bool ShowRadioButtons() {
  // This UI is only for prototyping and is not intended for shipping.
  DCHECK_EQ(features::kDesktopPWAsTabStripSettings.default_state,
            base::FEATURE_DISABLED_BY_DEFAULT);
  return base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip) &&
         base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripSettings);
}

// When pre-populating the shortcut name field (using the web app title) we
// should try to make some effort to not suggest things we know work extra
// poorly when used as filenames in the OS. This is especially problematic when
// creating shortcuts to pages that have no title, because then the URL of the
// page will be used as a suggestion, and (if accepted by the user) the shortcut
// name will look really weird. For example, MacOS will convert a colon (:) to a
// forward-slash (/), and Windows will convert the colons to spaces. MacOS even
// goes a step further and collapses multiple consecutive forward-slashes in
// localized names into a single forward-slash. That means, using 'https://foo'
// as an example, a shortcut with a display name of 'https/foo' is created on
// MacOS and 'https   foo' on Windows. By stripping away the schema, we will be
// greatly reducing the frequency of shortcuts having weird names. Note: This
// does not affect user's ability to use URLs as a shortcut name (which would
// result in a weird filename), it only restricts what we suggest as titles.
std::u16string NormalizeSuggestedAppTitle(const std::u16string& title) {
  std::u16string normalized = title;
  if (base::StartsWith(normalized, u"https://")) {
    normalized = normalized.substr(8);
  }
  if (base::StartsWith(normalized, u"http://")) {
    normalized = normalized.substr(7);
  }
  return normalized;
}

}  // namespace

WebAppConfirmationView::~WebAppConfirmationView() {}

WebAppConfirmationView::WebAppConfirmationView(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    chrome::AppInstallationAcceptanceCallback callback)
    : web_app_info_(std::move(web_app_info)), callback_(std::move(callback)) {
  DCHECK(web_app_info_);
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // Define the table layout.
  constexpr int textfield_width = 320;
  auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      ->AddColumn(views::LayoutAlignment::kStretch,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        layout_provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, textfield_width, 0)
      .AddRows(1, views::TableLayout::kFixedSize)
      .AddPaddingRow(
          views::TableLayout::kFixedSize,
          layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL))
      .AddRows(ShowRadioButtons() ? 3 : 1, views::TableLayout::kFixedSize);

  gfx::Size image_size(web_app::kWebAppIconSmall, web_app::kWebAppIconSmall);
  gfx::ImageSkia image(
      std::make_unique<WebAppInfoImageSource>(web_app::kWebAppIconSmall,
                                              web_app_info_->icon_bitmaps.any),
      image_size);

  // Builds the header row child views.
  auto builder =
      views::Builder<WebAppConfirmationView>(this)
          .SetButtonLabel(
              ui::DIALOG_BUTTON_OK,
              l10n_util::GetStringUTF16(IDS_CREATE_SHORTCUTS_BUTTON_LABEL))
          .SetModalType(ui::MODAL_TYPE_CHILD)
          .SetTitle(IDS_ADD_TO_OS_LAUNCH_SURFACE_BUBBLE_TITLE)
          .set_margins(layout_provider->GetDialogInsetsForContentType(
              views::DialogContentType::kControl,
              views::DialogContentType::kText))
          .AddChildren(views::Builder<views::ImageView>()
                           .SetImageSize(image_size)
                           .SetImage(image),
                       views::Builder<views::Textfield>()
                           .CopyAddressTo(&title_tf_)
                           .SetText(NormalizeSuggestedAppTitle(
                               g_title_to_use_for_app != nullptr
                                   ? base::ASCIIToUTF16(g_title_to_use_for_app)
                                   : web_app_info_->title))
                           .SetAccessibleName(l10n_util::GetStringUTF16(
                               IDS_BOOKMARK_APP_AX_BUBBLE_NAME_LABEL))
                           .SetController(this));

  const auto display_mode = web_app_info_->user_display_mode;
  // Build the content child views.
  if (ShowRadioButtons()) {
    constexpr int kRadioGroupId = 0;
    builder.AddChildren(
        views::Builder<views::View>(),  // Skip the first column.
        views::Builder<views::RadioButton>()
            .CopyAddressTo(&open_as_tab_radio_)
            .SetText(
                l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TAB))
            .SetGroup(kRadioGroupId)
            .SetChecked(display_mode ==
                        web_app::mojom::UserDisplayMode::kBrowser),
        views::Builder<views::View>(),  // Column skip.
        views::Builder<views::RadioButton>()
            .CopyAddressTo(&open_as_window_radio_)
            .SetText(l10n_util::GetStringUTF16(
                IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW))
            .SetGroup(kRadioGroupId)
            .SetChecked(
                display_mode != web_app::mojom::UserDisplayMode::kBrowser &&
                display_mode != web_app::mojom::UserDisplayMode::kTabbed),
        views::Builder<views::View>(),  // Column skip.
        views::Builder<views::RadioButton>()
            .CopyAddressTo(&open_as_tabbed_window_radio_)
            .SetText(l10n_util::GetStringUTF16(
                IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TABBED_WINDOW))
            .SetGroup(kRadioGroupId)
            .SetChecked(display_mode ==
                        web_app::mojom::UserDisplayMode::kTabbed));
  } else {
    builder.AddChildren(
        views::Builder<views::View>(),  // Column skip.
        views::Builder<views::Checkbox>()
            .CopyAddressTo(&open_as_window_checkbox_)
            .SetText(l10n_util::GetStringUTF16(
                IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW))
            .SetChecked(display_mode !=
                        web_app::mojom::UserDisplayMode::kBrowser));
  }

  std::move(builder).BuildChildren();

  if (g_auto_check_open_in_window_for_testing) {
    if (ShowRadioButtons())
      open_as_window_radio_->SetChecked(true);
    else
      open_as_window_checkbox_->SetChecked(true);
  }

  title_tf_->SelectAll(true);
}

views::View* WebAppConfirmationView::GetInitiallyFocusedView() {
  return title_tf_;
}

bool WebAppConfirmationView::ShouldShowCloseButton() const {
  return false;
}

void WebAppConfirmationView::WindowClosing() {
  if (callback_) {
    DCHECK(web_app_info_);
    std::move(callback_).Run(false, std::move(web_app_info_));
  }
}

bool WebAppConfirmationView::Accept() {
  DCHECK(web_app_info_);
  web_app_info_->title = GetTrimmedTitle();
  if (ShowRadioButtons()) {
    if (open_as_tabbed_window_radio_->GetChecked()) {
      web_app_info_->user_display_mode =
          web_app::mojom::UserDisplayMode::kTabbed;
    } else {
      web_app_info_->user_display_mode =
          open_as_window_radio_->GetChecked()
              ? web_app::mojom::UserDisplayMode::kStandalone
              : web_app::mojom::UserDisplayMode::kBrowser;
    }
  } else {
    web_app_info_->user_display_mode =
        open_as_window_checkbox_->GetChecked()
            ? web_app::mojom::UserDisplayMode::kStandalone
            : web_app::mojom::UserDisplayMode::kBrowser;
  }
  std::move(callback_).Run(true, std::move(web_app_info_));
  return true;
}

bool WebAppConfirmationView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? !GetTrimmedTitle().empty() : true;
}

void WebAppConfirmationView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(title_tf_, sender);
  DialogModelChanged();
}

std::u16string WebAppConfirmationView::GetTrimmedTitle() const {
  std::u16string title(title_tf_->GetText());
  base::TrimWhitespace(title, base::TRIM_ALL, &title);
  return title;
}

BEGIN_METADATA(WebAppConfirmationView, views::DialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, TrimmedTitle)
END_METADATA

namespace chrome {

void ShowWebAppInstallDialog(content::WebContents* web_contents,
                             std::unique_ptr<WebAppInstallInfo> web_app_info,
                             AppInstallationAcceptanceCallback callback) {
  auto* dialog =
      new WebAppConfirmationView(std::move(web_app_info), std::move(callback));
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);

  if (g_auto_accept_web_app_for_testing) {
    dialog->AcceptDialog();
  }
}

void SetAutoAcceptWebAppDialogForTesting(bool auto_accept,
                                         bool auto_open_in_window) {
  g_auto_accept_web_app_for_testing = auto_accept;
  g_auto_check_open_in_window_for_testing = auto_open_in_window;
}

void SetOverrideTitleForTesting(const char* title_to_use) {
  g_title_to_use_for_app = title_to_use;
}

}  // namespace chrome
