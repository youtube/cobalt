// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_BROWSER_EXPOSED_UTILITY_INTERFACES_H_
#define CHROME_UTILITY_BROWSER_EXPOSED_UTILITY_INTERFACES_H_

namespace mojo {
class BinderMap;
}

// Registers with |binders| any interfaces exposed to the browser process by
// utility processes when run with elevated privileges.
void ExposeElevatedChromeUtilityInterfacesToBrowser(mojo::BinderMap* binders);

#endif  // CHROME_UTILITY_BROWSER_EXPOSED_UTILITY_INTERFACES_H_
