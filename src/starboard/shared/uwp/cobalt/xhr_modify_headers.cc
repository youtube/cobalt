// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/xhr/xhr_modify_headers.h"

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"

#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/shared/uwp/app_accessors.h"
#include "starboard/shared/uwp/async_utils.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"

using Windows::Security::Authentication::Web::Core::WebTokenResponse;
using Windows::Security::Authentication::Web::Core::WebTokenRequestResult;
using Windows::Security::Authentication::Web::Core::WebTokenRequestStatus;
using Windows::System::UserAuthenticationStatus;

namespace sbwin32 = starboard::shared::win32;
namespace sbuwp = starboard::shared::uwp;

namespace {

const char kCobaltBootstrapUrl[] =
    "https://www.youtube.com/api/xbox/cobalt_bootstrap";
const char kXboxLiveAccountProviderId[] = "https://xsts.auth.xboxlive.com";

// The name of the header to send the STS token out on.
const char kXauthHeaderName[] = "Authorization";

// The prefix for the value of the Authorization header when there is an XAuth
// token to attach to the request. The token itself should directly follow this
// prefix.
const char kXauthHeaderPrefix[] = "XBL3.0 x=";

// The name of the header to detect on requests as a trigger to add an STS token
// for the given RelyingPartyId (the value of the header). The header itself
// will be removed from the outgoing request, and replaced with an Authorization
// header with a valid STS token for the current primary user.
const base::StringPiece kXauthTriggerHeaderName = "X-STS-RelyingPartyId";

class LastTokenCache {
  bool has_last_token_ = false;
  std::string last_token_;
  starboard::Mutex mutex_;
 public:
  bool MaybeGetLastToken(std::string* out) {
    starboard::ScopedLock lock(mutex_);
    out->assign(last_token_);
    return has_last_token_;
  }

  void SetLastToken(const std::string& in) {
    starboard::ScopedLock lock(mutex_);
    has_last_token_ = true;
    last_token_ = in;
  }
};

SB_ONCE_INITIALIZE_FUNCTION(LastTokenCache, GetLastTokenCache);

bool PopulateToken(const std::string& relying_party, std::string* out) {
  out->clear();
  DCHECK(!sbuwp::GetDispatcher()->HasThreadAccess)
      << "Must not be called from the UWP main thread";

  WebTokenRequestResult^ token_result;
  try {
    token_result = sbuwp::TryToFetchSsoToken(relying_party).get();
  } catch (Platform::Exception^) {
    token_result = nullptr;
  }
  if (!token_result ||
      token_result->ResponseStatus != WebTokenRequestStatus::Success ||
      token_result->ResponseData->Size != 1) {
    // The token is valid for 16 hours, however there appears to be
    // no easy way for us to check on the client side when it expires.
    // We must use it hourly.
    // However, it appears that sometimes asking for a new token will
    // fail for unknown reasons.
    // Therefore, simply use the last token we had in case it's still good.
    // Note that while "relying_party" may change slightly, it
    // never changes any of the token's attributes.
    return GetLastTokenCache()->MaybeGetLastToken(out);
  }

  WebTokenResponse^ token_response = token_result->ResponseData->GetAt(0);
  *out = sbwin32::platformStringToString(token_response->Token);
  GetLastTokenCache()->SetLastToken(
      sbwin32::platformStringToString(token_response->Token));
  // Always get the token through the cache so the exceptional case
  // is less exceptional.
  return GetLastTokenCache()->MaybeGetLastToken(out);
}

void AppendUrlPath(const std::string& path, std::string* url_parameter) {
  DCHECK(url_parameter);

  if (path.empty()) {
    return;
  }

  std::string& url(*url_parameter);

  // if path starts with a '/' and url ends with a '/', remove trailing slash
  // from url before appending the path
  if (!url.empty() && *(url.rbegin()) == '/' && path[0] == '/') {
    url.resize(url.size() - 1);
  }
  url.append(path);
}

bool StringStartsWith(const std::string& str, const char* starts_with) {
  size_t starts_with_length = SbStringGetLength(starts_with);
  if (str.size() < starts_with_length) {
    return false;
  }

  std::string sub_str = str.substr(0, starts_with_length);
  return sub_str == starts_with;
}

}  // namespace

namespace cobalt {
namespace loader {

std::string CobaltFetchMaybeAddHeader(const GURL& url) {
  std::string out_string;
  if (!StringStartsWith(url.spec(), kCobaltBootstrapUrl)) {
    return out_string;
  }

  if (!PopulateToken(url.spec(), &out_string)) {
    return out_string;
  }
  std::string result;
  result.append(kXauthHeaderName);
  result.append(": ");
  result.append(out_string);

  return result;
}

}  // namespace loader
}  // namespace cobalt

namespace cobalt {
namespace xhr {

void CobaltXhrModifyHeader(const GURL& request_url,
                           net::HttpRequestHeaders* headers) {
  DCHECK(headers);

  std::string relying_party;
  bool trigger_header_found = headers->GetHeader(kXauthTriggerHeaderName,
    &relying_party);

  if (!trigger_header_found) {
    return;
  }

  if (request_url.has_path()) {
    std::string request_url_path = request_url.path();
    AppendUrlPath(request_url_path, &relying_party);
  }

  std::string out_string;
  if (!PopulateToken(relying_party, &out_string)) {
    return;
  }
  headers->RemoveHeader(kXauthTriggerHeaderName);
  headers->SetHeader(kXauthHeaderName, out_string);
}

}  // namespace xhr
}  // namespace cobalt
