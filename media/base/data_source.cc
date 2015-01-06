// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/data_source.h"

#include "base/logging.h"

namespace media {

// static
const int DataSource::kReadError = -1;

DataSourceHost::~DataSourceHost() {}

DataSource::DataSource() : host_(NULL) {}

DataSource::~DataSource() {}

void DataSource::set_host(DataSourceHost* host) {
  DCHECK(host);
  DCHECK(!host_);
  host_ = host;
}

void DataSource::SetPlaybackRate(float playback_rate) {}

DataSourceHost* DataSource::host() { return host_; }

}  // namespace media
