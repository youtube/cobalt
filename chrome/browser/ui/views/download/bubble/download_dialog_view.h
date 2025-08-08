// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_primary_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/link.h"

class Browser;

// This view represents the 'main view' that is shown when the user clicks on
// the download toolbar button. Unlike the partial view, it does not
// automatically close. It also has a header and close button, as well as a
// footer with a link to chrome://downloads.
class DownloadDialogView : public DownloadBubblePrimaryView {
 public:
  METADATA_HEADER(DownloadDialogView);
  DownloadDialogView(const DownloadDialogView&) = delete;
  DownloadDialogView& operator=(const DownloadDialogView&) = delete;

  DownloadDialogView(
      base::WeakPtr<Browser> browser,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      const DownloadBubbleRowListViewInfo& info);
  ~DownloadDialogView() override;

 private:
  // DownloadBubblePrimaryView
  base::StringPiece GetVisibleTimeHistogramName() const override;

  void CloseBubble();
  void ShowAllDownloads();
  void AddHeader();
  void AddFooter();

  base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler_;
  base::WeakPtr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_
