// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory_impl.h"

#include "base/stl_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_job_manager.h"

namespace net {

URLRequestJobFactoryImpl::URLRequestJobFactoryImpl() {}

URLRequestJobFactoryImpl::~URLRequestJobFactoryImpl() {
  STLDeleteValues(&protocol_handler_map_);
  STLDeleteElements(&interceptors_);
}

bool URLRequestJobFactoryImpl::SetProtocolHandler(
    const std::string& scheme,
    ProtocolHandler* protocol_handler) {
  DCHECK(CalledOnValidThread());

  if (!protocol_handler) {
    ProtocolHandlerMap::iterator it = protocol_handler_map_.find(scheme);
    if (it == protocol_handler_map_.end())
      return false;

    delete it->second;
    protocol_handler_map_.erase(it);
    return true;
  }

  if (ContainsKey(protocol_handler_map_, scheme))
    return false;
  protocol_handler_map_[scheme] = protocol_handler;
  return true;
}

void URLRequestJobFactoryImpl::AddInterceptor(Interceptor* interceptor) {
  DCHECK(CalledOnValidThread());
  CHECK(interceptor);

  interceptors_.push_back(interceptor);
}

URLRequestJob* URLRequestJobFactoryImpl::MaybeCreateJobWithInterceptor(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  DCHECK(CalledOnValidThread());
  URLRequestJob* job = NULL;

  if (!(request->load_flags() & LOAD_DISABLE_INTERCEPT)) {
    InterceptorList::const_iterator i;
    for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
      job = (*i)->MaybeIntercept(request, network_delegate);
      if (job)
        return job;
    }
  }
  return NULL;
}

URLRequestJob* URLRequestJobFactoryImpl::MaybeCreateJobWithProtocolHandler(
    const std::string& scheme,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  DCHECK(CalledOnValidThread());
  ProtocolHandlerMap::const_iterator it = protocol_handler_map_.find(scheme);
  if (it == protocol_handler_map_.end())
    return NULL;
  return it->second->MaybeCreateJob(request, network_delegate);
}

URLRequestJob* URLRequestJobFactoryImpl::MaybeInterceptRedirect(
    const GURL& location,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  DCHECK(CalledOnValidThread());
  URLRequestJob* job = NULL;

  if (!(request->load_flags() & LOAD_DISABLE_INTERCEPT)) {
    InterceptorList::const_iterator i;
    for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
      job = (*i)->MaybeInterceptRedirect(location, request, network_delegate);
      if (job)
        return job;
    }
  }
  return NULL;
}

URLRequestJob* URLRequestJobFactoryImpl::MaybeInterceptResponse(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  DCHECK(CalledOnValidThread());
  URLRequestJob* job = NULL;

  if (!(request->load_flags() & LOAD_DISABLE_INTERCEPT)) {
    InterceptorList::const_iterator i;
    for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
      job = (*i)->MaybeInterceptResponse(request, network_delegate);
      if (job)
        return job;
    }
  }
  return NULL;
}

bool URLRequestJobFactoryImpl::IsHandledProtocol(
    const std::string& scheme) const {
  DCHECK(CalledOnValidThread());
  InterceptorList::const_iterator i;
  for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
    if ((*i)->WillHandleProtocol(scheme))
      return true;
  }
  return ContainsKey(protocol_handler_map_, scheme) ||
      URLRequestJobManager::GetInstance()->SupportsScheme(scheme);
}

bool URLRequestJobFactoryImpl::IsHandledURL(const GURL& url) const {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }
  return IsHandledProtocol(url.scheme());
}

}  // namespace net
