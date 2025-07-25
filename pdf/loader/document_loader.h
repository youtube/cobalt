// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_LOADER_DOCUMENT_LOADER_H_
#define PDF_LOADER_DOCUMENT_LOADER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

namespace chrome_pdf {

class URLLoaderWrapper;

class DocumentLoader {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    // Creates new URLLoader based on client settings.
    virtual std::unique_ptr<URLLoaderWrapper> CreateURLLoader() = 0;
    // Notification called when all outstanding pending requests are complete.
    virtual void OnPendingRequestComplete() = 0;
    // Notification called when new data is available.
    virtual void OnNewDataReceived() = 0;
    // Notification called when document is fully loaded.
    virtual void OnDocumentComplete() = 0;
    // Notification called when document loading is canceled.
    virtual void OnDocumentCanceled() = 0;
  };

  virtual ~DocumentLoader() = default;

  virtual bool Init(std::unique_ptr<URLLoaderWrapper> loader,
                    const std::string& url) = 0;

  // Data access interface. Return true if successful.
  virtual bool GetBlock(uint32_t position, uint32_t size, void* buf) const = 0;

  // Data availability interface. Return true if data is available.
  virtual bool IsDataAvailable(uint32_t position, uint32_t size) const = 0;

  // Data request interface.
  virtual void RequestData(uint32_t position, uint32_t size) = 0;

  virtual bool IsDocumentComplete() const = 0;
  virtual uint32_t GetDocumentSize() const = 0;
  virtual uint32_t BytesReceived() const = 0;

  // Clear pending requests from the queue.
  virtual void ClearPendingRequests() = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_LOADER_DOCUMENT_LOADER_H_
