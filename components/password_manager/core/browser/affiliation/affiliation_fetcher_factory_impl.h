// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_FACTORY_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_FACTORY_IMPL_H_

#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_factory.h"

namespace password_manager {

class AffiliationFetcherFactoryImpl : public AffiliationFetcherFactory {
 public:
  AffiliationFetcherFactoryImpl();
  ~AffiliationFetcherFactoryImpl() override;

  std::unique_ptr<AffiliationFetcherInterface> CreateInstance(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate) override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_FETCHER_FACTORY_IMPL_H_
