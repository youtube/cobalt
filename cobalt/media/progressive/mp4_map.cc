// Copyright 2012 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/media/progressive/mp4_map.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "cobalt/media/base/endian_util.h"
#include "cobalt/media/progressive/mp4_parser.h"

namespace cobalt {
namespace media {

// ==== TableCache =============================================================

MP4Map::TableCache::TableCache(uint64 table_offset, uint32 entry_count,
                               uint32 entry_size, uint32 cache_size_entries,
                               scoped_refptr<DataSourceReader> reader)
    : entry_size_(entry_size),
      entry_count_(entry_count),
      cache_size_entries_(cache_size_entries),
      table_offset_(table_offset),
      reader_(reader),
      cache_first_entry_number_(-1),
      cache_entry_count_(0) {}

uint8* MP4Map::TableCache::GetBytesAtEntry(uint32 entry_number) {
  // don't fetch the unfetchable
  if (entry_number >= entry_count_) {
    return NULL;
  }
  // this query within valid range for the current cache table?
  if (entry_number < cache_first_entry_number_ ||
      entry_number >= cache_first_entry_number_ + cache_entry_count_) {
    // Calculate first entry in table keeping cache size alignment in table.
    // Always cache one more entry as in stss we need to use the first entry
    // of the next cache slot as an upper bound.
    cache_entry_count_ = cache_size_entries_ + 1;
    cache_first_entry_number_ =
        (entry_number / cache_size_entries_) * cache_size_entries_;
    // see if we have exceeded our table bounds
    if (cache_first_entry_number_ + cache_entry_count_ > entry_count_) {
      cache_entry_count_ = entry_count_ - cache_first_entry_number_;
    }
    // drop old data
    cache_.clear();
    DCHECK_GE(cache_entry_count_, 0);
    int bytes_to_read = cache_entry_count_ * entry_size_;
    cache_.resize(bytes_to_read);
    uint64 file_offset =
        table_offset_ + (cache_first_entry_number_ * entry_size_);
    int bytes_read =
        reader_->BlockingRead(file_offset, bytes_to_read, &cache_[0]);
    if (bytes_read < bytes_to_read) {
      cache_entry_count_ = 0;
      return NULL;
    }
  }
  // cache is assumed to be valid and to contain the entry from here on
  DCHECK_GE(entry_number, cache_first_entry_number_);
  DCHECK_LT(entry_number, cache_first_entry_number_ + cache_entry_count_);

  uint32 cache_offset = entry_number - cache_first_entry_number_;
  return &cache_[0] + (cache_offset * entry_size_);
}

bool MP4Map::TableCache::ReadU32Entry(uint32 entry_number, uint32* entry) {
  if (uint8* data = GetBytesAtEntry(entry_number)) {
    *entry = endian_util::load_uint32_big_endian(data);
    return true;
  }

  return false;
}

bool MP4Map::TableCache::ReadU32PairEntry(uint32 entry_number, uint32* first,
                                          uint32* second) {
  if (uint8* data = GetBytesAtEntry(entry_number)) {
    if (first) *first = endian_util::load_uint32_big_endian(data);
    if (second) *second = endian_util::load_uint32_big_endian(data + 4);
    return true;
  }

  return false;
}

bool MP4Map::TableCache::ReadU32EntryIntoU64(uint32 entry_number,
                                             uint64* entry) {
  if (uint8* data = GetBytesAtEntry(entry_number)) {
    *entry = endian_util::load_uint32_big_endian(data);
    return true;
  }

  return false;
}

bool MP4Map::TableCache::ReadU64Entry(uint32 entry_number, uint64* entry) {
  if (uint8* data = GetBytesAtEntry(entry_number)) {
    *entry = endian_util::load_uint64_big_endian(data);
    return true;
  }

  return false;
}

// ==== MP4Map ============================================================

// atom | name                  | size | description, (*) means optional table
// -----+-----------------------+------+----------------------------------------
// co64 | chunk offset (64-bit) | 8    | per-chunk list of chunk file offsets
// ctts | composition offset    | 8    | (*) run-length sample number to cts
// stco | chunk offset (32-bit) | 4    | per-chunk list of chunk file offsets
// stsc | sample-to-chunk       | 12   | chunk number to samples per chunk
// stss | sync sample           | 4    | (*) list of keyframe sample numbers
// stts | time-to-sample        | 8    | run-length sample number to duration
// stsz | sample size           | 4    | per-sample list of sample sizes

MP4Map::MP4Map(scoped_refptr<DataSourceReader> reader)
    : reader_(reader),
      current_chunk_sample_(0),
      next_chunk_sample_(0),
      current_chunk_offset_(0),
      highest_valid_sample_number_(UINT32_MAX),
      ctts_first_sample_(0),
      ctts_sample_offset_(0),
      ctts_next_first_sample_(0),
      ctts_table_index_(0),
      stsc_first_chunk_(0),
      stsc_first_chunk_sample_(0),
      stsc_samples_per_chunk_(0),
      stsc_next_first_chunk_(0),
      stsc_next_first_chunk_sample_(0),
      stsc_table_index_(0),
      stss_last_keyframe_(0),
      stss_next_keyframe_(0),
      stss_table_index_(0),
      stts_first_sample_(0),
      stts_first_sample_time_(0),
      stts_sample_duration_(0),
      stts_next_first_sample_(0),
      stts_next_first_sample_time_(0),
      stts_table_index_(0),
      stsz_default_size_(0) {}

bool MP4Map::IsComplete() {
  // all required table pointers must be valid for map to function
  return (co64_ || stco_) && stsc_ && stts_ && (stsz_ || stsz_default_size_);
}

// The sample size is a lookup in the stsz table, which is indexed per sample
// number.
bool MP4Map::GetSize(uint32 sample_number, uint32* size_out) {
  DCHECK(size_out);
  DCHECK(stsz_ || stsz_default_size_);

  if (sample_number > highest_valid_sample_number_) {
    return false;
  }

  if (stsz_default_size_) {
    *size_out = stsz_default_size_;
    return true;
  }

  return stsz_->ReadU32Entry(sample_number, size_out);
}

// We first must integrate the stsc table to find the chunk number that the
// sample resides in, and the first sample number in that chunk. We look up that
// chunk offset from the stco or co64, which are indexed by chunk number. We
// then use the stsz to sum samples to the byte offset with that chunk. The sum
// of the chunk offset and the byte offset within the chunk is the offset of
// the sample.
bool MP4Map::GetOffset(uint32 sample_number, uint64* offset_out) {
  DCHECK(offset_out);
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
    uint32 chunk_range_offset = sample_offset / stsc_samples_per_chunk_;
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
      if (!co64_->ReadU64Entry(chunk_number, &current_chunk_offset_))
        return false;
    } else if (!stco_->ReadU32EntryIntoU64(chunk_number,
                                           &current_chunk_offset_)) {
      return false;
    }
  }

  // at this point we should have sample_number within the range of our chunk
  // offset summation saved state
  DCHECK_LE(current_chunk_sample_, sample_number);
  DCHECK_LT(sample_number, next_chunk_sample_);

  if (stsz_default_size_ > 0) {
    current_chunk_offset_ +=
        (sample_number - current_chunk_sample_) * stsz_default_size_;
    current_chunk_sample_ = sample_number;
  } else {
    // sum sample sizes within chunk to get to byte offset of sample
    while (current_chunk_sample_ < sample_number) {
      uint32 sample_size = 0;
      if (!GetSize(current_chunk_sample_, &sample_size)) {
        return false;
      }
      current_chunk_offset_ += sample_size;
      current_chunk_sample_++;
    }
  }

  *offset_out = current_chunk_offset_;
  return true;
}

// Given a current sample number we integrate through the stts to find the
// duration of the current sample, and at the same time integrate through the
// durations to find the dts of that sample number. We then integrate sample
// numbers through the ctts to find the composition time offset, which we add to
// the dts to return the pts.
bool MP4Map::GetTimestamp(uint32 sample_number, uint64* timestamp_out) {
  DCHECK(timestamp_out);
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
  *timestamp_out = dts + ctts_sample_offset_;
  return true;
}

// Sum through the stts to find the duration of the given sample_number.
bool MP4Map::GetDuration(uint32 sample_number, uint32* duration_out) {
  DCHECK(duration_out);
  if (sample_number > highest_valid_sample_number_) {
    return false;
  }

  if (!stts_AdvanceToSample(sample_number)) {
    return false;
  }
  DCHECK_LT(sample_number, stts_next_first_sample_);
  DCHECK_GE(sample_number, stts_first_sample_);
  *duration_out = stts_sample_duration_;
  return true;
}

bool MP4Map::GetIsKeyframe(uint32 sample_number, bool* is_keyframe_out) {
  DCHECK(is_keyframe_out);
  if (sample_number > highest_valid_sample_number_) {
    return false;
  }

  // no stts means every frame is a keyframe
  if (!stss_) {
    *is_keyframe_out = true;
    return true;
  }

  // check for keyframe match on either range value
  if (sample_number == stss_next_keyframe_) {
    *is_keyframe_out = true;
    return stss_AdvanceStep();
  } else if (sample_number == stss_last_keyframe_) {
    *is_keyframe_out = true;
    return true;
  }

  // this could be for a much earlier sample number, check if we are within
  // current range of sample numbers
  if (sample_number < stss_last_keyframe_ ||
      sample_number > stss_next_keyframe_) {
    // search for containing entry
    if (!stss_FindNearestKeyframe(sample_number)) {
      return false;
    }
  }
  // sample number must be in range of keyframe states
  DCHECK_GE(sample_number, stss_last_keyframe_);
  DCHECK_LT(sample_number, stss_next_keyframe_);
  // stss_FindNearestKeyframe returns exact matches to
  // sample_number in the stss_last_keyframe_ variable, so
  // we check that for equality
  *is_keyframe_out = (sample_number == stss_last_keyframe_);

  return true;
}

bool MP4Map::IsEOS(uint32 sample_number) {
  return (sample_number > highest_valid_sample_number_);
}

// First look up the sample number for the provided timestamp by integrating
// timestamps through the stts. Then do a binary search on the stss to find the
// keyframe nearest that sample number.
bool MP4Map::GetKeyframe(uint64 timestamp, uint32* sample_out) {
  DCHECK(sample_out);
  // Advance stts to the provided timestamp range
  if (!stts_AdvanceToTime(timestamp)) {
    return false;
  }
  // ensure we got the correct sample duration range
  DCHECK_LT(timestamp, stts_next_first_sample_time_);
  DCHECK_GE(timestamp, stts_first_sample_time_);
  // calculate sample number containing this timestamp
  uint64 time_offset_within_range = timestamp - stts_first_sample_time_;
  uint32 sample_number =
      stts_first_sample_ + (time_offset_within_range / stts_sample_duration_);

  // TODO: ctts?

  // binary search on stts to find nearest keyframe beneath this sample number
  if (stss_) {
    if (!stss_FindNearestKeyframe(sample_number)) {
      return false;
    }
    *sample_out = stss_last_keyframe_;
  } else {
    // an absent stts means every frame is a key frame, we can provide sample
    // directly.
    *sample_out = sample_number;
  }
  return true;
}

// Set up map state and load first part of table, or entire table if it is small
// enough, for each of the supported atoms.
bool MP4Map::SetAtom(uint32 four_cc, uint64 offset, uint64 size,
                     uint32 cache_size_entries, const uint8* atom) {
  // All map atoms are variable-length tables starting with 4 bytes of
  // version/flag info followed by a uint32 indicating the number of items in
  // table. The stsz atom bucks tradition by putting an optional default value
  // at index 4.
  uint32 count = 0;
  uint64 table_offset = offset + 8;
  if (four_cc == kAtomType_stsz) {
    if (size < 12) {
      return false;
    }
    stsz_default_size_ = endian_util::load_uint32_big_endian(atom + 4);
    count = endian_util::load_uint32_big_endian(atom + 8);
    highest_valid_sample_number_ =
        std::min(count - 1, highest_valid_sample_number_);
    // if a non-zero default size is provided don't bother loading the table
    if (stsz_default_size_) {
      stsz_ = NULL;
      return true;
    }

    table_offset += 4;
  } else {
    if (size < 8) {
      return false;
    }
    count = endian_util::load_uint32_big_endian(atom + 4);
  }

  // if cache_size_entries is 0 we are to cache the entire table
  if (cache_size_entries == 0) {
    cache_size_entries = count;
  }

  bool atom_init = false;
  // initialize the appropriate table cache dependent on table type
  switch (four_cc) {
    case kAtomType_co64:
      co64_ = new TableCache(table_offset, count, kEntrySize_co64,
                             cache_size_entries, reader_);
      if (co64_) atom_init = co64_Init();
      break;

    case kAtomType_ctts:
      ctts_ = new TableCache(table_offset, count, kEntrySize_ctts,
                             cache_size_entries, reader_);
      if (ctts_) atom_init = ctts_Init();
      break;

    case kAtomType_stco:
      stco_ = new TableCache(table_offset, count, kEntrySize_stco,
                             cache_size_entries, reader_);
      if (stco_) atom_init = stco_Init();
      break;

    case kAtomType_stsc:
      stsc_ = new TableCache(table_offset, count, kEntrySize_stsc,
                             cache_size_entries, reader_);
      if (stsc_) atom_init = stsc_Init();
      break;

    case kAtomType_stss:
      stss_ = new TableCache(table_offset, count, kEntrySize_stss,
                             cache_size_entries, reader_);
      if (stss_) atom_init = stss_Init();
      break;

    case kAtomType_stts:
      stts_ = new TableCache(table_offset, count, kEntrySize_stts,
                             cache_size_entries, reader_);
      if (stts_) atom_init = stts_Init();
      break;

    case kAtomType_stsz:
      stsz_ = new TableCache(table_offset, count, kEntrySize_stsz,
                             cache_size_entries, reader_);
      if (stsz_) atom_init = stsz_Init();
      break;

    default:
      NOTREACHED() << "unknown atom type provided to mp4 map";
      break;
  }

  return atom_init;
}

bool MP4Map::co64_Init() {
  DCHECK(co64_);
  // load offset of first chunk into current_chunk_offset_
  if (co64_->GetEntryCount() > 0) {
    // can drop any stco table already allocated
    stco_ = NULL;
    // load initial value of current_chunk_offset_ for 0th chunk
    return co64_->ReadU64Entry(0, &current_chunk_offset_);
  }

  co64_ = NULL;

  return true;
}

// The ctts table has the following per-entry layout:
// uint32 sample count
// uint32 composition offset in ticks
//
bool MP4Map::ctts_Init() {
  DCHECK(ctts_);
  // get cache segment vector to reserve table entries in advance
  int cache_segments =
      (ctts_->GetEntryCount() / ctts_->GetCacheSizeEntries()) + 1;
  ctts_samples_.reserve(cache_segments);
  if (ctts_->GetEntryCount() > 0) {
    // save the start of the first table integration at 0
    ctts_samples_.push_back(0);
    ctts_table_index_ = 0;
    ctts_first_sample_ = 0;
    // load first entry in table, to start integration
    return ctts_->ReadU32PairEntry(0, &ctts_next_first_sample_,
                                   &ctts_sample_offset_);
  }
  // drop empty ctts_ table
  ctts_ = NULL;

  return true;
}

// To find the composition offset of a given sample number we must integrate
// through the ctts to find the range of samples containing sample_number. Note
// that the ctts is an optional table.
bool MP4Map::ctts_AdvanceToSample(uint32 sample_number) {
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
  int next_cache_index = (ctts_table_index_ / ctts_->GetCacheSizeEntries()) + 1;
  if ((next_cache_index < ctts_samples_.size()) &&
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
      uint32 sample_count;
      if (!ctts_->ReadU32PairEntry(ctts_table_index_, &sample_count,
                                   &ctts_sample_offset_))
        return false;
      ctts_next_first_sample_ = ctts_first_sample_ + sample_count;
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

bool MP4Map::ctts_SlipCacheToSample(uint32 sample_number,
                                    int starting_cache_index) {
  DCHECK_LT(starting_cache_index, ctts_samples_.size());
  int cache_index = starting_cache_index;
  for (; cache_index + 1 < ctts_samples_.size(); cache_index++) {
    if (sample_number < ctts_samples_[cache_index + 1]) {
      break;
    }
  }
  ctts_first_sample_ = ctts_samples_[cache_index];
  ctts_table_index_ = cache_index * ctts_->GetCacheSizeEntries();
  // read sample count and duration to set next values
  uint32 sample_count;
  if (!ctts_->ReadU32PairEntry(ctts_table_index_, &sample_count,
                               &ctts_sample_offset_))
    return false;
  ctts_next_first_sample_ = ctts_first_sample_ + sample_count;
  return true;
}

bool MP4Map::stco_Init() {
  DCHECK(stco_);
  // load offset of first chunk into current_chunk_offset_
  if (stco_->GetEntryCount() > 0) {
    co64_ = NULL;
    return stco_->ReadU32EntryIntoU64(0, &current_chunk_offset_);
  }

  stco_ = NULL;

  return true;
}

// The stsc table has the following per-entry layout:
// uint32 first chunk number with this sample count
// uint32 samples-per-chunk
// uint32 sample description id (unused)
bool MP4Map::stsc_Init() {
  DCHECK(stsc_);
  // set up vector to correct final size
  int cache_segments =
      (stsc_->GetEntryCount() / stsc_->GetCacheSizeEntries()) + 1;
  stsc_sample_sums_.reserve(cache_segments);
  // there must always be at least 1 entry in a valid stsc table
  if (stsc_->GetEntryCount() > 0) {
    stsc_first_chunk_ = 0;
    stsc_first_chunk_sample_ = 0;
    // first cached entry is always 0
    stsc_sample_sums_.push_back(0);
    if (!stsc_->ReadU32PairEntry(0, NULL, &stsc_samples_per_chunk_)) {
      stsc_ = NULL;
      return false;
    }
    // look up next first chunk at next index in table
    if (stsc_->GetEntryCount() > 1) {
      if (!stsc_->ReadU32PairEntry(1, &stsc_next_first_chunk_, NULL)) {
        stsc_ = NULL;
        return false;
      }
      --stsc_next_first_chunk_;
      stsc_next_first_chunk_sample_ =
          stsc_next_first_chunk_ * stsc_samples_per_chunk_;
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

  return true;
}

// To find the chunk number of an arbitrary sample we have to sum the
// samples-per-chunk value multiplied by the number of chunks with that sample
// count until the sum exceeds the sample number, then calculate the chunk
// number from that range of per-sample chunk sizes. Since this map is meant
// to be consumed incrementally and with minimal memory consumption we calculate
// this integration step only when needed, and save results for each cached
// piece of the table, to avoid having to recalculate needed data.
bool MP4Map::stsc_AdvanceToSample(uint32 sample_number) {
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
  int next_cache_index = (stsc_table_index_ / stsc_->GetCacheSizeEntries()) + 1;
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
      if (!stsc_->ReadU32PairEntry(stsc_table_index_, NULL,
                                   &stsc_samples_per_chunk_)) {
        return false;
      }
      // we need to look up next table entry to determine next first chunk
      if (stsc_table_index_ + 1 < stsc_->GetEntryCount()) {
        // look up next first chunk
        if (!stsc_->ReadU32PairEntry(stsc_table_index_ + 1,
                                     &stsc_next_first_chunk_, NULL)) {
          return false;
        }
        --stsc_next_first_chunk_;
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

bool MP4Map::stsc_SlipCacheToSample(uint32 sample_number,
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
  if (!stsc_->ReadU32PairEntry(stsc_table_index_, &stsc_first_chunk_,
                               &stsc_samples_per_chunk_)) {
    return false;
  }
  // load current and next values
  --stsc_first_chunk_;
  if (stsc_table_index_ + 1 < stsc_->GetEntryCount()) {
    if (!stsc_->ReadU32PairEntry(stsc_table_index_ + 1, &stsc_next_first_chunk_,
                                 NULL)) {
      return false;
    }
    --stsc_next_first_chunk_;
    stsc_next_first_chunk_sample_ =
        stsc_first_chunk_sample_ +
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

// stss is a list of sample numbers that are keyframes.
bool MP4Map::stss_Init() {
  int cache_segments =
      (stss_->GetEntryCount() / stss_->GetCacheSizeEntries()) + 1;
  stss_keyframes_.reserve(cache_segments);
  // empty stss means every frame is a keyframe, same as not
  // providing one
  if (stss_->GetEntryCount() > 0) {
    // identify first keyframe from first entry in stss
    if (!stss_->ReadU32Entry(0, &stss_last_keyframe_)) {
      stss_ = NULL;
      return false;
    }
    --stss_last_keyframe_;
    stss_keyframes_.push_back(stss_last_keyframe_);
    stss_next_keyframe_ = stss_last_keyframe_;
    stss_table_index_ = 0;
  } else {
    stss_ = NULL;
  }

  return true;
}

// advance by one table entry through stss, updating cache if necessary
bool MP4Map::stss_AdvanceStep() {
  DCHECK(stss_);
  stss_last_keyframe_ = stss_next_keyframe_;
  stss_table_index_++;
  if (stss_table_index_ < stss_->GetEntryCount()) {
    if (!stss_->ReadU32Entry(stss_table_index_, &stss_next_keyframe_)) {
      return false;
    }
    --stss_next_keyframe_;
    if (!(stss_table_index_ % stss_->GetCacheSizeEntries())) {
      int cache_index = stss_table_index_ / stss_->GetCacheSizeEntries();
      // only add if this is the first time we've encountered this number
      if (cache_index == stss_keyframes_.size()) {
        stss_keyframes_.push_back(stss_next_keyframe_);
      }
      DCHECK_EQ(stss_next_keyframe_, stss_keyframes_[cache_index]);
    }
  } else {
    stss_next_keyframe_ = UINT32_MAX;
  }
  return true;
}

bool MP4Map::stss_FindNearestKeyframe(uint32 sample_number) {
  DCHECK(stss_);
  // it is assumed that there's at least one cache entry created by
  // stss_Init();
  DCHECK_GT(stss_keyframes_.size(), 0);
  int cache_entry_number = stss_keyframes_.size() - 1;
  int total_cache_entries =
      (stss_->GetEntryCount() + stss_->GetCacheSizeEntries() - 1) /
      stss_->GetCacheSizeEntries();
  // if there's more than one cache entry we can search the cached
  // entries for the entry containing our keyframe, otherwise we skip
  // directly to the binary search of the single cached entry
  if (total_cache_entries > 1) {
    // if the sample number resides within the range of cached entries
    // we search those to find right table cache entry to load
    if (sample_number < stss_keyframes_[cache_entry_number]) {
      int lower_bound = 0;
      int upper_bound = stss_keyframes_.size();
      // binary search to find range
      while (lower_bound <= upper_bound) {
        cache_entry_number = lower_bound + ((upper_bound - lower_bound) / 2);
        if (sample_number < stss_keyframes_[cache_entry_number]) {
          upper_bound = cache_entry_number - 1;
        } else {  // sample_number >= stss_keyframes_[cache_entry_number]
          // if we are at end of list or next cache entry is higher than sample
          // number we consider it a match
          if (cache_entry_number == stss_keyframes_.size() - 1 ||
              sample_number < stss_keyframes_[cache_entry_number + 1]) {
            break;
          }
          lower_bound = cache_entry_number + 1;
        }
      }
    }
    // We've gotten as close as we can using the cached values and must handle
    // two cases. (a) is that we know that sample_number is contained in the
    // cache_entry_number, because we know that:
    // stts_keyframes_[cache_entry_number] <= sample_number <
    //                              stts_keyframes_[cache_entry_number + 1]
    // (b) is that we only know:
    // stts_keyframes_[stts_keyframes_.size() - 1] <= sample_number
    // because we have not cached an upper bound to sample_number.
    // First step is to make (b) in to (a) by advancing through cache entries
    // until last table entry in cache > sample_number or until we arrive
    // at the cache entry in the table.
    while ((cache_entry_number == stss_keyframes_.size() - 1) &&
           cache_entry_number < total_cache_entries - 1) {
      // Use the first key frame in next cache as upper bound.
      int next_cached_entry_number =
          (cache_entry_number + 1) * stss_->GetCacheSizeEntries();
      uint32 next_cached_keyframe;
      if (!stss_->ReadU32Entry(next_cached_entry_number,
                               &next_cached_keyframe)) {
        return false;
      }
      --next_cached_keyframe;
      // if this keyframe is higher than our sample number we're in the right
      // table, stop
      if (sample_number < next_cached_keyframe) {
        break;
      }
      // ok, we need to look in to the next cache entry, advance
      cache_entry_number++;
      int first_table_entry_number =
          cache_entry_number * stss_->GetCacheSizeEntries();
      uint32 first_keyframe_in_cache_entry;
      if (!stss_->ReadU32Entry(first_table_entry_number,
                               &first_keyframe_in_cache_entry)) {
        return false;
      }
      --first_keyframe_in_cache_entry;
      // save first entry in keyframe cache
      stss_keyframes_.push_back(first_keyframe_in_cache_entry);
    }
    // make sure we have an upper bound
    if (cache_entry_number != total_cache_entries - 1 &&
        cache_entry_number == stss_keyframes_.size() - 1) {
      int next_cached_entry_number =
          ((cache_entry_number + 1) * stss_->GetCacheSizeEntries());
      uint32 next_cached_keyframe;
      if (!stss_->ReadU32Entry(next_cached_entry_number,
                               &next_cached_keyframe)) {
        return false;
      }
      --next_cached_keyframe;
      stss_keyframes_.push_back(next_cached_keyframe);
    }
    // ok, now we assume we are in state (a), and that we're either
    // at the end of the table or within the cache entry bounds for our
    // sample number
    DCHECK(stss_keyframes_[cache_entry_number] <= sample_number &&
           (cache_entry_number == total_cache_entries - 1 ||
            sample_number < stss_keyframes_[cache_entry_number + 1]));
  }
  // binary search within stss cache entry for keyframes bounding sample_number
  int lower_bound = cache_entry_number * stss_->GetCacheSizeEntries();
  int upper_bound = std::min(lower_bound + stss_->GetCacheSizeEntries(),
                             stss_->GetEntryCount());

  while (lower_bound <= upper_bound) {
    stss_table_index_ = lower_bound + ((upper_bound - lower_bound) / 2);
    if (!stss_->ReadU32Entry(stss_table_index_, &stss_last_keyframe_)) {
      return false;
    }
    --stss_last_keyframe_;
    if (sample_number < stss_last_keyframe_) {
      upper_bound = stss_table_index_ - 1;
    } else {  // sample_number >= last_keyframe
      lower_bound = stss_table_index_ + 1;
      // if this is the last entry in the table, we can stop here.
      if (lower_bound == stss_->GetEntryCount()) {
        stss_next_keyframe_ = UINT32_MAX;
        break;
      }
      // load next entry in table, see if we actually found the upper bound
      if (!stss_->ReadU32Entry(lower_bound, &stss_next_keyframe_)) {
        return false;
      }
      --stss_next_keyframe_;
      if (sample_number < stss_next_keyframe_) {
        stss_table_index_ = lower_bound;
        break;
      }
    }
  }
  return sample_number >= stss_last_keyframe_ &&
         sample_number < stss_next_keyframe_;
}

// The stts table has the following per-entry layout:
// uint32 sample count - number of sequential samples with this duration
// uint32 sample duration - duration in ticks of this sample range
bool MP4Map::stts_Init() {
  int cache_segments =
      (stts_->GetEntryCount() / stts_->GetCacheSizeEntries()) + 1;
  stts_samples_.reserve(cache_segments);
  stts_timestamps_.reserve(cache_segments);
  // need at least one entry in valid stts
  if (stts_->GetEntryCount() > 0) {
    // integration starts at 0 for both cache entries
    stts_samples_.push_back(0);
    stts_timestamps_.push_back(0);
    if (!stts_->ReadU32PairEntry(0, &stts_next_first_sample_,
                                 &stts_sample_duration_)) {
      stts_ = NULL;
      return false;
    }
    stts_first_sample_ = 0;
    stts_first_sample_time_ = 0;
    stts_next_first_sample_time_ =
        stts_next_first_sample_ * stts_sample_duration_;
    stts_table_index_ = 0;
  } else {
    stts_ = NULL;
  }

  return true;
}

bool MP4Map::stts_AdvanceToSample(uint32 sample_number) {
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
  int next_cache_index = (stts_table_index_ / stts_->GetCacheSizeEntries()) + 1;
  if ((next_cache_index < stts_samples_.size()) &&
      (sample_number >= stts_samples_[next_cache_index])) {
    if (!stts_SlipCacheToSample(sample_number, next_cache_index)) {
      return false;
    }
  }

  // integrate through the stts until sample_number is within current range
  while (stts_next_first_sample_ <= sample_number) {
    if (!stts_IntegrateStep()) {
      return false;
    }
  }
  return true;
}

// Move our integration steps to a previously saved entry in the cache tables.
// Searches linearly through the vector of old cached values, so can accept a
// starting index to do the search from.
bool MP4Map::stts_SlipCacheToSample(uint32 sample_number,
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
  uint32 sample_count;
  // read sample count and duration to set next values
  if (!stts_->ReadU32PairEntry(stts_table_index_, &sample_count,
                               &stts_sample_duration_)) {
    return false;
  }
  stts_next_first_sample_ = stts_first_sample_ + sample_count;
  stts_next_first_sample_time_ =
      stts_first_sample_time_ + (sample_count * stts_sample_duration_);
  return true;
}

bool MP4Map::stts_AdvanceToTime(uint64 timestamp) {
  DCHECK(stts_);

  if (timestamp < stts_first_sample_time_) {
    if (!stts_SlipCacheToTime(timestamp, 0)) {
      return false;
    }
  }

  // sample number could also be well ahead of this cache segment, if we've
  // previously calculated summations ahead let's skip to the correct one
  int next_cache_index = (stts_table_index_ / stts_->GetCacheSizeEntries()) + 1;
  if ((next_cache_index < stts_timestamps_.size()) &&
      (timestamp >= stts_timestamps_[next_cache_index])) {
    if (!stts_SlipCacheToTime(timestamp, next_cache_index)) {
      return false;
    }
  }

  // integrate through the stts until sample_number is within current range
  while (stts_next_first_sample_time_ <= timestamp) {
    if (!stts_IntegrateStep()) {
      return false;
    }
  }
  return true;
}

bool MP4Map::stts_IntegrateStep() {
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
    uint32 sample_count;
    if (!stts_->ReadU32PairEntry(stts_table_index_, &sample_count,
                                 &stts_sample_duration_)) {
      return false;
    }
    // calculate next sample number from range size
    stts_next_first_sample_ = stts_first_sample_ + sample_count;
    // and load duration of this sample range
    stts_next_first_sample_time_ =
        stts_first_sample_time_ + (sample_count * stts_sample_duration_);
  } else {
    // We've gone beyond the range defined by the last entry in the stts.
    // this is an error.
    highest_valid_sample_number_ =
        std::min(highest_valid_sample_number_, stts_first_sample_ - 1);
    return false;
  }
  return true;
}

bool MP4Map::stts_SlipCacheToTime(uint64 timestamp, int starting_cache_index) {
  DCHECK_LT(starting_cache_index, stts_timestamps_.size());
  int cache_index = starting_cache_index;
  for (; cache_index + 1 < stts_timestamps_.size(); cache_index++) {
    if (timestamp < stts_timestamps_[cache_index + 1]) {
      break;
    }
  }
  stts_first_sample_ = stts_samples_[cache_index];
  stts_first_sample_time_ = stts_timestamps_[cache_index];
  stts_table_index_ = cache_index * stts_->GetCacheSizeEntries();
  // read sample count and duration to set next values
  uint32 sample_count;
  if (!stts_->ReadU32PairEntry(stts_table_index_, &sample_count,
                               &stts_sample_duration_)) {
    return false;
  }
  stts_next_first_sample_ = stts_first_sample_ + sample_count;
  stts_next_first_sample_time_ =
      stts_first_sample_time_ + (sample_count * stts_sample_duration_);
  return true;
}

bool MP4Map::stsz_Init() { return stsz_->GetBytesAtEntry(0) != NULL; }

}  // namespace media
}  // namespace cobalt
