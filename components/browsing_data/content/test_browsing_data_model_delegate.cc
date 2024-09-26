// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/test_browsing_data_model_delegate.h"

namespace browsing_data {
TestBrowsingDataModelDelegate::TestBrowsingDataModelDelegate() = default;
TestBrowsingDataModelDelegate::~TestBrowsingDataModelDelegate() = default;

void TestBrowsingDataModelDelegate::GetAllDataKeys(
    base::OnceCallback<void(std::vector<DelegateEntry>)> callback) {
  auto testOrigin = url::Origin::Create(GURL("https://a.test"));
  std::vector<DelegateEntry> data_keys = {
      DelegateEntry(testOrigin,
                    static_cast<BrowsingDataModel::StorageType>(
                        StorageType::kTestDelegateType),
                    0)};
  delegated_entries.insert({testOrigin,
                            {static_cast<BrowsingDataModel::StorageType>(
                                StorageType::kTestDelegateType)}});
  std::move(callback).Run(data_keys);
}

void TestBrowsingDataModelDelegate::RemoveDataKey(
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::StorageTypeSet storage_types,
    base::OnceClosure callback) {
  if (delegated_entries.contains(data_key)) {
    DCHECK(storage_types.Has(static_cast<BrowsingDataModel::StorageType>(
        StorageType::kTestDelegateType)));
    delegated_entries.erase(data_key);
  }
  std::move(callback).Run();
}

}  // namespace browsing_data