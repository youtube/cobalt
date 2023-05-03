// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADERS_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADERS_H_

#include <memory>
#include <set>
#include <string>

namespace net {
class HttpRequestHeaders;
struct NetworkTrafficAnnotationTag;
class URLRequest;
}

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
}  // namespace network

class GURL;

namespace variations {

enum class InIncognito { kNo, kYes };

enum class SignedIn { kNo, kYes };

// The name string for the header for variations information.
extern const char kClientDataHeader[];

// Adds Chrome experiment and metrics state as custom headers to |headers|.
// The content of the headers will depend on |incognito| and |signed_in|
// parameters. It is fine to pass SignedIn::NO if the state is not known to the
// caller. This will prevent addition of ids of type
// GOOGLE_WEB_PROPERTIES_SIGNED_IN, which is not the case for any ids that come
// from the variations server. These headers are never transmitted to non-Google
// web sites, which is checked based on the destination |url|.
// Returns true if custom headers are added. Returns false otherwise.
bool AppendVariationHeaders(const GURL& url,
                            InIncognito incognito,
                            SignedIn signed_in,
                            net::HttpRequestHeaders* headers);

// Adds Chrome experiment and metrics state as custom headers to |headers|
// when the signed-in state is not known to the caller; See above for details.
bool AppendVariationHeadersUnknownSignedIn(const GURL& url,
                                           InIncognito incognito,
                                           net::HttpRequestHeaders* headers);

// Strips the variation header if |new_location| does not point to a location
// that should receive it. This is being called by the ChromeNetworkDelegate.
// Components calling AppendVariationsHeaders() don't need to take care of this.
void StripVariationHeaderIfNeeded(const GURL& new_location,
                                  net::URLRequest* request);

// Creates a SimpleURLLoader that will include variations headers for requests
// to Google and ensures they're removed if a redirect to a non-Google URL
// occurs.
std::unique_ptr<network::SimpleURLLoader>
CreateSimpleURLLoaderWithVariationsHeaders(
    std::unique_ptr<network::ResourceRequest> request,
    InIncognito incognito,
    SignedIn signed_in,
    const net::NetworkTrafficAnnotationTag& annotation_tag);

// Creates a SimpleURLLoader that will include variations headers for requests
// to Google when the signed-in state is unknown and ensures they're removed
// if a redirect to a non-Google URL occurs.
std::unique_ptr<network::SimpleURLLoader>
CreateSimpleURLLoaderWithVariationsHeadersUnknownSignedIn(
    std::unique_ptr<network::ResourceRequest> request,
    InIncognito incognito,
    const net::NetworkTrafficAnnotationTag& annotation_tag);

// Checks whether variation headers should be appended to requests to the
// specified |url|. Returns true for google.<TLD> and youtube.<TLD> URLs with
// the https scheme.
bool ShouldAppendVariationHeaders(const GURL& url);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADERS_H_
