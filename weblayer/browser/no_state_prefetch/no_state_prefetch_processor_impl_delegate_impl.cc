// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/no_state_prefetch_processor_impl_delegate_impl.h"

#include "components/no_state_prefetch/browser/no_state_prefetch_link_manager.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_link_manager_factory.h"

namespace weblayer {

prerender::NoStatePrefetchLinkManager*
NoStatePrefetchProcessorImplDelegateImpl::GetNoStatePrefetchLinkManager(
    content::BrowserContext* browser_context) {
  return NoStatePrefetchLinkManagerFactory::GetForBrowserContext(
      browser_context);
}

}  // namespace weblayer
