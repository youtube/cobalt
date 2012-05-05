// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_manager.h"

#include <algorithm>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "base/string_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/url_request/url_request_about_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_data_job.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_ftp_job.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {

// The built-in set of protocol factories
namespace {

struct SchemeToFactory {
  const char* scheme;
  URLRequest::ProtocolFactory* factory;
};

}  // namespace

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

URLRequestJob* URLRequestJobManager::CreateJob(
    URLRequest* request) const {
  DCHECK(IsAllowedThread());

  // If we are given an invalid URL, then don't even try to inspect the scheme.
  if (!request->url().is_valid())
    return new URLRequestErrorJob(request, ERR_INVALID_URL);

  // We do this here to avoid asking interceptors about unsupported schemes.
  const URLRequestJobFactory* job_factory = NULL;
  if (request->context())
    job_factory = request->context()->job_factory();

  const std::string& scheme = request->url().scheme();  // already lowercase
  if (job_factory) {
    if (!job_factory->IsHandledProtocol(scheme)) {
      return new URLRequestErrorJob(request, ERR_UNKNOWN_URL_SCHEME);
    }
  } else if (!SupportsScheme(scheme)) {
    return new URLRequestErrorJob(request, ERR_UNKNOWN_URL_SCHEME);
  }

  // THREAD-SAFETY NOTICE:
  //   We do not need to acquire the lock here since we are only reading our
  //   data structures.  They should only be modified on the current thread.

  // See if the request should be intercepted.
  //

  if (job_factory) {
    URLRequestJob* job = job_factory->MaybeCreateJobWithInterceptor(request);
    if (job)
      return job;
  }

  // TODO(willchan): Remove this in favor of URLRequestJobFactory::Interceptor.
  if (!(request->load_flags() & LOAD_DISABLE_INTERCEPT)) {
    InterceptorList::const_iterator i;
    for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
      URLRequestJob* job = (*i)->MaybeIntercept(request);
      if (job)
        return job;
    }
  }

  if (job_factory) {
    URLRequestJob* job =
        job_factory->MaybeCreateJobWithProtocolHandler(scheme, request);
    if (job)
      return job;
  }

  // TODO(willchan): Remove this in favor of
  // URLRequestJobFactory::ProtocolHandler.
  // See if the request should be handled by a registered protocol factory.
  // If the registered factory returns null, then we want to fall-back to the
  // built-in protocol factory.
  FactoryMap::const_iterator i = factories_.find(scheme);
  if (i != factories_.end()) {
    URLRequestJob* job = i->second(request, scheme);
    if (job)
      return job;
  }

  // See if the request should be handled by a built-in protocol factory.
  for (size_t i = 0; i < arraysize(kBuiltinFactories); ++i) {
    if (scheme == kBuiltinFactories[i].scheme) {
      URLRequestJob* job = (kBuiltinFactories[i].factory)(request, scheme);
      DCHECK(job);  // The built-in factories are not expected to fail!
      return job;
    }
  }

  // If we reached here, then it means that a registered protocol factory
  // wasn't interested in handling the URL.  That is fairly unexpected, and we
  // don't have a specific error to report here :-(
  LOG(WARNING) << "Failed to map: " << request->url().spec();
  return new URLRequestErrorJob(request, ERR_FAILED);
}

URLRequestJob* URLRequestJobManager::MaybeInterceptRedirect(
    URLRequest* request,
    const GURL& location) const {
  DCHECK(IsAllowedThread());
  if (!request->url().is_valid() ||
      request->load_flags() & LOAD_DISABLE_INTERCEPT ||
      request->status().status() == URLRequestStatus::CANCELED) {
    return NULL;
  }

  const URLRequestJobFactory* job_factory = NULL;
  if (request->context())
    job_factory = request->context()->job_factory();

  const std::string& scheme = request->url().scheme();  // already lowercase
  if (job_factory) {
    if (!job_factory->IsHandledProtocol(scheme)) {
      return NULL;
    }
  } else if (!SupportsScheme(scheme)) {
    return NULL;
  }

  URLRequestJob* job = NULL;
  if (job_factory)
    job = job_factory->MaybeInterceptRedirect(location, request);
  if (job)
    return job;

  InterceptorList::const_iterator i;
  for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
    job = (*i)->MaybeInterceptRedirect(request, location);
    if (job)
      return job;
  }
  return NULL;
}

URLRequestJob* URLRequestJobManager::MaybeInterceptResponse(
    URLRequest* request) const {
  DCHECK(IsAllowedThread());
  if (!request->url().is_valid() ||
      request->load_flags() & LOAD_DISABLE_INTERCEPT ||
      request->status().status() == URLRequestStatus::CANCELED) {
    return NULL;
  }

  const URLRequestJobFactory* job_factory = NULL;
  if (request->context())
    job_factory = request->context()->job_factory();

  const std::string& scheme = request->url().scheme();  // already lowercase
  if (job_factory) {
    if (!job_factory->IsHandledProtocol(scheme)) {
      return NULL;
    }
  } else if (!SupportsScheme(scheme)) {
    return NULL;
  }

  URLRequestJob* job = NULL;
  if (job_factory)
    job = job_factory->MaybeInterceptResponse(request);
  if (job)
    return job;

  InterceptorList::const_iterator i;
  for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
    job = (*i)->MaybeInterceptResponse(request);
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

URLRequest::ProtocolFactory* URLRequestJobManager::RegisterProtocolFactory(
    const std::string& scheme,
    URLRequest::ProtocolFactory* factory) {
  DCHECK(IsAllowedThread());

  base::AutoLock locked(lock_);

  URLRequest::ProtocolFactory* old_factory;
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
    URLRequest::Interceptor* interceptor) {
  DCHECK(IsAllowedThread());

  base::AutoLock locked(lock_);

  DCHECK(std::find(interceptors_.begin(), interceptors_.end(), interceptor) ==
         interceptors_.end());
  interceptors_.push_back(interceptor);
}

void URLRequestJobManager::UnregisterRequestInterceptor(
    URLRequest::Interceptor* interceptor) {
  DCHECK(IsAllowedThread());

  base::AutoLock locked(lock_);

  InterceptorList::iterator i =
      std::find(interceptors_.begin(), interceptors_.end(), interceptor);
  DCHECK(i != interceptors_.end());
  interceptors_.erase(i);
}

URLRequestJobManager::URLRequestJobManager()
    : allowed_thread_(0),
      allowed_thread_initialized_(false) {
}

URLRequestJobManager::~URLRequestJobManager() {}

}  // namespace net
