// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_remover.h"

class DIPSService;

namespace content {

class BrowserContext;

// DipsDelegate is an interface that lets the //content layer
// provide embedder specific configuration for DIPS (Bounce Tracking
// Mitigations).
//
// Instances can be obtained via
// ContentBrowserClient::CreateDipsDelegate().
class CONTENT_EXPORT DipsDelegate {
 public:
  // The mask that will be used in place of GetRemoveMask() when embedders
  // return nullptr from ContentBrowserClient::CreateDipsDelegate(). This should
  // contain everything that can be deleted by domain or origin.
  static constexpr uint64_t kDefaultRemoveMask =
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
      content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE |
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES |
      content::BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX |
      content::BrowsingDataRemover::DATA_TYPE_CACHE |
      content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
      content::BrowsingDataRemover::DATA_TYPE_RELATED_WEBSITE_SETS_PERMISSIONS;

  virtual ~DipsDelegate();

  // DIPS will be enabled in browser contexts for which this returns true.
  virtual bool ShouldEnableDips(BrowserContext* browser_context) = 0;

  // Called once for each DIPSService instance when it's created.
  // DIPSService::Get() is guaranteed to return the given instance if called
  // i.e., DIPSService::Get(browser_context) == dips_service.
  virtual void OnDipsServiceCreated(BrowserContext* browser_context,
                                    DIPSService* dips_service) = 0;

  // Get the `remove_mask` that DIPS will pass to BrowsingDataRemover::Remove()
  // to delete storage for a site. This allows DIPS to clear types of storage
  // added by embedders.
  virtual uint64_t GetRemoveMask() = 0;

  // DIPS keeps separate records of storage and interactions for relevant sites.
  // It clears storage records for sites when their cookies are deleted, and
  // clears interaction records for sites when this method returns true, given
  // the `remove_mask` that a client passed to BrowsingDataRemover::Remove().
  virtual bool ShouldDeleteInteractionRecords(uint64_t remove_mask) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_
