// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RANGES_H_
#define MEDIA_BASE_RANGES_H_

#include <ostream>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/base/media_export.h"

namespace media {

// Ranges allows holding an ordered list of ranges of [start,end) intervals.
// The canonical example use-case is holding the list of ranges of buffered
// bytes or times in a <video> tag.
template<class T>  // Endpoint type; typically a base::TimeDelta or an int64.
class Ranges {
 public:
  // Allow copy & assign.

  // Add (start,end) to this object, coallescing overlaps as appropriate.
  // Returns the number of stored ranges, post coallescing.
  size_t Add(T start, T end);

  // Return the number of disjoint ranges.
  size_t size() const;

  // Return the "i"'th range's start & end (0-based).
  T start(int i) const;
  T end(int i) const;

  // Clear all ranges.
  void clear();

 private:
  // Disjoint, in increasing order of start.
  std::vector<std::pair<T, T> > ranges_;
};

//////////////////////////////////////////////////////////////////////
// EVERYTHING BELOW HERE IS IMPLEMENTATION DETAIL!!
//////////////////////////////////////////////////////////////////////

template<class T>
size_t Ranges<T>::Add(T start, T end) {
  if (start == end)  // Nothing to be done with empty ranges.
    return ranges_.size();

  DCHECK(start < end);
  size_t i;
  // Walk along the array of ranges until |start| is no longer larger than the
  // current interval's end.
  for (i = 0; i < ranges_.size() && ranges_[i].second < start; ++i) {
    // Empty body
  }

  // Now we know |start| belongs in the i'th slot.
  // If i is the end of the range, append new range and done.
  if (i == ranges_.size()) {
    ranges_.push_back(std::make_pair(start, end));
    return ranges_.size();
  }

  // If |end| is less than i->first, then [start,end) is a new (non-overlapping)
  // i'th entry pushing everyone else back, and done.
  if (end < ranges_[i].first) {
    ranges_.insert(ranges_.begin() + i, std::make_pair(start, end));
    return ranges_.size();
  }

  // Easy cases done.  Getting here means there is overlap between [start,end)
  // and the existing ranges.

  // Now: start <= i->second && i->first <= end
  if (start < ranges_[i].first)
    ranges_[i].first = start;
  if (ranges_[i].second < end)
    ranges_[i].second = end;

  // Now: [start,end) is contained in the i'th range, and we'd be done, except
  // for the fact that the newly-extended i'th range might now overlap
  // subsequent ranges.  Merge until discontinuities appear.  Note that there's
  // no need to test/merge previous ranges, since needing that would mean the
  // original loop went too far.
  while ((i + 1) < ranges_.size() &&
         ranges_[i + 1].first <= ranges_[i].second) {
    ranges_[i].second = std::max(ranges_[i].second, ranges_[i + 1].second);
    ranges_.erase(ranges_.begin() + i + 1);
  }

  return ranges_.size();
}

template<class T>
size_t Ranges<T>::size() const {
  return ranges_.size();
}

template<class T>
T Ranges<T>::start(int i) const {
  return ranges_[i].first;
}

template<class T>
T Ranges<T>::end(int i) const {
  return ranges_[i].second;
}

template<class T>
void Ranges<T>::clear() {
  ranges_.clear();
}

}  // namespace media

#endif  // MEDIA_BASE_RANGES_H_
