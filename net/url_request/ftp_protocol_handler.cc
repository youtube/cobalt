// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/ftp_protocol_handler.h"

#include "base/logging.h"
#include "net/url_request/url_request_ftp_job.h"

namespace net {

FtpProtocolHandler::FtpProtocolHandler(
    NetworkDelegate* network_delegate,
    FtpTransactionFactory* ftp_transaction_factory,
    FtpAuthCache* ftp_auth_cache)
    : network_delegate_(network_delegate),
      ftp_transaction_factory_(ftp_transaction_factory),
      ftp_auth_cache_(ftp_auth_cache) {
  DCHECK(ftp_transaction_factory_);
  DCHECK(ftp_auth_cache_);
}

URLRequestJob* FtpProtocolHandler::MaybeCreateJob(
    URLRequest* request) const {
  return new URLRequestFtpJob(request,
                              network_delegate_,
                              ftp_transaction_factory_,
                              ftp_auth_cache_);
}

}  // namespace net
