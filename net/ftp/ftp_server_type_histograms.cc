// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_server_type_histograms.h"

#include "base/histogram.h"

namespace net {

// We're using a histogram as a group of counters.  We're only interested in
// the values of the counters.  Ignore the shape, average, and standard
// deviation of the histograms because they are meaningless.
//
// We use two groups of counters.  In the first group (counter1), each counter
// is a boolean (0 or 1) that indicates whether the user has seen an FTP server
// of that type during that session.  In the second group (counter2), each
// counter is the number of transactions with FTP server of that type the user
// has made during that session.
//
// Each histogram has an unused bucket at the end to allow seamless future
// expansion.
void UpdateFtpServerTypeHistograms(FtpServerType type) {
  static bool had_server_type[NUM_OF_SERVER_TYPES];
  static scoped_refptr<Histogram> counter1 =
      LinearHistogram::LinearHistogramFactoryGet("Net.HadFtpServerType",
                                                 1, NUM_OF_SERVER_TYPES,
                                                 NUM_OF_SERVER_TYPES + 1);
  static scoped_refptr<Histogram> counter2 =
      LinearHistogram::LinearHistogramFactoryGet("Net.FtpServerTypeCount",
                                                 1, NUM_OF_SERVER_TYPES,
                                                 NUM_OF_SERVER_TYPES + 1);

  if (type >= 0 && type < NUM_OF_SERVER_TYPES) {
    if (!had_server_type[type]) {
      had_server_type[type] = true;
      counter1->SetFlags(kUmaTargetedHistogramFlag);
      counter1->Add(type);
    }
  }
  counter2->SetFlags(kUmaTargetedHistogramFlag);
  counter2->Add(type);
}

}  // namespace net
