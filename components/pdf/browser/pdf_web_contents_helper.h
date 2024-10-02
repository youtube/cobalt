// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/mojom/pdf.mojom.h"
#include "ui/touch_selection/selection_event_type.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"

namespace content {
class RenderWidgetHost;
class WebContents;
}

namespace pdf {

class PDFWebContentsHelperClient;
class PDFWebContentsHelperTest;

// Per-WebContents class to handle PDF messages.
class PDFWebContentsHelper
    : public content::WebContentsUserData<PDFWebContentsHelper>,
      public content::RenderWidgetHostObserver,
      public mojom::PdfService,
      public ui::TouchSelectionControllerClient,
      public ui::TouchSelectionMenuClient,
      public content::TouchSelectionControllerClientManager::Observer {
 public:
  PDFWebContentsHelper(const PDFWebContentsHelper&) = delete;
  PDFWebContentsHelper& operator=(const PDFWebContentsHelper&) = delete;

  ~PDFWebContentsHelper() override;

  static void CreateForWebContentsWithClient(
      content::WebContents* contents,
      std::unique_ptr<PDFWebContentsHelperClient> client);
  static void BindPdfService(
      mojo::PendingAssociatedReceiver<mojom::PdfService> pdf_service,
      content::RenderFrameHost* rfh);

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  // ui::TouchSelectionControllerClient:
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override {}
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  void OnDragUpdate(const ui::TouchSelectionDraggable::Type type,
                    const gfx::PointF& position) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;
  void DidScroll() override;

  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void RunContextMenu() override;
  bool ShouldShowQuickMenu() override;
  std::u16string GetSelectedText() override;

  // ui::TouchSelectionControllerClientManager::Observer:
  void OnManagerWillDestroy(
      content::TouchSelectionControllerClientManager* manager) override;

  // pdf::mojom::PdfService:
  void SetListener(mojo::PendingRemote<mojom::PdfListener> listener) override;
  void HasUnsupportedFeature() override;
  void SaveUrlAs(const GURL& url,
                 network::mojom::ReferrerPolicy policy) override;
  void UpdateContentRestrictions(int32_t content_restrictions) override;
  void SelectionChanged(const gfx::PointF& left,
                        int32_t left_height,
                        const gfx::PointF& right,
                        int32_t right_height) override;
  void SetPluginCanSave(bool can_save) override;

 private:
  friend class content::WebContentsUserData<PDFWebContentsHelper>;
  friend class PDFWebContentsHelperTest;

  PDFWebContentsHelper(content::WebContents* web_contents,
                       std::unique_ptr<PDFWebContentsHelperClient> client);

  void InitTouchSelectionClientManager();
  gfx::PointF ConvertFromRoot(const gfx::PointF& point_f);
  gfx::PointF ConvertToRoot(const gfx::PointF& point_f);
  gfx::PointF ConvertHelper(const gfx::PointF& point_f, float scale);

  content::RenderFrameHostReceiverSet<mojom::PdfService> pdf_service_receivers_;
  std::unique_ptr<PDFWebContentsHelperClient> const client_;
  raw_ptr<content::TouchSelectionControllerClientManager>
      touch_selection_controller_client_manager_ = nullptr;

  // The `RenderWidgetHost` associated to the frame containing the PDF plugin.
  // This should be null until the plugin is known to have been created; the
  // signal comes from `SetListener()`.
  raw_ptr<content::RenderWidgetHost> pdf_rwh_ = nullptr;

  // Latest selection bounds received from PDFium.
  gfx::PointF selection_left_;
  int32_t selection_left_height_ = 0;
  gfx::PointF selection_right_;
  int32_t selection_right_height_ = 0;
  bool has_selection_ = false;

  mojo::Remote<mojom::PdfListener> remote_pdf_client_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_H_
