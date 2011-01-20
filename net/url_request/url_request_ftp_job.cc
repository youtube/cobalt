// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_ftp_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"

namespace net {

URLRequestFtpJob::URLRequestFtpJob(URLRequest* request)
    : URLRequestJob(request),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          start_callback_(this, &URLRequestFtpJob::OnStartCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &URLRequestFtpJob::OnReadCompleted)),
      read_in_progress_(false),
      context_(request->context()),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

// static
URLRequestJob* URLRequestFtpJob::Factory(URLRequest* request,
                                         const std::string& scheme) {
  DCHECK_EQ(scheme, "ftp");

  int port = request->url().IntPort();
  if (request->url().has_port() &&
    !IsPortAllowedByFtp(port) && !IsPortAllowedByOverride(port))
    return new URLRequestErrorJob(request, ERR_UNSAFE_PORT);

  DCHECK(request->context());
  DCHECK(request->context()->ftp_transaction_factory());
  return new URLRequestFtpJob(request);
}

bool URLRequestFtpJob::GetMimeType(std::string* mime_type) const {
  if (transaction_->GetResponseInfo()->is_directory_listing) {
    *mime_type = "text/vnd.chromium.ftp-dir";
    return true;
  }
  return false;
}

URLRequestFtpJob::~URLRequestFtpJob() {
}

void URLRequestFtpJob::StartTransaction() {
  // Create a transaction.
  DCHECK(!transaction_.get());
  DCHECK(request_->context());
  DCHECK(request_->context()->ftp_transaction_factory());

  transaction_.reset(
      request_->context()->ftp_transaction_factory()->CreateTransaction());

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  int rv;
  if (transaction_.get()) {
    rv = transaction_->Start(
        &request_info_, &start_callback_, request_->net_log());
    if (rv == ERR_IO_PENDING)
      return;
  } else {
    rv = ERR_FAILED;
  }
  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestFtpJob::OnStartCompleted, rv));
}

void URLRequestFtpJob::OnStartCompleted(int result) {
  // Clear the IO_PENDING status
  SetStatus(URLRequestStatus());

  // FTP obviously doesn't have HTTP Content-Length header. We have to pass
  // the content size information manually.
  set_expected_content_size(
      transaction_->GetResponseInfo()->expected_content_size);

  if (result == OK) {
    NotifyHeadersComplete();
  } else if (transaction_->GetResponseInfo()->needs_auth) {
    GURL origin = request_->url().GetOrigin();
    if (server_auth_ && server_auth_->state == AUTH_STATE_HAVE_AUTH) {
      request_->context()->ftp_auth_cache()->Remove(origin,
                                                    server_auth_->username,
                                                    server_auth_->password);
    } else if (!server_auth_) {
      server_auth_ = new AuthData();
    }
    server_auth_->state = AUTH_STATE_NEED_AUTH;

    FtpAuthCache::Entry* cached_auth =
        request_->context()->ftp_auth_cache()->Lookup(origin);

    if (cached_auth) {
      // Retry using cached auth data.
      SetAuth(cached_auth->username, cached_auth->password);
    } else {
      // Prompt for a username/password.
      NotifyHeadersComplete();
    }
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestFtpJob::OnReadCompleted(int result) {
  read_in_progress_ = false;
  if (result == 0) {
    NotifyDone(URLRequestStatus());
  } else if (result < 0) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  } else {
    // Clear the IO_PENDING status
    SetStatus(URLRequestStatus());
  }
  NotifyReadComplete(result);
}

void URLRequestFtpJob::RestartTransactionWithAuth() {
  DCHECK(server_auth_ && server_auth_->state == AUTH_STATE_HAVE_AUTH);

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartWithAuth(server_auth_->username,
                                         server_auth_->password,
                                         &start_callback_);
  if (rv == ERR_IO_PENDING)
    return;

  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestFtpJob::OnStartCompleted, rv));
}

void URLRequestFtpJob::Start() {
  DCHECK(!transaction_.get());
  request_info_.url = request_->url();
  StartTransaction();
}

void URLRequestFtpJob::Kill() {
  if (!transaction_.get())
    return;
  transaction_.reset();
  URLRequestJob::Kill();
  method_factory_.RevokeAll();
}

LoadState URLRequestFtpJob::GetLoadState() const {
  return transaction_.get() ?
      transaction_->GetLoadState() : LOAD_STATE_IDLE;
}

bool URLRequestFtpJob::NeedsAuth() {
  // Note that we only have to worry about cases where an actual FTP server
  // requires auth (and not a proxy), because connecting to FTP via proxy
  // effectively means the browser communicates via HTTP, and uses HTTP's
  // Proxy-Authenticate protocol when proxy servers require auth.
  return server_auth_ && server_auth_->state == AUTH_STATE_NEED_AUTH;
}

void URLRequestFtpJob::GetAuthChallengeInfo(
    scoped_refptr<AuthChallengeInfo>* result) {
  DCHECK((server_auth_ != NULL) &&
         (server_auth_->state == AUTH_STATE_NEED_AUTH));
  scoped_refptr<AuthChallengeInfo> auth_info(new AuthChallengeInfo);
  auth_info->is_proxy = false;
  auth_info->host_and_port = ASCIIToWide(
      GetHostAndPort(request_->url()));
  auth_info->scheme = L"";
  auth_info->realm = L"";
  result->swap(auth_info);
}

void URLRequestFtpJob::SetAuth(const string16& username,
                               const string16& password) {
  DCHECK(NeedsAuth());
  server_auth_->state = AUTH_STATE_HAVE_AUTH;
  server_auth_->username = username;
  server_auth_->password = password;

  request_->context()->ftp_auth_cache()->Add(request_->url().GetOrigin(),
                                             username, password);

  RestartTransactionWithAuth();
}

void URLRequestFtpJob::CancelAuth() {
  DCHECK(NeedsAuth());
  server_auth_->state = AUTH_STATE_CANCELED;

  // Once the auth is cancelled, we proceed with the request as though
  // there were no auth.  Schedule this for later so that we don't cause
  // any recursing into the caller as a result of this call.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestFtpJob::OnStartCompleted, OK));
}

uint64 URLRequestFtpJob::GetUploadProgress() const {
  return 0;
}

bool URLRequestFtpJob::ReadRawData(IOBuffer* buf,
                                   int buf_size,
                                   int *bytes_read) {
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!read_in_progress_);

  int rv = transaction_->Read(buf, buf_size, &read_callback_);
  if (rv >= 0) {
    *bytes_read = rv;
    return true;
  }

  if (rv == ERR_IO_PENDING) {
    read_in_progress_ = true;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }
  return false;
}

}  // namespace net
