// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/instruction-selection-phase.h"

#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/sidetable.h"
#include "src/diagnostics/code-tracer.h"

namespace v8::internal::compiler::turboshaft {

namespace {

void TraceSequence(OptimizedCompilationInfo* info,
                   InstructionSequence* sequence, JSHeapBroker* broker,
                   CodeTracer* code_tracer, const char* phase_name) {
  if (info->trace_turbo_json()) {
    UnparkedScopeIfNeeded scope(broker);
    AllowHandleDereference allow_deref;
    TurboJsonFile json_of(info, std::ios_base::app);
    json_of << "{\"name\":\"" << phase_name << "\",\"type\":\"sequence\""
            << ",\"blocks\":" << InstructionSequenceAsJSON{sequence}
            << ",\"register_allocation\":{"
            << "\"fixed_double_live_ranges\": {}"
            << ",\"fixed_live_ranges\": {}"
            << ",\"live_ranges\": {}"
            << "}},\n";
  }
  if (info->trace_turbo_graph()) {
    UnparkedScopeIfNeeded scope(broker);
    AllowHandleDereference allow_deref;
    CodeTracer::StreamScope tracing_scope(code_tracer);
    tracing_scope.stream() << "----- Instruction sequence " << phase_name
                           << " -----\n"
                           << *sequence;
  }
}

}  // namespace

// Compute the special reverse-post-order block ordering, which is essentially
// a RPO of the graph where loop bodies are contiguous. Properties:
// 1. If block A is a predecessor of B, then A appears before B in the order,
//    unless B is a loop header and A is in the loop headed at B
//    (i.e. A -> B is a backedge).
// => If block A dominates block B, then A appears before B in the order.
// => If block A is a loop header, A appears before all blocks in the loop
//    headed at A.
// 2. All loops are contiguous in the order (i.e. no intervening blocks that
//    do not belong to the loop.)
// Note a simple RPO traversal satisfies (1) but not (2).
// TODO(nicohartmann@): Investigate faster and simpler alternatives.
class TurboshaftSpecialRPONumberer {
 public:
  // Numbering for BasicBlock::rpo_number for this block traversal:
  static const int kBlockOnStack = -2;
  static const int kBlockVisited1 = -3;
  static const int kBlockVisited2 = -4;
  static const int kBlockUnvisited = -1;

  using Backedge = std::pair<const Block*, size_t>;

  struct SpecialRPOStackFrame {
    const Block* block = nullptr;
    size_t index = 0;
    base::SmallVector<Block*, 4> successors;

    SpecialRPOStackFrame(const Block* block, size_t index,
                         base::SmallVector<Block*, 4> successors)
        : block(block), index(index), successors(std::move(successors)) {}
  };

  struct LoopInfo {
    const Block* header;
    base::SmallVector<Block const*, 2> outgoing;
    BitVector* members;
    LoopInfo* prev;
    const Block* end;
    const Block* start;

    void AddOutgoing(Zone* zone, const Block* block) {
      outgoing.push_back(block);
    }
  };

  struct BlockData {
    static constexpr size_t kNoLoopNumber = std::numeric_limits<size_t>::max();
    int32_t rpo_number = kBlockUnvisited;
    size_t loop_number = kNoLoopNumber;
    const Block* rpo_next = nullptr;
  };

  TurboshaftSpecialRPONumberer(const Graph& graph, Zone* zone)
      : graph_(&graph), block_data_(graph.block_count(), zone), zone_(zone) {}

  ZoneVector<uint32_t> ComputeSpecialRPO() {
    std::stack<SpecialRPOStackFrame> stack;
    std::vector<Backedge> backedges;
    size_t num_loops = 0;

    auto Push = [&](const Block* block) {
      auto succs = SuccessorBlocks(*block, *graph_);
      stack.emplace(block, 0, succs);
      set_rpo_number(block, kBlockOnStack);
    };

    const Block* entry = &graph_->StartBlock();

    // Find correct insertion point within existing order.
    const Block* order = nullptr;

    Push(&graph_->StartBlock());

    while (!stack.empty()) {
      SpecialRPOStackFrame& frame = stack.top();

      if (frame.index < frame.successors.size()) {
        // Process the next successor.
        const Block* succ = frame.successors[frame.index++];
        if (rpo_number(succ) == kBlockVisited1) continue;
        if (rpo_number(succ) == kBlockOnStack) {
          // The successor is on the stack, so this is a backedge (cycle).
          DCHECK_EQ(frame.index - 1, 0);
          backedges.emplace_back(frame.block, frame.index - 1);
          // Assign a new loop number to the header.
          DCHECK(!has_loop_number(succ));
          set_loop_number(succ, num_loops++);
        } else {
          // Push the successor onto the stack.
          DCHECK_EQ(rpo_number(succ), kBlockUnvisited);
          Push(succ);
        }
      } else {
        // Finished with all successors; pop the stack and add the block.
        order = PushFront(order, frame.block);
        set_rpo_number(frame.block, kBlockVisited1);
        stack.pop();
      }
    }

    // If no loops were encountered, then the order we computed was correct.
    if (num_loops == 0) return ComputeBlockPermutation(entry);

    // Otherwise, compute the loop information from the backedges in order
    // to perform a traversal that groups loop bodies together.
    ComputeLoopInfo(num_loops, backedges);

    // Initialize the "loop stack". We assume that the entry cannot be a loop
    // header.
    CHECK(!has_loop_number(entry));
    LoopInfo* loop = nullptr;
    order = nullptr;

    // Perform an iterative post-order traversal, visiting loop bodies before
    // edges that lead out of loops. Visits each block once, but linking loop
    // sections together is linear in the loop size, so overall is
    // O(|B| + max(loop_depth) * max(|loop|))
    DCHECK(stack.empty());
    Push(&graph_->StartBlock());
    while (!stack.empty()) {
      SpecialRPOStackFrame& frame = stack.top();
      const Block* block = frame.block;
      const Block* succ = nullptr;

      if (frame.index < frame.successors.size()) {
        // Process the next normal successor.
        succ = frame.successors[frame.index++];
      } else if (has_loop_number(block)) {
        // Process additional outgoing edges from the loop header.
        if (rpo_number(block) == kBlockOnStack) {
          // Finish the loop body the first time the header is left on the
          // stack.
          DCHECK_NOT_NULL(loop);
          DCHECK_EQ(loop->header, block);
          loop->start = PushFront(order, block);
          order = loop->end;
          set_rpo_number(block, kBlockVisited2);
          // Pop the loop stack and continue visiting outgoing edges within
          // the context of the outer loop, if any.
          loop = loop->prev;
          // We leave the loop header on the stack; the rest of this iteration
          // and later iterations will go through its outgoing edges list.
        }

        // Use the next outgoing edge if there are any.
        size_t outgoing_index = frame.index - frame.successors.size();
        LoopInfo* info = &loops_[loop_number(block)];
        DCHECK_NE(loop, info);
        if (block != entry && outgoing_index < info->outgoing.size()) {
          succ = info->outgoing[outgoing_index];
          ++frame.index;
        }
      }

      if (succ != nullptr) {
        // Process the next successor.
        if (rpo_number(succ) == kBlockOnStack) continue;
        if (rpo_number(succ) == kBlockVisited2) continue;
        DCHECK_EQ(kBlockVisited1, rpo_number(succ));
        if (loop != nullptr && !loop->members->Contains(succ->index().id())) {
          // The successor is not in the current loop or any nested loop.
          // Add it to the outgoing edges of this loop and visit it later.
          loop->AddOutgoing(zone_, succ);
        } else {
          // Push the successor onto the stack.
          Push(succ);
          if (has_loop_number(succ)) {
            // Push the inner loop onto the loop stack.
            DCHECK_LT(loop_number(succ), num_loops);
            LoopInfo* next = &loops_[loop_number(succ)];
            next->end = order;
            next->prev = loop;
            loop = next;
          }
        }
      } else {
        // Finish with all successors of the current block.
        if (has_loop_number(block)) {
          // If we are going to pop a loop header, then add its entire body.
          LoopInfo* info = &loops_[loop_number(block)];
          for (const Block* b = info->start; true;
               b = block_data_[b->index()].rpo_next) {
            if (block_data_[b->index()].rpo_next == info->end) {
              PushFront(order, b);
              info->end = order;
              break;
            }
          }
          order = info->start;
        } else {
          // Pop a single node off the stack and add it to the order.
          order = PushFront(order, block);
          set_rpo_number(block, kBlockVisited2);
        }
        stack.pop();
      }
    }

    return ComputeBlockPermutation(entry);
  }

 private:
  // Computes loop membership from the backedges of the control flow graph.
  void ComputeLoopInfo(size_t num_loops, std::vector<Backedge>& backedges) {
    std::stack<const Block*> stack;

    // Extend loop information vector.
    loops_.resize(num_loops, LoopInfo{});

    // Compute loop membership starting from backedges.
    // O(max(loop_depth) * |loop|)
    for (auto [backedge, header_index] : backedges) {
      const Block* header = SuccessorBlocks(*backedge, *graph_)[header_index];
      DCHECK(header->IsLoop());
      size_t loop_num = loop_number(header);
      DCHECK_NULL(loops_[loop_num].header);
      loops_[loop_num].header = header;
      loops_[loop_num].members =
          zone_->New<BitVector>(graph_->block_count(), zone_);

      if (backedge != header) {
        // As long as the header doesn't have a backedge to itself,
        // Push the member onto the queue and process its predecessors.
        DCHECK(!loops_[loop_num].members->Contains(backedge->index().id()));
        loops_[loop_num].members->Add(backedge->index().id());
        stack.push(backedge);
      }

      // Propagate loop membership backwards. All predecessors of M up to the
      // loop header H are members of the loop too. O(|blocks between M and H|).
      while (!stack.empty()) {
        const Block* block = stack.top();
        stack.pop();
        for (const Block* pred : block->PredecessorsIterable()) {
          if (pred != header) {
            if (!loops_[loop_num].members->Contains(pred->index().id())) {
              loops_[loop_num].members->Add(pred->index().id());
              stack.push(pred);
            }
          }
        }
      }
    }
  }

  ZoneVector<uint32_t> ComputeBlockPermutation(const Block* entry) {
    ZoneVector<uint32_t> result(graph_->block_count(), zone_);
    size_t i = 0;
    for (const Block* b = entry; b; b = block_data_[b->index()].rpo_next) {
      result[i++] = b->index().id();
    }
    DCHECK_EQ(i, graph_->block_count());
    return result;
  }

  int32_t rpo_number(const Block* block) const {
    return block_data_[block->index()].rpo_number;
  }

  void set_rpo_number(const Block* block, int32_t rpo_number) {
    block_data_[block->index()].rpo_number = rpo_number;
  }

  bool has_loop_number(const Block* block) const {
    return block_data_[block->index()].loop_number != BlockData::kNoLoopNumber;
  }

  size_t loop_number(const Block* block) const {
    DCHECK(has_loop_number(block));
    return block_data_[block->index()].loop_number;
  }

  void set_loop_number(const Block* block, size_t loop_number) {
    block_data_[block->index()].loop_number = loop_number;
  }

  const Block* PushFront(const Block* head, const Block* block) {
    block_data_[block->index()].rpo_next = head;
    return block;
  }

  const Graph* graph_;
  FixedBlockSidetable<BlockData> block_data_;
  std::vector<LoopInfo> loops_;
  Zone* zone_;
};

base::Optional<BailoutReason> InstructionSelectionPhase::Run(
    Zone* temp_zone, const CallDescriptor* call_descriptor, Linkage* linkage,
    CodeTracer* code_tracer) {
  PipelineData* data = &PipelineData::Get();
  Graph& graph = PipelineData::Get().graph();

  // Compute special RPO order....
  TurboshaftSpecialRPONumberer numberer(graph, temp_zone);
  auto schedule = numberer.ComputeSpecialRPO();
  graph.ReorderBlocks(base::VectorOf(schedule));

  // Determine deferred blocks.
  graph.StartBlock().set_custom_data(
      0, Block::CustomDataKind::kDeferredInSchedule);
  for (Block& block : graph.blocks()) {
    const Block* predecessor = block.LastPredecessor();
    if (predecessor == nullptr) {
      continue;
    } else if (block.IsLoop()) {
      // We only consider the forward edge for loop headers.
      predecessor = predecessor->NeighboringPredecessor();
      DCHECK_NOT_NULL(predecessor);
      DCHECK_EQ(predecessor->NeighboringPredecessor(), nullptr);
      block.set_custom_data(predecessor->get_custom_data(
                                Block::CustomDataKind::kDeferredInSchedule),
                            Block::CustomDataKind::kDeferredInSchedule);
    } else if (predecessor->NeighboringPredecessor() == nullptr) {
      // This block has only a single predecessor. Due to edge-split form, those
      // are the only blocks that can be the target of a branch-like op which
      // might potentially provide a BranchHint to defer this block.
      const bool is_deferred =
          predecessor->get_custom_data(
              Block::CustomDataKind::kDeferredInSchedule) ||
          IsUnlikelySuccessor(predecessor, &block, graph);
      block.set_custom_data(is_deferred,
                            Block::CustomDataKind::kDeferredInSchedule);
    } else {
      block.set_custom_data(true, Block::CustomDataKind::kDeferredInSchedule);
      for (; predecessor; predecessor = predecessor->NeighboringPredecessor()) {
        // If there is a single predecessor that is not deferred, then block is
        // also not deferred.
        if (!predecessor->get_custom_data(
                Block::CustomDataKind::kDeferredInSchedule)) {
          block.set_custom_data(false,
                                Block::CustomDataKind::kDeferredInSchedule);
          break;
        }
      }
    }
  }

  // Print graph once before instruction selection.
  turboshaft::PrintTurboshaftGraph(temp_zone, code_tracer,
                                   "before instruction selection");

  // Initialize an instruction sequence.
  data->InitializeInstructionSequence(call_descriptor);

  // Run the actual instruction selection.
  InstructionSelector selector = InstructionSelector::ForTurboshaft(
      temp_zone, graph.op_id_count(), linkage, data->sequence(), &graph,
      data->frame(),
      data->info()->switch_jump_table()
          ? InstructionSelector::kEnableSwitchJumpTable
          : InstructionSelector::kDisableSwitchJumpTable,
      &data->info()->tick_counter(), data->broker(),
      data->address_of_max_unoptimized_frame_height(),
      data->address_of_max_pushed_argument_count(),
      data->info()->source_positions()
          ? InstructionSelector::kAllSourcePositions
          : InstructionSelector::kCallSourcePositions,
      InstructionSelector::SupportedFeatures(),
      v8_flags.turbo_instruction_scheduling
          ? InstructionSelector::kEnableScheduling
          : InstructionSelector::kDisableScheduling,
      data->assembler_options().enable_root_relative_access
          ? InstructionSelector::kEnableRootsRelativeAddressing
          : InstructionSelector::kDisableRootsRelativeAddressing,
      data->info()->trace_turbo_json()
          ? InstructionSelector::kEnableTraceTurboJson
          : InstructionSelector::kDisableTraceTurboJson);
  if (base::Optional<BailoutReason> bailout = selector.SelectInstructions()) {
    return bailout;
  }
  TraceSequence(data->info(), data->sequence(), data->broker(), code_tracer,
                "after instruction selection");
  return base::nullopt;
}

}  // namespace v8::internal::compiler::turboshaft
