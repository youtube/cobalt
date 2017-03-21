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
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_error_job.h"
#if !defined(COBALT)
#include "net/url_request/url_request_file_dir_job.h"
#endif

#if defined(OS_WIN)
#include "base/win/shortcut.h"
#endif

namespace net {

URLRequestFileJob::FileMetaInfo::FileMetaInfo()
    : file_size(0),
      mime_type_result(false),
      file_exists(false),
      is_directory(false) {
}

URLRequestFileJob::URLRequestFileJob(URLRequest* request,
                                     NetworkDelegate* network_delegate,
                                     const FilePath& file_path)
    : URLRequestJob(request, network_delegate),
      file_path_(file_path),
      stream_(new FileStream(NULL)),
      remaining_bytes_(0),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

// static
URLRequestJob* URLRequestFileJob::Factory(URLRequest* request,
                                          NetworkDelegate* network_delegate,
                                          const std::string& scheme) {
  FilePath file_path;
  const bool is_file = FileURLToFilePath(request->url(), &file_path);

  // Check file access permissions.
  if (!network_delegate ||
      !network_delegate->CanAccessFile(*request, file_path)) {
    return new URLRequestErrorJob(request, network_delegate, ERR_ACCESS_DENIED);
  }


  // We need to decide whether to create URLRequestFileJob for file access or
  // URLRequestFileDirJob for directory access. To avoid accessing the
  // filesystem, we only look at the path string here.
  // The code in the URLRequestFileJob::Start() method discovers that a path,
  // which doesn't end with a slash, should really be treated as a directory,
  // and it then redirects to the URLRequestFileDirJob.
  if (is_file &&
      file_util::EndsWithSeparator(file_path) &&
      file_path.IsAbsolute())
#if defined(COBALT)
    // We don't support FileDirJob.
    return new URLRequestErrorJob(request, network_delegate, ERR_ACCESS_DENIED);
#else
    return new URLRequestFileDirJob(request, network_delegate, file_path);
#endif

  // Use a regular file request job for all non-directories (including invalid
  // file names).
  return new URLRequestFileJob(request, network_delegate, file_path);
}

void URLRequestFileJob::Start() {
  FileMetaInfo* meta_info = new FileMetaInfo();
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&URLRequestFileJob::FetchMetaInfo, file_path_,
                 base::Unretained(meta_info)),
      base::Bind(&URLRequestFileJob::DidFetchMetaInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(meta_info)),
      true);
}

void URLRequestFileJob::Kill() {
  stream_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

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

  int rv = stream_->Read(dest, dest_size,
                         base::Bind(&URLRequestFileJob::DidRead,
                                    weak_ptr_factory_.GetWeakPtr()));
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
  if (meta_info_.is_directory) {
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
  resolved = base::win::ResolveShortcut(new_path, &new_path, NULL);

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
  DCHECK(request_);
  if (meta_info_.mime_type_result) {
    *mime_type = meta_info_.mime_type;
    return true;
  }
  return false;
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

URLRequestFileJob::~URLRequestFileJob() {
}

void URLRequestFileJob::FetchMetaInfo(const FilePath& file_path,
                                      FileMetaInfo* meta_info) {
  base::PlatformFileInfo platform_info;
  meta_info->file_exists = file_util::GetFileInfo(file_path, &platform_info);
  if (meta_info->file_exists) {
    meta_info->file_size = platform_info.size;
    meta_info->is_directory = platform_info.is_directory;
  }
  // On Windows GetMimeTypeFromFile() goes to the registry. Thus it should be
  // done in WorkerPool.
  meta_info->mime_type_result = GetMimeTypeFromFile(file_path,
                                                    &meta_info->mime_type);
}

void URLRequestFileJob::DidFetchMetaInfo(const FileMetaInfo* meta_info) {
  meta_info_ = *meta_info;

  // We use URLRequestFileJob to handle files as well as directories without
  // trailing slash.
  // If a directory does not exist, we return ERR_FILE_NOT_FOUND. Otherwise,
  // we will append trailing slash and redirect to FileDirJob.
  // A special case is "\" on Windows. We should resolve as invalid.
  // However, Windows resolves "\" to "C:\", thus reports it as existent.
  // So what happens is we append it with trailing slash and redirect it to
  // FileDirJob where it is resolved as invalid.
  if (!meta_info_.file_exists) {
    DidOpen(ERR_FILE_NOT_FOUND);
    return;
  }
  if (meta_info_.is_directory) {
    DidOpen(OK);
    return;
  }

  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream_->Open(file_path_, flags,
                         base::Bind(&URLRequestFileJob::DidOpen,
                                    weak_ptr_factory_.GetWeakPtr()));
  if (rv != ERR_IO_PENDING)
    DidOpen(rv);
}

void URLRequestFileJob::DidOpen(int result) {
  if (result != OK) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
    return;
  }

  if (!byte_range_.ComputeBounds(meta_info_.file_size)) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
               ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  remaining_bytes_ = byte_range_.last_byte_position() -
                     byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  if (remaining_bytes_ > 0 && byte_range_.first_byte_position() != 0) {
    int rv = stream_->Seek(FROM_BEGIN, byte_range_.first_byte_position(),
                           base::Bind(&URLRequestFileJob::DidSeek,
                                      weak_ptr_factory_.GetWeakPtr()));
    if (rv != ERR_IO_PENDING) {
      // stream_->Seek() failed, so pass an intentionally erroneous value
      // into DidSeek().
      DidSeek(-1);
    }
  } else {
    // We didn't need to call stream_->Seek() at all, so we pass to DidSeek()
    // the value that would mean seek success. This way we skip the code
    // handling seek failure.
    DidSeek(byte_range_.first_byte_position());
  }
}

void URLRequestFileJob::DidSeek(int64 result) {
  if (result != byte_range_.first_byte_position()) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
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
