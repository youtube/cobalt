// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PAGE_CONTEXT_FETCHER_H_
#define CHROME_BROWSER_GLIC_GLIC_PAGE_CONTEXT_FETCHER_H_

#include <cstddef>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_tab_data.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents_observer.h"
#include "pdf/mojom/pdf.mojom-forward.h"
#include "third_party/skia/include/core/SkSize.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

namespace glic {

// Coordinates fetching multiple types of page context.
class GlicPageContextFetcher : public content::WebContentsObserver {
 public:
  GlicPageContextFetcher();
  ~GlicPageContextFetcher() override;

  // Fetches the page context. May be called at most once.
  // TODO(harringtond): This API is error-prone, consider making this a static
  // function so that Fetch() can't be called multiple times.
  void Fetch(
      FocusedTabData focused_tab_data,
      const mojom::GetTabContextOptions& options,
      glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback);

  // content::WebContentsObserver.
  void PrimaryPageChanged(content::Page& page) override;

 private:
  void GetTabScreenshot(content::WebContents& web_contents);
  void ReceivedViewportBitmap(const SkBitmap& bitmap);
  void RecievedJpegScreenshot(
      std::optional<std::vector<uint8_t>> screenshot_jpeg_data);
  void ReceivedInnerText(
      std::unique_ptr<content_extraction::InnerTextResult> result);
  void ReceivedAnnotatedPageContent(
      std::optional<optimization_guide::proto::AnnotatedPageContent>);
  void RunCallbackIfComplete();
  void ReceivedPdfBytes(pdf::mojom::PdfListener_GetPdfBytesStatus status,
                        const std::vector<uint8_t>& pdf_bytes,
                        uint32_t page_count);

  base::WeakPtr<GlicPageContextFetcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback_;

  // Intermediate results:

  // Whether work is complete for each task, does not imply success.
  bool screenshot_done_ = false;
  bool inner_text_done_ = false;
  bool pdf_done_ = false;
  bool annotated_page_content_done_ = false;
  // Whether the primary page has changed since context fetching began.
  bool primary_page_changed_ = false;
  url::Origin pdf_origin_;
  std::optional<std::vector<uint8_t>> screenshot_jpeg_data_;
  SkISize screenshot_dimensions_;
  glic::mojom::ScreenshotPtr screenshot_;
  std::unique_ptr<content_extraction::InnerTextResult> inner_text_result_;
  std::vector<uint8_t> pdf_bytes_;
  std::optional<pdf::mojom::PdfListener_GetPdfBytesStatus> pdf_status_;
  std::optional<optimization_guide::proto::AnnotatedPageContent>
      annotated_page_content_;
  base::TimeTicks start_time_;

  base::WeakPtrFactory<GlicPageContextFetcher> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_PAGE_CONTEXT_FETCHER_H_
