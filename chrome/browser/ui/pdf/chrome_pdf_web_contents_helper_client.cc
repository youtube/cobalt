// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"

#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/pdf/pdf_frame_util.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/common/content_restriction.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace {

content::WebContents* GetWebContentsToUse(
    content::RenderFrameHost* render_frame_host) {
  // If we're viewing the PDF in a MimeHandlerViewGuest, use its embedder
  // WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHost(render_frame_host);
  return guest_view
             ? guest_view->embedder_web_contents()
             : content::WebContents::FromRenderFrameHost(render_frame_host);
}

}  // namespace

ChromePDFWebContentsHelperClient::ChromePDFWebContentsHelperClient() = default;

ChromePDFWebContentsHelperClient::~ChromePDFWebContentsHelperClient() = default;

content::RenderFrameHost* ChromePDFWebContentsHelperClient::FindPdfFrame(
    content::WebContents* contents) {
  content::RenderFrameHost* main_frame = contents->GetPrimaryMainFrame();
  content::RenderFrameHost* pdf_frame =
      pdf_frame_util::FindPdfChildFrame(main_frame);
  return pdf_frame ? pdf_frame : main_frame;
}

void ChromePDFWebContentsHelperClient::UpdateContentRestrictions(
    content::RenderFrameHost* render_frame_host,
    int content_restrictions) {
  // Speculative short-term-fix while we get at the root of
  // https://crbug.com/752822 .
  content::WebContents* web_contents_to_use =
      GetWebContentsToUse(render_frame_host);
  if (!web_contents_to_use)
    return;

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(web_contents_to_use);
  // |core_tab_helper| is null for WebViewGuest.
  if (core_tab_helper)
    core_tab_helper->UpdateContentRestrictions(content_restrictions);
}

void ChromePDFWebContentsHelperClient::OnPDFHasUnsupportedFeature(
    content::WebContents* contents) {
  // There is no more Adobe plugin for PDF so there is not much we can do in
  // this case. Maybe simply download the file.
}

void ChromePDFWebContentsHelperClient::OnSaveURL(
    content::WebContents* contents) {
  RecordDownloadSource(DOWNLOAD_INITIATED_BY_PDF_SAVE);
}

void ChromePDFWebContentsHelperClient::SetPluginCanSave(
    content::RenderFrameHost* render_frame_host,
    bool can_save) {
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHost(render_frame_host);
  if (guest_view)
    guest_view->SetPluginCanSave(can_save);
}
