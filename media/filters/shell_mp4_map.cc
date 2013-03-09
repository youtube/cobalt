/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/filters/shell_mp4_map.h"

#include "base/stringprintf.h"
#include "lb_platform.h"
#include "media/base/shell_filter_graph_log.h"
#include "media/filters/shell_mp4_parser.h"

namespace media {

// ==== TableCache =============================================================

ShellMP4Map::TableCache::TableCache(
    uint64 table_offset,
    uint32 entry_count,
    uint32 entry_size,
    uint32 cache_size_entries,
    scoped_refptr<ShellDataSourceReader> reader,
    scoped_refptr<ShellFilterGraphLog> filter_graph_log)
    : entry_size_(entry_size)
    , entry_count_(entry_count)
    , cache_size_entries_(cache_size_entries)
    , table_offset_(table_offset)
    , reader_(reader)
    , filter_graph_log_(filter_graph_log)
    , cache_first_entry_number_(0)
    , cache_entry_count_(0) {
  // pre-fill the table
  GetBytesAtEntry(0);
}

uint8* ShellMP4Map::TableCache::GetBytesAtEntry(uint32 entry_number) {
  // don't fetch the unfetchable
  if (entry_number >= entry_count_) {
    return NULL;
  }
  // this query within valid range for the current cache table?
  if (entry_number < cache_first_entry_number_ ||
      entry_number >= cache_first_entry_number_ + cache_entry_count_) {
    // calculate first entry in table keeping cache size alignment in table
    cache_entry_count_ = cache_size_entries_;
    // save old first entry number for callback
    uint32 old_first_entry = cache_first_entry_number_;
    cache_first_entry_number_ = (entry_number / cache_entry_count_) *
        cache_entry_count_;
    // see if we have exceeded our table bounds
    if (cache_first_entry_number_ + cache_entry_count_ > entry_count_) {
      cache_entry_count_ = entry_count_ - cache_first_entry_number_;
    }
    // drop old data to allow ShellBufferFactory to defrag
    cache_ = NULL;
    int bytes_to_read = cache_entry_count_ * entry_size_;
    cache_ = ShellBufferFactory::Instance()->AllocateArray(bytes_to_read,
                                                           filter_graph_log_);
    if (!cache_ || !cache_->Get()) {
      cache_entry_count_ = 0;
      return NULL;
    }
    uint64 file_offset = table_offset_ +
        (cache_first_entry_number_ * entry_size_);
    int bytes_read = reader_->BlockingRead(file_offset,
                                           bytes_to_read,
                                           cache_->Get());
    if (bytes_read < bytes_to_read) {
      cache_entry_count_ = 0;
      return NULL;
    }
  }
  // cache is assumed to be valid and to contain the entry from here on
  DCHECK(cache_->Get());
  DCHECK_GE(entry_number, cache_first_entry_number_);
  DCHECK_LT(entry_number, cache_first_entry_number_ + cache_entry_count_);

  uint32 cache_offset = entry_number - cache_first_entry_number_;
  return cache_->Get() + (cache_offset * entry_size_);
}

// ==== ShellMP4Map ============================================================

// atom | name                  | size | description, (*) means optional table
// -----+-----------------------+------+----------------------------------------
// co64 | chunk offset (64-bit) | 8    | per-chunk list of chunk file offsets
// ctts | composition offset    | 8    | (*) run-length sample number to cts
// stco | chunk offset (32-bit) | 4    | per-chunk list of chunk file offsets
// stsc | sample-to-chunk       | 12   | chunk number to samples per chunk
// stss | sync sample           | 4    | (*) list of keyframe sample numbers
// stts | time-to-sample        | 8    | run-length sample number to duration
// stsz | sample size           | 4    | per-sample list of sample sizes

ShellMP4Map::ShellMP4Map(scoped_refptr<ShellDataSourceReader> reader,
                         scoped_refptr<ShellFilterGraphLog> filter_graph_log)
    : reader_(reader)
    , filter_graph_log_(filter_graph_log)
    , current_chunk_sample_(0)
    , next_chunk_sample_(0)
    , current_chunk_offset_(0)
    , highest_valid_sample_number_(UINT32_MAX)
    , ctts_table_index_(0)
    , ctts_first_sample_(0)
    , ctts_sample_offset_(0)
    , ctts_next_first_sample_(0)
    , stsc_first_chunk_(0)
    , stsc_first_chunk_sample_(0)
    , stsc_samples_per_chunk_(0)
    , stsc_next_first_chunk_(0)
    , stsc_next_first_chunk_sample_(0)
    , stsc_table_index_(0)
    , stsz_default_size_(0)
    , stts_first_sample_(0)
    , stts_first_sample_time_(0)
    , stts_sample_duration_(0)
    , stts_next_first_sample_(0)
    , stts_table_index_(0) {
}

bool ShellMP4Map::IsComplete() {
  // all required table pointers must be valid for map to function
  return (co64_ || stco_) && stsc_ && stts_ && (stsz_ || stsz_default_size_);
}

// The sample size is a lookup in the stsz table, which is indexed per sample
// number.
bool ShellMP4Map::GetSize(uint32 sample_number, uint32& size_out) {
  DCHECK(stsz_ || stsz_default_size_);

  if (sample_number > highest_valid_sample_number_) {
    return false;
  }

  if (stsz_default_size_) {
    size_out = stsz_default_size_;
  } else {
    // advanced cached table to this sample number entry
    uint8* stsz_entry = stsz_->GetBytesAtEntry(sample_number);
    if (!stsz_entry) {
      return false;
    }
    size_out = LB::Platform::load_uint32_big_endian(stsz_entry);
  }
  return true;
}

// We first must integrate the stsc table to find the chunk number that the
// sample resides in, and the first sample number in that chunk. We look up that
// chunk offset from the stco or co64, which are indexed by chunk number. We
// then use the stsz to sum samples to the byte offset with that chunk. The sum
// of the chunk offset and the byte offset within the chunk is the offset of
// the sample.
bool ShellMP4Map::GetOffset(uint32 sample_number, uint64& offset_out) {
  DCHECK(stsc_);
  DCHECK(stco_ || co64_);
  DCHECK(stsz_ || stsz_default_size_);

  if (sample_number > highest_valid_sample_number_) {
    return false;
  }

  // check for sequential access of sample numbers within the same chunk
  if (sample_number < current_chunk_sample_ ||
    sample_number >= next_chunk_sample_) {
    // integrate through stsc until we find the chunk range containing sample
    if (!stsc_AdvanceToSample(sample_number)) {
      return false;
    }
    // make sure stsc advance did its job correctly
    DCHECK_GE(sample_number, stsc_first_chunk_sample_);

    // calculate chunk number based on chunk sample size for this range
    uint32 sample_offset = sample_number - stsc_first_chunk_sample_;
    uint32 chunk_range_offset =  sample_offset / stsc_samples_per_chunk_;
    uint32 chunk_number = stsc_first_chunk_ + chunk_range_offset;
    // should be within the range of chunks with this sample size
    DCHECK_LT(chunk_number, stsc_next_first_chunk_);
    // update first sample number contained within this chunk
    current_chunk_sample_ = stsc_first_chunk_sample_ +
        (chunk_range_offset * stsc_samples_per_chunk_);
    // update first sample number of next chunk
    next_chunk_sample_ = current_chunk_sample_ + stsc_samples_per_chunk_;
    // find offset of this chunk within the file from co64/stco
    if (co64_) {
      uint8* co64_entry = co64_->GetBytesAtEntry(chunk_number);
      if (!co64_entry) {
        return false;
      }
      current_chunk_offset_ = LB::Platform::load_uint64_big_endian(co64_entry);
    } else {
      uint8* stco_entry = stco_->GetBytesAtEntry(chunk_number);
      if (!stco_entry) {
        return false;
      }
      current_chunk_offset_ = LB::Platform::load_uint32_big_endian(stco_entry);
    }
  }

  // at this point we should have sample_number within the range of our chunk
  // offset summation saved state
  DCHECK_LE(current_chunk_sample_, sample_number);
  DCHECK_LT(sample_number, next_chunk_sample_);

  if (stsz_default_size_ > 0) {
    current_chunk_offset_ += (sample_number - current_chunk_sample_) *
                              stsz_default_size_;
    current_chunk_sample_ = sample_number;
  } else {
    // sum sample sizes within chunk to get to byte offset of sample
    while (current_chunk_sample_ < sample_number) {
      uint32 sample_size = 0;
      if (!GetSize(current_chunk_sample_, sample_size)) {
        return false;
      }
      current_chunk_offset_ += sample_size;
      current_chunk_sample_++;
    }
  }

  offset_out = current_chunk_offset_;
  return true;
}

// Given a current sample number we integrate through the stts to find the
// duration of the current sample, and at the same time integrate through the
// durations to find the dts of that sample number. We then integrate sample
// numbers through the ctts to find the composition time offset, which we add to
// the dts to return the pts.
bool ShellMP4Map::GetTimestamp(uint32 sample_number, uint64& timestamp_out) {
  if (sample_number > highest_valid_sample_number_) {
    return false;
  }

  if (!stts_AdvanceToSample(sample_number)) {
    return false;
  }
  DCHECK_LT(sample_number, stts_next_first_sample_);
  DCHECK_GE(sample_number, stts_first_sample_);
  uint64 dts = stts_first_sample_time_ +
      (sample_number - stts_first_sample_) * stts_sample_duration_;
  if (ctts_) {
    if (!ctts_AdvanceToSample(sample_number)) {
      return false;
    }
    DCHECK_LT(sample_number, ctts_next_first_sample_);
    DCHECK_GE(sample_number, ctts_first_sample_);
  }
  timestamp_out = dts + ctts_sample_offset_;
  return true;
}

// Sum through the stts to find the duration of the given sample_number.
bool ShellMP4Map::GetDuration(uint32 sample_number, uint32& duration_out) {
  if (sample_number > highest_valid_sample_number_) {
    return false;
  }

  if (!stts_AdvanceToSample(sample_number)) {
    return false;
  }
  DCHECK_LT(sample_number, stts_next_first_sample_);
  DCHECK_GE(sample_number, stts_first_sample_);
  duration_out = stts_sample_duration_;
  return true;
}

bool ShellMP4Map::GetIsKeyframe(uint32 sample_number, bool& is_keyframe_out) {
  if (sample_number > highest_valid_sample_number_) {
    return false;
  }

  // This will not work for seeking but for simple playback all that this means
  // is that the full AnnexB prepend will be attached to every video frame, a
  // bit wasteful but otherwise harmless.
  is_keyframe_out = true;
  return true;
}

bool ShellMP4Map::IsEOS(uint32 sample_number) {
  return (sample_number > highest_valid_sample_number_);
}


// First look up the sample number for the provided timestamp by integrating
// timestamps through the stts. Then do a binary search on the stss to find the
// keyframe nearest that sample number.
bool ShellMP4Map::GetKeyframe(uint64 timestamp, uint32& sample_out) {
  NOTIMPLEMENTED();
  return false;
}

// Set up map state and load first part of table, or entire table if it is small
// enough, for each of the supporated atoms.
void ShellMP4Map::SetAtom(uint32 four_cc,
                          uint64 offset,
                          uint64 size,
                          uint32 cache_size_entries,
                          const uint8* atom) {
  // All map atoms are variable-length tables starting with 4 bytes of
  // version/flag info followed by a uint32 indicating the number of items in
  // table. The stsz atom bucks tradition by putting an optional default value
  // at index 4.
  uint32 count = 0;
  uint64 table_offset = offset + 8;
  if (four_cc == kAtomType_stsz) {
    if (size < 12) {
      return;
    }
    stsz_default_size_ = LB::Platform::load_uint32_big_endian(atom + 4);
    // if a non-zero default size is provided don't bother loading the table
    if (stsz_default_size_) {
      stsz_ = NULL;
      return;
    }
    count = LB::Platform::load_uint32_big_endian(atom + 8);
    highest_valid_sample_number_ = std::min(count - 1,
                                            highest_valid_sample_number_);
    table_offset += 4;
  } else {
    if (size < 8) {
      return;
    }
    count = LB::Platform::load_uint32_big_endian(atom + 4);
  }

  // TODO: size sanity-checking against per-entry size

  // initialize the appropriate table cache dependent on table type
  switch (four_cc) {
    case kAtomType_co64:
      co64_ = new TableCache(table_offset,
                             count,
                             kEntrySize_co64,
                             cache_size_entries,
                             reader_,
                             filter_graph_log_);
      co64_Init();
      break;

    case kAtomType_ctts:
      ctts_ = new TableCache(table_offset,
                             count,
                             kEntrySize_ctts,
                             cache_size_entries,
                             reader_,
                             filter_graph_log_);
      ctts_Init();
      break;

    case kAtomType_stco:
      stco_ = new TableCache(table_offset,
                             count,
                             kEntrySize_stco,
                             cache_size_entries,
                             reader_,
                             filter_graph_log_);
      stco_Init();
      break;

    case kAtomType_stsc:
      stsc_ = new TableCache(table_offset,
                             count,
                             kEntrySize_stsc,
                             cache_size_entries,
                             reader_,
                             filter_graph_log_);
      stsc_Init();
      break;

    case kAtomType_stss:
      // idea: to reduce cache trash on stss loads perhaps we could save the
      // ranges of each cache segment, then do a binary search on those segments
      // first..
      stss_ = new TableCache(table_offset,
                             count,
                             kEntrySize_stss,
                             cache_size_entries,
                             reader_,
                             filter_graph_log_);
      break;

    case kAtomType_stts:
      stts_ = new TableCache(table_offset,
                             count,
                             kEntrySize_stts,
                             cache_size_entries,
                             reader_,
                             filter_graph_log_);
      stts_Init();
      break;

    case kAtomType_stsz:
      stsz_ = new TableCache(table_offset,
                             count,
                             kEntrySize_stsz,
                             cache_size_entries,
                             reader_,
                             filter_graph_log_);
      break;

    default:
      NOTREACHED() << "unknown atom type provided to mp4 map";
      break;
  }
}

void ShellMP4Map::co64_Init() {
  DCHECK(co64_);
  // load offset of first chunk into current_chunk_offset_
  if (co64_->GetEntryCount() > 0) {
    // can drop any stco table already allocated
    stco_ = NULL;
    // load initial value of current_chunk_offset_ for 0th chunk
    uint8* co64_bytes = co64_->GetBytesAtEntry(0);
    current_chunk_offset_ = LB::Platform::load_uint64_big_endian(co64_bytes);
  } else {
    stco_ = NULL;
  }
}

// The ctts table has the following per-entry layout:
// uint32 sample count
// uint32 composition offset in ticks
//
void ShellMP4Map::ctts_Init() {
  DCHECK(ctts_);
  // get cache segment vector to reserve table entries in advance
  int cache_segments = (ctts_->GetEntryCount() /
                        ctts_->GetCacheSizeEntries()) + 1;
  ctts_samples_.reserve(cache_segments);
  if (ctts_->GetEntryCount() > 0) {
    // save the start of the first table integration at 0
    ctts_samples_.push_back(0);
    // load first entry in table, to start integration
    uint8* ctts_entry = ctts_->GetBytesAtEntry(0);
    if (!ctts_entry) {
      NOTREACHED();
      return;
    }
    ctts_table_index_ = 0;
    ctts_first_sample_ = 0;
    ctts_next_first_sample_ = LB::Platform::load_uint32_big_endian(ctts_entry);
    ctts_sample_offset_ = LB::Platform::load_uint32_big_endian(ctts_entry + 4);
  } else {
    // drop empty ctts_ table
    ctts_ = NULL;
  }
}

// To find the composition offset of a given sample number we must integrate
// through the ctts to find the range of samples containing sample_number. Note
// that the ctts is an optional table.
bool ShellMP4Map::ctts_AdvanceToSample(uint32 sample_number) {
  // ctts table is optional, so treat not having one as non-fatal
  if (!ctts_) {
    return true;
  }
  // sample number could be before our saved first sample, meaning we've
  // gone backward in sample numbers and will need to restart integration at
  // the nearest saved sample count starting at a cache entry
  if (sample_number < ctts_first_sample_) {
    if (!ctts_SlipCacheToSample(sample_number, 0)) {
      return false;
    }
  }

  // sample_number could also be ahead of our current range, for example when
  // seeking forward. See if we've calculated these values ahead of us before,
  // and if we can slip forward to them
  int next_cache_index = (ctts_table_index_ /
                          ctts_->GetCacheSizeEntries()) + 1;
  if ((next_cache_index < ctts_samples_.size() < ctts_samples_.size()) &&
      (sample_number >= ctts_samples_[next_cache_index])) {
    if (!ctts_SlipCacheToSample(sample_number, next_cache_index)) {
      return false;
    }
  }

  // perform integration until sample number is within correct ctts range
  while (ctts_next_first_sample_ <= sample_number) {
    // next first sample is now our the first sample
    ctts_first_sample_ = ctts_next_first_sample_;
    // advance to next entry in table
    ctts_table_index_++;
    // If this would be a new cache entry, keep a record of integration up
    // to this point so we don't have to start from 0 on seeking back
    if (!(ctts_table_index_ % ctts_->GetCacheSizeEntries())) {
      int cache_index = ctts_table_index_ / ctts_->GetCacheSizeEntries();
      // check that this is our first time with these data
      if (cache_index == ctts_samples_.size()) {
        ctts_samples_.push_back(ctts_first_sample_);
      }
      // our integration at this point should always match any stored record
      DCHECK_EQ(ctts_first_sample_, ctts_samples_[cache_index]);
    }

    if (ctts_table_index_ < ctts_->GetEntryCount()) {
      // load the sample count to determine next first sample
      uint8* ctts_entry = ctts_->GetBytesAtEntry(ctts_table_index_);
      if (!ctts_entry) {
        return false;
      }
      uint32 sample_count = LB::Platform::load_uint32_big_endian(ctts_entry);
      ctts_next_first_sample_ = ctts_first_sample_ + sample_count;
      // load the offset as well
      ctts_sample_offset_ =
          LB::Platform::load_uint32_big_endian(ctts_entry + 4);
    } else {
      // This means that the last entry in the table specified a sample range
      // that this sample number has exceeded, and so the ctts of this sample
      // number is undefined. While not a fatal error it's kind of a weird
      // state, we set the offset back to zero and extend the next_first_sample
      // to infinity
      DLOG(WARNING) << base::StringPrintf(
          "out of range sample number %d in ctts, last valid sample number: %d",
          sample_number, ctts_next_first_sample_);
      ctts_sample_offset_ = 0;
      ctts_next_first_sample_ = UINT32_MAX;
      break;
    }
  }
  return true;
}

bool ShellMP4Map::ctts_SlipCacheToSample(uint32 sample_number,
                                         int starting_cache_index) {
  DCHECK_LT(starting_cache_index, ctts_samples_.size());
  int cache_index = starting_cache_index;
  for (; cache_index + 1 < ctts_samples_.size(); cache_index++) {
    if (sample_number < ctts_samples_[cache_index + 1]) {
      break;
    }
  }
  ctts_first_sample_ = stts_samples_[cache_index];
  ctts_table_index_ = cache_index * ctts_->GetCacheSizeEntries();
  // read sample count and duration to set next values
  uint8* ctts_entry = ctts_->GetBytesAtEntry(ctts_table_index_);
  if (!ctts_entry) {
    return false;
  }
  uint32 sample_count = LB::Platform::load_uint32_big_endian(ctts_entry);
  ctts_next_first_sample_ = ctts_first_sample_ + sample_count;
  ctts_sample_offset_ = LB::Platform::load_uint32_big_endian(ctts_entry + 4);
  ctts_table_index_++;
  return true;
}

void ShellMP4Map::stco_Init() {
  DCHECK(stco_);
  // load offset of first chunk into current_chunk_offset_
  if (stco_->GetEntryCount() > 0) {
    uint8* stco_bytes = stco_->GetBytesAtEntry(0);
    current_chunk_offset_ = LB::Platform::load_uint32_big_endian(stco_bytes);
  } else {
    stco_ = NULL;
  }
}

// The stsc table has the following per-entry layout:
// uint32 first chunk number with this sample count
// uint32 samples-per-chunk
// uint32 sample description id (unused)
void ShellMP4Map::stsc_Init() {
  DCHECK(stsc_);
  // set up vector to correct final size
  int cache_segments = (stsc_->GetEntryCount() /
                        stsc_->GetCacheSizeEntries()) + 1;
  stsc_sample_sums_.reserve(cache_segments);
  // there must always be at least 1 entry in a valid stsc table
  if (stsc_->GetEntryCount() > 0) {
    stsc_first_chunk_ = 0;
    stsc_first_chunk_sample_ = 0;
    // first cached entry is always 0
    stsc_sample_sums_.push_back(0);
    uint8* stsc_entry = stsc_->GetBytesAtEntry(0);
    if (!stsc_entry) {
      NOTREACHED();
      // invalidate table
      stsc_ = NULL;
      return;
    }
    // initialize integration step variables
    stsc_samples_per_chunk_ =
        LB::Platform::load_uint32_big_endian(stsc_entry + 4);
    // look up next first chunk at next index in table
    if (stsc_->GetEntryCount() > 1) {
      stsc_entry = stsc_->GetBytesAtEntry(1);
      if (!stsc_entry) {
        NOTREACHED();
        stsc_ = NULL;
        return;
      }
      stsc_next_first_chunk_ =
          LB::Platform::load_uint32_big_endian(stsc_entry) - 1;
      stsc_next_first_chunk_sample_ = stsc_next_first_chunk_ *
                                      stsc_samples_per_chunk_;
    } else {
      // every chunk in the file has the sample sample count, set next first
      // chunk to highest valid chunk number.
      stsc_next_first_chunk_ = UINT32_MAX;
      stsc_next_first_chunk_sample_ = UINT32_MAX;
    }
    stsc_table_index_ = 0;

    // since we known the size of the first chunk we can set next_chunk_sample_
    next_chunk_sample_ = stsc_samples_per_chunk_;

  } else {
    stsc_ = NULL;
  }
}

// To find the chunk number of an abritrary sample we have to sum the
// samples-per-chunk value multiplied by the number of chunks with that sample
// count until the sum exceeds the sample number, then calculate the chunk
// number from that range of per-sample chunk sizes. Since this map is meant
// to be consumed incrementally and with minimal memory consumption we calculate
// this integration step only when needed, and save results for each cached
// piece of the table, to avoid having to recalculate needed data.
bool ShellMP4Map::stsc_AdvanceToSample(uint32 sample_number) {
  DCHECK(stsc_);
  // sample_number could be before first chunk, meaning that we are seeking
  // backwards and have left the current chunk. Find the closest part of the
  // cached table and integrate forward from there.
  if (sample_number < stsc_first_chunk_sample_) {
    if (!stsc_SlipCacheToSample(sample_number, 0)) {
      return false;
    }
  }

  // sample_number could also be well head of our current piece of the
  // cache, so see if we can re-use any previously calculated summations to
  // skip to the nearest cache entry
  int next_cache_index = (stsc_table_index_ /
                          stsc_->GetCacheSizeEntries()) + 1;
  if ((next_cache_index < stsc_sample_sums_.size()) &&
      (sample_number >= stsc_sample_sums_[next_cache_index])) {
    if (!stsc_SlipCacheToSample(sample_number, next_cache_index)) {
      return false;
    }
  }

  // Integrate through each table entry until we find sample_number in range
  while (stsc_next_first_chunk_sample_ <= sample_number) {
    // advance to next chunk sample range
    stsc_first_chunk_sample_ = stsc_next_first_chunk_sample_;
    // our next_first_chunk is now our first chunk
    stsc_first_chunk_ = stsc_next_first_chunk_;
    // advance to next entry in table
    stsc_table_index_++;
    // if we've advanced to a new segment of the cache, update the saved
    // integration values
    if (!(stsc_table_index_ % stsc_->GetCacheSizeEntries())) {
      int cache_index = stsc_table_index_ / stsc_->GetCacheSizeEntries();
      // check that this is our first time with these data
      if (cache_index == stsc_sample_sums_.size()) {
        stsc_sample_sums_.push_back(stsc_first_chunk_sample_);
      }
      // our integration at this point should always match any stored record
      DCHECK_EQ(stsc_first_chunk_sample_, stsc_sample_sums_[cache_index]);
    }
    if (stsc_table_index_ < stsc_->GetEntryCount()) {
      // look up our new sample rate
      uint8* stsc_entry = stsc_->GetBytesAtEntry(stsc_table_index_);
      if (!stsc_entry) {
        return false;
      }
      // second uint32 in table is the samples per chunk
      stsc_samples_per_chunk_ =
          LB::Platform::load_uint32_big_endian(stsc_entry + 4);
      // we need to look up next table entry to determine next first chunk
      if (stsc_table_index_ + 1 < stsc_->GetEntryCount()) {
        // look up next first chunk
        stsc_entry = stsc_->GetBytesAtEntry(stsc_table_index_ + 1);
        if (!stsc_entry) {
          return false;
        }
        stsc_next_first_chunk_ =
            LB::Platform::load_uint32_big_endian(stsc_entry) - 1;
        // carry sum of first_samples forward to next chunk range
        stsc_next_first_chunk_sample_ +=
            (stsc_next_first_chunk_ - stsc_first_chunk_) *
             stsc_samples_per_chunk_;
      } else {
        // this is the normal place to encounter the end of the chunk table.
        // set the next chunk to the highest valid chunk number
        stsc_next_first_chunk_ = UINT32_MAX;
        stsc_next_first_chunk_sample_ = UINT32_MAX;
      }
    } else {
      // We should normally encounter the end of the chunk table on lookup
      // of the next_first_chunk_ within the if clause associated with this
      // else. Something has gone wrong.
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool ShellMP4Map::stsc_SlipCacheToSample(uint32 sample_number,
                                         int starting_cache_index) {
  DCHECK_LT(starting_cache_index, stsc_sample_sums_.size());
  // look through old sample sums for the first entry that exceeds sample
  // sample_number, we want the entry right before that
  int cache_index = starting_cache_index;
  for (; cache_index + 1 < stsc_sample_sums_.size(); cache_index++) {
    if (sample_number < stsc_sample_sums_[cache_index + 1]) {
      break;
    }
  }
  // jump to new spot in table
  stsc_first_chunk_sample_ = stsc_sample_sums_[cache_index];
  stsc_table_index_ = cache_index * stsc_->GetCacheSizeEntries();
  uint8* stsc_entry = stsc_->GetBytesAtEntry(stsc_table_index_);
  if (!stsc_entry) {
    return false;
  }
  // load current and next values
  stsc_first_chunk_ = LB::Platform::load_uint32_big_endian(stsc_entry) - 1;
  stsc_samples_per_chunk_ =
      LB::Platform::load_uint32_big_endian(stsc_entry + 4);
  if (stsc_table_index_ + 1 < stsc_->GetEntryCount()) {
    stsc_entry = stsc_->GetBytesAtEntry(stsc_table_index_ + 1);
    if (!stsc_entry) {
      return false;
    }
    stsc_next_first_chunk_ =
        LB::Platform::load_uint32_big_endian(stsc_entry) - 1;
    stsc_next_first_chunk_sample_ = stsc_first_chunk_sample_ +
        ((stsc_next_first_chunk_ - stsc_first_chunk_) *
          stsc_samples_per_chunk_);
  } else {
    // We seem to have cached an entry 1 entry before end of the table, and
    // are seeking to a region contained in that last entry in the table.
    stsc_next_first_chunk_ = UINT32_MAX;
    stsc_next_first_chunk_sample_ = UINT32_MAX;
  }
  return true;
}

// The stts table has the following per-entry layout:
// uint32 sample count - number of sequential samples with this duration
// uint32 sample duration - duration in ticks of this sample range
void ShellMP4Map::stts_Init() {
  int cache_segments = (stts_->GetEntryCount() /
                        stts_->GetCacheSizeEntries()) + 1;
  stts_samples_.reserve(cache_segments);
  stts_timestamps_.reserve(cache_segments);
  // need at least one entry in valid stts
  if (stts_->GetEntryCount() > 0) {
    // integration starts at 0 for both cache entries
    stts_samples_.push_back(0);
    stts_timestamps_.push_back(0);
    uint8* stts_entry = stts_->GetBytesAtEntry(0);
    if (!stts_entry) {
      stts_ = NULL;
      return;
    }
    stts_first_sample_ = 0;
    stts_first_sample_time_ = 0;
    stts_next_first_sample_ = LB::Platform::load_uint32_big_endian(stts_entry);
    stts_sample_duration_ =
        LB::Platform::load_uint32_big_endian(stts_entry + 4);
    stts_table_index_ = 0;
  } else {
    stts_ = NULL;
  }
}

bool ShellMP4Map::stts_AdvanceToSample(uint32 sample_number) {
  DCHECK(stts_);
  // sample_number could be before our current sample range, in which case
  // we skip to the nearest table entry before sample_number and integrate
  // forward to the sample_number again.
  if (sample_number < stts_first_sample_) {
    if (!stts_SlipCacheToSample(sample_number, 0)) {
      return false;
    }
  }

  // sample number could also be well ahead of this cache segment, if we've
  // previously calculated summations ahead let's skip to the correct one
  int next_cache_index = (stts_table_index_ /
                          stts_->GetCacheSizeEntries()) + 1;
  if ((next_cache_index < stts_samples_.size()) &&
      (sample_number >= stts_samples_[next_cache_index])) {
    if (!stts_SlipCacheToSample(sample_number, next_cache_index)) {
      return false;
    }
  }

  // integrate through the stts until sample_number is within current range
  while (stts_next_first_sample_ <= sample_number) {
    // advance time to next sample range
    uint32 range_size = stts_next_first_sample_ - stts_first_sample_;
    stts_first_sample_time_ += (range_size * stts_sample_duration_);
    // advance sample counter to next range
    stts_first_sample_ = stts_next_first_sample_;
    // bump table counter to next entry
    stts_table_index_++;
    // see if we just crossed a cache boundary and should cache results
    if (!(stts_table_index_ % stts_->GetCacheSizeEntries())) {
      int cache_index = stts_table_index_ / stts_->GetCacheSizeEntries();
      // check that this is our first time with these data
      if (cache_index == stts_samples_.size()) {
        // both tables should always grow together
        DCHECK_EQ(stts_samples_.size(), stts_timestamps_.size());
        stts_samples_.push_back(stts_first_sample_);
        stts_timestamps_.push_back(stts_first_sample_time_);
      }
      // our integration at this point should always match any stored record
      DCHECK_EQ(stts_first_sample_, stts_samples_[cache_index]);
      DCHECK_EQ(stts_first_sample_time_, stts_timestamps_[cache_index]);
    }
    if (stts_table_index_ < stts_->GetEntryCount()) {
      // load next entry data
      uint8* stts_entry = stts_->GetBytesAtEntry(stts_table_index_);
      if (!stts_entry) {
        return false;
      }
      uint32 sample_count = LB::Platform::load_uint32_big_endian(stts_entry);
      // calculate next sample number from range size
      stts_next_first_sample_ = stts_first_sample_ + sample_count;
      // and load duration of this sample range
      stts_sample_duration_ =
          LB::Platform::load_uint32_big_endian(stts_entry + 4);
    } else {
      // We've gone beyond the range defined by the last entry in the stts.
      // this is an error.
      highest_valid_sample_number_ = std::min(highest_valid_sample_number_,
                                              stts_first_sample_ -  1);
      return false;
    }
  }
  return true;
}

// Move our integration steps to a previously saved entry in the cache tables.
// Searches linearly through the vector of old cached values, so can accept a
// starting index to do the search from.
bool ShellMP4Map::stts_SlipCacheToSample(uint32 sample_number,
                                         int starting_cache_index) {
  DCHECK_LT(starting_cache_index, stts_samples_.size());
  int cache_index = starting_cache_index;
  for (; cache_index + 1 < stts_samples_.size(); cache_index++) {
    if (sample_number < stts_samples_[cache_index + 1]) {
      break;
    }
  }
  stts_first_sample_ = stts_samples_[cache_index];
  stts_first_sample_time_ = stts_timestamps_[cache_index];
  stts_table_index_ = cache_index * stts_->GetCacheSizeEntries();
  // read sample count and duration to set next values
  uint8* stts_entry = stts_->GetBytesAtEntry(stts_table_index_);
  if (!stts_entry) {
    return false;
  }
  uint32 sample_count = LB::Platform::load_uint32_big_endian(stts_entry);
  stts_next_first_sample_ = stts_first_sample_ + sample_count;
  stts_sample_duration_ = LB::Platform::load_uint32_big_endian(stts_entry + 4);
  return true;
}

}  // namespace media
