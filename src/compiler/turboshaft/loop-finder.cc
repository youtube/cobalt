// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/loop-finder.h"

namespace v8::internal::compiler::turboshaft {

void LoopFinder::Run() {
  ZoneVector<Block*> all_loops(phase_zone_);
  for (Block& block : base::Reversed(input_graph_->blocks())) {
    if (block.IsLoop()) {
      LoopInfo info = VisitLoop(&block);
      loop_header_info_.insert({&block, info});
    }
  }
}

// Update the `parent_loops_` of all of the blocks that are inside of the loop
// that starts on `header`.
LoopFinder::LoopInfo LoopFinder::VisitLoop(Block* header) {
  Block* backedge = header->LastPredecessor();
  DCHECK(backedge->LastOperation(*input_graph_).Is<GotoOp>());
  DCHECK_EQ(backedge->LastOperation(*input_graph_).Cast<GotoOp>().destination,
            header);
  DCHECK_GE(backedge->index().id(), header->index().id());

  LoopInfo info;
  // The header is skipped by the while-loop below, so we initialize {info} with
  // the `op_count` from {header}, and a `block_count` of 1 (= the header).
  info.op_count = OpCountUpperBound(header);
  info.start = header;
  info.end = backedge;
  info.block_count = 1;

  queue_.clear();
  queue_.push_back(backedge);
  while (!queue_.empty()) {
    const Block* curr = queue_.back();
    queue_.pop_back();
    if (curr == header) continue;
    if (loop_headers_[curr->index()] != nullptr) {
      Block* curr_parent = loop_headers_[curr->index()];
      if (curr_parent == header) {
        // If {curr}'s parent is already marked as being {header}, then we've
        // already visited {curr}.
        continue;
      } else {
        // If {curr}'s parent is not {header}, then {curr} is part of an inner
        // loop. We should continue the search on the loop header: the
        // predecessors of {curr} will all be in this inner loop.
        queue_.push_back(curr_parent);
        info.has_inner_loops = true;
        continue;
      }
    }
    info.block_count++;
    info.op_count += OpCountUpperBound(curr);
    loop_headers_[curr->index()] = header;
    Block* pred_start = curr->LastPredecessor();
    if (curr->IsLoop()) {
      // Skipping the backedge of inner loops since we don't want to visit inner
      // loops now (they should already have been visited).
      DCHECK_NOT_NULL(pred_start);
      pred_start = pred_start->NeighboringPredecessor();
      info.has_inner_loops = true;
    }
    for (Block* pred : NeighboringPredecessorIterable(pred_start)) {
      queue_.push_back(pred);
    }
  }

  return info;
}

}  // namespace v8::internal::compiler::turboshaft
