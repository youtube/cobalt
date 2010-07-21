// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_controller.h"

#include "base/string_util.h"
#include "net/base/host_resolver.h"
#include "net/base/net_util.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"

namespace net {

namespace {

// Returns a log message for all the response headers related to the auth
// challenge.
std::string AuthChallengeLogMessage(HttpResponseHeaders* headers) {
  std::string msg;
  std::string header_val;
  void* iter = NULL;
  while (headers->EnumerateHeader(&iter, "proxy-authenticate", &header_val)) {
    msg.append("\n  Has header Proxy-Authenticate: ");
    msg.append(header_val);
  }

  iter = NULL;
  while (headers->EnumerateHeader(&iter, "www-authenticate", &header_val)) {
    msg.append("\n  Has header WWW-Authenticate: ");
    msg.append(header_val);
  }

  // RFC 4559 requires that a proxy indicate its support of NTLM/Negotiate
  // authentication with a "Proxy-Support: Session-Based-Authentication"
  // response header.
  iter = NULL;
  while (headers->EnumerateHeader(&iter, "proxy-support", &header_val)) {
    msg.append("\n  Has header Proxy-Support: ");
    msg.append(header_val);
  }

  return msg;
}

}  // namespace

HttpAuthController::HttpAuthController(
    HttpAuth::Target target,
    const GURL& auth_url,
    scoped_refptr<HttpNetworkSession> session)
    : target_(target),
      auth_url_(auth_url),
      auth_origin_(auth_url.GetOrigin()),
      auth_path_(HttpAuth::AUTH_PROXY ? std::string() : auth_url.path()),
      embedded_identity_used_(false),
      default_credentials_used_(false),
      session_(session),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &HttpAuthController::OnIOComplete)),
      user_callback_(NULL) {
}

HttpAuthController::~HttpAuthController() {
  user_callback_ = NULL;
}

int HttpAuthController::MaybeGenerateAuthToken(const HttpRequestInfo* request,
                                               CompletionCallback* callback,
                                               const BoundNetLog& net_log) {
  bool needs_auth = HaveAuth() || SelectPreemptiveAuth(net_log);
  if (!needs_auth)
    return OK;
  const std::wstring* username = NULL;
  const std::wstring* password = NULL;
  if (identity_.source != HttpAuth::IDENT_SRC_DEFAULT_CREDENTIALS) {
    username = &identity_.username;
    password = &identity_.password;
  }
  DCHECK(auth_token_.empty());
  DCHECK(NULL == user_callback_);
  int rv = handler_->GenerateAuthToken(username,
                                       password,
                                       request,
                                       &io_callback_,
                                       &auth_token_);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  else
    OnIOComplete(rv);
  // This error occurs with GSSAPI, if the user has not already logged in.
  if (rv == ERR_MISSING_AUTH_CREDENTIALS)
    rv = OK;
  return rv;
}

bool HttpAuthController::SelectPreemptiveAuth(const BoundNetLog& net_log) {
  DCHECK(!HaveAuth());
  DCHECK(identity_.invalid);

  // Don't do preemptive authorization if the URL contains a username/password,
  // since we must first be challenged in order to use the URL's identity.
  if (auth_url_.has_username())
    return false;

  // SelectPreemptiveAuth() is on the critical path for each request, so it
  // is expected to be fast. LookupByPath() is fast in the common case, since
  // the number of http auth cache entries is expected to be very small.
  // (For most users in fact, it will be 0.)
  HttpAuthCache::Entry* entry = session_->auth_cache()->LookupByPath(
      auth_origin_, auth_path_);
  if (!entry)
    return false;

  // Try to create a handler using the previous auth challenge.
  scoped_ptr<HttpAuthHandler> handler_preemptive;
  int rv_create = session_->http_auth_handler_factory()->
      CreatePreemptiveAuthHandlerFromString(entry->auth_challenge(), target_,
                                            auth_origin_,
                                            entry->IncrementNonceCount(),
                                            net_log, &handler_preemptive);
  if (rv_create != OK)
    return false;

  // Set the state
  identity_.source = HttpAuth::IDENT_SRC_PATH_LOOKUP;
  identity_.invalid = false;
  identity_.username = entry->username();
  identity_.password = entry->password();
  handler_.swap(handler_preemptive);
  return true;
}

void HttpAuthController::AddAuthorizationHeader(
    HttpRequestHeaders* authorization_headers) {
  DCHECK(HaveAuth());
  authorization_headers->SetHeader(
      HttpAuth::GetAuthorizationHeaderName(target_), auth_token_);
  auth_token_.clear();
}

int HttpAuthController::HandleAuthChallenge(
    scoped_refptr<HttpResponseHeaders> headers,
    bool do_not_send_server_auth,
    bool establishing_tunnel,
    const BoundNetLog& net_log) {
  DCHECK(headers);
  DCHECK(auth_origin_.is_valid());

  LOG(INFO) << "The " << HttpAuth::GetAuthTargetString(target_) << " "
            << auth_origin_ << " requested auth"
            << AuthChallengeLogMessage(headers.get());

  // The auth we tried just failed, hence it can't be valid. Remove it from
  // the cache so it won't be used again.
  // TODO(wtc): IsFinalRound is not the right condition.  In a multi-round
  // auth sequence, the server may fail the auth in round 1 if our first
  // authorization header is broken.  We should inspect response_.headers to
  // determine if the server already failed the auth or wants us to continue.
  // See http://crbug.com/21015.
  if (HaveAuth() && handler_->IsFinalRound()) {
    InvalidateRejectedAuthFromCache();
    handler_.reset();
    identity_ = HttpAuth::Identity();
  }

  identity_.invalid = true;

  if (target_ != HttpAuth::AUTH_SERVER || !do_not_send_server_auth) {
    // Find the best authentication challenge that we support.
    HttpAuth::ChooseBestChallenge(session_->http_auth_handler_factory(),
                                  headers, target_, auth_origin_,
                                  disabled_schemes_, net_log,
                                  &handler_);
  }

  if (!handler_.get()) {
    if (establishing_tunnel) {
      LOG(ERROR) << "Can't perform auth to the "
                 << HttpAuth::GetAuthTargetString(target_) << " "
                 << auth_origin_ << " when establishing a tunnel"
                 << AuthChallengeLogMessage(headers.get());

      // We are establishing a tunnel, we can't show the error page because an
      // active network attacker could control its contents.  Instead, we just
      // fail to establish the tunnel.
      DCHECK(target_ == HttpAuth::AUTH_PROXY);
      return ERR_PROXY_AUTH_UNSUPPORTED;
    }
    // We found no supported challenge -- let the transaction continue
    // so we end up displaying the error page.
    return OK;
  }

  if (handler_->NeedsIdentity()) {
    // Pick a new auth identity to try, by looking to the URL and auth cache.
    // If an identity to try is found, it is saved to identity_.
    SelectNextAuthIdentityToTry();
  } else {
    // Proceed with the existing identity or a null identity.
    //
    // TODO(wtc): Add a safeguard against infinite transaction restarts, if
    // the server keeps returning "NTLM".
    identity_.invalid = false;
  }

  // From this point on, we are restartable.

  if (identity_.invalid) {
    // We have exhausted all identity possibilities, all we can do now is
    // pass the challenge information back to the client.
    PopulateAuthChallenge();
  } else {
    auth_info_ = NULL;
  }

  return OK;
}

void HttpAuthController::ResetAuth(const std::wstring& username,
                                   const std::wstring& password) {
  DCHECK(identity_.invalid || (username.empty() && password.empty()));

  if (identity_.invalid) {
    // Update the username/password.
    identity_.source = HttpAuth::IDENT_SRC_EXTERNAL;
    identity_.invalid = false;
    identity_.username = username;
    identity_.password = password;
  }

  DCHECK(identity_.source != HttpAuth::IDENT_SRC_PATH_LOOKUP);

  // Add the auth entry to the cache before restarting. We don't know whether
  // the identity is valid yet, but if it is valid we want other transactions
  // to know about it. If an entry for (origin, handler->realm()) already
  // exists, we update it.
  //
  // If identity_.source is HttpAuth::IDENT_SRC_NONE or
  // HttpAuth::IDENT_SRC_DEFAULT_CREDENTIALS, identity_ contains no
  // identity because identity is not required yet or we're using default
  // credentials.
  //
  // TODO(wtc): For NTLM_SSPI, we add the same auth entry to the cache in
  // round 1 and round 2, which is redundant but correct.  It would be nice
  // to add an auth entry to the cache only once, preferrably in round 1.
  // See http://crbug.com/21015.
  switch (identity_.source) {
    case HttpAuth::IDENT_SRC_NONE:
    case HttpAuth::IDENT_SRC_DEFAULT_CREDENTIALS:
      break;
    default:
      session_->auth_cache()->Add(auth_origin_, handler_->realm(),
                                  handler_->scheme(), handler_->challenge(),
                                  identity_.username, identity_.password,
                                  auth_path_);
      break;
  }
}

void HttpAuthController::InvalidateRejectedAuthFromCache() {
  DCHECK(HaveAuth());

  // TODO(eroman): this short-circuit can be relaxed. If the realm of
  // the preemptively used auth entry matches the realm of the subsequent
  // challenge, then we can invalidate the preemptively used entry.
  // Otherwise as-is we may send the failed credentials one extra time.
  if (identity_.source == HttpAuth::IDENT_SRC_PATH_LOOKUP)
    return;

  // Clear the cache entry for the identity we just failed on.
  // Note: we require the username/password to match before invalidating
  // since the entry in the cache may be newer than what we used last time.
  session_->auth_cache()->Remove(auth_origin_, handler_->realm(),
                                 handler_->scheme(), identity_.username,
                                 identity_.password);
}

bool HttpAuthController::SelectNextAuthIdentityToTry() {
  DCHECK(handler_.get());
  DCHECK(identity_.invalid);

  // Try to use the username/password encoded into the URL first.
  if (target_ == HttpAuth::AUTH_SERVER && auth_url_.has_username() &&
      !embedded_identity_used_) {
    identity_.source = HttpAuth::IDENT_SRC_URL;
    identity_.invalid = false;
    // Extract the username:password from the URL.
    GetIdentityFromURL(auth_url_,
                       &identity_.username,
                       &identity_.password);
    embedded_identity_used_ = true;
    // TODO(eroman): If the password is blank, should we also try combining
    // with a password from the cache?
    return true;
  }

  // Check the auth cache for a realm entry.
  HttpAuthCache::Entry* entry =
    session_->auth_cache()->Lookup(auth_origin_, handler_->realm(),
                                   handler_->scheme());

  if (entry) {
    identity_.source = HttpAuth::IDENT_SRC_REALM_LOOKUP;
    identity_.invalid = false;
    identity_.username = entry->username();
    identity_.password = entry->password();
    return true;
  }

  // Use default credentials (single sign on) if this is the first attempt
  // at identity.  Do not allow multiple times as it will infinite loop.
  // We use default credentials after checking the auth cache so that if
  // single sign-on doesn't work, we won't try default credentials for future
  // transactions.
  if (!default_credentials_used_ && handler_->AllowsDefaultCredentials()) {
    identity_.source = HttpAuth::IDENT_SRC_DEFAULT_CREDENTIALS;
    identity_.invalid = false;
    default_credentials_used_ = true;
    return true;
  }

  return false;
}

void HttpAuthController::PopulateAuthChallenge() {
  // Populates response_.auth_challenge with the authentication challenge info.
  // This info is consumed by URLRequestHttpJob::GetAuthChallengeInfo().

  auth_info_ = new AuthChallengeInfo;
  auth_info_->is_proxy = target_ == HttpAuth::AUTH_PROXY;
  auth_info_->host_and_port = ASCIIToWide(GetHostAndPort(auth_origin_));
  auth_info_->scheme = ASCIIToWide(handler_->scheme());
  // TODO(eroman): decode realm according to RFC 2047.
  auth_info_->realm = ASCIIToWide(handler_->realm());
}

void HttpAuthController::OnIOComplete(int result) {
  // This error occurs with GSSAPI, if the user has not already logged in.
  // In that case, disable the current scheme as it cannot succeed.
  if (result == ERR_MISSING_AUTH_CREDENTIALS) {
    DisableAuthScheme(handler_->scheme());
    auth_token_.clear();
    result = OK;
  }
  if (user_callback_) {
    CompletionCallback* c = user_callback_;
    user_callback_ = NULL;
    c->Run(result);
  }
}

bool HttpAuthController::IsAuthSchemeDisabled(const std::string& scheme) const {
  return disabled_schemes_.find(scheme) != disabled_schemes_.end();
}

void HttpAuthController::DisableAuthScheme(const std::string& scheme) {
  disabled_schemes_.insert(scheme);
}

}  // namespace net
