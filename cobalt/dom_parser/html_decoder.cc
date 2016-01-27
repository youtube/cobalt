/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
    const base::SourceLocation& input_location,
    const base::Closure& done_callback,
    const base::Callback<void(const std::string&)>& error_callback)
    : libxml_html_parser_wrapper_(
          new LibxmlHTMLParserWrapper(document, parent_node, reference_node,
                                      input_location, error_callback)),
      document_(document),
      done_callback_(done_callback) {}

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

  csp::ResponseHeaders csp_headers(headers);
  if (document_->csp_delegate()->OnReceiveHeaders(csp_headers)) {
    return loader::kLoadResponseContinue;
  } else {
    DLOG(ERROR) << "Failure receiving Content Security Policy headers";
    return loader::kLoadResponseAbort;
  }
}

void HTMLDecoder::DecodeChunk(const char* data, size_t size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  libxml_html_parser_wrapper_->DecodeChunk(data, size);
}

void HTMLDecoder::Finish() {
  DCHECK(thread_checker_.CalledOnValidThread());
  libxml_html_parser_wrapper_->Finish();
  if (!done_callback_.is_null()) {
    done_callback_.Run();
  }
}

}  // namespace dom_parser
}  // namespace cobalt
