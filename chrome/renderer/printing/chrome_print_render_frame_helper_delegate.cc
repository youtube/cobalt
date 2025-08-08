// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/printing/chrome_print_render_frame_helper_delegate.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/common/pdf_util.h"
#include "extensions/renderer/guest_view/mime_handler_view/post_message_support.h"
#endif  // BUILDFLAG(ENABLE_PDF)

ChromePrintRenderFrameHelperDelegate::ChromePrintRenderFrameHelperDelegate() =
    default;

ChromePrintRenderFrameHelperDelegate::~ChromePrintRenderFrameHelperDelegate() =
    default;

// Return the PDF object element if `frame` is the out of process PDF extension
// or its child frame.
blink::WebElement ChromePrintRenderFrameHelperDelegate::GetPdfElement(
    blink::WebLocalFrame* frame) {
#if BUILDFLAG(ENABLE_PDF)
  if (frame->Parent() &&
      IsPdfInternalPluginAllowedOrigin(frame->Parent()->GetSecurityOrigin())) {
    auto plugin_element = frame->GetDocument().QuerySelector("embed");
    DCHECK(!plugin_element.IsNull());
    return plugin_element;
  }
#endif  // BUILDFLAG(ENABLE_PDF)
  return blink::WebElement();
}

bool ChromePrintRenderFrameHelperDelegate::IsPrintPreviewEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kDisablePrintPreview);
}

bool ChromePrintRenderFrameHelperDelegate::OverridePrint(
    blink::WebLocalFrame* frame) {
#if BUILDFLAG(ENABLE_PDF)
  auto* post_message_support =
      extensions::PostMessageSupport::FromWebLocalFrame(frame);
  if (post_message_support) {
    // This message is handled in chrome/browser/resources/pdf/pdf_viewer.js and
    // instructs the PDF plugin to print. This is to make window.print() on a
    // PDF plugin document correctly print the PDF. See
    // https://crbug.com/448720.
    base::Value::Dict message;
    message.Set("type", "print");
    post_message_support->PostMessageFromValue(base::Value(std::move(message)));
    return true;
  }
#endif  // BUILDFLAG(ENABLE_PDF)
  return false;
}

bool ChromePrintRenderFrameHelperDelegate::ShouldGenerateTaggedPDF() {
  return true;
}
