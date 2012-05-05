// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For loading files, we make use of overlapped i/o to ensure that reading from
// the filesystem (e.g., a network filesystem) does not block the calling
// thread.  An alternative approach would be to use a background thread or pool
// of threads, but it seems better to leverage the operating system's ability
// to do background file reads for us.
//
// Since overlapped reads require a 'static' buffer for the duration of the
// asynchronous read, the URLRequestFileJob keeps a buffer as a member var.  In
// URLRequestFileJob::Read, data is simply copied from the object's buffer into
// the given buffer.  If there is no data to copy, the URLRequestFileJob
// attempts to read more from the file to fill its buffer.  If reading from the
// file does not complete synchronously, then the URLRequestFileJob waits for a
// signal from the OS that the overlapped read has completed.  It does so by
// leveraging the MessageLoop::WatchObject API.

#include "net/url_request/url_request_file_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_dir_job.h"

namespace net {

class URLRequestFileJob::AsyncResolver
    : public base::RefCountedThreadSafe<URLRequestFileJob::AsyncResolver> {
 public:
  explicit AsyncResolver(URLRequestFileJob* owner)
      : owner_(owner), owner_loop_(MessageLoop::current()) {
  }

  void Resolve(const FilePath& file_path) {
    base::PlatformFileInfo file_info;
    bool exists = file_util::GetFileInfo(file_path, &file_info);
    base::AutoLock locked(lock_);
    if (owner_loop_) {
      owner_loop_->PostTask(
          FROM_HERE,
          base::Bind(&AsyncResolver::ReturnResults, this, exists, file_info));
    }
  }

  void Cancel() {
    owner_ = NULL;

    base::AutoLock locked(lock_);
    owner_loop_ = NULL;
  }

 private:
  friend class base::RefCountedThreadSafe<URLRequestFileJob::AsyncResolver>;

  ~AsyncResolver() {}

  void ReturnResults(bool exists, const base::PlatformFileInfo& file_info) {
    if (owner_)
      owner_->DidResolve(exists, file_info);
  }

  URLRequestFileJob* owner_;

  base::Lock lock_;
  MessageLoop* owner_loop_;
};

URLRequestFileJob::URLRequestFileJob(URLRequest* request,
                                     const FilePath& file_path)
    : URLRequestJob(request),
      file_path_(file_path),
      stream_(NULL),
      is_directory_(false),
      remaining_bytes_(0) {
}

// static
URLRequestJob* URLRequestFileJob::Factory(URLRequest* request,
                                          const std::string& scheme) {
  FilePath file_path;
  const bool is_file = FileURLToFilePath(request->url(), &file_path);

  // Check file access permissions.
  if (!IsFileAccessAllowed(*request, file_path))
    return new URLRequestErrorJob(request, ERR_ACCESS_DENIED);

  // We need to decide whether to create URLRequestFileJob for file access or
  // URLRequestFileDirJob for directory access. To avoid accessing the
  // filesystem, we only look at the path string here.
  // The code in the URLRequestFileJob::Start() method discovers that a path,
  // which doesn't end with a slash, should really be treated as a directory,
  // and it then redirects to the URLRequestFileDirJob.
  if (is_file &&
      file_util::EndsWithSeparator(file_path) &&
      file_path.IsAbsolute())
    return new URLRequestFileDirJob(request, file_path);

  // Use a regular file request job for all non-directories (including invalid
  // file names).
  return new URLRequestFileJob(request, file_path);
}

void URLRequestFileJob::Start() {
  DCHECK(!async_resolver_);
  async_resolver_ = new AsyncResolver(this);
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&AsyncResolver::Resolve, async_resolver_.get(), file_path_),
      true);
}

void URLRequestFileJob::Kill() {
  // URL requests should not block on the disk!
  //   http://code.google.com/p/chromium/issues/detail?id=59849
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  stream_.CloseSync();

  if (async_resolver_) {
    async_resolver_->Cancel();
    async_resolver_ = NULL;
  }

  URLRequestJob::Kill();
}

bool URLRequestFileJob::ReadRawData(IOBuffer* dest, int dest_size,
                                    int *bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0);

  if (remaining_bytes_ < dest_size)
    dest_size = static_cast<int>(remaining_bytes_);

  // If we should copy zero bytes because |remaining_bytes_| is zero, short
  // circuit here.
  if (!dest_size) {
    *bytes_read = 0;
    return true;
  }

  int rv = stream_.Read(dest, dest_size,
                        base::Bind(&URLRequestFileJob::DidRead,
                                   base::Unretained(this)));
  if (rv >= 0) {
    // Data is immediately available.
    *bytes_read = rv;
    remaining_bytes_ -= rv;
    DCHECK_GE(remaining_bytes_, 0);
    return true;
  }

  // Otherwise, a read error occured.  We may just need to wait...
  if (rv == ERR_IO_PENDING) {
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }
  return false;
}

bool URLRequestFileJob::IsRedirectResponse(GURL* location,
                                           int* http_status_code) {
  if (is_directory_) {
    // This happens when we discovered the file is a directory, so needs a
    // slash at the end of the path.
    std::string new_path = request_->url().path();
    new_path.push_back('/');
    GURL::Replacements replacements;
    replacements.SetPathStr(new_path);

    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 301;  // simulate a permanent redirect
    return true;
  }

#if defined(OS_WIN)
  // Follow a Windows shortcut.
  // We just resolve .lnk file, ignore others.
  if (!LowerCaseEqualsASCII(file_path_.Extension(), ".lnk"))
    return false;

  FilePath new_path = file_path_;
  bool resolved;
  resolved = file_util::ResolveShortcut(&new_path);

  // If shortcut is not resolved succesfully, do not redirect.
  if (!resolved)
    return false;

  *location = FilePathToFileURL(new_path);
  *http_status_code = 301;
  return true;
#else
  return false;
#endif
}

Filter* URLRequestFileJob::SetupFilter() const {
  // Bug 9936 - .svgz files needs to be decompressed.
  return LowerCaseEqualsASCII(file_path_.Extension(), ".svgz")
      ? Filter::GZipFactory() : NULL;
}

bool URLRequestFileJob::GetMimeType(std::string* mime_type) const {
  // URL requests should not block on the disk!  On Windows this goes to the
  // registry.
  //   http://code.google.com/p/chromium/issues/detail?id=59849
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  DCHECK(request_);
  return GetMimeTypeFromFile(file_path_, mime_type);
}

void URLRequestFileJob::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(HttpRequestHeaders::kRange, &range_header)) {
    // We only care about "Range" header here.
    std::vector<HttpByteRange> ranges;
    if (HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request,
        // because we need to do multipart encoding here.
        // TODO(hclam): decide whether we want to support multiple range
        // requests.
        NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                    ERR_REQUEST_RANGE_NOT_SATISFIABLE));
      }
    }
  }
}

// static
bool URLRequestFileJob::IsFileAccessAllowed(const URLRequest& request,
                                            const FilePath& path) {
  const URLRequestContext* context = request.context();
  if (!context)
    return false;
  const NetworkDelegate* delegate = context->network_delegate();
  if (delegate)
    return delegate->CanAccessFile(request, path);
  return false;
}

URLRequestFileJob::~URLRequestFileJob() {
  DCHECK(!async_resolver_);
}

void URLRequestFileJob::DidResolve(
    bool exists, const base::PlatformFileInfo& file_info) {
  async_resolver_ = NULL;

  // We may have been orphaned...
  if (!request_)
    return;

  is_directory_ = file_info.is_directory;

  int rv = OK;
  // We use URLRequestFileJob to handle files as well as directories without
  // trailing slash.
  // If a directory does not exist, we return ERR_FILE_NOT_FOUND. Otherwise,
  // we will append trailing slash and redirect to FileDirJob.
  // A special case is "\" on Windows. We should resolve as invalid.
  // However, Windows resolves "\" to "C:\", thus reports it as existent.
  // So what happens is we append it with trailing slash and redirect it to
  // FileDirJob where it is resolved as invalid.
  if (!exists) {
    rv = ERR_FILE_NOT_FOUND;
  } else if (!is_directory_) {
    // URL requests should not block on the disk!
    //   http://code.google.com/p/chromium/issues/detail?id=59849
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    int flags = base::PLATFORM_FILE_OPEN |
                base::PLATFORM_FILE_READ |
                base::PLATFORM_FILE_ASYNC;
    rv = stream_.OpenSync(file_path_, flags);
  }

  if (rv != OK) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
    return;
  }

  if (!byte_range_.ComputeBounds(file_info.size)) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
               ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  remaining_bytes_ = byte_range_.last_byte_position() -
                     byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  // URL requests should not block on the disk!
  //   http://code.google.com/p/chromium/issues/detail?id=59849
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    // Do the seek at the beginning of the request.
    if (remaining_bytes_ > 0 &&
        byte_range_.first_byte_position() != 0 &&
        byte_range_.first_byte_position() !=
        stream_.SeekSync(FROM_BEGIN, byte_range_.first_byte_position())) {
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                  ERR_REQUEST_RANGE_NOT_SATISFIABLE));
      return;
    }
  }

  set_expected_content_size(remaining_bytes_);
  NotifyHeadersComplete();
}

void URLRequestFileJob::DidRead(int result) {
  if (result > 0) {
    SetStatus(URLRequestStatus());  // Clear the IO_PENDING status
  } else if (result == 0) {
    NotifyDone(URLRequestStatus());
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  }

  remaining_bytes_ -= result;
  DCHECK_GE(remaining_bytes_, 0);

  NotifyReadComplete(result);
}

}  // namespace net
