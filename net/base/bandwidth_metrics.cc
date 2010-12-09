// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/singleton.h"
#include "net/base/bandwidth_metrics.h"

namespace net {

ScopedBandwidthMetrics::ScopedBandwidthMetrics()
    : metrics_(Singleton<BandwidthMetrics>::get()),
      started_(false) {
}

}  // namespace net
