// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/dummy_histogram.h"

#include <memory>

#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/values.h"

namespace base {

namespace {

// Helper classes for DummyHistogram.
class DummySampleCountIterator : public SampleCountIterator {
 public:
  DummySampleCountIterator() = default;
  DummySampleCountIterator(const DummySampleCountIterator&) = delete;
  DummySampleCountIterator& operator=(const DummySampleCountIterator&) = delete;
  ~DummySampleCountIterator() override = default;

  // SampleCountIterator:
  bool Done() const override { return true; }
  void Next() override { NOTREACHED(); }
  void Get(HistogramBase::Sample* min,
           int64_t* max,
           HistogramBase::Count* count) override {
    NOTREACHED();
  }
};

class DummyHistogramSamples : public HistogramSamples {
 public:
  DummyHistogramSamples()
      : HistogramSamples(0, std::make_unique<LocalMetadata>()) {}
  DummyHistogramSamples(const DummyHistogramSamples&) = delete;
  DummyHistogramSamples& operator=(const DummyHistogramSamples&) = delete;

  // HistogramSamples:
  void Accumulate(HistogramBase::Sample value,
                  HistogramBase::Count count) override {}
  HistogramBase::Count GetCount(HistogramBase::Sample value) const override {
    return HistogramBase::Count();
  }
  HistogramBase::Count TotalCount() const override {
    return HistogramBase::Count();
  }
  std::unique_ptr<SampleCountIterator> Iterator() const override {
    return std::make_unique<DummySampleCountIterator>();
  }
  std::unique_ptr<SampleCountIterator> ExtractingIterator() override {
    return std::make_unique<DummySampleCountIterator>();
  }
  bool AddSubtractImpl(SampleCountIterator* iter, Operator op) override {
    return true;
  }
};

}  // namespace

// static
DummyHistogram* DummyHistogram::GetInstance() {
  static base::NoDestructor<DummyHistogram> dummy_histogram;
  return dummy_histogram.get();
}

uint64_t DummyHistogram::name_hash() const {
  return HashMetricName(histogram_name());
}

HistogramType DummyHistogram::GetHistogramType() const {
  return DUMMY_HISTOGRAM;
}

bool DummyHistogram::HasConstructionArguments(
    Sample expected_minimum,
    Sample expected_maximum,
    size_t expected_bucket_count) const {
  return true;
}

bool DummyHistogram::AddSamplesFromPickle(PickleIterator* iter) {
  return true;
}

std::unique_ptr<HistogramSamples> DummyHistogram::SnapshotSamples() const {
  return std::make_unique<DummyHistogramSamples>();
}

std::unique_ptr<HistogramSamples> DummyHistogram::SnapshotUnloggedSamples()
    const {
  return std::make_unique<DummyHistogramSamples>();
}

std::unique_ptr<HistogramSamples> DummyHistogram::SnapshotDelta() {
  return std::make_unique<DummyHistogramSamples>();
}

std::unique_ptr<HistogramSamples> DummyHistogram::SnapshotFinalDelta() const {
  return std::make_unique<DummyHistogramSamples>();
}

Value::Dict DummyHistogram::ToGraphDict() const {
  return Value::Dict();
}

Value::Dict DummyHistogram::GetParameters() const {
  return Value::Dict();
}

}  // namespace base
