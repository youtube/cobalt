/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/location.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"

namespace cobalt {
namespace dom {

Location::Location(const GURL& url,
                   const base::Callback<void(const GURL&)>& navigation_callback,
                   const csp::SecurityCallback& security_callback)
    : ALLOW_THIS_IN_INITIALIZER_LIST(url_utils_(
          url, base::Bind(&Location::Replace, base::Unretained(this)))),
      navigation_callback_(navigation_callback),
      security_callback_(security_callback) {}

void Location::Replace(const std::string& url) {
  // When the replace(url) method is invoked, the UA must resolve the argument,
  // relative to the API base URL specified by the entry settings object, and if
  // that is successful, navigate the browsing context to the specified url with
  // replacement enabled and exceptions enabled.
  GURL new_url = url_utils_.url().Resolve(url);
  if (new_url.is_valid()) {
    if (!security_callback_.Run(new_url, false /* did redirect */)) {
      DLOG(INFO) << "URL " << new_url
                 << " is rejected by policy, aborting the navigation.";
      return;
    }

    if (!navigation_callback_.is_null()) {
      navigation_callback_.Run(new_url);
    }
  }
}

}  // namespace dom
}  // namespace cobalt
