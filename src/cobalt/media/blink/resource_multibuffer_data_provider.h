// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_RESOURCE_MULTIBUFFER_DATA_PROVIDER_H_
#define COBALT_MEDIA_BLINK_RESOURCE_MULTIBUFFER_DATA_PROVIDER_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/media/blink/active_loader.h"
#include "cobalt/media/blink/media_blink_export.h"
#include "cobalt/media/blink/multibuffer.h"
#include "cobalt/media/blink/url_index.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace media {

class MEDIA_BLINK_EXPORT ResourceMultiBufferDataProvider
    : NON_EXPORTED_BASE(public MultiBuffer::DataProvider),
      NON_EXPORTED_BASE(public blink::WebURLLoaderClient) {
 public:
  // NUmber of times we'll retry if the connection fails.
  enum { kMaxRetries = 30 };

  ResourceMultiBufferDataProvider(UrlData* url_data, MultiBufferBlockId pos);
  ~ResourceMultiBufferDataProvider() OVERRIDE;

  // Virtual for testing purposes.
  virtual void Start();

  // MultiBuffer::DataProvider implementation
  MultiBufferBlockId Tell() const OVERRIDE;
  bool Available() const OVERRIDE;
  int64_t AvailableBytes() const OVERRIDE;
  scoped_refptr<DataBuffer> Read() OVERRIDE;
  void SetDeferred(bool defer) OVERRIDE;

  // blink::WebURLLoaderClient implementation.
  bool willFollowRedirect(
      blink::WebURLLoader* loader, blink::WebURLRequest& newRequest,
      const blink::WebURLResponse& redirectResponse) OVERRIDE;
  void didSendData(blink::WebURLLoader* loader, uint64 bytesSent,
                   uint64 totalBytesToBeSent) OVERRIDE;
  void didReceiveResponse(blink::WebURLLoader* loader,
                          const blink::WebURLResponse& response) OVERRIDE;
  void didDownloadData(blink::WebURLLoader* loader, int data_length,
                       int encoded_data_length) OVERRIDE;
  void didReceiveData(blink::WebURLLoader* loader, const char* data,
                      int data_length, int encoded_data_length,
                      int encoded_body_length) OVERRIDE;
  void didReceiveCachedMetadata(blink::WebURLLoader* loader, const char* data,
                                int dataLength) OVERRIDE;
  void didFinishLoading(blink::WebURLLoader* loader, double finishTime,
                        int64_t total_encoded_data_length) OVERRIDE;
  void didFail(blink::WebURLLoader* loader, const blink::WebURLError&) OVERRIDE;

  // Use protected instead of private for testing purposes.
 protected:
  friend class MultibufferDataSourceTest;
  friend class ResourceMultiBufferDataProviderTest;
  friend class MockBufferedDataSource;

  // Callback used when we're asked to fetch data after the end of the file.
  void Terminate();

  // Parse a Content-Range header into its component pieces and return true if
  // each of the expected elements was found & parsed correctly.
  // |*instance_size| may be set to kPositionNotSpecified if the range ends in
  // "/*".
  // NOTE: only public for testing!  This is an implementation detail of
  // VerifyPartialResponse (a private method).
  static bool ParseContentRange(const std::string& content_range_str,
                                int64_t* first_byte_position,
                                int64_t* last_byte_position,
                                int64_t* instance_size);

  int64_t byte_pos() const;
  int64_t block_size() const;

  // If we have made a range request, verify the response from the server.
  bool VerifyPartialResponse(const blink::WebURLResponse& response,
                             const scoped_refptr<UrlData>& url_data);

  // Current Position.
  MultiBufferBlockId pos_;

  // This is where we actually get read data from.
  // We don't need (or want) a scoped_refptr for this one, because
  // we are owned by it. Note that we may change this when we encounter
  // a redirect because we actually change ownership.
  UrlData* url_data_;

  // Temporary storage for incoming data.
  std::list<scoped_refptr<DataBuffer>> fifo_;

  // How many retries have we done at the current position.
  int retries_;

  // Copy of url_data_->cors_mode()
  // const to make it obvious that redirects cannot change it.
  const UrlData::CORSMode cors_mode_;

  // The origin for the initial request.
  // const to make it obvious that redirects cannot change it.
  const GURL origin_;

  // Keeps track of an active WebURLLoader and associated state.
  std::unique_ptr<ActiveLoader> active_loader_;

  // Injected WebURLLoader instance for testing purposes.
  std::unique_ptr<blink::WebURLLoader> test_loader_;

  // When we encounter a redirect, this is the source of the redirect.
  GURL redirects_to_;

  base::WeakPtrFactory<ResourceMultiBufferDataProvider> weak_factory_;
};

}  // namespace media

#endif  // COBALT_MEDIA_BLINK_RESOURCE_MULTIBUFFER_DATA_PROVIDER_H_
