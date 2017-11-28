// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_IMPL_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_IMPL_H_

#include <map>
#include <vector>
#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {

class NET_EXPORT URLRequestJobFactoryImpl : public URLRequestJobFactory {
 public:
  URLRequestJobFactoryImpl();
  virtual ~URLRequestJobFactoryImpl();

  // URLRequestJobFactory implementation
  virtual bool SetProtocolHandler(const std::string& scheme,
                          ProtocolHandler* protocol_handler) override;
  virtual void AddInterceptor(Interceptor* interceptor) override;
  virtual URLRequestJob* MaybeCreateJobWithInterceptor(
      URLRequest* request, NetworkDelegate* network_delegate) const override;
  virtual URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      URLRequest* request,
      NetworkDelegate* network_delegate) const override;
  virtual URLRequestJob* MaybeInterceptRedirect(
      const GURL& location,
      URLRequest* request,
      NetworkDelegate* network_delegate) const override;
  virtual URLRequestJob* MaybeInterceptResponse(
      URLRequest* request, NetworkDelegate* network_delegate) const override;
  virtual bool IsHandledProtocol(const std::string& scheme) const override;
  virtual bool IsHandledURL(const GURL& url) const override;

 private:
  typedef std::map<std::string, ProtocolHandler*> ProtocolHandlerMap;
  typedef std::vector<Interceptor*> InterceptorList;

  ProtocolHandlerMap protocol_handler_map_;
  InterceptorList interceptors_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestJobFactoryImpl);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_IMPL_H_
