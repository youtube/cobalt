// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/connection_type_histograms.h"

#include "base/histogram.h"

namespace net {

// We're using a histogram as a group of counters.  We're only interested in
// the values of the counters.  Ignore the shape, average, and standard
// deviation of the histograms because they are meaningless.
//
// We use two groups of counters.  In the first group (counter1), each counter
// is a boolean (0 or 1) that indicates whether the user has seen a connection
// of that type during that session.  In the second group (counter2), each
// counter is the number of connections of that type the user has seen during
// that session.
//
// Each histogram has an unused bucket at the end to allow seamless future
// expansion.
void UpdateConnectionTypeHistograms(ConnectionType type, bool success) {
  static bool had_connection_type[NUM_OF_CONNECTION_TYPES];
  static scoped_refptr<Histogram> had_histogram =
      LinearHistogram::LinearHistogramFactoryGet("Net.HadConnectionType2",
                                                 1, NUM_OF_CONNECTION_TYPES,
                                                 NUM_OF_CONNECTION_TYPES + 1);
  static scoped_refptr<Histogram> success_histogram =
      LinearHistogram::LinearHistogramFactoryGet("Net.ConnectionTypeCount2",
                                                 1, NUM_OF_CONNECTION_TYPES,
                                                 NUM_OF_CONNECTION_TYPES + 1);
  static scoped_refptr<Histogram> failed_histogram =
      LinearHistogram::LinearHistogramFactoryGet("Net.ConnectionTypeFailCount2",
                                                 1, NUM_OF_CONNECTION_TYPES,
                                                 NUM_OF_CONNECTION_TYPES + 1);

  if (type >= 0 && type < NUM_OF_CONNECTION_TYPES) {
    if (!had_connection_type[type]) {
      had_connection_type[type] = true;
      had_histogram->SetFlags(kUmaTargetedHistogramFlag);
      had_histogram->Add(type);
    }

    Histogram* histogram;
    histogram = success ? success_histogram.get() : failed_histogram.get();
    histogram->SetFlags(kUmaTargetedHistogramFlag);
    histogram->Add(type);
  } else {
    NOTREACHED();  // Someone's logging an invalid type!
  }
}

}  // namespace net
