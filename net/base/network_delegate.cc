// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_delegate.h"

#include "base/logging.h"

namespace net {

int NetworkDelegate::NotifyBeforeURLRequest(URLRequest* request,
                                            CompletionCallback* callback,
                                            GURL* new_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  DCHECK(callback);
  return OnBeforeURLRequest(request, callback, new_url);
}

int NetworkDelegate::NotifyBeforeSendHeaders(URLRequest* request,
                                             CompletionCallback* callback,
                                             HttpRequestHeaders* headers) {
  DCHECK(CalledOnValidThread());
  DCHECK(headers);
  DCHECK(callback);
  return OnBeforeSendHeaders(request, callback, headers);
}

void NetworkDelegate::NotifySendHeaders(URLRequest* request,
                                        const HttpRequestHeaders& headers) {
  DCHECK(CalledOnValidThread());
  OnSendHeaders(request, headers);
}

void NetworkDelegate::NotifyResponseStarted(URLRequest* request) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  OnResponseStarted(request);
}

void NetworkDelegate::NotifyRawBytesRead(const URLRequest& request,
                                         int bytes_read) {
  DCHECK(CalledOnValidThread());
  OnRawBytesRead(request, bytes_read);
}

void NetworkDelegate::NotifyBeforeRedirect(URLRequest* request,
                                           const GURL& new_location) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  OnBeforeRedirect(request, new_location);
}

void NetworkDelegate::NotifyCompleted(URLRequest* request) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  OnCompleted(request);
}

void NetworkDelegate::NotifyURLRequestDestroyed(URLRequest* request) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  OnURLRequestDestroyed(request);
}

void NetworkDelegate::NotifyPACScriptError(int line_number,
                                           const string16& error) {
  DCHECK(CalledOnValidThread());
  OnPACScriptError(line_number, error);
}

void NetworkDelegate::NotifyAuthRequired(URLRequest* request,
                                         const AuthChallengeInfo& auth_info) {
  DCHECK(CalledOnValidThread());
  OnAuthRequired(request, auth_info);
}

}  // namespace net
