// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_base.h"

#include <climits>

#include "base/logging.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace base {

std::string HistogramTypeToString(HistogramType type) {
  switch(type) {
    case HISTOGRAM:
      return "HISTOGRAM";
    case LINEAR_HISTOGRAM:
      return "LINEAR_HISTOGRAM";
    case BOOLEAN_HISTOGRAM:
      return "BOOLEAN_HISTOGRAM";
    case CUSTOM_HISTOGRAM:
      return "CUSTOM_HISTOGRAM";
    case SPARSE_HISTOGRAM:
      return "SPARSE_HISTOGRAM";
    default:
      NOTREACHED();
  }
  return "UNKNOWN";
}

const HistogramBase::Sample HistogramBase::kSampleType_MAX = INT_MAX;

HistogramBase::HistogramBase(const std::string& name)
    : histogram_name_(name),
      flags_(kNoFlags) {}

HistogramBase::~HistogramBase() {}

void HistogramBase::SetFlags(int32 flags) {
  flags_ |= flags;
}

void HistogramBase::ClearFlags(int32 flags) {
  flags_ &= ~flags;
}

void HistogramBase::WriteJSON(std::string* output) const {
  Count count;
  scoped_ptr<ListValue> buckets(new ListValue());
  GetCountAndBucketData(&count, buckets.get());
  scoped_ptr<DictionaryValue> parameters(new DictionaryValue());
  GetParameters(parameters.get());

  JSONStringValueSerializer serializer(output);
  DictionaryValue root;
  root.SetString("name", histogram_name());
  root.SetInteger("count", count);
  root.SetInteger("flags", flags());
  root.Set("params", parameters.release());
  root.Set("buckets", buckets.release());
  serializer.Serialize(root);
}

}  // namespace base
