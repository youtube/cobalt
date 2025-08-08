// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_API_CALL_FLOW_H_
#define GOOGLE_APIS_GAIA_OAUTH2_API_CALL_FLOW_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}

namespace net {
class HttpRequestHeaders;
}

// Base class for all classes that implement a flow to call OAuth2 enabled APIs,
// given an access token to the service.  This class abstracts the basic steps
// and exposes template methods for sub-classes to implement for API specific
// details.
class COMPONENT_EXPORT(GOOGLE_APIS) OAuth2ApiCallFlow {
 public:
  OAuth2ApiCallFlow();

  OAuth2ApiCallFlow(const OAuth2ApiCallFlow&) = delete;
  OAuth2ApiCallFlow& operator=(const OAuth2ApiCallFlow&) = delete;

  virtual ~OAuth2ApiCallFlow();

  // Start the flow.
  virtual void Start(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token);

 protected:
  // Template methods for sub-classes.

  // Methods to help create the API request.
  virtual GURL CreateApiCallUrl() = 0;
  virtual net::HttpRequestHeaders CreateApiCallHeaders();
  virtual std::string CreateApiCallBody() = 0;
  virtual std::string CreateApiCallBodyContentType();

  // Returns the request type (e.g. GET, POST) for the |body| that will be sent
  // with the request.
  virtual std::string GetRequestTypeForBody(const std::string& body);

  // Called when the API call ends without network error to check whether the
  // request succeeded, to decide which of the following 2 process functions to
  // call. Should be overriden by subclasses if the expected success response
  // code is not 200 or 204.
  virtual bool IsExpectedSuccessCode(int code) const;

  // Sub-classes can expose an appropriate observer interface by implementing
  // these template methods.
  // Called when there is no network error and IsExpectedSuccessCode() returns
  // true. |body| may be null.
  virtual void ProcessApiCallSuccess(
      const network::mojom::URLResponseHead* head,
      std::unique_ptr<std::string> body) = 0;

  // Called when there is a network error or IsExpectedSuccessCode() returns
  // false. |head| or |body| might be null.
  virtual void ProcessApiCallFailure(
      int net_error,
      const network::mojom::URLResponseHead* head,
      std::unique_ptr<std::string> body) = 0;

  virtual net::PartialNetworkTrafficAnnotationTag
  GetNetworkTrafficAnnotationTag() = 0;

 private:
  enum State {
    INITIAL,
    API_CALL_STARTED,
    API_CALL_DONE,
    ERROR_STATE
  };

  // Called when loading has finished.
  void OnURLLoadComplete(std::unique_ptr<std::string> body);

  // Creates an instance of SimpleURLLoader that does not send or save cookies.
  // Template method CreateApiCallUrl is used to get the URL.
  // Template method CreateApiCallBody is used to get the body.
  // The http method will be GET if body is empty, POST otherwise.
  std::unique_ptr<network::SimpleURLLoader> CreateURLLoader(
      const std::string& access_token);

  // Helper methods to implement the state machine for the flow.
  void BeginApiCall();
  void EndApiCall(std::unique_ptr<std::string> body);

  State state_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_API_CALL_FLOW_H_
