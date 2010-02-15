// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_FACTORY_H
#define NET_HTTP_HTTP_AUTH_HANDLER_FACTORY_H

#include <map>

#include "net/http/http_auth.h"

class GURL;

namespace net {

class HttpAuthHandler;

// An HttpAuthHandlerFactory is used to create HttpAuthHandler objects.
class HttpAuthHandlerFactory {
 public:
  HttpAuthHandlerFactory() {}
  virtual ~HttpAuthHandlerFactory() {}

  // Creates an HttpAuthHandler object based on the authentication
  // challenge specified by |*challenge|. |challenge| must point to a valid
  // non-NULL tokenizer.
  //
  // If an HttpAuthHandler object  is successfully created it is passed back to
  // the caller through |*handler| and OK is returned.
  //
  // If |*challenge| specifies an unsupported authentication scheme, |*handler|
  // is set to NULL and ERR_UNSUPPORTED_AUTH_SCHEME is returned.
  //
  // If |*challenge| is improperly formed, |*handler| is set to NULL and
  // ERR_INVALID_RESPONSE is returned.
  //
  // |*challenge| should not be reused after a call to |CreateAuthHandler()|,
  virtual int CreateAuthHandler(HttpAuth::ChallengeTokenizer* challenge,
                                HttpAuth::Target target,
                                const GURL& origin,
                                scoped_refptr<HttpAuthHandler>* handler) = 0;

  // Creates an HTTP authentication handler based on the authentication
  // challenge string |challenge|.
  // This is a convenience function which creates a ChallengeTokenizer for
  // |challenge| and calls |CreateAuthHandler|. See |CreateAuthHandler| for
  // more details on return values.
  int CreateAuthHandlerFromString(const std::string& challenge,
                                  HttpAuth::Target target,
                                  const GURL& origin,
                                  scoped_refptr<HttpAuthHandler>* handler);

  // Creates a standard HttpAuthHandlerFactory. The caller is responsible
  // for deleting the factory.
  // The default factory support Basic, Digest, NTLM, and Negotiate schemes.
  static HttpAuthHandlerFactory* CreateDefault();

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpAuthHandlerFactory);
};

// The HttpAuthHandlerRegistryFactory dispatches create requests out
// to other factories based on the auth scheme.
class HttpAuthHandlerRegistryFactory : public HttpAuthHandlerFactory {
 public:
  HttpAuthHandlerRegistryFactory();
  virtual ~HttpAuthHandlerRegistryFactory();

  // Registers a |factory| that will be used for a particular HTTP
  // authentication scheme such as Basic, Digest, or Negotiate.
  // The |*factory| object is assumed to be new-allocated, and its lifetime
  // will be managed by this HttpAuthHandlerRegistryFactory object (including
  // deleting it when it is no longer used.
  // A NULL |factory| value means that HttpAuthHandlers's will not be created
  // for |scheme|. If a factory object used to exist for |scheme|, it will be
  // deleted.
  void RegisterSchemeFactory(const std::string& scheme,
                             HttpAuthHandlerFactory* factory);

  // Creates an auth handler by dispatching out to the registered factories
  // based on the first token in |challenge|.
  virtual int CreateAuthHandler(HttpAuth::ChallengeTokenizer* challenge,
                                HttpAuth::Target target,
                                const GURL& origin,
                                scoped_refptr<HttpAuthHandler>* handler);

 private:
  typedef std::map<std::string, HttpAuthHandlerFactory*> FactoryMap;

  FactoryMap factory_map_;
  DISALLOW_COPY_AND_ASSIGN(HttpAuthHandlerRegistryFactory);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_FACTORY_H
