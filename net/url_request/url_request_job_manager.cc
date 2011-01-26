// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_manager.h"

#include <algorithm>

#include "build/build_config.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_about_job.h"
#include "net/url_request/url_request_data_job.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_ftp_job.h"
#include "net/url_request/url_request_http_job.h"

// The built-in set of protocol factories
namespace {

struct SchemeToFactory {
  const char* scheme;
  net::URLRequest::ProtocolFactory* factory;
};

}  // namespace

namespace net {

static const SchemeToFactory kBuiltinFactories[] = {
  { "http", URLRequestHttpJob::Factory },
  { "https", URLRequestHttpJob::Factory },
  { "file", URLRequestFileJob::Factory },
  { "ftp", URLRequestFtpJob::Factory },
  { "about", URLRequestAboutJob::Factory },
  { "data", URLRequestDataJob::Factory },
};

// static
URLRequestJobManager* URLRequestJobManager::GetInstance() {
  return Singleton<URLRequestJobManager>::get();
}

net::URLRequestJob* URLRequestJobManager::CreateJob(
    net::URLRequest* request) const {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif

  // If we are given an invalid URL, then don't even try to inspect the scheme.
  if (!request->url().is_valid())
    return new net::URLRequestErrorJob(request, net::ERR_INVALID_URL);

  // We do this here to avoid asking interceptors about unsupported schemes.
  const std::string& scheme = request->url().scheme();  // already lowercase
  if (!SupportsScheme(scheme))
    return new net::URLRequestErrorJob(request, net::ERR_UNKNOWN_URL_SCHEME);

  // THREAD-SAFETY NOTICE:
  //   We do not need to acquire the lock here since we are only reading our
  //   data structures.  They should only be modified on the current thread.

  // See if the request should be intercepted.
  if (!(request->load_flags() & net::LOAD_DISABLE_INTERCEPT)) {
    InterceptorList::const_iterator i;
    for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
      net::URLRequestJob* job = (*i)->MaybeIntercept(request);
      if (job)
        return job;
    }
  }

  // See if the request should be handled by a registered protocol factory.
  // If the registered factory returns null, then we want to fall-back to the
  // built-in protocol factory.
  FactoryMap::const_iterator i = factories_.find(scheme);
  if (i != factories_.end()) {
    net::URLRequestJob* job = i->second(request, scheme);
    if (job)
      return job;
  }

  // See if the request should be handled by a built-in protocol factory.
  for (size_t i = 0; i < arraysize(kBuiltinFactories); ++i) {
    if (scheme == kBuiltinFactories[i].scheme) {
      net::URLRequestJob* job = (kBuiltinFactories[i].factory)(request, scheme);
      DCHECK(job);  // The built-in factories are not expected to fail!
      return job;
    }
  }

  // If we reached here, then it means that a registered protocol factory
  // wasn't interested in handling the URL.  That is fairly unexpected, and we
  // don't know have a specific error to report here :-(
  LOG(WARNING) << "Failed to map: " << request->url().spec();
  return new net::URLRequestErrorJob(request, net::ERR_FAILED);
}

net::URLRequestJob* URLRequestJobManager::MaybeInterceptRedirect(
    net::URLRequest* request,
    const GURL& location) const {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif
  if ((request->load_flags() & net::LOAD_DISABLE_INTERCEPT) ||
      (request->status().status() == URLRequestStatus::CANCELED) ||
      !request->url().is_valid() ||
      !SupportsScheme(request->url().scheme()))
    return NULL;

  InterceptorList::const_iterator i;
  for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
    net::URLRequestJob* job = (*i)->MaybeInterceptRedirect(request, location);
    if (job)
      return job;
  }
  return NULL;
}

net::URLRequestJob* URLRequestJobManager::MaybeInterceptResponse(
    net::URLRequest* request) const {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif
  if ((request->load_flags() & net::LOAD_DISABLE_INTERCEPT) ||
      (request->status().status() == URLRequestStatus::CANCELED) ||
      !request->url().is_valid() ||
      !SupportsScheme(request->url().scheme()))
    return NULL;

  InterceptorList::const_iterator i;
  for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
    net::URLRequestJob* job = (*i)->MaybeInterceptResponse(request);
    if (job)
      return job;
  }
  return NULL;
}

bool URLRequestJobManager::SupportsScheme(const std::string& scheme) const {
  // The set of registered factories may change on another thread.
  {
    base::AutoLock locked(lock_);
    if (factories_.find(scheme) != factories_.end())
      return true;
  }

  for (size_t i = 0; i < arraysize(kBuiltinFactories); ++i)
    if (LowerCaseEqualsASCII(scheme, kBuiltinFactories[i].scheme))
      return true;

  return false;
}

net::URLRequest::ProtocolFactory* URLRequestJobManager::RegisterProtocolFactory(
    const std::string& scheme,
    net::URLRequest::ProtocolFactory* factory) {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif

  base::AutoLock locked(lock_);

  net::URLRequest::ProtocolFactory* old_factory;
  FactoryMap::iterator i = factories_.find(scheme);
  if (i != factories_.end()) {
    old_factory = i->second;
  } else {
    old_factory = NULL;
  }
  if (factory) {
    factories_[scheme] = factory;
  } else if (i != factories_.end()) {  // uninstall any old one
    factories_.erase(i);
  }
  return old_factory;
}

void URLRequestJobManager::RegisterRequestInterceptor(
    net::URLRequest::Interceptor* interceptor) {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif

  base::AutoLock locked(lock_);

  DCHECK(std::find(interceptors_.begin(), interceptors_.end(), interceptor) ==
         interceptors_.end());
  interceptors_.push_back(interceptor);
}

void URLRequestJobManager::UnregisterRequestInterceptor(
    net::URLRequest::Interceptor* interceptor) {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif

  base::AutoLock locked(lock_);

  InterceptorList::iterator i =
      std::find(interceptors_.begin(), interceptors_.end(), interceptor);
  DCHECK(i != interceptors_.end());
  interceptors_.erase(i);
}

URLRequestJobManager::URLRequestJobManager() : enable_file_access_(false) {
#ifndef NDEBUG
  allowed_thread_ = 0;
  allowed_thread_initialized_ = false;
#endif
}

URLRequestJobManager::~URLRequestJobManager() {}

}  // namespace net
