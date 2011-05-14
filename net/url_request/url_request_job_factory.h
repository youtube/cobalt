// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_H_
#pragma once

#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"

class GURL;

namespace net {

class URLRequest;
class URLRequestJob;

class URLRequestJobFactory : public base::NonThreadSafe {
 public:
  class ProtocolHandler {
   public:
    virtual ~ProtocolHandler();

    virtual URLRequestJob* MaybeCreateJob(URLRequest* request) const = 0;
  };

  class Interceptor {
   public:
    virtual ~Interceptor();

    // Called for every request made.  Should return a new job to handle the
    // request if it should be intercepted, or NULL to allow the request to
    // be handled in the normal manner.
    virtual URLRequestJob* MaybeIntercept(URLRequest* request) const = 0;

    // Called after having received a redirect response, but prior to the
    // the request delegate being informed of the redirect. Can return a new
    // job to replace the existing job if it should be intercepted, or NULL
    // to allow the normal handling to continue. If a new job is provided,
    // the delegate never sees the original redirect response, instead the
    // response produced by the intercept job will be returned.
    virtual URLRequestJob* MaybeInterceptRedirect(
        const GURL& location,
        URLRequest* request) const = 0;

    // Called after having received a final response, but prior to the
    // the request delegate being informed of the response. This is also
    // called when there is no server response at all to allow interception
    // on DNS or network errors. Can return a new job to replace the existing
    // job if it should be intercepted, or NULL to allow the normal handling to
    // continue. If a new job is provided, the delegate never sees the original
    // response, instead the response produced by the intercept job will be
    // returned.
    virtual URLRequestJob* MaybeInterceptResponse(
        URLRequest* request) const = 0;
  };

  URLRequestJobFactory();
  ~URLRequestJobFactory();

  // Sets the ProtocolHandler for a scheme. Returns true on success, false on
  // failure (a ProtocolHandler already exists for |scheme|). On success,
  // URLRequestJobFactory takes ownership of |protocol_handler|.
  bool SetProtocolHandler(const std::string& scheme,
                          ProtocolHandler* protocol_handler);

  // Takes ownership of |interceptor|. Adds it to the end of the Interceptor
  // list.
  void AddInterceptor(Interceptor* interceptor);

  URLRequestJob* MaybeCreateJobWithInterceptor(URLRequest* request) const;

  URLRequestJob* MaybeCreateJobWithProtocolHandler(const std::string& scheme,
                                                   URLRequest* request) const;

  bool IsHandledProtocol(const std::string& scheme) const;

  bool IsHandledURL(const GURL& url) const;

 private:
  typedef std::map<std::string, ProtocolHandler*> ProtocolHandlerMap;
  typedef std::vector<Interceptor*> InterceptorList;

  ProtocolHandlerMap protocol_handler_map_;
  InterceptorList interceptors_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestJobFactory);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_FACTORY_H_
