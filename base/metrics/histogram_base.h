// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_BASE_H_
#define BASE_METRICS_HISTOGRAM_BASE_H_

#include <climits>
#include <string>

#include "base/base_export.h"

namespace base {

class BASE_EXPORT HistogramBase {
 public:
  typedef int Sample;  // Used for samples.
  typedef int Count;   // Used to count samples.

  static const Sample kSampleType_MAX = INT_MAX;

  HistogramBase(const std::string& name);
  virtual ~HistogramBase();

  std::string histogram_name() const { return histogram_name_; }

  virtual void Add(Sample value) = 0;

  // The following methods provide graphical histogram displays.
  virtual void WriteHTMLGraph(std::string* output) const = 0;
  virtual void WriteAscii(std::string* output) const = 0;

 private:
  const std::string histogram_name_;
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_BASE_H_
