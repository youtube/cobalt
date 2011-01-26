// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_file_dir_job.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"

#if defined(OS_POSIX)
#include <sys/stat.h>
#endif

namespace net {

URLRequestFileDirJob::URLRequestFileDirJob(URLRequest* request,
                                           const FilePath& dir_path)
    : URLRequestJob(request),
      dir_path_(dir_path),
      canceled_(false),
      list_complete_(false),
      wrote_header_(false),
      read_pending_(false),
      read_buffer_length_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

void URLRequestFileDirJob::StartAsync() {
  DCHECK(!lister_);

  // TODO(willchan): This is stupid.  We should tell |lister_| not to call us
  // back.  Fix this stupidity.

  // AddRef so that *this* cannot be destroyed while the lister_
  // is trying to feed us data.

  AddRef();
  lister_ = new DirectoryLister(dir_path_, this);
  lister_->Start();

  NotifyHeadersComplete();
}

void URLRequestFileDirJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestFileDirJob::StartAsync));
}

void URLRequestFileDirJob::Kill() {
  if (canceled_)
    return;

  canceled_ = true;

  // Don't call CloseLister or dispatch an error to the URLRequest because
  // we want OnListDone to be called to also write the error to the output
  // stream. OnListDone will notify the URLRequest at this time.
  if (lister_)
    lister_->Cancel();

  URLRequestJob::Kill();

  method_factory_.RevokeAll();
}

bool URLRequestFileDirJob::ReadRawData(IOBuffer* buf, int buf_size,
                                       int *bytes_read) {
  DCHECK(bytes_read);
  *bytes_read = 0;

  if (is_done())
    return true;

  if (FillReadBuffer(buf->data(), buf_size, bytes_read))
    return true;

  // We are waiting for more data
  read_pending_ = true;
  read_buffer_ = buf;
  read_buffer_length_ = buf_size;
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  return false;
}

bool URLRequestFileDirJob::GetMimeType(std::string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

bool URLRequestFileDirJob::GetCharset(std::string* charset) {
  // All the filenames are converted to UTF-8 before being added.
  *charset = "utf-8";
  return true;
}

void URLRequestFileDirJob::OnListFile(
    const DirectoryLister::DirectoryListerData& data) {
  // We wait to write out the header until we get the first file, so that we
  // can catch errors from DirectoryLister and show an error page.
  if (!wrote_header_) {
#if defined(OS_WIN)
    const string16& title = dir_path_.value();
#elif defined(OS_POSIX)
    // TODO(jungshik): Add SysNativeMBToUTF16 to sys_string_conversions.
    // On Mac, need to add NFKC->NFC conversion either here or in file_path.
    // On Linux, the file system encoding is not defined, but we assume that
    // SysNativeMBToWide takes care of it at least for now. We can try something
    // more sophisticated if necessary later.
    const string16& title = WideToUTF16(
        base::SysNativeMBToWide(dir_path_.value()));
#endif
    data_.append(GetDirectoryListingHeader(title));
    wrote_header_ = true;
  }

#if defined(OS_WIN)
  int64 size = (static_cast<unsigned __int64>(data.info.nFileSizeHigh) << 32) |
      data.info.nFileSizeLow;

  // Note that we should not convert ftLastWriteTime to the local time because
  // ICU's datetime formatting APIs expect time in UTC and take into account
  // the timezone before formatting.
  data_.append(GetDirectoryListingEntry(
      data.info.cFileName, std::string(),
      (data.info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false,
      size,
      base::Time::FromFileTime(data.info.ftLastWriteTime)));

#elif defined(OS_POSIX)
  // TOOD(jungshik): The same issue as for the directory name.
  data_.append(GetDirectoryListingEntry(
      WideToUTF16(base::SysNativeMBToWide(data.info.filename)),
      data.info.filename,
      S_ISDIR(data.info.stat.st_mode),
      data.info.stat.st_size,
      base::Time::FromTimeT(data.info.stat.st_mtime)));
#endif

  // TODO(darin): coalesce more?
  CompleteRead();
}

void URLRequestFileDirJob::OnListDone(int error) {
  CloseLister();

  if (canceled_) {
    read_pending_ = false;
    // No need for NotifyCanceled() since canceled_ is set inside Kill().
  } else if (error) {
    read_pending_ = false;
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, error));
  } else {
    list_complete_ = true;
    CompleteRead();
  }

  Release();  // The Lister is finished; may delete *this*
}

URLRequestFileDirJob::~URLRequestFileDirJob() {
  DCHECK(read_pending_ == false);
  DCHECK(lister_ == NULL);
}

void URLRequestFileDirJob::CloseLister() {
  if (lister_) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
    lister_ = NULL;
  }
}

void URLRequestFileDirJob::CompleteRead() {
  if (read_pending_) {
    int bytes_read;
    if (FillReadBuffer(read_buffer_->data(), read_buffer_length_,
                       &bytes_read)) {
      // We completed the read, so reset the read buffer.
      read_pending_ = false;
      read_buffer_ = NULL;
      read_buffer_length_ = 0;

      SetStatus(URLRequestStatus());
      NotifyReadComplete(bytes_read);
    } else {
      NOTREACHED();
      // TODO: Better error code.
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, 0));
    }
  }
}

bool URLRequestFileDirJob::FillReadBuffer(char *buf, int buf_size,
                                          int *bytes_read) {
  DCHECK(bytes_read);

  *bytes_read = 0;

  int count = std::min(buf_size, static_cast<int>(data_.size()));
  if (count) {
    memcpy(buf, &data_[0], count);
    data_.erase(0, count);
    *bytes_read = count;
    return true;
  } else if (list_complete_) {
    // EOF
    return true;
  }
  return false;
}

}  // namespace net
