// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
#pragma once

#include <string>

#include "base/memory/weak_ptr.h"
#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/ftp/ftp_request_info.h"
#include "net/ftp/ftp_transaction.h"
#include "net/url_request/url_request_job.h"

namespace net {

class URLRequestContext;

// A URLRequestJob subclass that is built on top of FtpTransaction. It
// provides an implementation for FTP.
class URLRequestFtpJob : public URLRequestJob {
 public:
  explicit URLRequestFtpJob(URLRequest* request);

  static URLRequestJob* Factory(URLRequest* request,
                                const std::string& scheme);

  // Overridden from URLRequestJob:
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual HostPortPair GetSocketAddress() const OVERRIDE;

 private:
  virtual ~URLRequestFtpJob();

  void StartTransaction();

  void OnStartCompleted(int result);
  void OnReadCompleted(int result);

  void RestartTransactionWithAuth();

  void LogFtpServerType(char server_type);

  // Overridden from URLRequestJob:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual LoadState GetLoadState() const OVERRIDE;
  virtual bool NeedsAuth() OVERRIDE;
  virtual void GetAuthChallengeInfo(
      scoped_refptr<AuthChallengeInfo>* auth_info) OVERRIDE;
  virtual void SetAuth(const AuthCredentials& credentials) OVERRIDE;
  virtual void CancelAuth() OVERRIDE;

  // TODO(ibrar):  Yet to give another look at this function.
  virtual uint64 GetUploadProgress() const OVERRIDE;
  virtual bool ReadRawData(IOBuffer* buf,
                           int buf_size,
                           int *bytes_read) OVERRIDE;

  FtpRequestInfo request_info_;
  scoped_ptr<FtpTransaction> transaction_;

  bool read_in_progress_;

  scoped_refptr<AuthData> server_auth_;

  base::WeakPtrFactory<URLRequestFtpJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFtpJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
