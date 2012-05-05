// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "net/base/file_stream.h"
#include "net/base/net_export.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace file_util {
struct FileInfo;
}

namespace net {

// A request job that handles reading file URLs
class NET_EXPORT URLRequestFileJob : public URLRequestJob {
 public:
  URLRequestFileJob(URLRequest* request, const FilePath& file_path);

  static URLRequest::ProtocolFactory Factory;

#if defined(OS_CHROMEOS)
  static bool AccessDisabled(const FilePath& file_path);
#endif

  // URLRequestJob:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual bool IsRedirectResponse(GURL* location,
                                  int* http_status_code) OVERRIDE;
  virtual Filter* SetupFilter() const OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const HttpRequestHeaders& headers) OVERRIDE;

 protected:
  virtual ~URLRequestFileJob();

  // The OS-specific full path name of the file
  FilePath file_path_;

 private:
  // Tests to see if access to |path| is allowed. If g_allow_file_access_ is
  // true, then this will return true. If the NetworkDelegate associated with
  // the |request| says it's OK, then this will also return true.
  static bool IsFileAccessAllowed(const URLRequest& request,
                                  const FilePath& path);

  // Callback after fetching file info on a background thread.
  void DidResolve(bool exists, const base::PlatformFileInfo& file_info);

  // Callback after data is asynchronously read from the file.
  void DidRead(int result);

  FileStream stream_;
  bool is_directory_;

  HttpByteRange byte_range_;
  int64 remaining_bytes_;

  // The initial file metadata is fetched on a background thread.
  // AsyncResolver runs that task.
  class AsyncResolver;
  friend class AsyncResolver;
  scoped_refptr<AsyncResolver> async_resolver_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFileJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
