// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_context_fetcher.h"

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace dictation {

namespace {

constexpr base::ByteSize kInnerTextLimit = base::KiBU(200);

}  // namespace

DictationContextFetcher::DictationContextFetcher() = default;
DictationContextFetcher::~DictationContextFetcher() = default;

void DictationContextFetcher::Fetch(const Target& target,
                                    GetContextCallback callback) {
  content::RenderFrameHost* rfh = target.GetRenderFrameHost();
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    DictationContext context;
    context.editable_content = target.GetSelectedText();
    std::move(callback).Run(std::move(context));
    return;
  }

  page_content_annotations::FetchPageContextOptions options;
  options.inner_text_bytes_limit = kInnerTextLimit.InBytes();
  options.annotated_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path=*/true);
  options.annotated_page_content_options->include_same_site_only = true;

  page_content_annotations::FetchPageContext(
      *web_contents, options,
      /*progress_listener=*/nullptr,
      base::BindOnce(&DictationContextFetcher::OnPageContextFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     target.GetSelectedText()));
}

void DictationContextFetcher::OnPageContextFetched(
    GetContextCallback callback,
    const std::string& editable_content,
    page_content_annotations::FetchPageContextResultCallbackArg result) {
  DictationContext context;
  context.editable_content = editable_content;
  // TODO(b/527240600): Handle errors
  // TODO(b/525845074): Implement CSE/IRM protections
  if (result.has_value()) {
    auto& fetch_result = *result;
    if (fetch_result->annotated_page_content_result.has_value()) {
      context.annotated_page_content =
          std::move(fetch_result->annotated_page_content_result.value().proto);
    }
    if (fetch_result->inner_text_result.has_value()) {
      context.inner_text =
          std::move(fetch_result->inner_text_result->inner_text);
    }
  }
  std::move(callback).Run(std::move(context));
}

}  // namespace dictation
