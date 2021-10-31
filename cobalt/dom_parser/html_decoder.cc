// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/dom_parser/html_decoder.h"

#include "cobalt/csp/content_security_policy.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom_parser/libxml_html_parser_wrapper.h"
#include "cobalt/loader/net_fetcher.h"

namespace cobalt {
namespace dom_parser {

HTMLDecoder::HTMLDecoder(
    const scoped_refptr<dom::Document>& document,
    const scoped_refptr<dom::Node>& parent_node,
    const scoped_refptr<dom::Node>& reference_node,
    const int dom_max_element_depth, const base::SourceLocation& input_location,
    const loader::Decoder::OnCompleteFunction& load_complete_callback,
    const bool should_run_scripts, const csp::CSPHeaderPolicy require_csp)
    : libxml_html_parser_wrapper_(new LibxmlHTMLParserWrapper(
          document, parent_node, reference_node, dom_max_element_depth,
          input_location, load_complete_callback, should_run_scripts)),
      document_(document),
      require_csp_(require_csp),
      load_complete_callback_(load_complete_callback) {}

HTMLDecoder::~HTMLDecoder() {}

loader::LoadResponseType HTMLDecoder::OnResponseStarted(
    loader::Fetcher* fetcher,
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  DCHECK(headers);
  // Only NetFetchers can call this, since only they have
  // the response headers.
  loader::NetFetcher* net_fetcher =
      base::polymorphic_downcast<loader::NetFetcher*>(fetcher);
  net::URLFetcher* url_fetcher = net_fetcher->url_fetcher();
  if (url_fetcher->GetURL() != url_fetcher->GetOriginalURL()) {
    document_->NotifyUrlChanged(url_fetcher->GetURL());
  }

  if (headers->HasHeaderValue("cobalt-jit", "disable")) {
    document_->DisableJit();
  } else if (headers->HasHeader("cobalt-jit")) {
    std::string value;
    headers->GetNormalizedHeader("cobalt-jit", &value);
    LOG(WARNING) << "Invalid value for \"cobalt-jit\" header: " << value;
  }

  csp::ResponseHeaders csp_headers(headers);
  if (document_->csp_delegate()->OnReceiveHeaders(csp_headers) ||
      require_csp_ == csp::kCSPOptional) {
    return loader::kLoadResponseContinue;
  } else {
    LOG(ERROR) << "Failure receiving Content Security Policy headers "
                  "for URL: " << url_fetcher->GetURL() << ".";
    LOG(ERROR) << "The server *must* send CSP headers or Cobalt will not "
                  "load the page.";
    return loader::kLoadResponseAbort;
  }
}

void HTMLDecoder::DecodeChunk(const char* data, size_t size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  libxml_html_parser_wrapper_->DecodeChunk(data, size);
}

void HTMLDecoder::Finish() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  libxml_html_parser_wrapper_->Finish();
  if (!load_complete_callback_.is_null()) {
    load_complete_callback_.Run(base::nullopt);
  }
}

}  // namespace dom_parser
}  // namespace cobalt
