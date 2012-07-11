// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_FTP_PROTOCOL_HANDLER_H_
#define NET_URL_REQUEST_FTP_PROTOCOL_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {

class FtpAuthCache;
class FtpTransactionFactory;
class NetworkDelegate;
class URLRequestJob;

// Implements a ProtocolHandler for FTP.
class NET_EXPORT FtpProtocolHandler :
    public URLRequestJobFactory::ProtocolHandler {
 public:
  FtpProtocolHandler(NetworkDelegate* network_delegate,
                     FtpTransactionFactory* ftp_transaction_factory,
                     FtpAuthCache* ftp_auth_cache);
  virtual URLRequestJob* MaybeCreateJob(URLRequest* request) const OVERRIDE;

 private:
  NetworkDelegate* network_delegate_;
  FtpTransactionFactory* ftp_transaction_factory_;
  FtpAuthCache* ftp_auth_cache_;

  DISALLOW_COPY_AND_ASSIGN(FtpProtocolHandler);
};

}  // namespace net

#endif  // NET_URL_REQUEST_FTP_PROTOCOL_HANDLER_H_
