// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/memory_tracker/tool/internal_fragmentation_tool.h"
// C++ headers.
#include <algorithm>
#include <map>
#include <set>
#include <vector>
// Cobalt headers.
#include "cobalt/browser/memory_tracker/tool/histogram_table_csv_base.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "nb/bit_cast.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

namespace {

using nb::analytics::AllocationRecord;
using nb::analytics::AllocationVisitor;
using nb::analytics::MemoryTracker;

class FragmentationProcessor : private AllocationVisitor {
 public:
  explicit FragmentationProcessor(uintptr_t page_size)
      : page_size_(page_size) {}

  void Process(MemoryTracker* tracker,
               size_t* allocated_memory_in_pages,
               size_t* used_memory_in_segments) {
    memory_segments_.erase(memory_segments_.begin(), memory_segments_.end());
    *allocated_memory_in_pages = 0;
    *used_memory_in_segments = 0;
    tracker->Accept(this);

    // Note that because of the fine course locking used for
    // the memory tracker, it's accepted that there can be
    // an occasional overlapping memory segment, where the memory region
    // was deallocated and then recycled during the segment collection phase.
    // This will then show up as an overlapping region. This is resolved
    // by selecting the first memory segment, and discarding any
    // memory segments that overlap it.
    // While this can skew results, the rate of overlaps in practice is so low
    // that we can essentially ignore it without warning the user.
    std::sort(memory_segments_.begin(), memory_segments_.end());
    std::vector<Segment> copy_no_overlaps;
    copy_no_overlaps.reserve(memory_segments_.size());
    for (const Segment& seg : memory_segments_) {
      if (copy_no_overlaps.empty() ||
          !seg.Intersects(copy_no_overlaps.back())) {
        copy_no_overlaps.push_back(seg);
      }
    }
    memory_segments_.swap(copy_no_overlaps);

    if (!memory_segments_.empty()) {
      std::set<uintptr_t> page_ids;

      std::vector<Segment> sub_segments;
      for (const Segment& seg : memory_segments_) {
        if (!seg.size()) {  // Ignore 0-size segments.
          continue;
        }
        // Use erase() instead of clear(), because it has stronger
        // guarantees about recycling memory in the vector.
        sub_segments.erase(sub_segments.begin(), sub_segments.end());
        // Memory allocation segments may span multiple pages. In this
        // case we will break them up and store them in sub_segments.
        seg.SplitAcrossPageBoundaries(page_size_, &sub_segments);

        for (const Segment& sub_seg : sub_segments) {
          uintptr_t page_id = GetPageId(sub_seg.begin());
          page_ids.insert(page_id);
        }
      }
      *allocated_memory_in_pages = page_ids.size() * page_size_;
    }
    *used_memory_in_segments = CountUsedMemoryInSegments();
  }

 private:
  // Implements AllocationVisitor::Visitor().
  bool Visit(const void* memory,
             const AllocationRecord& alloc_record) override {
    const char* memory_begin = static_cast<const char*>(memory);
    const char* memory_end = memory_begin + alloc_record.size;
    memory_segments_.push_back(Segment(nullptr, memory_begin, memory_end));
    return true;
  }
  uintptr_t GetPageId(const char* ptr) const {
    return nb::bit_cast<uintptr_t>(ptr) / page_size_;
  }
  size_t CountUsedMemoryInSegments() const {
    if (memory_segments_.empty()) {
      return 0;
    }
    size_t total_bytes = 0;
    const Segment* prev_segment = nullptr;
    for (const Segment& seg : memory_segments_) {
      size_t size = seg.size();
      if (prev_segment && prev_segment->Intersects(seg)) {
        // Sanity check - FragmentationProcessor::Process() is expected
        // to resolve overlapping memory segments that occur from the
        // concurrent nature of memory collection.
        SB_NOTREACHED() << "Memory segments intersected!";
        size = 0;
      }
      prev_segment = &seg;
      total_bytes += size;
    }
    return total_bytes;
  }

  std::vector<Segment> memory_segments_;
  const uintptr_t page_size_;
};

}  // namespace.

InternalFragmentationTool::InternalFragmentationTool() {
}

InternalFragmentationTool::~InternalFragmentationTool() {
}

std::string InternalFragmentationTool::tool_name() const  {
  return "InternalFragmentationTool";
}

void InternalFragmentationTool::Run(Params* params) {
  // Run function does almost nothing.
  params->logger()->Output("InternalFragmentationTool running...\n");

  Timer output_timer(base::TimeDelta::FromSeconds(30));
  Timer sample_timer(base::TimeDelta::FromMilliseconds(50));

  MemoryBytesHistogramCSV histogram_table;
  histogram_table.set_title("Internal Fragmentation - Probably undercounts");

  FragmentationProcessor visitor(SB_MEMORY_PAGE_SIZE);

  // If we get a finish signal then this will break out of the loop.
  while (!params->wait_for_finish_signal(250 * kSbTimeMillisecond)) {
    // LOG CSV.
    if (output_timer.UpdateAndIsExpired()) {
      std::stringstream ss;
      ss << kNewLine << histogram_table.ToString() << kNewLine << kNewLine;
      params->logger()->Output(ss.str());
    }

    // ADD A HISTOGRAM SAMPLE.
    if (sample_timer.UpdateAndIsExpired()) {
      histogram_table.BeginRow(params->time_since_start());

      size_t allocated_memory_in_pages = 0;
      size_t used_memory = 0;
      visitor.Process(params->memory_tracker(),
                      &allocated_memory_in_pages,
                      &used_memory);

      SB_DCHECK(allocated_memory_in_pages > used_memory)
          << "allocated_memory_in_pages: " << allocated_memory_in_pages << ","
          << "used_memory: " << used_memory;

      histogram_table.AddRowValue("TotalReservedCpuMemoryInPages(MB)",
                                  allocated_memory_in_pages);
      histogram_table.AddRowValue("TotalCpuMemoryInUse(MB)",
                                  used_memory);
      histogram_table.FinalizeRow();
    }

    // COMPRESS TABLE WHEN FULL.
    //
    // Table is full, therefore eliminate half of the elements.
    // Reduce sample frequency to match.
    if (histogram_table.NumberOfRows() >= 100) {
      // Compression step.
      histogram_table.RemoveOddElements();

      // By doubling the sampling time this keeps the table linear with
      // respect to time. If sampling time was not doubled then there
      // would be time distortion in the graph.
      sample_timer.ScaleTriggerTime(2.0);
      sample_timer.Restart();
    }
  }
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
