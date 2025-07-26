// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_FUCHSIA_QUERYABLE_DATA_HOST_FUCHSIA_H_
#define CHROMECAST_FUCHSIA_QUERYABLE_DATA_HOST_FUCHSIA_H_

#include <string>

#include "base/values.h"
#include "chromecast/activity/queryable_data_host.h"

namespace chromecast {

// Sends queryable data through FIDL to the Cast Runner (Fuchsia).
class QueryableDataHostFuchsia : public QueryableDataHost {
 public:
  QueryableDataHostFuchsia();

  QueryableDataHostFuchsia(const QueryableDataHostFuchsia&) = delete;
  QueryableDataHostFuchsia& operator=(const QueryableDataHostFuchsia&) = delete;

  ~QueryableDataHostFuchsia() override;

  // chromecast::QueryableDataHost implementation:
  void SendQueryableValue(const std::string& key,
                          const base::Value& value) override;
};

}  // namespace chromecast

#endif  // CHROMECAST_FUCHSIA_QUERYABLE_DATA_HOST_FUCHSIA_H_
