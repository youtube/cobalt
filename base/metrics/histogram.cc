// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Histogram is an object that aggregates statistics, and can summarize them in
// various forms, including ASCII graphical, HTML, and numerically (as a
// vector of numbers corresponding to each of the aggregating buckets).
// See header file for details and examples.

#include "base/metrics/histogram.h"

#include <math.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/pickle.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"

namespace base {

typedef Histogram::Count Count;

// static
const size_t Histogram::kBucketCount_MAX = 10000u;

scoped_refptr<Histogram> Histogram::FactoryGet(const std::string& name,
    Sample minimum, Sample maximum, size_t bucket_count, Flags flags) {
  scoped_refptr<Histogram> histogram(NULL);

  // Defensive code.
  if (minimum < 1)
    minimum = 1;
  if (maximum > kSampleType_MAX - 1)
    maximum = kSampleType_MAX - 1;

  if (!StatisticsRecorder::FindHistogram(name, &histogram)) {
    histogram = new Histogram(name, minimum, maximum, bucket_count);
    StatisticsRecorder::FindHistogram(name, &histogram);
  }

  DCHECK_EQ(HISTOGRAM, histogram->histogram_type());
  DCHECK(histogram->HasConstructorArguments(minimum, maximum, bucket_count));
  histogram->SetFlags(flags);
  return histogram;
}

scoped_refptr<Histogram> Histogram::FactoryTimeGet(const std::string& name,
                                                   TimeDelta minimum,
                                                   TimeDelta maximum,
                                                   size_t bucket_count,
                                                   Flags flags) {
  return FactoryGet(name, minimum.InMilliseconds(), maximum.InMilliseconds(),
                    bucket_count, flags);
}

void Histogram::Add(int value) {
  if (value > kSampleType_MAX - 1)
    value = kSampleType_MAX - 1;
  if (value < 0)
    value = 0;
  size_t index = BucketIndex(value);
  DCHECK_GE(value, ranges(index));
  DCHECK_LT(value, ranges(index + 1));
  Accumulate(value, 1, index);
}

void Histogram::AddBoolean(bool value) {
  DCHECK(false);
}

void Histogram::AddSampleSet(const SampleSet& sample) {
  sample_.Add(sample);
}

void Histogram::SetRangeDescriptions(const DescriptionPair descriptions[]) {
  DCHECK(false);
}

// The following methods provide a graphical histogram display.
void Histogram::WriteHTMLGraph(std::string* output) const {
  // TBD(jar) Write a nice HTML bar chart, with divs an mouse-overs etc.
  output->append("<PRE>");
  WriteAscii(true, "<br>", output);
  output->append("</PRE>");
}

void Histogram::WriteAscii(bool graph_it, const std::string& newline,
                           std::string* output) const {
  // Get local (stack) copies of all effectively volatile class data so that we
  // are consistent across our output activities.
  SampleSet snapshot;
  SnapshotSample(&snapshot);
  Count sample_count = snapshot.TotalCount();

  WriteAsciiHeader(snapshot, sample_count, output);
  output->append(newline);

  // Prepare to normalize graphical rendering of bucket contents.
  double max_size = 0;
  if (graph_it)
    max_size = GetPeakBucketSize(snapshot);

  // Calculate space needed to print bucket range numbers.  Leave room to print
  // nearly the largest bucket range without sliding over the histogram.
  size_t largest_non_empty_bucket = bucket_count() - 1;
  while (0 == snapshot.counts(largest_non_empty_bucket)) {
    if (0 == largest_non_empty_bucket)
      break;  // All buckets are empty.
    --largest_non_empty_bucket;
  }

  // Calculate largest print width needed for any of our bucket range displays.
  size_t print_width = 1;
  for (size_t i = 0; i < bucket_count(); ++i) {
    if (snapshot.counts(i)) {
      size_t width = GetAsciiBucketRange(i).size() + 1;
      if (width > print_width)
        print_width = width;
    }
  }

  int64 remaining = sample_count;
  int64 past = 0;
  // Output the actual histogram graph.
  for (size_t i = 0; i < bucket_count(); ++i) {
    Count current = snapshot.counts(i);
    if (!current && !PrintEmptyBucket(i))
      continue;
    remaining -= current;
    std::string range = GetAsciiBucketRange(i);
    output->append(range);
    for (size_t j = 0; range.size() + j < print_width + 1; ++j)
      output->push_back(' ');
    if (0 == current && i < bucket_count() - 1 && 0 == snapshot.counts(i + 1)) {
      while (i < bucket_count() - 1 && 0 == snapshot.counts(i + 1))
        ++i;
      output->append("... ");
      output->append(newline);
      continue;  // No reason to plot emptiness.
    }
    double current_size = GetBucketSize(current, i);
    if (graph_it)
      WriteAsciiBucketGraph(current_size, max_size, output);
    WriteAsciiBucketContext(past, current, remaining, i, output);
    output->append(newline);
    past += current;
  }
  DCHECK_EQ(sample_count, past);
}

// static
std::string Histogram::SerializeHistogramInfo(const Histogram& histogram,
                                              const SampleSet& snapshot) {
  DCHECK_NE(NOT_VALID_IN_RENDERER, histogram.histogram_type());

  Pickle pickle;
  pickle.WriteString(histogram.histogram_name());
  pickle.WriteInt(histogram.declared_min());
  pickle.WriteInt(histogram.declared_max());
  pickle.WriteSize(histogram.bucket_count());
  pickle.WriteInt(histogram.range_checksum());
  pickle.WriteInt(histogram.histogram_type());
  pickle.WriteInt(histogram.flags());

  snapshot.Serialize(&pickle);
  return std::string(static_cast<const char*>(pickle.data()), pickle.size());
}

// static
bool Histogram::DeserializeHistogramInfo(const std::string& histogram_info) {
  if (histogram_info.empty()) {
      return false;
  }

  Pickle pickle(histogram_info.data(),
                static_cast<int>(histogram_info.size()));
  std::string histogram_name;
  int declared_min;
  int declared_max;
  size_t bucket_count;
  int range_checksum;
  int histogram_type;
  int pickle_flags;
  SampleSet sample;

  void* iter = NULL;
  if (!pickle.ReadString(&iter, &histogram_name) ||
      !pickle.ReadInt(&iter, &declared_min) ||
      !pickle.ReadInt(&iter, &declared_max) ||
      !pickle.ReadSize(&iter, &bucket_count) ||
      !pickle.ReadInt(&iter, &range_checksum) ||
      !pickle.ReadInt(&iter, &histogram_type) ||
      !pickle.ReadInt(&iter, &pickle_flags) ||
      !sample.Histogram::SampleSet::Deserialize(&iter, pickle)) {
    LOG(ERROR) << "Pickle error decoding Histogram: " << histogram_name;
    return false;
  }
  DCHECK(pickle_flags & kIPCSerializationSourceFlag);
  // Since these fields may have come from an untrusted renderer, do additional
  // checks above and beyond those in Histogram::Initialize()
  if (declared_max <= 0 || declared_min <= 0 || declared_max < declared_min ||
      INT_MAX / sizeof(Count) <= bucket_count || bucket_count < 2) {
    LOG(ERROR) << "Values error decoding Histogram: " << histogram_name;
    return false;
  }

  Flags flags = static_cast<Flags>(pickle_flags & ~kIPCSerializationSourceFlag);

  DCHECK_NE(NOT_VALID_IN_RENDERER, histogram_type);

  scoped_refptr<Histogram> render_histogram(NULL);

  if (histogram_type == HISTOGRAM) {
    render_histogram = Histogram::FactoryGet(
        histogram_name, declared_min, declared_max, bucket_count, flags);
  } else if (histogram_type == LINEAR_HISTOGRAM) {
    render_histogram = LinearHistogram::FactoryGet(
        histogram_name, declared_min, declared_max, bucket_count, flags);
  } else if (histogram_type == BOOLEAN_HISTOGRAM) {
    render_histogram = BooleanHistogram::FactoryGet(histogram_name, flags);
  } else {
    LOG(ERROR) << "Error Deserializing Histogram Unknown histogram_type: "
               << histogram_type;
    return false;
  }

  DCHECK_EQ(render_histogram->declared_min(), declared_min);
  DCHECK_EQ(render_histogram->declared_max(), declared_max);
  DCHECK_EQ(render_histogram->bucket_count(), bucket_count);
  DCHECK_EQ(render_histogram->range_checksum(), range_checksum);
  DCHECK_EQ(render_histogram->histogram_type(), histogram_type);

  if (render_histogram->flags() & kIPCSerializationSourceFlag) {
    DVLOG(1) << "Single process mode, histogram observed and not copied: "
             << histogram_name;
  } else {
    DCHECK_EQ(flags & render_histogram->flags(), flags);
    render_histogram->AddSampleSet(sample);
  }

  return true;
}

//------------------------------------------------------------------------------
// Methods for the validating a sample and a related histogram.
//------------------------------------------------------------------------------

Histogram::Inconsistencies Histogram::FindCorruption(
    const SampleSet& snapshot) const {
  int inconsistencies = NO_INCONSISTENCIES;
  Sample previous_range = -1;  // Bottom range is always 0.
  Sample checksum = 0;
  int64 count = 0;
  for (size_t index = 0; index < bucket_count(); ++index) {
    count += snapshot.counts(index);
    int new_range = ranges(index);
    checksum += new_range;
    if (previous_range >= new_range)
      inconsistencies |= BUCKET_ORDER_ERROR;
    previous_range = new_range;
  }

  if (checksum != range_checksum_)
    inconsistencies |= RANGE_CHECKSUM_ERROR;

  int64 delta64 = snapshot.redundant_count() - count;
  if (delta64 != 0) {
    int delta = static_cast<int>(delta64);
    if (delta != delta64)
      delta = INT_MAX;  // Flag all giant errors as INT_MAX.
    // Since snapshots of histograms are taken asynchronously relative to
    // sampling (and snapped from different threads), it is pretty likely that
    // we'll catch a redundant count that doesn't match the sample count.  We
    // allow for a certain amount of slop before flagging this as an
    // inconsistency.  Even with an inconsistency, we'll snapshot it again (for
    // UMA in about a half hour, so we'll eventually get the data, if it was
    // not the result of a corruption.  If histograms show that 1 is "too tight"
    // then we may try to use 2 or 3 for this slop value.
    const int kCommonRaceBasedCountMismatch = 1;
    if (delta > 0) {
      UMA_HISTOGRAM_COUNTS("Histogram.InconsistentCountHigh", delta);
      if (delta > kCommonRaceBasedCountMismatch)
        inconsistencies |= COUNT_HIGH_ERROR;
    } else {
      DCHECK_GT(0, delta);
      UMA_HISTOGRAM_COUNTS("Histogram.InconsistentCountLow", -delta);
      if (-delta > kCommonRaceBasedCountMismatch)
        inconsistencies |= COUNT_LOW_ERROR;
    }
  }
  return static_cast<Inconsistencies>(inconsistencies);
}

Histogram::ClassType Histogram::histogram_type() const {
  return HISTOGRAM;
}

Histogram::Sample Histogram::ranges(size_t i) const {
  return ranges_[i];
}

size_t Histogram::bucket_count() const {
  return bucket_count_;
}

// Do a safe atomic snapshot of sample data.
// This implementation assumes we are on a safe single thread.
void Histogram::SnapshotSample(SampleSet* sample) const {
  // Note locking not done in this version!!!
  *sample = sample_;
}

bool Histogram::HasConstructorArguments(Sample minimum,
                                        Sample maximum,
                                        size_t bucket_count) {
  return ((minimum == declared_min_) && (maximum == declared_max_) &&
          (bucket_count == bucket_count_));
}

bool Histogram::HasConstructorTimeDeltaArguments(TimeDelta minimum,
                                                 TimeDelta maximum,
                                                 size_t bucket_count) {
  return ((minimum.InMilliseconds() == declared_min_) &&
          (maximum.InMilliseconds() == declared_max_) &&
          (bucket_count == bucket_count_));
}

Histogram::Histogram(const std::string& name, Sample minimum,
                     Sample maximum, size_t bucket_count)
  : histogram_name_(name),
    declared_min_(minimum),
    declared_max_(maximum),
    bucket_count_(bucket_count),
    flags_(kNoFlags),
    ranges_(bucket_count + 1, 0),
    range_checksum_(0),
    sample_() {
  Initialize();
}

Histogram::Histogram(const std::string& name, TimeDelta minimum,
                     TimeDelta maximum, size_t bucket_count)
  : histogram_name_(name),
    declared_min_(static_cast<int> (minimum.InMilliseconds())),
    declared_max_(static_cast<int> (maximum.InMilliseconds())),
    bucket_count_(bucket_count),
    flags_(kNoFlags),
    ranges_(bucket_count + 1, 0),
    range_checksum_(0),
    sample_() {
  Initialize();
}

Histogram::~Histogram() {
  if (StatisticsRecorder::dump_on_exit()) {
    std::string output;
    WriteAscii(true, "\n", &output);
    LOG(INFO) << output;
  }

  // Just to make sure most derived class did this properly...
  DCHECK(ValidateBucketRanges());
  DCHECK(HasValidRangeChecksum());
}

bool Histogram::PrintEmptyBucket(size_t index) const {
  return true;
}

// Calculate what range of values are held in each bucket.
// We have to be careful that we don't pick a ratio between starting points in
// consecutive buckets that is sooo small, that the integer bounds are the same
// (effectively making one bucket get no values).  We need to avoid:
//   ranges_[i] == ranges_[i + 1]
// To avoid that, we just do a fine-grained bucket width as far as we need to
// until we get a ratio that moves us along at least 2 units at a time.  From
// that bucket onward we do use the exponential growth of buckets.
void Histogram::InitializeBucketRange() {
  double log_max = log(static_cast<double>(declared_max()));
  double log_ratio;
  double log_next;
  size_t bucket_index = 1;
  Sample current = declared_min();
  SetBucketRange(bucket_index, current);
  while (bucket_count() > ++bucket_index) {
    double log_current;
    log_current = log(static_cast<double>(current));
    // Calculate the count'th root of the range.
    log_ratio = (log_max - log_current) / (bucket_count() - bucket_index);
    // See where the next bucket would start.
    log_next = log_current + log_ratio;
    int next;
    next = static_cast<int>(floor(exp(log_next) + 0.5));
    if (next > current)
      current = next;
    else
      ++current;  // Just do a narrow bucket, and keep trying.
    SetBucketRange(bucket_index, current);
  }
  ResetRangeChecksum();

  DCHECK_EQ(bucket_count(), bucket_index);
}

size_t Histogram::BucketIndex(Sample value) const {
  // Use simple binary search.  This is very general, but there are better
  // approaches if we knew that the buckets were linearly distributed.
  DCHECK_LE(ranges(0), value);
  DCHECK_GT(ranges(bucket_count()), value);
  size_t under = 0;
  size_t over = bucket_count();
  size_t mid;

  do {
    DCHECK_GE(over, under);
    mid = under + (over - under)/2;
    if (mid == under)
      break;
    if (ranges(mid) <= value)
      under = mid;
    else
      over = mid;
  } while (true);

  DCHECK_LE(ranges(mid), value);
  CHECK_GT(ranges(mid+1), value);
  return mid;
}

// Use the actual bucket widths (like a linear histogram) until the widths get
// over some transition value, and then use that transition width.  Exponentials
// get so big so fast (and we don't expect to see a lot of entries in the large
// buckets), so we need this to make it possible to see what is going on and
// not have 0-graphical-height buckets.
double Histogram::GetBucketSize(Count current, size_t i) const {
  DCHECK_GT(ranges(i + 1), ranges(i));
  static const double kTransitionWidth = 5;
  double denominator = ranges(i + 1) - ranges(i);
  if (denominator > kTransitionWidth)
    denominator = kTransitionWidth;  // Stop trying to normalize.
  return current/denominator;
}

void Histogram::ResetRangeChecksum() {
  range_checksum_ = CalculateRangeChecksum();
}

const std::string Histogram::GetAsciiBucketRange(size_t i) const {
  std::string result;
  if (kHexRangePrintingFlag & flags_)
    StringAppendF(&result, "%#x", ranges(i));
  else
    StringAppendF(&result, "%d", ranges(i));
  return result;
}

// Update histogram data with new sample.
void Histogram::Accumulate(Sample value, Count count, size_t index) {
  // Note locking not done in this version!!!
  sample_.Accumulate(value, count, index);
}

void Histogram::SetBucketRange(size_t i, Sample value) {
  DCHECK_GT(bucket_count_, i);
  ranges_[i] = value;
}

bool Histogram::ValidateBucketRanges() const {
  // Standard assertions that all bucket ranges should satisfy.
  DCHECK_EQ(bucket_count_ + 1, ranges_.size());
  DCHECK_EQ(0, ranges_[0]);
  DCHECK_EQ(declared_min(), ranges_[1]);
  DCHECK_EQ(declared_max(), ranges_[bucket_count_ - 1]);
  DCHECK_EQ(kSampleType_MAX, ranges_[bucket_count_]);
  return true;
}

void Histogram::Initialize() {
  sample_.Resize(*this);
  if (declared_min_ < 1)
    declared_min_ = 1;
  if (declared_max_ > kSampleType_MAX - 1)
    declared_max_ = kSampleType_MAX - 1;
  DCHECK_LE(declared_min_, declared_max_);
  DCHECK_GT(bucket_count_, 1u);
  CHECK_LT(bucket_count_, kBucketCount_MAX);
  size_t maximal_bucket_count = declared_max_ - declared_min_ + 2;
  DCHECK_LE(bucket_count_, maximal_bucket_count);
  DCHECK_EQ(0, ranges_[0]);
  ranges_[bucket_count_] = kSampleType_MAX;
  InitializeBucketRange();
  DCHECK(ValidateBucketRanges());
  StatisticsRecorder::Register(this);
}

bool Histogram::HasValidRangeChecksum() const {
  return CalculateRangeChecksum() == range_checksum_;
}

Histogram::Sample Histogram::CalculateRangeChecksum() const {
  DCHECK_EQ(ranges_.size(), bucket_count() + 1);
  Sample checksum = 0;
  for (size_t index = 0; index < bucket_count(); ++index) {
    checksum += ranges(index);
  }
  return checksum;
}

//------------------------------------------------------------------------------
// Private methods

double Histogram::GetPeakBucketSize(const SampleSet& snapshot) const {
  double max = 0;
  for (size_t i = 0; i < bucket_count() ; ++i) {
    double current_size = GetBucketSize(snapshot.counts(i), i);
    if (current_size > max)
      max = current_size;
  }
  return max;
}

void Histogram::WriteAsciiHeader(const SampleSet& snapshot,
                                 Count sample_count,
                                 std::string* output) const {
  StringAppendF(output,
                "Histogram: %s recorded %d samples",
                histogram_name().c_str(),
                sample_count);
  if (0 == sample_count) {
    DCHECK_EQ(snapshot.sum(), 0);
  } else {
    double average = static_cast<float>(snapshot.sum()) / sample_count;
    double variance = static_cast<float>(snapshot.square_sum())/sample_count
                      - average * average;
    double standard_deviation = sqrt(variance);

    StringAppendF(output,
                  ", average = %.1f, standard deviation = %.1f",
                  average, standard_deviation);
  }
  if (flags_ & ~kHexRangePrintingFlag)
    StringAppendF(output, " (flags = 0x%x)", flags_ & ~kHexRangePrintingFlag);
}

void Histogram::WriteAsciiBucketContext(const int64 past,
                                        const Count current,
                                        const int64 remaining,
                                        const size_t i,
                                        std::string* output) const {
  double scaled_sum = (past + current + remaining) / 100.0;
  WriteAsciiBucketValue(current, scaled_sum, output);
  if (0 < i) {
    double percentage = past / scaled_sum;
    StringAppendF(output, " {%3.1f%%}", percentage);
  }
}

void Histogram::WriteAsciiBucketValue(Count current, double scaled_sum,
                                      std::string* output) const {
  StringAppendF(output, " (%d = %3.1f%%)", current, current/scaled_sum);
}

void Histogram::WriteAsciiBucketGraph(double current_size, double max_size,
                                      std::string* output) const {
  const int k_line_length = 72;  // Maximal horizontal width of graph.
  int x_count = static_cast<int>(k_line_length * (current_size / max_size)
                                 + 0.5);
  int x_remainder = k_line_length - x_count;

  while (0 < x_count--)
    output->append("-");
  output->append("O");
  while (0 < x_remainder--)
    output->append(" ");
}

//------------------------------------------------------------------------------
// Methods for the Histogram::SampleSet class
//------------------------------------------------------------------------------

Histogram::SampleSet::SampleSet()
    : counts_(),
      sum_(0),
      square_sum_(0),
      redundant_count_(0) {
}

Histogram::SampleSet::~SampleSet() {
}

void Histogram::SampleSet::Resize(const Histogram& histogram) {
  counts_.resize(histogram.bucket_count(), 0);
}

void Histogram::SampleSet::CheckSize(const Histogram& histogram) const {
  DCHECK_EQ(histogram.bucket_count(), counts_.size());
}


void Histogram::SampleSet::Accumulate(Sample value,  Count count,
                                      size_t index) {
  DCHECK(count == 1 || count == -1);
  counts_[index] += count;
  sum_ += count * value;
  square_sum_ += (count * value) * static_cast<int64>(value);
  redundant_count_ += count;
  DCHECK_GE(counts_[index], 0);
  DCHECK_GE(sum_, 0);
  DCHECK_GE(square_sum_, 0);
  DCHECK_GE(redundant_count_, 0);
}

Count Histogram::SampleSet::TotalCount() const {
  Count total = 0;
  for (Counts::const_iterator it = counts_.begin();
       it != counts_.end();
       ++it) {
    total += *it;
  }
  return total;
}

void Histogram::SampleSet::Add(const SampleSet& other) {
  DCHECK_EQ(counts_.size(), other.counts_.size());
  sum_ += other.sum_;
  square_sum_ += other.square_sum_;
  redundant_count_ += other.redundant_count_;
  for (size_t index = 0; index < counts_.size(); ++index)
    counts_[index] += other.counts_[index];
}

void Histogram::SampleSet::Subtract(const SampleSet& other) {
  DCHECK_EQ(counts_.size(), other.counts_.size());
  // Note: Race conditions in snapshotting a sum or square_sum may lead to
  // (temporary) negative values when snapshots are later combined (and deltas
  // calculated).  As a result, we don't currently CHCEK() for positive values.
  sum_ -= other.sum_;
  square_sum_ -= other.square_sum_;
  redundant_count_ -= other.redundant_count_;
  for (size_t index = 0; index < counts_.size(); ++index) {
    counts_[index] -= other.counts_[index];
    DCHECK_GE(counts_[index], 0);
  }
}

bool Histogram::SampleSet::Serialize(Pickle* pickle) const {
  pickle->WriteInt64(sum_);
  pickle->WriteInt64(square_sum_);
  pickle->WriteInt64(redundant_count_);
  pickle->WriteSize(counts_.size());

  for (size_t index = 0; index < counts_.size(); ++index) {
    pickle->WriteInt(counts_[index]);
  }

  return true;
}

bool Histogram::SampleSet::Deserialize(void** iter, const Pickle& pickle) {
  DCHECK_EQ(counts_.size(), 0u);
  DCHECK_EQ(sum_, 0);
  DCHECK_EQ(square_sum_, 0);
  DCHECK_EQ(redundant_count_, 0);

  size_t counts_size;

  if (!pickle.ReadInt64(iter, &sum_) ||
      !pickle.ReadInt64(iter, &square_sum_) ||
      !pickle.ReadInt64(iter, &redundant_count_) ||
      !pickle.ReadSize(iter, &counts_size)) {
    return false;
  }

  if (counts_size == 0)
    return false;

  int count = 0;
  for (size_t index = 0; index < counts_size; ++index) {
    int i;
    if (!pickle.ReadInt(iter, &i))
      return false;
    counts_.push_back(i);
    count += i;
  }
  DCHECK_EQ(count, redundant_count_);
  return count == redundant_count_;
}

//------------------------------------------------------------------------------
// LinearHistogram: This histogram uses a traditional set of evenly spaced
// buckets.
//------------------------------------------------------------------------------

LinearHistogram::~LinearHistogram() {
}

scoped_refptr<Histogram> LinearHistogram::FactoryGet(const std::string& name,
                                                     Sample minimum,
                                                     Sample maximum,
                                                     size_t bucket_count,
                                                     Flags flags) {
  scoped_refptr<Histogram> histogram(NULL);

  if (minimum < 1)
    minimum = 1;
  if (maximum > kSampleType_MAX - 1)
    maximum = kSampleType_MAX - 1;

  if (!StatisticsRecorder::FindHistogram(name, &histogram)) {
    histogram = new LinearHistogram(name, minimum, maximum, bucket_count);
    StatisticsRecorder::FindHistogram(name, &histogram);
  }

  DCHECK_EQ(LINEAR_HISTOGRAM, histogram->histogram_type());
  DCHECK(histogram->HasConstructorArguments(minimum, maximum, bucket_count));
  histogram->SetFlags(flags);
  return histogram;
}

scoped_refptr<Histogram> LinearHistogram::FactoryTimeGet(
    const std::string& name,
    TimeDelta minimum,
    TimeDelta maximum,
    size_t bucket_count,
    Flags flags) {
  return FactoryGet(name, minimum.InMilliseconds(), maximum.InMilliseconds(),
                    bucket_count, flags);
}

Histogram::ClassType LinearHistogram::histogram_type() const {
  return LINEAR_HISTOGRAM;
}

void LinearHistogram::SetRangeDescriptions(
    const DescriptionPair descriptions[]) {
  for (int i =0; descriptions[i].description; ++i) {
    bucket_description_[descriptions[i].sample] = descriptions[i].description;
  }
}

LinearHistogram::LinearHistogram(const std::string& name,
                                 Sample minimum,
                                 Sample maximum,
                                 size_t bucket_count)
    : Histogram(name, minimum >= 1 ? minimum : 1, maximum, bucket_count) {
  InitializeBucketRange();
  DCHECK(ValidateBucketRanges());
}

LinearHistogram::LinearHistogram(const std::string& name,
                                 TimeDelta minimum,
                                 TimeDelta maximum,
                                 size_t bucket_count)
    : Histogram(name, minimum >= TimeDelta::FromMilliseconds(1) ?
                                 minimum : TimeDelta::FromMilliseconds(1),
                maximum, bucket_count) {
  // Do a "better" (different) job at init than a base classes did...
  InitializeBucketRange();
  DCHECK(ValidateBucketRanges());
}

void LinearHistogram::InitializeBucketRange() {
  DCHECK_GT(declared_min(), 0);  // 0 is the underflow bucket here.
  double min = declared_min();
  double max = declared_max();
  size_t i;
  for (i = 1; i < bucket_count(); ++i) {
    double linear_range = (min * (bucket_count() -1 - i) + max * (i - 1)) /
                          (bucket_count() - 2);
    SetBucketRange(i, static_cast<int> (linear_range + 0.5));
  }
  ResetRangeChecksum();
}

double LinearHistogram::GetBucketSize(Count current, size_t i) const {
  DCHECK_GT(ranges(i + 1), ranges(i));
  // Adjacent buckets with different widths would have "surprisingly" many (few)
  // samples in a histogram if we didn't normalize this way.
  double denominator = ranges(i + 1) - ranges(i);
  return current/denominator;
}

const std::string LinearHistogram::GetAsciiBucketRange(size_t i) const {
  int range = ranges(i);
  BucketDescriptionMap::const_iterator it = bucket_description_.find(range);
  if (it == bucket_description_.end())
    return Histogram::GetAsciiBucketRange(i);
  return it->second;
}

bool LinearHistogram::PrintEmptyBucket(size_t index) const {
  return bucket_description_.find(ranges(index)) == bucket_description_.end();
}


//------------------------------------------------------------------------------
// This section provides implementation for BooleanHistogram.
//------------------------------------------------------------------------------

scoped_refptr<Histogram> BooleanHistogram::FactoryGet(const std::string& name,
                                                      Flags flags) {
  scoped_refptr<Histogram> histogram(NULL);

  if (!StatisticsRecorder::FindHistogram(name, &histogram)) {
    histogram = new BooleanHistogram(name);
    StatisticsRecorder::FindHistogram(name, &histogram);
  }

  DCHECK_EQ(BOOLEAN_HISTOGRAM, histogram->histogram_type());
  histogram->SetFlags(flags);
  return histogram;
}

Histogram::ClassType BooleanHistogram::histogram_type() const {
  return BOOLEAN_HISTOGRAM;
}

void BooleanHistogram::AddBoolean(bool value) {
  Add(value ? 1 : 0);
}

BooleanHistogram::BooleanHistogram(const std::string& name)
    : LinearHistogram(name, 1, 2, 3) {
}

//------------------------------------------------------------------------------
// CustomHistogram:
//------------------------------------------------------------------------------

scoped_refptr<Histogram> CustomHistogram::FactoryGet(
    const std::string& name,
    const std::vector<Sample>& custom_ranges,
    Flags flags) {
  scoped_refptr<Histogram> histogram(NULL);

  // Remove the duplicates in the custom ranges array.
  std::vector<int> ranges = custom_ranges;
  ranges.push_back(0);  // Ensure we have a zero value.
  std::sort(ranges.begin(), ranges.end());
  ranges.erase(std::unique(ranges.begin(), ranges.end()), ranges.end());
  if (ranges.size() <= 1) {
    DCHECK(false);
    // Note that we pushed a 0 in above, so for defensive code....
    ranges.push_back(1);  // Put in some data so we can index to [1].
  }

  DCHECK_LT(ranges.back(), kSampleType_MAX);

  if (!StatisticsRecorder::FindHistogram(name, &histogram)) {
    histogram = new CustomHistogram(name, ranges);
    StatisticsRecorder::FindHistogram(name, &histogram);
  }

  DCHECK_EQ(histogram->histogram_type(), CUSTOM_HISTOGRAM);
  DCHECK(histogram->HasConstructorArguments(ranges[1], ranges.back(),
                                            ranges.size()));
  histogram->SetFlags(flags);
  return histogram;
}

Histogram::ClassType CustomHistogram::histogram_type() const {
  return CUSTOM_HISTOGRAM;
}

CustomHistogram::CustomHistogram(const std::string& name,
                                 const std::vector<Sample>& custom_ranges)
    : Histogram(name, custom_ranges[1], custom_ranges.back(),
                custom_ranges.size()) {
  DCHECK_GT(custom_ranges.size(), 1u);
  DCHECK_EQ(custom_ranges[0], 0);
  ranges_vector_ = &custom_ranges;
  InitializeBucketRange();
  ranges_vector_ = NULL;
  DCHECK(ValidateBucketRanges());
}

void CustomHistogram::InitializeBucketRange() {
  DCHECK_LE(ranges_vector_->size(), bucket_count());
  for (size_t index = 0; index < ranges_vector_->size(); ++index)
    SetBucketRange(index, (*ranges_vector_)[index]);
  ResetRangeChecksum();
}

double CustomHistogram::GetBucketSize(Count current, size_t i) const {
  return 1;
}

//------------------------------------------------------------------------------
// The next section handles global (central) support for all histograms, as well
// as startup/teardown of this service.
//------------------------------------------------------------------------------

// This singleton instance should be started during the single threaded portion
// of main(), and hence it is not thread safe.  It initializes globals to
// provide support for all future calls.
StatisticsRecorder::StatisticsRecorder() {
  DCHECK(!histograms_);
  if (lock_ == NULL) {
    // This will leak on purpose. It's the only way to make sure we won't race
    // against the static uninitialization of the module while one of our
    // static methods relying on the lock get called at an inappropriate time
    // during the termination phase. Since it's a static data member, we will
    // leak one per process, which would be similar to the instance allocated
    // during static initialization and released only on  process termination.
    lock_ = new base::Lock;
  }
  base::AutoLock auto_lock(*lock_);
  histograms_ = new HistogramMap;
}

StatisticsRecorder::~StatisticsRecorder() {
  DCHECK(histograms_ && lock_);

  if (dump_on_exit_) {
    std::string output;
    WriteGraph("", &output);
    LOG(INFO) << output;
  }
  // Clean up.
  HistogramMap* histograms = NULL;
  {
    base::AutoLock auto_lock(*lock_);
    histograms = histograms_;
    histograms_ = NULL;
  }
  delete histograms;
  // We don't delete lock_ on purpose to avoid having to properly protect
  // against it going away after we checked for NULL in the static methods.
}

// static
bool StatisticsRecorder::IsActive() {
  if (lock_ == NULL)
    return false;
  base::AutoLock auto_lock(*lock_);
  return NULL != histograms_;
}

// Note: We can't accept a ref_ptr to |histogram| because we *might* not keep a
// reference, and we are called while in the Histogram constructor. In that
// scenario, a ref_ptr would have incremented the ref count when the histogram
// was passed to us, decremented it when we returned, and the instance would be
// destroyed before assignment (when value was returned by new).
// static
void StatisticsRecorder::Register(Histogram* histogram) {
  if (lock_ == NULL)
    return;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return;
  const std::string name = histogram->histogram_name();
  // Avoid overwriting a previous registration.
  if (histograms_->end() == histograms_->find(name))
    (*histograms_)[name] = histogram;
}

// static
void StatisticsRecorder::WriteHTMLGraph(const std::string& query,
                                        std::string* output) {
  if (!IsActive())
    return;
  output->append("<html><head><title>About Histograms");
  if (!query.empty())
    output->append(" - " + query);
  output->append("</title>"
                 // We'd like the following no-cache... but it doesn't work.
                 // "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
                 "</head><body>");

  Histograms snapshot;
  GetSnapshot(query, &snapshot);
  for (Histograms::iterator it = snapshot.begin();
       it != snapshot.end();
       ++it) {
    (*it)->WriteHTMLGraph(output);
    output->append("<br><hr><br>");
  }
  output->append("</body></html>");
}

// static
void StatisticsRecorder::WriteGraph(const std::string& query,
                                    std::string* output) {
  if (!IsActive())
    return;
  if (query.length())
    StringAppendF(output, "Collections of histograms for %s\n", query.c_str());
  else
    output->append("Collections of all histograms\n");

  Histograms snapshot;
  GetSnapshot(query, &snapshot);
  for (Histograms::iterator it = snapshot.begin();
       it != snapshot.end();
       ++it) {
    (*it)->WriteAscii(true, "\n", output);
    output->append("\n");
  }
}

// static
void StatisticsRecorder::GetHistograms(Histograms* output) {
  if (lock_ == NULL)
    return;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return;
  for (HistogramMap::iterator it = histograms_->begin();
       histograms_->end() != it;
       ++it) {
    DCHECK_EQ(it->first, it->second->histogram_name());
    output->push_back(it->second);
  }
}

bool StatisticsRecorder::FindHistogram(const std::string& name,
                                       scoped_refptr<Histogram>* histogram) {
  if (lock_ == NULL)
    return false;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return false;
  HistogramMap::iterator it = histograms_->find(name);
  if (histograms_->end() == it)
    return false;
  *histogram = it->second;
  return true;
}

// private static
void StatisticsRecorder::GetSnapshot(const std::string& query,
                                     Histograms* snapshot) {
  if (lock_ == NULL)
    return;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return;
  for (HistogramMap::iterator it = histograms_->begin();
       histograms_->end() != it;
       ++it) {
    if (it->first.find(query) != std::string::npos)
      snapshot->push_back(it->second);
  }
}

// static
StatisticsRecorder::HistogramMap* StatisticsRecorder::histograms_ = NULL;
// static
base::Lock* StatisticsRecorder::lock_ = NULL;
// static
bool StatisticsRecorder::dump_on_exit_ = false;

}  // namespace base
