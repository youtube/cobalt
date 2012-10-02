// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/values.h"

using std::string;
using std::vector;

namespace {

std::string ClassTypeToString(base::Histogram::ClassType type) {
  switch(type) {
    case base::Histogram::HISTOGRAM:
      return "HISTOGRAM";
      break;
    case base::Histogram::LINEAR_HISTOGRAM:
      return "LINEAR_HISTOGRAM";
      break;
    case base::Histogram::BOOLEAN_HISTOGRAM:
      return "BOOLEAN_HISTOGRAM";
      break;
    case base::Histogram::CUSTOM_HISTOGRAM:
      return "CUSTOM_HISTOGRAM";
      break;
    default:
      NOTREACHED();
      break;
  }
  return "UNKNOWN";
}

}  // namespace

namespace base {

typedef HistogramBase::Count Count;
typedef HistogramBase::Sample Sample;

// static
const size_t Histogram::kBucketCount_MAX = 16384u;

// TODO(rtenneti): delete this code after debugging.
void CheckCorruption(const Histogram& histogram, bool new_histogram) {
  const std::string& histogram_name = histogram.histogram_name();
  char histogram_name_buf[128];
  base::strlcpy(histogram_name_buf,
                histogram_name.c_str(),
                arraysize(histogram_name_buf));
  base::debug::Alias(histogram_name_buf);

  bool debug_new_histogram[1];
  debug_new_histogram[0] = new_histogram;
  base::debug::Alias(debug_new_histogram);

  Sample previous_range = -1;  // Bottom range is always 0.
  for (size_t index = 0; index < histogram.bucket_count(); ++index) {
    int new_range = histogram.ranges(index);
    CHECK_LT(previous_range, new_range);
    previous_range = new_range;
  }

  CHECK(histogram.bucket_ranges()->HasValidChecksum());
}

Histogram* Histogram::FactoryGet(const string& name,
                                 Sample minimum,
                                 Sample maximum,
                                 size_t bucket_count,
                                 int32 flags) {
  bool valid_arguments =
      InspectConstructionArguments(name, &minimum, &maximum, &bucket_count);
  DCHECK(valid_arguments);

  Histogram* histogram = StatisticsRecorder::FindHistogram(name);
  if (!histogram) {
    // To avoid racy destruction at shutdown, the following will be leaked.
    BucketRanges* ranges = new BucketRanges(bucket_count + 1);
    InitializeBucketRanges(minimum, maximum, bucket_count, ranges);
    const BucketRanges* registered_ranges =
        StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges);

    Histogram* tentative_histogram =
        new Histogram(name, minimum, maximum, bucket_count, registered_ranges);
    CheckCorruption(*tentative_histogram, true);

    tentative_histogram->SetFlags(flags);
    histogram =
        StatisticsRecorder::RegisterOrDeleteDuplicate(tentative_histogram);
  }
  // TODO(rtenneti): delete this code after debugging.
  CheckCorruption(*histogram, false);

  CHECK_EQ(HISTOGRAM, histogram->histogram_type());
  CHECK(histogram->HasConstructionArguments(minimum, maximum, bucket_count));
  return histogram;
}

Histogram* Histogram::FactoryTimeGet(const string& name,
                                     TimeDelta minimum,
                                     TimeDelta maximum,
                                     size_t bucket_count,
                                     int32 flags) {
  return FactoryGet(name, minimum.InMilliseconds(), maximum.InMilliseconds(),
                    bucket_count, flags);
}

TimeTicks Histogram::DebugNow() {
#ifndef NDEBUG
  return TimeTicks::Now();
#else
  return TimeTicks();
#endif
}

// Calculate what range of values are held in each bucket.
// We have to be careful that we don't pick a ratio between starting points in
// consecutive buckets that is sooo small, that the integer bounds are the same
// (effectively making one bucket get no values).  We need to avoid:
//   ranges(i) == ranges(i + 1)
// To avoid that, we just do a fine-grained bucket width as far as we need to
// until we get a ratio that moves us along at least 2 units at a time.  From
// that bucket onward we do use the exponential growth of buckets.
//
// static
void Histogram::InitializeBucketRanges(Sample minimum,
                                       Sample maximum,
                                       size_t bucket_count,
                                       BucketRanges* ranges) {
  DCHECK_EQ(ranges->size(), bucket_count + 1);
  double log_max = log(static_cast<double>(maximum));
  double log_ratio;
  double log_next;
  size_t bucket_index = 1;
  Sample current = minimum;
  ranges->set_range(bucket_index, current);
  while (bucket_count > ++bucket_index) {
    double log_current;
    log_current = log(static_cast<double>(current));
    // Calculate the count'th root of the range.
    log_ratio = (log_max - log_current) / (bucket_count - bucket_index);
    // See where the next bucket would start.
    log_next = log_current + log_ratio;
    Sample next;
    next = static_cast<int>(floor(exp(log_next) + 0.5));
    if (next > current)
      current = next;
    else
      ++current;  // Just do a narrow bucket, and keep trying.
    ranges->set_range(bucket_index, current);
  }
  ranges->set_range(ranges->size() - 1, HistogramBase::kSampleType_MAX);
  ranges->ResetChecksum();
}

void Histogram::Add(int value) {
  DCHECK_EQ(0, ranges(0));
  DCHECK_EQ(kSampleType_MAX, ranges(bucket_count_));

  if (value > kSampleType_MAX - 1)
    value = kSampleType_MAX - 1;
  if (value < 0)
    value = 0;
  samples_->Accumulate(value, 1);
}

void Histogram::AddBoolean(bool value) {
  DCHECK(false);
}

void Histogram::AddSamples(const HistogramSamples& samples) {
  samples_->Add(samples);
}

bool Histogram::AddSamplesFromPickle(PickleIterator* iter) {
  return samples_->AddFromPickle(iter);
}

void Histogram::SetRangeDescriptions(const DescriptionPair descriptions[]) {
  DCHECK(false);
}

// The following methods provide a graphical histogram display.
void Histogram::WriteHTMLGraph(string* output) const {
  // TBD(jar) Write a nice HTML bar chart, with divs an mouse-overs etc.
  output->append("<PRE>");
  WriteAsciiImpl(true, "<br>", output);
  output->append("</PRE>");
}

void Histogram::WriteAscii(string* output) const {
  WriteAsciiImpl(true, "\n", output);
}

// static
string Histogram::SerializeHistogramInfo(const Histogram& histogram,
                                         const HistogramSamples& snapshot) {
  DCHECK_NE(NOT_VALID_IN_RENDERER, histogram.histogram_type());
  DCHECK(histogram.bucket_ranges()->HasValidChecksum());

  Pickle pickle;
  pickle.WriteString(histogram.histogram_name());
  pickle.WriteInt(histogram.declared_min());
  pickle.WriteInt(histogram.declared_max());
  pickle.WriteUInt64(histogram.bucket_count());
  pickle.WriteUInt32(histogram.bucket_ranges()->checksum());
  pickle.WriteInt(histogram.histogram_type());
  pickle.WriteInt(histogram.flags());

  histogram.SerializeRanges(&pickle);

  snapshot.Serialize(&pickle);

  return string(static_cast<const char*>(pickle.data()), pickle.size());
}

// static
bool Histogram::DeserializeHistogramInfo(const string& histogram_info) {
  if (histogram_info.empty()) {
    return false;
  }

  Pickle pickle(histogram_info.data(),
                static_cast<int>(histogram_info.size()));
  string histogram_name;
  int declared_min;
  int declared_max;
  uint64 bucket_count;
  uint32 range_checksum;
  int histogram_type;
  int pickle_flags;

  PickleIterator iter(pickle);
  if (!iter.ReadString(&histogram_name) ||
      !iter.ReadInt(&declared_min) ||
      !iter.ReadInt(&declared_max) ||
      !iter.ReadUInt64(&bucket_count) ||
      !iter.ReadUInt32(&range_checksum) ||
      !iter.ReadInt(&histogram_type) ||
      !iter.ReadInt(&pickle_flags)) {
    DLOG(ERROR) << "Pickle error decoding Histogram: " << histogram_name;
    return false;
  }

  DCHECK(pickle_flags & kIPCSerializationSourceFlag);
  // Since these fields may have come from an untrusted renderer, do additional
  // checks above and beyond those in Histogram::Initialize()
  if (declared_max <= 0 || declared_min <= 0 || declared_max < declared_min ||
      INT_MAX / sizeof(Count) <= bucket_count || bucket_count < 2) {
    DLOG(ERROR) << "Values error decoding Histogram: " << histogram_name;
    return false;
  }

  Flags flags = static_cast<Flags>(pickle_flags & ~kIPCSerializationSourceFlag);

  DCHECK_NE(NOT_VALID_IN_RENDERER, histogram_type);

  Histogram* render_histogram(NULL);

  if (histogram_type == HISTOGRAM) {
    render_histogram = Histogram::FactoryGet(
        histogram_name, declared_min, declared_max, bucket_count, flags);
  } else if (histogram_type == LINEAR_HISTOGRAM) {
    render_histogram = LinearHistogram::FactoryGet(
        histogram_name, declared_min, declared_max, bucket_count, flags);
  } else if (histogram_type == BOOLEAN_HISTOGRAM) {
    render_histogram = BooleanHistogram::FactoryGet(histogram_name, flags);
  } else if (histogram_type == CUSTOM_HISTOGRAM) {
    vector<Sample> sample_ranges(bucket_count);
    if (!CustomHistogram::DeserializeRanges(&iter, &sample_ranges)) {
      DLOG(ERROR) << "Pickle error decoding ranges: " << histogram_name;
      return false;
    }
    render_histogram =
        CustomHistogram::FactoryGet(histogram_name, sample_ranges, flags);
  } else {
    DLOG(ERROR) << "Error Deserializing Histogram Unknown histogram_type: "
                << histogram_type;
    return false;
  }

  DCHECK_EQ(render_histogram->declared_min(), declared_min);
  DCHECK_EQ(render_histogram->declared_max(), declared_max);
  DCHECK_EQ(render_histogram->bucket_count(), bucket_count);
  DCHECK_EQ(render_histogram->histogram_type(), histogram_type);

  if (render_histogram->bucket_ranges()->checksum() != range_checksum) {
    return false;
  }

  if (render_histogram->flags() & kIPCSerializationSourceFlag) {
    DVLOG(1) << "Single process mode, histogram observed and not copied: "
             << histogram_name;
    return true;
  }

  DCHECK_EQ(flags & render_histogram->flags(), flags);
  return render_histogram->AddSamplesFromPickle(&iter);
}

// static
const int Histogram::kCommonRaceBasedCountMismatch = 5;

Histogram::Inconsistencies Histogram::FindCorruption(
    const HistogramSamples& samples) const {
  int inconsistencies = NO_INCONSISTENCIES;
  Sample previous_range = -1;  // Bottom range is always 0.
  for (size_t index = 0; index < bucket_count(); ++index) {
    int new_range = ranges(index);
    if (previous_range >= new_range)
      inconsistencies |= BUCKET_ORDER_ERROR;
    previous_range = new_range;
  }

  if (!bucket_ranges()->HasValidChecksum())
    inconsistencies |= RANGE_CHECKSUM_ERROR;

  int64 delta64 = samples.redundant_count() - samples.TotalCount();
  if (delta64 != 0) {
    int delta = static_cast<int>(delta64);
    if (delta != delta64)
      delta = INT_MAX;  // Flag all giant errors as INT_MAX.
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

Sample Histogram::ranges(size_t i) const {
  return bucket_ranges_->range(i);
}

size_t Histogram::bucket_count() const {
  return bucket_count_;
}

scoped_ptr<SampleVector> Histogram::SnapshotSamples() const {
  scoped_ptr<SampleVector> samples(new SampleVector(bucket_ranges()));
  samples->Add(*samples_);
  return samples.Pass();
}

bool Histogram::HasConstructionArguments(Sample minimum,
                                         Sample maximum,
                                         size_t bucket_count) {
  return ((minimum == declared_min_) && (maximum == declared_max_) &&
          (bucket_count == bucket_count_));
}

Histogram::Histogram(const string& name,
                     Sample minimum,
                     Sample maximum,
                     size_t bucket_count,
                     const BucketRanges* ranges)
  : HistogramBase(name),
    bucket_ranges_(ranges),
    declared_min_(minimum),
    declared_max_(maximum),
    bucket_count_(bucket_count) {
  if (ranges)
    samples_.reset(new SampleVector(ranges));
}

Histogram::~Histogram() {
  if (StatisticsRecorder::dump_on_exit()) {
    string output;
    WriteAsciiImpl(true, "\n", &output);
    DLOG(INFO) << output;
  }
}

// static
bool Histogram::InspectConstructionArguments(const string& name,
                                             Sample* minimum,
                                             Sample* maximum,
                                             size_t* bucket_count) {
  // Defensive code for backward compatibility.
  if (*minimum < 1) {
    DVLOG(1) << "Histogram: " << name << " has bad minimum: " << *minimum;
    *minimum = 1;
  }
  if (*maximum >= kSampleType_MAX) {
    DVLOG(1) << "Histogram: " << name << " has bad maximum: " << *maximum;
    *maximum = kSampleType_MAX - 1;
  }
  if (*bucket_count >= kBucketCount_MAX) {
    DVLOG(1) << "Histogram: " << name << " has bad bucket_count: "
             << *bucket_count;
    *bucket_count = kBucketCount_MAX - 1;
  }

  if (*minimum >= *maximum)
    return false;
  if (*bucket_count < 3)
    return false;
  if (*bucket_count > static_cast<size_t>(*maximum - *minimum + 2))
    return false;
  return true;
}

bool Histogram::SerializeRanges(Pickle* pickle) const {
  return true;
}

bool Histogram::PrintEmptyBucket(size_t index) const {
  return true;
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

const string Histogram::GetAsciiBucketRange(size_t i) const {
  string result;
  if (kHexRangePrintingFlag & flags())
    StringAppendF(&result, "%#x", ranges(i));
  else
    StringAppendF(&result, "%d", ranges(i));
  return result;
}

//------------------------------------------------------------------------------
// Private methods

void Histogram::WriteAsciiImpl(bool graph_it,
                               const string& newline,
                               string* output) const {
  // Get local (stack) copies of all effectively volatile class data so that we
  // are consistent across our output activities.
  scoped_ptr<SampleVector> snapshot = SnapshotSamples();
  Count sample_count = snapshot->TotalCount();

  WriteAsciiHeader(*snapshot, sample_count, output);
  output->append(newline);

  // Prepare to normalize graphical rendering of bucket contents.
  double max_size = 0;
  if (graph_it)
    max_size = GetPeakBucketSize(*snapshot);

  // Calculate space needed to print bucket range numbers.  Leave room to print
  // nearly the largest bucket range without sliding over the histogram.
  size_t largest_non_empty_bucket = bucket_count() - 1;
  while (0 == snapshot->GetCountAtIndex(largest_non_empty_bucket)) {
    if (0 == largest_non_empty_bucket)
      break;  // All buckets are empty.
    --largest_non_empty_bucket;
  }

  // Calculate largest print width needed for any of our bucket range displays.
  size_t print_width = 1;
  for (size_t i = 0; i < bucket_count(); ++i) {
    if (snapshot->GetCountAtIndex(i)) {
      size_t width = GetAsciiBucketRange(i).size() + 1;
      if (width > print_width)
        print_width = width;
    }
  }

  int64 remaining = sample_count;
  int64 past = 0;
  // Output the actual histogram graph.
  for (size_t i = 0; i < bucket_count(); ++i) {
    Count current = snapshot->GetCountAtIndex(i);
    if (!current && !PrintEmptyBucket(i))
      continue;
    remaining -= current;
    string range = GetAsciiBucketRange(i);
    output->append(range);
    for (size_t j = 0; range.size() + j < print_width + 1; ++j)
      output->push_back(' ');
    if (0 == current && i < bucket_count() - 1 &&
        0 == snapshot->GetCountAtIndex(i + 1)) {
      while (i < bucket_count() - 1 &&
             0 == snapshot->GetCountAtIndex(i + 1)) {
        ++i;
      }
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

double Histogram::GetPeakBucketSize(const SampleVector& samples) const {
  double max = 0;
  for (size_t i = 0; i < bucket_count() ; ++i) {
    double current_size = GetBucketSize(samples.GetCountAtIndex(i), i);
    if (current_size > max)
      max = current_size;
  }
  return max;
}

void Histogram::WriteAsciiHeader(const SampleVector& samples,
                                 Count sample_count,
                                 string* output) const {
  StringAppendF(output,
                "Histogram: %s recorded %d samples",
                histogram_name().c_str(),
                sample_count);
  if (0 == sample_count) {
    DCHECK_EQ(samples.sum(), 0);
  } else {
    double average = static_cast<float>(samples.sum()) / sample_count;

    StringAppendF(output, ", average = %.1f", average);
  }
  if (flags() & ~kHexRangePrintingFlag)
    StringAppendF(output, " (flags = 0x%x)", flags() & ~kHexRangePrintingFlag);
}

void Histogram::WriteAsciiBucketContext(const int64 past,
                                        const Count current,
                                        const int64 remaining,
                                        const size_t i,
                                        string* output) const {
  double scaled_sum = (past + current + remaining) / 100.0;
  WriteAsciiBucketValue(current, scaled_sum, output);
  if (0 < i) {
    double percentage = past / scaled_sum;
    StringAppendF(output, " {%3.1f%%}", percentage);
  }
}

void Histogram::WriteAsciiBucketValue(Count current,
                                      double scaled_sum,
                                      string* output) const {
  StringAppendF(output, " (%d = %3.1f%%)", current, current/scaled_sum);
}

void Histogram::WriteAsciiBucketGraph(double current_size,
                                      double max_size,
                                      string* output) const {
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

void Histogram::GetParameters(DictionaryValue* params) const {
  params->SetString("type", ClassTypeToString(histogram_type()));
  params->SetInteger("min", declared_min());
  params->SetInteger("max", declared_max());
  params->SetInteger("bucket_count", static_cast<int>(bucket_count()));
}

void Histogram::GetCountAndBucketData(Count* count, ListValue* buckets) const {
  scoped_ptr<SampleVector> snapshot = SnapshotSamples();
  *count = snapshot->TotalCount();
  size_t index = 0;
  for (size_t i = 0; i < bucket_count(); ++i) {
    Sample count = snapshot->GetCountAtIndex(i);
    if (count > 0) {
      scoped_ptr<DictionaryValue> bucket_value(new DictionaryValue());
      bucket_value->SetInteger("low", ranges(i));
      if (i != bucket_count() - 1)
        bucket_value->SetInteger("high", ranges(i + 1));
      bucket_value->SetInteger("count", count);
      buckets->Set(index, bucket_value.release());
      ++index;
    }
  }
}

//------------------------------------------------------------------------------
// LinearHistogram: This histogram uses a traditional set of evenly spaced
// buckets.
//------------------------------------------------------------------------------

LinearHistogram::~LinearHistogram() {}

Histogram* LinearHistogram::FactoryGet(const string& name,
                                       Sample minimum,
                                       Sample maximum,
                                       size_t bucket_count,
                                       int32 flags) {
  bool valid_arguments = Histogram::InspectConstructionArguments(
      name, &minimum, &maximum, &bucket_count);
  DCHECK(valid_arguments);

  Histogram* histogram = StatisticsRecorder::FindHistogram(name);
  if (!histogram) {
    // To avoid racy destruction at shutdown, the following will be leaked.
    BucketRanges* ranges = new BucketRanges(bucket_count + 1);
    InitializeBucketRanges(minimum, maximum, bucket_count, ranges);
    const BucketRanges* registered_ranges =
        StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges);

    LinearHistogram* tentative_histogram =
        new LinearHistogram(name, minimum, maximum, bucket_count,
                            registered_ranges);
    CheckCorruption(*tentative_histogram, true);

    tentative_histogram->SetFlags(flags);
    histogram =
        StatisticsRecorder::RegisterOrDeleteDuplicate(tentative_histogram);
  }
  // TODO(rtenneti): delete this code after debugging.
  CheckCorruption(*histogram, false);

  CHECK_EQ(LINEAR_HISTOGRAM, histogram->histogram_type());
  CHECK(histogram->HasConstructionArguments(minimum, maximum, bucket_count));
  return histogram;
}

Histogram* LinearHistogram::FactoryTimeGet(const string& name,
                                           TimeDelta minimum,
                                           TimeDelta maximum,
                                           size_t bucket_count,
                                           int32 flags) {
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

LinearHistogram::LinearHistogram(const string& name,
                                 Sample minimum,
                                 Sample maximum,
                                 size_t bucket_count,
                                 const BucketRanges* ranges)
    : Histogram(name, minimum, maximum, bucket_count, ranges) {
}

double LinearHistogram::GetBucketSize(Count current, size_t i) const {
  DCHECK_GT(ranges(i + 1), ranges(i));
  // Adjacent buckets with different widths would have "surprisingly" many (few)
  // samples in a histogram if we didn't normalize this way.
  double denominator = ranges(i + 1) - ranges(i);
  return current/denominator;
}

const string LinearHistogram::GetAsciiBucketRange(size_t i) const {
  int range = ranges(i);
  BucketDescriptionMap::const_iterator it = bucket_description_.find(range);
  if (it == bucket_description_.end())
    return Histogram::GetAsciiBucketRange(i);
  return it->second;
}

bool LinearHistogram::PrintEmptyBucket(size_t index) const {
  return bucket_description_.find(ranges(index)) == bucket_description_.end();
}

// static
void LinearHistogram::InitializeBucketRanges(Sample minimum,
                                             Sample maximum,
                                             size_t bucket_count,
                                             BucketRanges* ranges) {
  DCHECK_EQ(ranges->size(), bucket_count + 1);
  double min = minimum;
  double max = maximum;
  size_t i;
  for (i = 1; i < bucket_count; ++i) {
    double linear_range =
        (min * (bucket_count -1 - i) + max * (i - 1)) / (bucket_count - 2);
    ranges->set_range(i, static_cast<Sample>(linear_range + 0.5));
  }
  ranges->set_range(ranges->size() - 1, HistogramBase::kSampleType_MAX);
  ranges->ResetChecksum();
}

//------------------------------------------------------------------------------
// This section provides implementation for BooleanHistogram.
//------------------------------------------------------------------------------

Histogram* BooleanHistogram::FactoryGet(const string& name, int32 flags) {
  Histogram* histogram = StatisticsRecorder::FindHistogram(name);
  if (!histogram) {
    // To avoid racy destruction at shutdown, the following will be leaked.
    BucketRanges* ranges = new BucketRanges(4);
    LinearHistogram::InitializeBucketRanges(1, 2, 3, ranges);
    const BucketRanges* registered_ranges =
        StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges);

    BooleanHistogram* tentative_histogram =
        new BooleanHistogram(name, registered_ranges);
    CheckCorruption(*tentative_histogram, true);

    tentative_histogram->SetFlags(flags);
    histogram =
        StatisticsRecorder::RegisterOrDeleteDuplicate(tentative_histogram);
  }
  // TODO(rtenneti): delete this code after debugging.
  CheckCorruption(*histogram, false);

  CHECK_EQ(BOOLEAN_HISTOGRAM, histogram->histogram_type());
  return histogram;
}

Histogram::ClassType BooleanHistogram::histogram_type() const {
  return BOOLEAN_HISTOGRAM;
}

void BooleanHistogram::AddBoolean(bool value) {
  Add(value ? 1 : 0);
}

BooleanHistogram::BooleanHistogram(const string& name,
                                   const BucketRanges* ranges)
    : LinearHistogram(name, 1, 2, 3, ranges) {}

//------------------------------------------------------------------------------
// CustomHistogram:
//------------------------------------------------------------------------------

Histogram* CustomHistogram::FactoryGet(const string& name,
                                       const vector<Sample>& custom_ranges,
                                       int32 flags) {
  CHECK(ValidateCustomRanges(custom_ranges));

  Histogram* histogram = StatisticsRecorder::FindHistogram(name);
  if (!histogram) {
    BucketRanges* ranges = CreateBucketRangesFromCustomRanges(custom_ranges);
    const BucketRanges* registered_ranges =
        StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges);

    // To avoid racy destruction at shutdown, the following will be leaked.
    CustomHistogram* tentative_histogram =
        new CustomHistogram(name, registered_ranges);
    CheckCorruption(*tentative_histogram, true);

    tentative_histogram->SetFlags(flags);

    histogram =
        StatisticsRecorder::RegisterOrDeleteDuplicate(tentative_histogram);
  }
  // TODO(rtenneti): delete this code after debugging.
  CheckCorruption(*histogram, false);

  CHECK_EQ(histogram->histogram_type(), CUSTOM_HISTOGRAM);
  return histogram;
}

Histogram::ClassType CustomHistogram::histogram_type() const {
  return CUSTOM_HISTOGRAM;
}

// static
vector<Sample> CustomHistogram::ArrayToCustomRanges(
    const Sample* values, size_t num_values) {
  vector<Sample> all_values;
  for (size_t i = 0; i < num_values; ++i) {
    Sample value = values[i];
    all_values.push_back(value);

    // Ensure that a guard bucket is added. If we end up with duplicate
    // values, FactoryGet will take care of removing them.
    all_values.push_back(value + 1);
  }
  return all_values;
}

CustomHistogram::CustomHistogram(const string& name,
                                 const BucketRanges* ranges)
    : Histogram(name,
                ranges->range(1),
                ranges->range(ranges->size() - 2),
                ranges->size() - 1,
                ranges) {}

bool CustomHistogram::SerializeRanges(Pickle* pickle) const {
  for (size_t i = 0; i < bucket_ranges()->size(); ++i) {
    if (!pickle->WriteInt(bucket_ranges()->range(i)))
      return false;
  }
  return true;
}

// static
bool CustomHistogram::DeserializeRanges(
    PickleIterator* iter, vector<Sample>* ranges) {
  for (size_t i = 0; i < ranges->size(); ++i) {
    if (!iter->ReadInt(&(*ranges)[i]))
      return false;
  }
  return true;
}

double CustomHistogram::GetBucketSize(Count current, size_t i) const {
  return 1;
}

// static
bool CustomHistogram::ValidateCustomRanges(
    const vector<Sample>& custom_ranges) {
  bool has_valid_range = false;
  for (size_t i = 0; i < custom_ranges.size(); i++) {
    Sample sample = custom_ranges[i];
    if (sample < 0 || sample > HistogramBase::kSampleType_MAX - 1)
      return false;
    if (sample != 0)
      has_valid_range = true;
  }
  return has_valid_range;
}

// static
BucketRanges* CustomHistogram::CreateBucketRangesFromCustomRanges(
      const vector<Sample>& custom_ranges) {
  // Remove the duplicates in the custom ranges array.
  vector<int> ranges = custom_ranges;
  ranges.push_back(0);  // Ensure we have a zero value.
  ranges.push_back(HistogramBase::kSampleType_MAX);
  std::sort(ranges.begin(), ranges.end());
  ranges.erase(std::unique(ranges.begin(), ranges.end()), ranges.end());

  BucketRanges* bucket_ranges = new BucketRanges(ranges.size());
  for (size_t i = 0; i < ranges.size(); i++) {
    bucket_ranges->set_range(i, ranges[i]);
  }
  bucket_ranges->ResetChecksum();
  return bucket_ranges;
}

}  // namespace base
