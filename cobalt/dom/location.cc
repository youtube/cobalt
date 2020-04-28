// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/location.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"

namespace cobalt {
namespace dom {

Location::Location(const GURL& url, const base::Closure& hashchange_callback,
                   const base::Callback<void(const GURL&)>& navigation_callback,
                   const csp::SecurityCallback& security_callback)
    : ALLOW_THIS_IN_INITIALIZER_LIST(url_utils_(
          url, base::Bind(&Location::Replace, base::Unretained(this)))),
      hashchange_callback_(hashchange_callback),
      navigation_callback_(navigation_callback),
      security_callback_(security_callback) {}

// Algorithm for Replace:
//   https://www.w3.org/TR/html50/browsers.html#dom-location-replace
void Location::Replace(const std::string& url) {
  // When the replace(url) method is invoked, the UA must resolve the argument,
  // relative to the API base URL specified by the entry settings object, and if
  // that is successful, navigate the browsing context to the specified url with
  // replacement enabled and exceptions enabled.

  GURL new_url = url_utils_.url().Resolve(url);
  if (!new_url.is_valid()) {
    DLOG(WARNING) << "New url is invalid, aborting the navigation.";
    return;
  }
  const GURL& old_url = url_utils_.url();

  // The following codes correspond to navigating the browsing context in HTML5.
  //   https://www.w3.org/TR/html50/browsers.html#navigate
  // Since navigation in Cobalt always goes through Location interface, it is
  // implemented here.

  // If new URL is the same as old URL and they have hash, the navigation should
  // be ignored.
  if (new_url == old_url && new_url.has_ref()) {
    return;
  }

  // Check new URL against security policy.
  if (!security_callback_.is_null() &&
      !security_callback_.Run(new_url, false /* did redirect */)) {
    DLOG(WARNING) << "URL " << new_url
                  << " is rejected by policy, aborting the navigation.";
    return;
  }

  // Call either hashchange callback or navigation callback.
  GURL::Replacements replacements;
  replacements.ClearRef();
  if (new_url.ReplaceComponents(replacements) ==
          old_url.ReplaceComponents(replacements) &&
      new_url.has_ref() && new_url.ref() != old_url.ref()) {
    url_utils_.set_url(new_url);
    if (!hashchange_callback_.is_null()) {
      hashchange_callback_.Run();
    }
  } else {
    if (!navigation_callback_.is_null()) {
      navigation_callback_.Run(new_url);
    }
  }
}

void Location::Reload() {
  if (!navigation_callback_.is_null()) {
    LOG(INFO) << "Reloading URL: " << url();
    navigation_callback_.Run(url());
  }
}

}  // namespace dom
}  // namespace cobalt
