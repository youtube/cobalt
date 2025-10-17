// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_

#include "chrome/browser/lens/core/mojom/lens_side_panel.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/tabs/public/tab_interface.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

class LensSearchController;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace content_extraction {
struct InnerTextResult;
}  // namespace content_extraction

namespace optimization_guide {
struct AIPageContentResult;
}  // namespace optimization_guide

using GetIsContextualSearchboxCallback =
    lens::mojom::LensSidePanelPageHandler::GetIsContextualSearchboxCallback;

namespace lens {

class LensSearchboxController;

// Callback type alias for page content bytes retrieved. Multiple pieces and
// types of content may be retrieved and returned in `page_contents`.
// `primary_content_type` is the main type used in the request flow and used to
// determine request params and whether updated requests need to be sent.
// `pdf_page_count` is the number of pages in the document being retrieved, not
// necessarily the number of pages in `bytes`. For example, if the document is a
// PDF, `pdf_page_count` is the number of pages in the PDF, while `bytes` could
// be empty because the PDF is too large.
using PageContentRetrievedCallback =
    base::OnceCallback<void(std::vector<lens::PageContent> page_contents,
                            lens::MimeType primary_content_type,
                            std::optional<uint32_t> pdf_page_count)>;

// Callback type alias for retrieving the text from the PDF pages one by one.
using PdfPartialPageTextRetrievedCallback =
    base::OnceCallback<void(std::vector<std::u16string> pdf_pages_text)>;

// Callback type alias for when the page context has been updated. This is used
// to allow requests to be made after the latest page context has been sent to
// the server.
using OnPageContextUpdatedCallback = base::OnceCallback<void()>;

// Controller responsible for handling contextualization logic for Lens flows.
// This includes grabbing content related to the page and issuing Lens requests
// so searchbox requests are contextualized.
class LensSearchContextualizationController {
 public:
  explicit LensSearchContextualizationController(
      LensSearchController* lens_search_controller);
  virtual ~LensSearchContextualizationController();

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. The contextualization flow is not currently
    // active.
    kOff,

    // The contextualization flow is in the process of initializing.
    kInitializing,

    // The contextualization flow is active.
    kActive,

    // TODO(crbug.com/335516480): Implement suspended state.
    kSuspended,
  };
  State state() { return state_; }

  // Starts the contextualization flow without the overlay being shown to the
  // user. Virtual for testing.
  virtual void StartContextualization(
      lens::LensOverlayInvocationSource invocation_source,
      OnPageContextUpdatedCallback callback);

  // Tries to fetch the underlying page content bytes to use for
  // contextualization. If page content can not be retrieved, the callback will
  // be run with no bytes.
  void GetPageContextualization(PageContentRetrievedCallback callback);

  // Tries to fetch the underlying page content bytes and update the query flow
  // with them. `callback` will be run whether the page context was updated or
  // not.
  void TryUpdatePageContextualization(
      OnPageContextUpdatedCallback callback);

#if BUILDFLAG(ENABLE_PDF)
  // Fetches the visible page index from the PDF renderer and then starts the
  // process of fetching the text from the PDF to be used for suggest signals.
  // This is a no-op if the tab is not a PDF. Once the partial text is
  // retrieved, the text is sent to the server via the query controller.
  void FetchVisiblePageIndexAndGetPartialPdfText(
      uint32_t page_count,
      PdfPartialPageTextRetrievedCallback callback);
#endif  // BUILDFLAG(ENABLE_PDF)

  // Resets the state of the contextualization controller to kOff.
  void ResetState();

  // Records the UMA for the metrics relating to the document where the
  // contextual search box was shown. If this is a webpage, records the size of
  // the innerHtml and the innerText. If this is a PDF, records the byte size of
  // the PDF and the number of pages. `pdf_page_count` is only used for PDFs.
  void RecordDocumentMetrics(std::optional<uint32_t> pdf_page_count);

  // Posts a task to the background thread to calculate the OCR DOM similarity
  // and then records the result. Only records the similarity once per session.
  // Only records the similarity if the OCR text and page content are available.
  void TryCalculateAndRecordOcrDomSimilarity();

  // Sets the text of the page. Used to calculate the OCR DOM similarity.
  // Should only be called once per session.
  void SetText(lens::mojom::TextPtr text);

  // TODO(crbug.com/418825720): Remove this code once the early start query flow
  // optimization is fully launched as this will no longer be needed as all
  // context updates will go through this controller. Sets the page content and
  // primary content type for the controller. Only used in when the start query
  // flow optimization is not enabled to ensure that the page content is still
  // passed to the contextualization controller even if it does not make the
  // request to the server.
  void SetPageContent(std::vector<lens::PageContent> page_contents,
                      lens::MimeType primary_content_type);

  bool IsActive() const { return state_ == State::kActive; }

 private:
  // Begin updating page contextualization by potentially taking a new
  // screenshot.
  void UpdatePageContextualization(std::vector<lens::PageContent> page_contents,
                                   lens::MimeType primary_content_type,
                                   std::optional<uint32_t> pdf_page_count);

  // Continue updating page contextualization by potentially getting the current
  // PDF page.
  void UpdatePageContextualizationPart2(
      std::vector<lens::PageContent> page_contents,
      lens::MimeType primary_content_type,
      std::optional<uint32_t> pdf_page_count,
      const SkBitmap& bitmap);

  // Updates the query flow with the new page content bytes and/or screenshot. A
  // request will only be sent if the bytes are different from the previous
  // bytes sent or the screenshot is different from the previous screenshot.
  void UpdatePageContextualizationPart3(
      std::vector<lens::PageContent> page_contents,
      lens::MimeType primary_content_type,
      std::optional<uint32_t> pdf_page_count,
      const SkBitmap& bitmap,
      std::optional<uint32_t> pdf_current_page);

  // Gets the inner HTML for contextualization if flag enabled. Otherwise skip
  // to MaybeGetInnerText().
  void MaybeGetInnerHtml(std::vector<lens::PageContent> page_contents,
                         content::RenderFrameHost* render_frame_host,
                         PageContentRetrievedCallback callback);

  // Callback for when the inner HTML is retrieved from the underlying page.
  // Calls MaybeGetInnerText().
  void OnInnerHtmlReceived(std::vector<lens::PageContent> page_contents,
                           content::RenderFrameHost* render_frame_host,
                           PageContentRetrievedCallback callback,
                           const std::optional<std::string>& result);

  // Gets the inner text for contextualization if flag enabled. Otherwise skip
  // to MaybeGetAnnotatedPageContent().
  void MaybeGetInnerText(std::vector<lens::PageContent> page_contents,
                         content::RenderFrameHost* render_frame_host,
                         PageContentRetrievedCallback callback);

  // Callback for when the inner text is retrieved from the underlying page.
  // Calls MaybeGetAnnotatedPageContent().
  void OnInnerTextReceived(
      std::vector<lens::PageContent> page_contents,
      content::RenderFrameHost* render_frame_host,
      PageContentRetrievedCallback callback,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  // Gets the annotated page content for contextualization if flag enabled.
  // Otherwise run the callback with the HTML and/or innerText.
  void MaybeGetAnnotatedPageContent(
      std::vector<lens::PageContent> page_contents,
      content::RenderFrameHost* render_frame_host,
      PageContentRetrievedCallback callback);

  // Callback for when the annotated page content is retrieved. Runs the
  // callback with the HTML, innerText, and/or annotated page content.
  void OnAnnotatedPageContentReceived(
      std::vector<lens::PageContent> page_contents,
      PageContentRetrievedCallback callback,
      std::optional<optimization_guide::AIPageContentResult> apc);

#if BUILDFLAG(ENABLE_PDF)
  // Gets the PDF bytes from the IPC call to the PDF renderer if the PDF
  // feature is enabled. Otherwise run the callback with no bytes.
  void MaybeGetPdfBytes(pdf::PDFDocumentHelper* pdf_helper,
                        PageContentRetrievedCallback callback);

  // Receives the PDF bytes from the IPC call to the PDF renderer and stores
  // them in initialization data. `pdf_page_count` is passed to the partial PDF
  // text fetch to be used to determine when to stop fetching.
  void OnPdfBytesReceived(PageContentRetrievedCallback callback,
                          pdf::mojom::PdfListener::GetPdfBytesStatus status,
                          const std::vector<uint8_t>& bytes,
                          uint32_t pdf_page_count);

  // Gets the partial text from the PDF to be used for suggest. Schedules for
  // the next page of text to be fetched, from the PDF in page order until
  // either 1) all the text is received or 2) the character limit is reached.
  // This method should only be called by GetPartialPdfText.
  void GetPartialPdfTextCallback(uint32_t page_index,
                                 uint32_t total_page_count,
                                 uint32_t total_characters_retrieved,
                                 const std::u16string& page_text);

  // Callback to run when the partial page text is retrieved from the PDF.
  void OnPdfPartialPageTextRetrieved(
      std::vector<std::u16string> pdf_pages_text);
#endif  // BUILDFLAG(ENABLE_PDF)

  bool IsScreenshotPossible(content::RenderWidgetHostView* view);

  void CaptureScreenshot(base::OnceCallback<void(const SkBitmap&)> callback);

  // Callback for when the screenshot is captured and initial request data is
  // ready.
  void DidCaptureScreenshot(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      int attempt_id,
      const SkBitmap& bitmap,
      const std::vector<gfx::Rect>& bounds,
      OnPageContextUpdatedCallback callback,
      std::optional<uint32_t> pdf_current_page);

  // Fetches the bounding boxes of all images within the current viewport.
  void FetchViewportImageBoundingBoxes(OnPageContextUpdatedCallback callback,
                                       const SkBitmap& bitmap);

  // Creates the mojo bounding boxes for the significant regions.
  std::vector<lens::mojom::CenterRotatedBoxPtr> ConvertSignificantRegionBoxes(
      const std::vector<gfx::Rect>& all_bounds);

  // Gets the current page number if viewing a PDF.
  void GetPdfCurrentPage(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      int attempt_id,
      const SkBitmap& bitmap,
      OnPageContextUpdatedCallback callback,
      const std::vector<gfx::Rect>& bounds);

  // Callback to record the size of the innerText once it is fetched.
  void RecordInnerTextSize(
      std::unique_ptr<content_extraction::InnerTextResult> result);

  // Callback to record the size of the innerHtml once it is fetched.
  void RecordInnerHtmlSize(const std::optional<std::string>& result);

  float GetUiScaleFactor();

  lens::LensOverlayQueryController* GetQueryController();
  lens::LensSearchboxController* GetSearchboxController();

  // The current state of the contextualization flow.
  State state_ = State::kOff;

  // Indicates whether the user is currently on a context eligible page.
  bool is_page_context_eligible_ = true;

  // The callback to run when the partial page text is retrieved. This is
  // populated when FetchVisiblePageIndexAndGetPartialPdfText is called.
  PdfPartialPageTextRetrievedCallback pdf_partial_page_text_retrieved_callback_;

  // The screenshot of the viewport.
  SkBitmap viewport_screenshot_;

  // The page url. Empty if it is not allowed to be shared.
  GURL page_url_;

  // The page title, if it is allowed to be shared.
  std::optional<std::string> page_title_;

  // The data of the content the user is viewing. There can be multiple
  // content types for a single page, so we store them all in this struct.
  std::vector<lens::PageContent> page_contents_;

  // The primary type of the data stored in page_contents_. This is the value
  // used to determine request params and what content to look at when
  // determining if the page_contents_ needs to be present.
  lens::MimeType primary_content_type_ = lens::MimeType::kUnknown;

  // The page count of the PDF document if page_content_type_ is kPdf.
  std::optional<uint32_t> pdf_page_count_;

  // The partial representation of a PDF document. The element at a given
  // index holds the text of the PDF page at the same index.
  std::vector<std::u16string> pdf_pages_text_;

  // The most visible page of the PDF document when the viewport was last
  // updated, if page_content_type_ is kPdf.
  std::optional<uint32_t> last_retrieved_most_visible_page_;

  // The callback for the caller to pass to this controller to be notified when
  // the page context has been updated and sent to the server.
  OnPageContextUpdatedCallback on_page_context_updated_callback_;

  // The text of the page. Used to calculate the OCR DOM similarity. Used once
  // per session and then cleared.
  lens::mojom::TextPtr text_;

  // The source of the invocation.
  lens::LensOverlayInvocationSource invocation_source_;

  // Whether the OCR DOM similarity has been recorded in the current session.
  bool ocr_dom_similarity_recorded_in_session_ = false;

  // Owns this.
  const raw_ptr<LensSearchController> lens_search_controller_;

  // Must be the last member.
  base::WeakPtrFactory<LensSearchContextualizationController> weak_ptr_factory_{
      this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_
