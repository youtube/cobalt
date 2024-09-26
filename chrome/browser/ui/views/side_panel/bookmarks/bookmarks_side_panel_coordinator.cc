// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/bookmarks/bookmarks_side_panel_coordinator.h"

#include "base/functional/callback.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

BookmarksSidePanelCoordinator::BookmarksSidePanelCoordinator(Browser* browser)
    : BrowserUserData<BookmarksSidePanelCoordinator>(*browser) {}

BookmarksSidePanelCoordinator::~BookmarksSidePanelCoordinator() = default;

void BookmarksSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kBookmarks,
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE),
      ui::ImageModel::FromVectorIcon(omnibox::kStarIcon, ui::kColorIcon),
      base::BindRepeating(
          &BookmarksSidePanelCoordinator::CreateBookmarksWebView,
          base::Unretained(this))));
}

std::unique_ptr<views::View>
BookmarksSidePanelCoordinator::CreateBookmarksWebView() {
  auto bookmarks_web_view =
      std::make_unique<SidePanelWebUIViewT<BookmarksSidePanelUI>>(
          base::RepeatingClosure(), base::RepeatingClosure(),
          std::make_unique<BubbleContentsWrapperT<BookmarksSidePanelUI>>(
              GURL(chrome::kChromeUIBookmarksSidePanelURL),
              GetBrowser().profile(), IDS_BOOKMARK_MANAGER_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false));
  bookmarks_web_view->SetProperty(views::kElementIdentifierKey,
                                  kBookmarkSidePanelWebViewElementId);
  extensions::BookmarkManagerPrivateDragEventRouter::CreateForWebContents(
      bookmarks_web_view.get()->contents_wrapper()->web_contents());
  return bookmarks_web_view;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BookmarksSidePanelCoordinator);
