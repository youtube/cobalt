// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/task.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace file_util {
struct FileInfo;
}

namespace net {

// A request job that handles reading file URLs
class URLRequestFileJob : public URLRequestJob {
 public:
  URLRequestFileJob(URLRequest* request, const FilePath& file_path);

  static URLRequest::ProtocolFactory Factory;

#if defined(OS_CHROMEOS)
  static bool AccessDisabled(const FilePath& file_path);
#endif

  // URLRequestJob:
  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(IOBuffer* buf, int buf_size, int* bytes_read);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);
  virtual Filter* SetupFilter() const;
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual void SetExtraRequestHeaders(const HttpRequestHeaders& headers);

 protected:
  virtual ~URLRequestFileJob();

  // The OS-specific full path name of the file
  FilePath file_path_;

 private:
  void DidResolve(bool exists, const base::PlatformFileInfo& file_info);
  void DidRead(int result);

  CompletionCallbackImpl<URLRequestFileJob> io_callback_;
  FileStream stream_;
  bool is_directory_;

  HttpByteRange byte_range_;
  int64 remaining_bytes_;

#if defined(OS_WIN)
  class AsyncResolver;
  friend class AsyncResolver;
  scoped_refptr<AsyncResolver> async_resolver_;
#endif

  ScopedRunnableMethodFactory<URLRequestFileJob> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFileJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
