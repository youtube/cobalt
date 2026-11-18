// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_exchange_utils.h"

#include "base/containers/flat_map.h"
#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/custom_data_helper.h"

namespace exo {
namespace {

using DataExchangeUtilTest = testing::Test;

TEST_F(DataExchangeUtilTest, FilterCustomData_RemovesFsKeys) {
  base::flat_map<std::u16string, std::u16string> custom_data;
  custom_data[u"text/uri-list"] = u"data";
  custom_data[u"fs/tag"] = u"filemanager-data";
  custom_data[u"fs/sources"] = u"secret.txt";
  custom_data[u"safe_key"] = u"safe_value";

  base::Pickle pickle;
  ui::WriteCustomDataToPickle(custom_data, &pickle);
  std::vector<uint8_t> data(pickle.AsBytes().begin(), pickle.AsBytes().end());

  std::optional<base::Pickle> filtered_pickle = FilterCustomData(data);
  ASSERT_TRUE(filtered_pickle.has_value());

  auto map = ui::ReadCustomDataIntoMap(*filtered_pickle);
  ASSERT_TRUE(map.has_value());

  EXPECT_EQ(map->size(), 2u);
  EXPECT_EQ((*map)[u"text/uri-list"], u"data");
  EXPECT_EQ((*map)[u"safe_key"], u"safe_value");
  EXPECT_TRUE(map->find(u"fs/tag") == map->end());
  EXPECT_TRUE(map->find(u"fs/sources") == map->end());
}

}  // namespace
}  // namespace exo
