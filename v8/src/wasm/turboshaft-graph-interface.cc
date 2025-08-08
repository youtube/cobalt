// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/turboshaft-graph-interface.h"

#include "src/common/globals.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/dataview-reducer.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "src/compiler/turboshaft/select-lowering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/compiler/turboshaft/wasm-assembler-helpers.h"
#include "src/compiler/wasm-compiler-definitions.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/inlining-tree.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8::internal::wasm {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

using Assembler =
    compiler::turboshaft::Assembler<compiler::turboshaft::reducer_list<
        compiler::turboshaft::SelectLoweringReducer,
        compiler::turboshaft::DataViewReducer,
        compiler::turboshaft::VariableReducer,
        compiler::turboshaft::RequiredOptimizationReducer>>;
using compiler::AccessBuilder;
using compiler::CallDescriptor;
using compiler::MemoryAccessKind;
using compiler::Operator;
using compiler::TrapId;
using compiler::turboshaft::CallOp;
using compiler::turboshaft::ConditionWithHint;
using compiler::turboshaft::ConstantOp;
using compiler::turboshaft::Float32;
using compiler::turboshaft::Float64;
using compiler::turboshaft::FloatRepresentation;
using compiler::turboshaft::Graph;
using compiler::turboshaft::Label;
using compiler::turboshaft::LoadOp;
using compiler::turboshaft::LoopLabel;
using compiler::turboshaft::MemoryRepresentation;
using compiler::turboshaft::Operation;
using compiler::turboshaft::OperationMatcher;
using compiler::turboshaft::OpIndex;
using compiler::turboshaft::PendingLoopPhiOp;
using compiler::turboshaft::RegisterRepresentation;
using compiler::turboshaft::Simd128ConstantOp;
using compiler::turboshaft::StackCheckOp;
using compiler::turboshaft::StoreOp;
using compiler::turboshaft::SupportedOperations;
using compiler::turboshaft::Tagged;
using compiler::turboshaft::Variable;
using compiler::turboshaft::WordRepresentation;
using TSBlock = compiler::turboshaft::Block;
using compiler::turboshaft::TSCallDescriptor;
using compiler::turboshaft::Uninitialized;
using compiler::turboshaft::V;
using compiler::turboshaft::Word32;
using compiler::turboshaft::Word64;
using compiler::turboshaft::WordPtr;

namespace {

#define DATAVIEW_OP_LIST(V) \
  V(BigInt64)               \
  V(BigUint64)              \
  V(Float32)                \
  V(Float64)                \
  V(Int8)                   \
  V(Int16)                  \
  V(Int32)                  \
  V(Uint8)                  \
  V(Uint16)                 \
  V(Uint32)

enum class DataViewOp {
#define V(Name) kGet##Name, kSet##Name,
  DATAVIEW_OP_LIST(V)
#undef V
};

ExternalArrayType GetExternalArrayType(DataViewOp op_type) {
  switch (op_type) {
#define V(Name)                \
  case DataViewOp::kGet##Name: \
  case DataViewOp::kSet##Name: \
    return kExternal##Name##Array;
    DATAVIEW_OP_LIST(V)
#undef V
  }
}

size_t GetTypeSize(DataViewOp op_type) {
  ExternalArrayType array_type = GetExternalArrayType(op_type);
  switch (array_type) {
#define ELEMENTS_KIND_TO_ELEMENT_SIZE(Type, type, TYPE, ctype) \
  case kExternal##Type##Array:                                 \
    return sizeof(ctype);

    TYPED_ARRAYS(ELEMENTS_KIND_TO_ELEMENT_SIZE)
#undef ELEMENTS_KIND_TO_ELEMENT_SIZE
  }
}

}  // namespace

class TurboshaftGraphBuildingInterface {
 public:
  enum Mode { kRegular, kInlinedUnhandled, kInlinedWithCatch };
  using ValidationTag = Decoder::FullValidationTag;
  using FullDecoder =
      WasmFullDecoder<ValidationTag, TurboshaftGraphBuildingInterface>;
  static constexpr bool kUsesPoppedArgs = true;
  static constexpr MemoryRepresentation kMaybeSandboxedPointer =
      V8_ENABLE_SANDBOX_BOOL ? MemoryRepresentation::SandboxedPointer()
                             : MemoryRepresentation::PointerSized();

  struct Value : public ValueBase<ValidationTag> {
    OpIndex op = OpIndex::Invalid();
    template <typename... Args>
    explicit Value(Args&&... args) V8_NOEXCEPT
        : ValueBase(std::forward<Args>(args)...) {}
  };

  struct Control : public ControlBase<Value, ValidationTag> {
    TSBlock* merge_block = nullptr;
    // for 'if', loops, and 'try' respectively.
    TSBlock* false_or_loop_or_catch_block = nullptr;
    V<Tagged> exception = OpIndex::Invalid();  // Only for 'try-catch'.

    template <typename... Args>
    explicit Control(Args&&... args) V8_NOEXCEPT
        : ControlBase(std::forward<Args>(args)...) {}
  };

  TurboshaftGraphBuildingInterface(
      Assembler& assembler, AssumptionsJournal* assumptions,
      ZoneVector<WasmInliningPosition>* inlining_positions, int func_index,
      const WireBytesStorage* wire_bytes)
      : mode_(kRegular),
        asm_(assembler),
        assumptions_(assumptions),
        inlining_positions_(inlining_positions),
        func_index_(func_index),
        wire_bytes_(wire_bytes) {}

  TurboshaftGraphBuildingInterface(
      Assembler& assembler, AssumptionsJournal* assumptions,
      ZoneVector<WasmInliningPosition>* inlining_positions, int func_index,
      const WireBytesStorage* wire_bytes, std::vector<OpIndex>* real_parameters,
      TSBlock* return_block, TSBlock* catch_block)
      : mode_(catch_block == nullptr ? kInlinedUnhandled : kInlinedWithCatch),
        asm_(assembler),
        assumptions_(assumptions),
        inlining_positions_(inlining_positions),
        func_index_(func_index),
        wire_bytes_(wire_bytes),
        real_parameters_(real_parameters),
        return_block_(return_block),
        return_catch_block_(catch_block) {
    DCHECK_NOT_NULL(real_parameters);
    DCHECK_NOT_NULL(return_block);
  }

  void StartFunction(FullDecoder* decoder) {
    if (mode_ == kRegular) __ Bind(__ NewBlock());
    // Set 0 as the current source position (before locals declarations).
    __ SetCurrentOrigin(WasmPositionToOpIndex(0, inlining_id_));
    ssa_env_.resize(decoder->num_locals());
    uint32_t index = 0;
    if (mode_ == kRegular) {
      static_assert(kWasmInstanceParameterIndex == 0);
      instance_node_ = __ WasmInstanceParameter();
      for (; index < decoder->sig_->parameter_count(); index++) {
        // Parameter indices are shifted by 1 because parameter 0 is the
        // instance.
        ssa_env_[index] = __ Parameter(
            index + 1, RepresentationFor(decoder->sig_->GetParam(index)));
      }
    } else {
      instance_node_ = (*real_parameters_)[0];
      for (; index < decoder->sig_->parameter_count(); index++) {
        // Parameter indices are shifted by 1 because parameter 0 is the
        // instance.
        ssa_env_[index] = (*real_parameters_)[index + 1];
      }
      return_phis_.phi_inputs.resize(decoder->sig_->return_count());
      return_phis_.phi_types.resize(decoder->sig_->return_count());
      for (size_t i = 0; i < decoder->sig_->return_count(); i++) {
        return_phis_.phi_types[i] = decoder->sig_->GetReturn(i);
      }
    }
    while (index < decoder->num_locals()) {
      ValueType type = decoder->local_type(index);
      OpIndex op;
      if (!type.is_defaultable()) {
        DCHECK(type.is_reference());
        // TODO(jkummerow): Consider using "the hole" instead, to make any
        // illegal uses more obvious.
        op = __ Null(type.AsNullable());
      } else {
        op = DefaultValue(type);
      }
      while (index < decoder->num_locals() &&
             decoder->local_type(index) == type) {
        ssa_env_[index++] = op;
      }
    }

    if (inlining_enabled(decoder)) {
      if (mode_ == kRegular) {
        if (v8_flags.liftoff) {
          inlining_decisions_ = decoder->zone_->New<InliningTree>(
              decoder->zone_, decoder->module_, func_index_,
              0,  // call count
              0   // wire byte size. We pass 0 so that the initial node is
                  // always expanded, regardless of budget.
          );
          inlining_decisions_->FullyExpand(
              decoder->module_->functions[func_index_].code.length());
        } else {
          set_no_liftoff_inlining_budget(std::max(
              100,
              static_cast<int>(
                  2 * decoder->module_->functions[func_index_].code.length())));
        }
      } else {
#if DEBUG
        if (v8_flags.liftoff && inlining_decisions_) {
          // DCHECK that `inlining_decisions_` is consistent.
          DCHECK(inlining_decisions_->is_inlined());
          DCHECK_EQ(inlining_decisions_->function_index(), func_index_);
          base::SharedMutexGuard<base::kShared> mutex_guard(
              &decoder->module_->type_feedback.mutex);
          if (inlining_decisions_->feedback_found()) {
            DCHECK_NE(
                decoder->module_->type_feedback.feedback_for_function.find(
                    func_index_),
                decoder->module_->type_feedback.feedback_for_function.end());
            DCHECK_EQ(inlining_decisions_->function_calls().size(),
                      decoder->module_->type_feedback.feedback_for_function
                          .find(func_index_)
                          ->second.feedback_vector.size());
            DCHECK_EQ(inlining_decisions_->function_calls().size(),
                      decoder->module_->type_feedback.feedback_for_function
                          .find(func_index_)
                          ->second.call_targets.size());
          }
        }
#endif
      }
    }

    if (mode_ == kRegular) {
      StackCheck(StackCheckOp::CheckKind::kFunctionHeaderCheck);
    }

    if (v8_flags.trace_wasm) {
      __ SetCurrentOrigin(
          WasmPositionToOpIndex(decoder->position(), inlining_id_));
      CallRuntime(Runtime::kWasmTraceEnter, {});
    }

    auto branch_hints_it = decoder->module_->branch_hints.find(func_index_);
    if (branch_hints_it != decoder->module_->branch_hints.end()) {
      branch_hints_ = &branch_hints_it->second;
    }
  }

  void StartFunctionBody(FullDecoder* decoder, Control* block) {}

  void FinishFunction(FullDecoder* decoder) {
    if (v8_flags.liftoff && inlining_decisions_ &&
        inlining_decisions_->feedback_found()) {
      DCHECK_EQ(
          feedback_slot_,
          static_cast<int>(inlining_decisions_->function_calls().size()) - 1);
    }
    if (mode_ == kRegular) {
      for (OpIndex index : __ output_graph().AllOperationIndices()) {
        SourcePosition position = OpIndexToSourcePosition(
            __ output_graph().operation_origins()[index]);
        __ output_graph().source_positions()[index] = position;
      }
    }
  }

  void OnFirstError(FullDecoder*) {}

  void NextInstruction(FullDecoder* decoder, WasmOpcode) {
    __ SetCurrentOrigin(
        WasmPositionToOpIndex(decoder->position(), inlining_id_));
  }

  // ******** Control Flow ********
  // The basic structure of control flow is {block_phis_}. It contains a mapping
  // from blocks to phi inputs corresponding to the SSA values plus the stack
  // merge values at the beginning of the block.
  // - When we create a new block (to be bound in the future), we register it to
  //   {block_phis_} with {NewBlockWithPhis}.
  // - When we encounter an jump to a block, we invoke {SetupControlFlowEdge}.
  // - Finally, when we bind a block, we setup its phis, the SSA environment,
  //   and its merge values, with {BindBlockAndGeneratePhis}.
  // - When we create a loop, we generate PendingLoopPhis for the SSA state and
  //   the incoming stack values. We also create a block which will act as a
  //   merge block for all loop backedges (since a loop in Turboshaft can only
  //   have one backedge). When we PopControl a loop, we enter the merge block
  //   to create its Phis for all backedges as necessary, and use those values
  //   to patch the backedge of the PendingLoopPhis of the loop.

  void Block(FullDecoder* decoder, Control* block) {
    block->merge_block = NewBlockWithPhis(decoder, block->br_merge());
  }

  void Loop(FullDecoder* decoder, Control* block) {
    TSBlock* loop = __ NewLoopHeader();
    __ Goto(loop);
    __ Bind(loop);
    for (uint32_t i = 0; i < decoder->num_locals(); i++) {
      OpIndex phi = __ PendingLoopPhi(
          ssa_env_[i], RepresentationFor(decoder->local_type(i)));
      ssa_env_[i] = phi;
    }
    uint32_t arity = block->start_merge.arity;
    Value* stack_base = arity > 0 ? decoder->stack_value(arity) : nullptr;
    for (uint32_t i = 0; i < arity; i++) {
      OpIndex phi = __ PendingLoopPhi(stack_base[i].op,
                                      RepresentationFor(stack_base[i].type));
      block->start_merge[i].op = phi;
    }

    StackCheck(StackCheckOp::CheckKind::kLoopCheck);

    TSBlock* loop_merge = NewBlockWithPhis(decoder, &block->start_merge);
    block->merge_block = loop_merge;
    block->false_or_loop_or_catch_block = loop;
  }

  void If(FullDecoder* decoder, const Value& cond, Control* if_block) {
    TSBlock* true_block = __ NewBlock();
    TSBlock* false_block = NewBlockWithPhis(decoder, nullptr);
    TSBlock* merge_block = NewBlockWithPhis(decoder, &if_block->end_merge);
    if_block->false_or_loop_or_catch_block = false_block;
    if_block->merge_block = merge_block;
    SetupControlFlowEdge(decoder, false_block);
    __ Branch({cond.op, GetBranchHint(decoder)}, true_block, false_block);
    __ Bind(true_block);
  }

  void Else(FullDecoder* decoder, Control* if_block) {
    if (if_block->reachable()) {
      SetupControlFlowEdge(decoder, if_block->merge_block);
      __ Goto(if_block->merge_block);
    }
    BindBlockAndGeneratePhis(decoder, if_block->false_or_loop_or_catch_block,
                             nullptr);
  }

  void BrOrRet(FullDecoder* decoder, uint32_t depth, uint32_t drop_values) {
    if (depth == decoder->control_depth() - 1) {
      DoReturn(decoder, drop_values);
    } else {
      Control* target = decoder->control_at(depth);
      SetupControlFlowEdge(decoder, target->merge_block, drop_values);
      __ Goto(target->merge_block);
    }
  }

  void BrIf(FullDecoder* decoder, const Value& cond, uint32_t depth) {
    // TODO(14108): Branch hints.
    BranchHint hint = GetBranchHint(decoder);
    if (depth == decoder->control_depth() - 1) {
      IF ({cond.op, hint}) {
        DoReturn(decoder, 0);
      }
      END_IF
    } else {
      Control* target = decoder->control_at(depth);
      SetupControlFlowEdge(decoder, target->merge_block);
      TSBlock* non_branching = __ NewBlock();
      __ Branch({cond.op, hint}, target->merge_block, non_branching);
      __ Bind(non_branching);
    }
  }

  void BrTable(FullDecoder* decoder, const BranchTableImmediate& imm,
               const Value& key) {
    compiler::turboshaft::SwitchOp::Case* cases =
        __ output_graph()
            .graph_zone()
            ->AllocateArray<compiler::turboshaft::SwitchOp::Case>(
                imm.table_count);
    BranchTableIterator<ValidationTag> new_block_iterator(decoder, imm);
    std::vector<TSBlock*> intermediate_blocks;
    TSBlock* default_case = nullptr;
    while (new_block_iterator.has_next()) {
      TSBlock* intermediate = __ NewBlock();
      intermediate_blocks.emplace_back(intermediate);
      uint32_t i = new_block_iterator.cur_index();
      if (i == imm.table_count) {
        default_case = intermediate;
      } else {
        cases[i] = {static_cast<int>(i), intermediate, BranchHint::kNone};
      }
      new_block_iterator.next();
    }
    DCHECK_NOT_NULL(default_case);
    __ Switch(key.op, base::VectorOf(cases, imm.table_count), default_case);

    int i = 0;
    BranchTableIterator<ValidationTag> branch_iterator(decoder, imm);
    while (branch_iterator.has_next()) {
      TSBlock* intermediate = intermediate_blocks[i];
      i++;
      __ Bind(intermediate);
      BrOrRet(decoder, branch_iterator.next(), 0);
    }
  }

  void FallThruTo(FullDecoder* decoder, Control* block) {
    // TODO(14108): Why is {block->reachable()} not reliable here? Maybe it is
    // not in other spots as well.
    if (__ current_block() != nullptr) {
      SetupControlFlowEdge(decoder, block->merge_block);
      __ Goto(block->merge_block);
    }
  }

  void PopControl(FullDecoder* decoder, Control* block) {
    switch (block->kind) {
      case kControlIf:
        if (block->reachable()) {
          SetupControlFlowEdge(decoder, block->merge_block);
          __ Goto(block->merge_block);
        }
        BindBlockAndGeneratePhis(decoder, block->false_or_loop_or_catch_block,
                                 nullptr);
        // Exceptionally for one-armed if, we cannot take the values from the
        // stack; we have to pass the stack values at the beginning of the
        // if-block.
        SetupControlFlowEdge(decoder, block->merge_block, 0, OpIndex::Invalid(),
                             &block->start_merge);
        __ Goto(block->merge_block);
        BindBlockAndGeneratePhis(decoder, block->merge_block,
                                 block->br_merge());
        break;
      case kControlIfElse:
      case kControlBlock:
      case kControlTry:
      case kControlTryCatch:
      case kControlTryCatchAll:
        // {block->reachable()} is not reliable here for exceptions, because
        // the decoder sets the reachability to the upper block's reachability
        // before calling this interface function.
        if (__ current_block() != nullptr) {
          SetupControlFlowEdge(decoder, block->merge_block);
          __ Goto(block->merge_block);
        }
        BindBlockAndGeneratePhis(decoder, block->merge_block,
                                 block->br_merge());
        break;
      case kControlLoop: {
        TSBlock* post_loop = NewBlockWithPhis(decoder, nullptr);
        if (block->reachable()) {
          SetupControlFlowEdge(decoder, post_loop);
          __ Goto(post_loop);
        }
        if (!block->false_or_loop_or_catch_block->IsBound()) {
          // The loop is unreachable. In this case, no operations have been
          // emitted for it. Do nothing.
        } else if (block->merge_block->PredecessorCount() == 0) {
          // Turns out, the loop has no backedges, i.e. it is not quite a loop
          // at all. Replace it with a merge, and its PendingPhis with one-input
          // phis.
          block->false_or_loop_or_catch_block->SetKind(
              compiler::turboshaft::Block::Kind::kMerge);
          auto to = __ output_graph()
                        .operations(*block->false_or_loop_or_catch_block)
                        .begin();
          for (uint32_t i = 0; i < ssa_env_.size() + block->br_merge()->arity;
               ++i, ++to) {
            // TODO(manoskouk): Add `->` operator to the iterator.
            PendingLoopPhiOp& pending_phi = (*to).Cast<PendingLoopPhiOp>();
            OpIndex replaced = __ output_graph().Index(*to);
            __ output_graph().Replace<compiler::turboshaft::PhiOp>(
                replaced, base::VectorOf({pending_phi.first()}),
                pending_phi.rep);
          }
        } else {
          // We abuse the start merge of the loop, which is not used otherwise
          // anymore, to store backedge inputs for the pending phi stack values
          // of the loop.
          BindBlockAndGeneratePhis(decoder, block->merge_block,
                                   block->br_merge());
          __ Goto(block->false_or_loop_or_catch_block);
          auto to = __ output_graph()
                        .operations(*block->false_or_loop_or_catch_block)
                        .begin();
          for (uint32_t i = 0; i < ssa_env_.size(); ++i, ++to) {
            PendingLoopPhiOp& pending_phi = (*to).Cast<PendingLoopPhiOp>();
            OpIndex replaced = __ output_graph().Index(*to);
            __ output_graph().Replace<compiler::turboshaft::PhiOp>(
                replaced, base::VectorOf({pending_phi.first(), ssa_env_[i]}),
                pending_phi.rep);
          }
          for (uint32_t i = 0; i < block->br_merge()->arity; ++i, ++to) {
            PendingLoopPhiOp& pending_phi = (*to).Cast<PendingLoopPhiOp>();
            OpIndex replaced = __ output_graph().Index(*to);
            __ output_graph().Replace<compiler::turboshaft::PhiOp>(
                replaced,
                base::VectorOf(
                    {pending_phi.first(), (*block->br_merge())[i].op}),
                pending_phi.rep);
          }
        }
        BindBlockAndGeneratePhis(decoder, post_loop, nullptr);
        break;
      }
    }
  }

  void DoReturn(FullDecoder* decoder, uint32_t drop_values) {
    size_t return_count = decoder->sig_->return_count();
    base::SmallVector<OpIndex, 8> return_values(return_count);
    Value* stack_base = return_count == 0
                            ? nullptr
                            : decoder->stack_value(static_cast<uint32_t>(
                                  return_count + drop_values));
    for (size_t i = 0; i < return_count; i++) {
      return_values[i] = stack_base[i].op;
    }
    if (v8_flags.trace_wasm) {
      V<WordPtr> info = __ IntPtrConstant(0);
      if (return_count == 1) {
        wasm::ValueType return_type = decoder->sig_->GetReturn(0);
        int size = return_type.value_kind_size();
        // TODO(14108): This won't fit everything.
        info = __ StackSlot(size, size);
        // TODO(14108): Write barrier might be needed.
        __ Store(
            info, return_values[0], StoreOp::Kind::RawAligned(),
            MemoryRepresentation::FromMachineType(return_type.machine_type()),
            compiler::kNoWriteBarrier);
      }
      CallRuntime(Runtime::kWasmTraceExit, {info});
    }
    if (mode_ == kRegular) {
      __ Return(__ Word32Constant(0), base::VectorOf(return_values));
    } else {
      // Do not add return values if we are in unreachable code.
      if (__ generating_unreachable_operations()) return;
      for (size_t i = 0; i < return_values.size(); i++) {
        return_phis_.phi_inputs[i].push_back(return_values[i]);
      }
      __ Goto(return_block_);
    }
  }

  void UnOp(FullDecoder* decoder, WasmOpcode opcode, const Value& value,
            Value* result) {
    result->op = UnOpImpl(opcode, value.op, value.type);
  }

  void BinOp(FullDecoder* decoder, WasmOpcode opcode, const Value& lhs,
             const Value& rhs, Value* result) {
    result->op = BinOpImpl(opcode, lhs.op, rhs.op);
  }

  void TraceInstruction(FullDecoder* decoder, uint32_t markid) {
    // TODO(14108): Implement.
  }

  void I32Const(FullDecoder* decoder, Value* result, int32_t value) {
    result->op = __ Word32Constant(value);
  }

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {
    result->op = __ Word64Constant(value);
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    result->op = __ Float32Constant(value);
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    result->op = __ Float64Constant(value);
  }

  void S128Const(FullDecoder* decoder, const Simd128Immediate& imm,
                 Value* result) {
    result->op = __ Simd128Constant(imm.value);
  }

  void RefNull(FullDecoder* decoder, ValueType type, Value* result) {
    result->op = __ Null(type);
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    result->op = __ WasmRefFunc(instance_node_, function_index);
  }

  void RefAsNonNull(FullDecoder* decoder, const Value& arg, Value* result) {
    result->op =
        __ AssertNotNull(arg.op, arg.type, TrapId::kTrapNullDereference);
  }

  void Drop(FullDecoder* decoder) {}

  void LocalGet(FullDecoder* decoder, Value* result,
                const IndexImmediate& imm) {
    result->op = ssa_env_[imm.index];
  }

  void LocalSet(FullDecoder* decoder, const Value& value,
                const IndexImmediate& imm) {
    ssa_env_[imm.index] = value.op;
  }

  void LocalTee(FullDecoder* decoder, const Value& value, Value* result,
                const IndexImmediate& imm) {
    ssa_env_[imm.index] = result->op = value.op;
  }

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate& imm) {
    result->op = __ GlobalGet(instance_node_, imm.global);
  }

  void GlobalSet(FullDecoder* decoder, const Value& value,
                 const GlobalIndexImmediate& imm) {
    __ GlobalSet(instance_node_, value.op, imm.global);
  }

  void Trap(FullDecoder* decoder, TrapReason reason) {
    __ TrapIfNot(__ Word32Constant(0), OpIndex::Invalid(),
                 GetTrapIdForTrap(reason));
    __ Unreachable();
  }

  void AssertNullTypecheck(FullDecoder* decoder, const Value& obj,
                           Value* result) {
    __ TrapIfNot(__ IsNull(obj.op, obj.type), OpIndex::Invalid(),
                 TrapId::kTrapIllegalCast);
    Forward(decoder, obj, result);
  }

  void AssertNotNullTypecheck(FullDecoder* decoder, const Value& obj,
                              Value* result) {
    __ AssertNotNull(obj.op, obj.type, TrapId::kTrapIllegalCast);
    Forward(decoder, obj, result);
  }

  void NopForTestingUnsupportedInLiftoff(FullDecoder* decoder) {
    Bailout(decoder);
  }

  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    using Implementation = compiler::turboshaft::SelectOp::Implementation;
    bool use_select = false;
    switch (tval.type.kind()) {
      case kI32:
        if (SupportedOperations::word32_select()) use_select = true;
        break;
      case kI64:
        if (SupportedOperations::word64_select()) use_select = true;
        break;
      case kF32:
        if (SupportedOperations::float32_select()) use_select = true;
        break;
      case kF64:
        if (SupportedOperations::float64_select()) use_select = true;
        break;
      case kRef:
      case kRefNull:
      case kS128:
        break;
      case kI8:
      case kI16:
      case kRtt:
      case kVoid:
      case kBottom:
        UNREACHABLE();
    }
    result->op = __ Select(
        cond.op, tval.op, fval.op, RepresentationFor(tval.type),
        BranchHint::kNone,
        use_select ? Implementation::kCMove : Implementation::kBranch);
  }

  // TODO(14108): Cache memories' starts and sizes. Consider VariableReducer,
  // LoadElimination, or manual handling like ssa_env_.
  void LoadMem(FullDecoder* decoder, LoadType type,
               const MemoryAccessImmediate& imm, const Value& index,
               Value* result) {
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif

    MemoryRepresentation repr =
        MemoryRepresentation::FromMachineType(type.mem_type());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       compiler::EnforceBoundsCheck::kCanOmitBoundsCheck);

    V<WordPtr> mem_start = MemStart(imm.memory->index);

    LoadOp::Kind load_kind = GetMemoryAccessKind(repr, strategy);

    // TODO(14108): If offset is in int range, use it as static offset.
    OpIndex load = __ Load(__ WordPtrAdd(mem_start, imm.offset), final_index,
                           load_kind, repr);
    OpIndex extended_load =
        (type.value_type() == kWasmI64 && repr.SizeInBytes() < 8)
            ? (repr.IsSigned() ? __ ChangeInt32ToInt64(load)
                               : __ ChangeUint32ToUint64(load))
            : load;

    if (v8_flags.trace_wasm_memory) {
      // TODO(14259): Implement memory tracing for multiple memories.
      CHECK_EQ(0, imm.memory->index);
      TraceMemoryOperation(false, repr, final_index, imm.offset);
    }

    result->op = extended_load;
  }

  void LoadTransform(FullDecoder* decoder, LoadType type,
                     LoadTransformationKind transform,
                     const MemoryAccessImmediate& imm, const Value& index,
                     Value* result) {
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif
    MemoryRepresentation repr =
        transform == LoadTransformationKind::kExtend
            ? MemoryRepresentation::Int64()
            : MemoryRepresentation::FromMachineType(type.mem_type());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       compiler::EnforceBoundsCheck::kCanOmitBoundsCheck);

    compiler::turboshaft::Simd128LoadTransformOp::LoadKind load_kind =
        GetMemoryAccessKind(repr, strategy);

    using TransformKind =
        compiler::turboshaft::Simd128LoadTransformOp::TransformKind;

    TransformKind transform_kind;

    if (transform == LoadTransformationKind::kExtend) {
      if (type.mem_type() == MachineType::Int8()) {
        transform_kind = TransformKind::k8x8S;
      } else if (type.mem_type() == MachineType::Uint8()) {
        transform_kind = TransformKind::k8x8U;
      } else if (type.mem_type() == MachineType::Int16()) {
        transform_kind = TransformKind::k16x4S;
      } else if (type.mem_type() == MachineType::Uint16()) {
        transform_kind = TransformKind::k16x4U;
      } else if (type.mem_type() == MachineType::Int32()) {
        transform_kind = TransformKind::k32x2S;
      } else if (type.mem_type() == MachineType::Uint32()) {
        transform_kind = TransformKind::k32x2U;
      } else {
        UNREACHABLE();
      }
    } else if (transform == LoadTransformationKind::kSplat) {
      if (type.mem_type() == MachineType::Int8()) {
        transform_kind = TransformKind::k8Splat;
      } else if (type.mem_type() == MachineType::Int16()) {
        transform_kind = TransformKind::k16Splat;
      } else if (type.mem_type() == MachineType::Int32()) {
        transform_kind = TransformKind::k32Splat;
      } else if (type.mem_type() == MachineType::Int64()) {
        transform_kind = TransformKind::k64Splat;
      } else {
        UNREACHABLE();
      }
    } else {
      if (type.mem_type() == MachineType::Int32()) {
        transform_kind = TransformKind::k32Zero;
      } else if (type.mem_type() == MachineType::Int64()) {
        transform_kind = TransformKind::k64Zero;
      } else {
        UNREACHABLE();
      }
    }

    OpIndex load = __ Simd128LoadTransform(
        __ WordPtrAdd(MemStart(imm.mem_index), imm.offset), final_index,
        load_kind, transform_kind, 0);

    if (v8_flags.trace_wasm_memory) {
      TraceMemoryOperation(false, repr, final_index, imm.offset);
    }

    result->op = load;
  }

  void LoadLane(FullDecoder* decoder, LoadType type, const Value& value,
                const Value& index, const MemoryAccessImmediate& imm,
                const uint8_t laneidx, Value* result) {
    using compiler::turboshaft::Simd128LaneMemoryOp;
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif

    MemoryRepresentation repr =
        MemoryRepresentation::FromMachineType(type.mem_type());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       compiler::EnforceBoundsCheck::kCanOmitBoundsCheck);
    Simd128LaneMemoryOp::Kind kind = GetMemoryAccessKind(repr, strategy);

    Simd128LaneMemoryOp::LaneKind lane_kind;

    switch (repr) {
      case MemoryRepresentation::Int8():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k8;
        break;
      case MemoryRepresentation::Int16():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k16;
        break;
      case MemoryRepresentation::Int32():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k32;
        break;
      case MemoryRepresentation::Int64():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k64;
        break;
      default:
        UNREACHABLE();
    }

    // TODO(14108): If `offset` is in int range, use it as static offset, or
    // consider using a larger type as offset.
    OpIndex load = __ Simd128LaneMemory(
        __ WordPtrAdd(MemStart(imm.mem_index), imm.offset), final_index,
        value.op, Simd128LaneMemoryOp::Mode::kLoad, kind, lane_kind, laneidx,
        0);

    if (v8_flags.trace_wasm_memory) {
      TraceMemoryOperation(false, repr, final_index, imm.offset);
    }

    result->op = load;
  }

  void StoreMem(FullDecoder* decoder, StoreType type,
                const MemoryAccessImmediate& imm, const Value& index,
                const Value& value) {
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif

    MemoryRepresentation repr =
        MemoryRepresentation::FromMachineRepresentation(type.mem_rep());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       wasm::kPartialOOBWritesAreNoops
                           ? compiler::EnforceBoundsCheck::kCanOmitBoundsCheck
                           : compiler::EnforceBoundsCheck::kNeedsBoundsCheck);

    V<WordPtr> mem_start = MemStart(imm.memory->index);

    StoreOp::Kind store_kind = GetMemoryAccessKind(repr, strategy);

    OpIndex store_value = value.op;
    if (value.type == kWasmI64 && repr.SizeInBytes() <= 4) {
      store_value = __ TruncateWord64ToWord32(store_value);
    }
    // TODO(14108): If offset is in int range, use it as static offset.
    __ Store(mem_start, __ WordPtrAdd(imm.offset, final_index), store_value,
             store_kind, repr, compiler::kNoWriteBarrier, 0);

    if (v8_flags.trace_wasm_memory) {
      // TODO(14259): Implement memory tracing for multiple memories.
      CHECK_EQ(0, imm.memory->index);
      TraceMemoryOperation(true, repr, final_index, imm.offset);
    }
  }

  void StoreLane(FullDecoder* decoder, StoreType type,
                 const MemoryAccessImmediate& imm, const Value& index,
                 const Value& value, const uint8_t laneidx) {
    using compiler::turboshaft::Simd128LaneMemoryOp;
#if defined(V8_TARGET_BIG_ENDIAN)
    // TODO(14108): Implement for big endian.
    Bailout(decoder);
#endif

    MemoryRepresentation repr =
        MemoryRepresentation::FromMachineRepresentation(type.mem_rep());

    auto [final_index, strategy] =
        BoundsCheckMem(imm.memory, repr, index.op, imm.offset,
                       kPartialOOBWritesAreNoops
                           ? compiler::EnforceBoundsCheck::kCanOmitBoundsCheck
                           : compiler::EnforceBoundsCheck::kNeedsBoundsCheck);
    Simd128LaneMemoryOp::Kind kind = GetMemoryAccessKind(repr, strategy);

    Simd128LaneMemoryOp::LaneKind lane_kind;

    switch (repr) {
      // TODO(manoskouk): Why use unsigned representations here as opposed to
      // LoadLane?
      case MemoryRepresentation::Uint8():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k8;
        break;
      case MemoryRepresentation::Uint16():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k16;
        break;
      case MemoryRepresentation::Uint32():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k32;
        break;
      case MemoryRepresentation::Uint64():
        lane_kind = Simd128LaneMemoryOp::LaneKind::k64;
        break;
      default:
        UNREACHABLE();
    }

    // TODO(14108): If `offset` is in int range, use it as static offset, or
    // consider using a larger type as offset.
    __ Simd128LaneMemory(__ WordPtrAdd(MemStart(imm.mem_index), imm.offset),
                         final_index, value.op,
                         Simd128LaneMemoryOp::Mode::kStore, kind, lane_kind,
                         laneidx, 0);

    if (v8_flags.trace_wasm_memory) {
      TraceMemoryOperation(true, repr, final_index, imm.offset);
    }
  }

  void CurrentMemoryPages(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                          Value* result) {
    V<WordPtr> result_wordptr =
        __ WordPtrShiftRightArithmetic(MemSize(imm.index), kWasmPageSizeLog2);
    // In the 32-bit case, truncation happens implicitly.
    if (imm.memory->is_memory64) {
      result->op = __ ChangeIntPtrToInt64(result_wordptr);
    } else {
      result->op = __ TruncateWordPtrToWord32(result_wordptr);
    }
  }

  void MemoryGrow(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                  const Value& value, Value* result) {
    if (!imm.memory->is_memory64) {
      result->op =
          CallBuiltinThroughJumptable(decoder, Builtin::kWasmMemoryGrow,
                                      {__ Word32Constant(imm.index), value.op});
    } else {
      Label<Word64> done(&asm_);

      IF (LIKELY(__ Uint64LessThanOrEqual(
              value.op, __ Word64Constant(static_cast<int64_t>(kMaxInt))))) {
        GOTO(done, __ ChangeInt32ToInt64(CallBuiltinThroughJumptable(
                       decoder, Builtin::kWasmMemoryGrow,
                       {__ Word32Constant(imm.index),
                        __ TruncateWord64ToWord32(value.op)})));
      }
      ELSE {
        GOTO(done, __ Word64Constant(int64_t{-1}));
      }
      END_IF

      BIND(done, result_64);

      result->op = result_64;
    }
  }

  V<Tagged> ExternRefToString(const Value value, bool null_succeeds = false) {
    wasm::ValueType target_type =
        null_succeeds ? kWasmStringRef : kWasmRefString;
    compiler::WasmTypeCheckConfig config{value.type, target_type};
    V<Map> rtt = OpIndex::Invalid();
    return __ WasmTypeCast(value.op, rtt, config);
  }

  void BuildModifyThreadInWasmFlagHelper(OpIndex thread_in_wasm_flag_address,
                                         bool new_value) {
    if (v8_flags.debug_code) {
      V<Word32> flag_value =
          __ Load(thread_in_wasm_flag_address, LoadOp::Kind::RawAligned(),
                  MemoryRepresentation::Int32(), 0);

      IF (UNLIKELY(__ Word32Equal(flag_value, new_value))) {
        OpIndex message_id = __ TaggedIndexConstant(static_cast<int32_t>(
            new_value ? AbortReason::kUnexpectedThreadInWasmSet
                      : AbortReason::kUnexpectedThreadInWasmUnset));
        CallRuntime(Runtime::kAbort, {message_id});
        __ Unreachable();
      }
      END_IF
    }

    __ Store(thread_in_wasm_flag_address, __ Word32Constant(new_value),
             LoadOp::Kind::RawAligned(), MemoryRepresentation::Int32(),
             compiler::kNoWriteBarrier);
  }

  void BuildModifyThreadInWasmFlag(bool new_value) {
    if (!trap_handler::IsTrapHandlerEnabled()) return;

    OpIndex isolate_root = __ LoadRootRegister();
    OpIndex thread_in_wasm_flag_address =
        __ Load(isolate_root, LoadOp::Kind::RawAligned().Immutable(),
                MemoryRepresentation::PointerSized(),
                Isolate::thread_in_wasm_flag_address_offset());
    BuildModifyThreadInWasmFlagHelper(thread_in_wasm_flag_address, new_value);
  }

  void TypeCheckDataView(FullDecoder* decoder, V<Tagged> dataview,
                         DataViewOp op_type) {
    Label<> done_label(&asm_);
    Label<> type_error_label(&asm_);
    GOTO_IF(__ IsSmi(dataview), type_error_label);
    GOTO_IF_NOT(__ HasInstanceType(dataview, InstanceType::JS_DATA_VIEW_TYPE),
                type_error_label);
    GOTO(done_label);

    BIND(type_error_label);
    Builtin builtin_to_call;
    switch (op_type) {
      case DataViewOp::kGetBigInt64:
        builtin_to_call = Builtin::kThrowDataViewGetBigInt64TypeError;
        break;
      case DataViewOp::kGetBigUint64:
        builtin_to_call = Builtin::kThrowDataViewGetBigUint64TypeError;
        break;
      case DataViewOp::kGetFloat32:
        builtin_to_call = Builtin::kThrowDataViewGetFloat32TypeError;
        break;
      case DataViewOp::kGetFloat64:
        builtin_to_call = Builtin::kThrowDataViewGetFloat64TypeError;
        break;
      case DataViewOp::kGetInt8:
        builtin_to_call = Builtin::kThrowDataViewGetInt8TypeError;
        break;
      case DataViewOp::kGetInt16:
        builtin_to_call = Builtin::kThrowDataViewGetInt16TypeError;
        break;
      case DataViewOp::kGetInt32:
        builtin_to_call = Builtin::kThrowDataViewGetInt32TypeError;
        break;
      case DataViewOp::kGetUint8:
        builtin_to_call = Builtin::kThrowDataViewGetUint8TypeError;
        break;
      case DataViewOp::kGetUint16:
        builtin_to_call = Builtin::kThrowDataViewGetUint16TypeError;
        break;
      case DataViewOp::kGetUint32:
        builtin_to_call = Builtin::kThrowDataViewGetUint32TypeError;
        break;
      case DataViewOp::kSetBigInt64:
        builtin_to_call = Builtin::kThrowDataViewSetBigInt64TypeError;
        break;
      case DataViewOp::kSetBigUint64:
        builtin_to_call = Builtin::kThrowDataViewSetBigUint64TypeError;
        break;
      case DataViewOp::kSetFloat32:
        builtin_to_call = Builtin::kThrowDataViewSetFloat32TypeError;
        break;
      case DataViewOp::kSetFloat64:
        builtin_to_call = Builtin::kThrowDataViewSetFloat64TypeError;
        break;
      case DataViewOp::kSetInt8:
        builtin_to_call = Builtin::kThrowDataViewSetInt8TypeError;
        break;
      case DataViewOp::kSetInt16:
        builtin_to_call = Builtin::kThrowDataViewSetInt16TypeError;
        break;
      case DataViewOp::kSetInt32:
        builtin_to_call = Builtin::kThrowDataViewSetInt32TypeError;
        break;
      case DataViewOp::kSetUint8:
        builtin_to_call = Builtin::kThrowDataViewSetUint8TypeError;
        break;
      case DataViewOp::kSetUint16:
        builtin_to_call = Builtin::kThrowDataViewSetUint16TypeError;
        break;
      case DataViewOp::kSetUint32:
        builtin_to_call = Builtin::kThrowDataViewSetUint32TypeError;
        break;
      default:
        UNREACHABLE();
    }
    CallBuiltinThroughJumptable(decoder, builtin_to_call, {dataview});
    __ Unreachable();

    BIND(done_label);
  }

  void DataViewRelatedBoundsCheck(FullDecoder* decoder, V<WordPtr> left,
                                  V<WordPtr> right, DataViewOp op_type) {
    Builtin builtin_to_call;
    IF (__ IntPtrLessThan(left, right)) {
      switch (op_type) {
        case DataViewOp::kGetBigInt64:
          builtin_to_call = Builtin::kThrowDataViewGetBigInt64OutOfBounds;
          break;
        case DataViewOp::kGetBigUint64:
          builtin_to_call = Builtin::kThrowDataViewGetBigUint64OutOfBounds;
          break;
        case DataViewOp::kGetFloat32:
          builtin_to_call = Builtin::kThrowDataViewGetFloat32OutOfBounds;
          break;
        case DataViewOp::kGetFloat64:
          builtin_to_call = Builtin::kThrowDataViewGetFloat64OutOfBounds;
          break;
        case DataViewOp::kGetInt8:
          builtin_to_call = Builtin::kThrowDataViewGetInt8OutOfBounds;
          break;
        case DataViewOp::kGetInt16:
          builtin_to_call = Builtin::kThrowDataViewGetInt16OutOfBounds;
          break;
        case DataViewOp::kGetInt32:
          builtin_to_call = Builtin::kThrowDataViewGetInt32OutOfBounds;
          break;
        case DataViewOp::kGetUint8:
          builtin_to_call = Builtin::kThrowDataViewGetUint8OutOfBounds;
          break;
        case DataViewOp::kGetUint16:
          builtin_to_call = Builtin::kThrowDataViewGetUint16OutOfBounds;
          break;
        case DataViewOp::kGetUint32:
          builtin_to_call = Builtin::kThrowDataViewGetUint32OutOfBounds;
          break;
        case DataViewOp::kSetBigInt64:
          builtin_to_call = Builtin::kThrowDataViewSetBigInt64OutOfBounds;
          break;
        case DataViewOp::kSetBigUint64:
          builtin_to_call = Builtin::kThrowDataViewSetBigUint64OutOfBounds;
          break;
        case DataViewOp::kSetFloat32:
          builtin_to_call = Builtin::kThrowDataViewSetFloat32OutOfBounds;
          break;
        case DataViewOp::kSetFloat64:
          builtin_to_call = Builtin::kThrowDataViewSetFloat64OutOfBounds;
          break;
        case DataViewOp::kSetInt8:
          builtin_to_call = Builtin::kThrowDataViewSetInt8OutOfBounds;
          break;
        case DataViewOp::kSetInt16:
          builtin_to_call = Builtin::kThrowDataViewSetInt16OutOfBounds;
          break;
        case DataViewOp::kSetInt32:
          builtin_to_call = Builtin::kThrowDataViewSetInt32OutOfBounds;
          break;
        case DataViewOp::kSetUint8:
          builtin_to_call = Builtin::kThrowDataViewSetUint8OutOfBounds;
          break;
        case DataViewOp::kSetUint16:
          builtin_to_call = Builtin::kThrowDataViewSetUint16OutOfBounds;
          break;
        case DataViewOp::kSetUint32:
          builtin_to_call = Builtin::kThrowDataViewSetUint32OutOfBounds;
          break;
        default:
          UNREACHABLE();
      }
      CallBuiltinThroughJumptable(decoder, builtin_to_call, {});
      __ Unreachable();
    }
    END_IF
  }

  void DetachedDataViewBufferCheck(FullDecoder* decoder, V<Tagged> dataview,
                                   DataViewOp op_type) {
    // TODO(evih): Make the buffer load immutable.
    V<Object> buffer = __ LoadField<Object>(
        dataview, compiler::AccessBuilder::ForJSArrayBufferViewBuffer());
    V<Word32> bit_field = __ LoadField<Word32>(
        buffer, compiler::AccessBuilder::ForJSArrayBufferBitField());
    V<Word32> is_detached =
        __ Word32BitwiseAnd(bit_field, JSArrayBuffer::WasDetachedBit::kMask);
    Builtin builtin_to_call;
    IF (is_detached) {
      switch (op_type) {
        case DataViewOp::kGetBigInt64:
          builtin_to_call = Builtin::kThrowDataViewGetBigInt64DetachedError;
          break;
        case DataViewOp::kGetBigUint64:
          builtin_to_call = Builtin::kThrowDataViewGetBigUint64DetachedError;
          break;
        case DataViewOp::kGetFloat32:
          builtin_to_call = Builtin::kThrowDataViewGetFloat32DetachedError;
          break;
        case DataViewOp::kGetFloat64:
          builtin_to_call = Builtin::kThrowDataViewGetFloat64DetachedError;
          break;
        case DataViewOp::kGetInt8:
          builtin_to_call = Builtin::kThrowDataViewGetInt8DetachedError;
          break;
        case DataViewOp::kGetInt16:
          builtin_to_call = Builtin::kThrowDataViewGetInt16DetachedError;
          break;
        case DataViewOp::kGetInt32:
          builtin_to_call = Builtin::kThrowDataViewGetInt32DetachedError;
          break;
        case DataViewOp::kGetUint8:
          builtin_to_call = Builtin::kThrowDataViewGetUint8DetachedError;
          break;
        case DataViewOp::kGetUint16:
          builtin_to_call = Builtin::kThrowDataViewGetUint16DetachedError;
          break;
        case DataViewOp::kGetUint32:
          builtin_to_call = Builtin::kThrowDataViewGetUint32DetachedError;
          break;
        case DataViewOp::kSetBigInt64:
          builtin_to_call = Builtin::kThrowDataViewSetBigInt64DetachedError;
          break;
        case DataViewOp::kSetBigUint64:
          builtin_to_call = Builtin::kThrowDataViewSetBigUint64DetachedError;
          break;
        case DataViewOp::kSetFloat32:
          builtin_to_call = Builtin::kThrowDataViewSetFloat32DetachedError;
          break;
        case DataViewOp::kSetFloat64:
          builtin_to_call = Builtin::kThrowDataViewSetFloat64DetachedError;
          break;
        case DataViewOp::kSetInt8:
          builtin_to_call = Builtin::kThrowDataViewSetInt8DetachedError;
          break;
        case DataViewOp::kSetInt16:
          builtin_to_call = Builtin::kThrowDataViewSetInt16DetachedError;
          break;
        case DataViewOp::kSetInt32:
          builtin_to_call = Builtin::kThrowDataViewSetInt32DetachedError;
          break;
        case DataViewOp::kSetUint8:
          builtin_to_call = Builtin::kThrowDataViewSetUint8DetachedError;
          break;
        case DataViewOp::kSetUint16:
          builtin_to_call = Builtin::kThrowDataViewSetUint16DetachedError;
          break;
        case DataViewOp::kSetUint32:
          builtin_to_call = Builtin::kThrowDataViewSetUint32DetachedError;
          break;
        default:
          UNREACHABLE();
      }
      CallBuiltinThroughJumptable(decoder, builtin_to_call, {});
      __ Unreachable();
    }
    END_IF
  }

  void PerformDataViewChecks(FullDecoder* decoder, V<Tagged> dataview,
                             V<WordPtr> offset, DataViewOp op_type) {
    TypeCheckDataView(decoder, dataview, op_type);
    DataViewRelatedBoundsCheck(decoder, offset, __ IntPtrConstant(0), op_type);
    DetachedDataViewBufferCheck(decoder, dataview, op_type);

    V<WordPtr> byte_length = __ LoadField<WordPtr>(
        dataview, AccessBuilder::ForJSArrayBufferViewByteLength());
    V<WordPtr> bytelength_minus_size =
        __ WordPtrSub(byte_length, GetTypeSize(op_type));
    DataViewRelatedBoundsCheck(decoder, bytelength_minus_size, offset, op_type);
  }

  OpIndex DataViewGetter(FullDecoder* decoder, const Value args[],
                         DataViewOp op_type) {
    V<Tagged> dataview = args[0].op;
    V<WordPtr> offset = __ ChangeInt32ToIntPtr(args[1].op);
    V<Word32> is_little_endian =
        (op_type == DataViewOp::kGetInt8 || op_type == DataViewOp::kGetUint8)
            ? __ Word32Constant(1)
            : args[2].op;

    PerformDataViewChecks(decoder, dataview, offset, op_type);

    V<WordPtr> data_ptr = __ LoadField<WordPtr>(
        dataview, compiler::AccessBuilder::ForJSDataViewDataPointer());
    return __ LoadDataViewElement(dataview, data_ptr, offset, is_little_endian,
                                  GetExternalArrayType(op_type));
  }

  void DataViewSetter(FullDecoder* decoder, const Value args[],
                      DataViewOp op_type) {
    V<Tagged> dataview = args[0].op;
    V<WordPtr> offset = __ ChangeInt32ToIntPtr(args[1].op);
    V<Word32> value = args[2].op;
    V<Word32> is_little_endian =
        (op_type == DataViewOp::kSetInt8 || op_type == DataViewOp::kSetUint8)
            ? __ Word32Constant(1)
            : args[3].op;

    PerformDataViewChecks(decoder, dataview, offset, op_type);

    V<WordPtr> data_ptr = __ LoadField<WordPtr>(
        dataview, compiler::AccessBuilder::ForJSDataViewDataPointer());
    __ StoreDataViewElement(dataview, data_ptr, offset, value, is_little_endian,
                            GetExternalArrayType(op_type));
  }

  bool HandleWellKnownImport(FullDecoder* decoder, uint32_t index,
                             const Value args[], Value returns[]) {
    if (!decoder->module_) return false;  // Only needed for tests.
    WellKnownImportsList& well_known_imports =
        decoder->module_->type_feedback.well_known_imports;
    using WKI = WellKnownImport;
    WKI imported_op = well_known_imports.get(index);
    OpIndex result;
    switch (imported_op) {
      case WKI::kUninstantiated:
      case WKI::kGeneric:
      case WKI::kLinkError:
        return false;

      // WebAssembly.String.* imports.
      case WKI::kStringCharCodeAt: {
        V<Tagged> string = ExternRefToString(args[0]);
        V<Tagged> view = __ StringAsWtf16(string);
        // TODO(14108): Annotate `view`'s type.
        result = GetCodeUnitImpl(decoder, view, args[1].op);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      }
      case WKI::kStringCodePointAt: {
        V<Tagged> string = ExternRefToString(args[0]);
        V<Tagged> view = __ StringAsWtf16(string);
        // TODO(14108): Annotate `view`'s type.
        result = StringCodePointAt(decoder, view, args[1].op);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      }
      case WKI::kStringCompare: {
        V<Tagged> a_string = ExternRefToString(args[0]);
        V<Tagged> b_string = ExternRefToString(args[1]);
        result = __ UntagSmi(CallBuiltinThroughJumptable(
            decoder, Builtin::kStringCompare, {a_string, b_string},
            Operator::kEliminatable));
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      }
      case WKI::kStringConcat: {
        V<Tagged> head_string = ExternRefToString(args[0]);
        V<Tagged> tail_string = ExternRefToString(args[1]);
        V<HeapObject> native_context = LOAD_IMMUTABLE_INSTANCE_FIELD(
            instance_node_, NativeContext,
            MemoryRepresentation::TaggedPointer());
        result = CallBuiltinThroughJumptable(
            decoder, Builtin::kStringAdd_CheckNone,
            {head_string, tail_string, native_context},
            Operator::kNoDeopt | Operator::kNoThrow);
        result = __ AnnotateWasmType(result, kWasmRefString);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      }
      case WKI::kStringEquals: {
        // Using nullable type guards here because this instruction needs to
        // handle {null} without trapping.
        static constexpr bool kNullSucceeds = true;
        V<Tagged> a_string = ExternRefToString(args[0], kNullSucceeds);
        V<Tagged> b_string = ExternRefToString(args[1], kNullSucceeds);
        result = StringEqImpl(decoder, a_string, b_string, kWasmExternRef,
                              kWasmExternRef);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      }
      case WKI::kStringFromCharCode: {
        V<Word32> capped = __ Word32BitwiseAnd(args[0].op, 0xFFFF);
        result = CallBuiltinThroughJumptable(decoder,
                                             Builtin::kWasmStringFromCodePoint,
                                             {capped}, Operator::kEliminatable);
        result = __ AnnotateWasmType(result, kWasmRefString);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      }
      case WKI::kStringFromCodePoint:
        // TODO(14179): Fix trapping when the result is unused.
        result = CallBuiltinThroughJumptable(
            decoder, Builtin::kWasmStringFromCodePoint, {args[0].op},
            Operator::kEliminatable);
        result = __ AnnotateWasmType(result, kWasmRefString);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      case WKI::kStringFromWtf16Array:
        result = CallBuiltinThroughJumptable(
            decoder, Builtin::kWasmStringNewWtf16Array,
            {NullCheck(args[0]), args[1].op, args[2].op},
            Operator::kNoDeopt | Operator::kNoThrow);
        result = __ AnnotateWasmType(result, kWasmRefString);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      case WKI::kStringFromWtf8Array:
        result = StringNewWtf8ArrayImpl(decoder, unibrow::Utf8Variant::kWtf8,
                                        args[0], args[1], args[2]);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      case WKI::kStringLength: {
        V<Tagged> string = ExternRefToString(args[0]);
        result = __ template LoadField<Word32>(
            string, compiler::AccessBuilder::ForStringLength());
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      }
      case WKI::kStringSubstring: {
        V<Tagged> string = ExternRefToString(args[0]);
        V<Tagged> view = __ StringAsWtf16(string);
        // TODO(12868): Consider annotating {view}'s type when the typing story
        //              for string views has been settled.
        result = CallBuiltinThroughJumptable(
            decoder, Builtin::kWasmStringViewWtf16Slice,
            {view, args[1].op, args[2].op}, Operator::kEliminatable);
        result = __ AnnotateWasmType(result, kWasmRefString);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      }
      case WKI::kStringToWtf16Array: {
        V<Tagged> string = ExternRefToString(args[0]);
        result = CallBuiltinThroughJumptable(
            decoder, Builtin::kWasmStringEncodeWtf16Array,
            {string, NullCheck(args[1]), args[2].op},
            Operator::kNoDeopt | Operator::kNoThrow);
        decoder->detected_->Add(kFeature_imported_strings);
        break;
      }

      // Other string-related imports.
      case WKI::kDoubleToString:
        BuildModifyThreadInWasmFlag(false);
        result =
            CallBuiltinThroughJumptable(decoder, Builtin::kWasmFloat64ToString,
                                        {args[0].op}, Operator::kEliminatable);
        result = __ AnnotateWasmType(result, kWasmRefString);
        BuildModifyThreadInWasmFlag(true);
        decoder->detected_->Add(
            returns[0].type.is_reference_to(wasm::HeapType::kString)
                ? kFeature_stringref
                : kFeature_imported_strings);
        break;
      case WKI::kIntToString:
        BuildModifyThreadInWasmFlag(false);
        result = CallBuiltinThroughJumptable(decoder, Builtin::kWasmIntToString,
                                             {args[0].op, args[1].op},
                                             Operator::kNoDeopt);
        result = __ AnnotateWasmType(result, kWasmRefString);
        BuildModifyThreadInWasmFlag(true);
        decoder->detected_->Add(
            returns[0].type.is_reference_to(wasm::HeapType::kString)
                ? kFeature_stringref
                : kFeature_imported_strings);
        break;
      case WKI::kParseFloat: {
        if (args[0].type.is_nullable()) {
          Label<Float64> done(&asm_);
          GOTO_IF(__ IsNull(args[0].op, wasm::kWasmStringRef), done,
                  __ Float64Constant(std::numeric_limits<double>::quiet_NaN()));

          BuildModifyThreadInWasmFlag(false);
          V<Float64> not_null_res = CallBuiltinThroughJumptable(
              decoder, Builtin::kWasmStringToDouble, {args[0].op},
              Operator::kEliminatable);
          BuildModifyThreadInWasmFlag(true);
          GOTO(done, not_null_res);

          BIND(done, result_f64);
          result = result_f64;
        } else {
          BuildModifyThreadInWasmFlag(false);
          result = CallBuiltinThroughJumptable(
              decoder, Builtin::kWasmStringToDouble, {args[0].op},
              Operator::kEliminatable);
          BuildModifyThreadInWasmFlag(true);
        }
        decoder->detected_->Add(kFeature_stringref);
        break;
      }
      case WKI::kStringIndexOf: {
        V<Tagged> string = args[0].op;
        V<Tagged> search = args[1].op;
        V<Word32> start = args[2].op;

        // If string is null, throw.
        if (args[0].type.is_nullable()) {
          IF (__ IsNull(string, wasm::kWasmStringRef)) {
            CallBuiltinThroughJumptable(decoder,
                                        Builtin::kThrowIndexOfCalledOnNull, {},
                                        Operator::kNoWrite);
            __ Unreachable();
          }
          END_IF
        }

        // If search is null, replace it with "null".
        if (args[1].type.is_nullable()) {
          Label<Tagged> search_done_label(&asm_);
          GOTO_IF_NOT(__ IsNull(search, wasm::kWasmStringRef),
                      search_done_label, search);
          GOTO(search_done_label, LOAD_ROOT(null_string));
          BIND(search_done_label, search_value);
          search = search_value;
        }

        // Clamp the start index.
        Label<Word32> clamped_start_label(&asm_);
        GOTO_IF(__ Int32LessThan(start, 0), clamped_start_label,
                __ Word32Constant(0));
        V<Word32> length = __ template LoadField<Word32>(
            string, compiler::AccessBuilder::ForStringLength());
        GOTO_IF(__ Int32LessThan(start, length), clamped_start_label, start);
        GOTO(clamped_start_label, length);
        BIND(clamped_start_label, clamped_start);
        start = clamped_start;

        // This can't overflow because we've clamped `start` above.
        V<Smi> start_smi = __ TagSmi(start);
        BuildModifyThreadInWasmFlag(false);
        V<Smi> result_value = CallBuiltinThroughJumptable(
            decoder, Builtin::kStringIndexOf, {string, search, start_smi},
            Operator::kEliminatable);
        BuildModifyThreadInWasmFlag(true);
        result = __ UntagSmi(result_value);
        decoder->detected_->Add(kFeature_stringref);
        break;
      }
      case WKI::kStringToLocaleLowerCaseStringref:
        // TODO(14108): Implement.
        return false;
      case WKI::kStringToLowerCaseStringref: {
#if V8_INTL_SUPPORT
        V<Tagged> string = args[0].op;
        if (args[0].type.is_nullable()) {
          IF (__ IsNull(string, wasm::kWasmStringRef)) {
            CallBuiltinThroughJumptable(decoder,
                                        Builtin::kThrowToLowerCaseCalledOnNull,
                                        {}, Operator::kNoWrite);
            __ Unreachable();
          }
          END_IF
          BuildModifyThreadInWasmFlag(false);
          result = CallBuiltinThroughJumptable(
              decoder, Builtin::kStringToLowerCaseIntl,
              {string, __ NoContextConstant()}, Operator::kEliminatable);
          result = __ AnnotateWasmType(result, kWasmRefString);
          BuildModifyThreadInWasmFlag(true);
        }
        decoder->detected_->Add(kFeature_stringref);
        break;
#else
        return false;
#endif
      }

      // DataView related imports.
      case WKI::kDataViewGetBigInt64: {
        result = DataViewGetter(decoder, args, DataViewOp::kGetBigInt64);
        break;
      }
      case WKI::kDataViewGetBigUint64:
        result = DataViewGetter(decoder, args, DataViewOp::kGetBigUint64);
        break;
      case WKI::kDataViewGetFloat32:
        result = DataViewGetter(decoder, args, DataViewOp::kGetFloat32);
        break;
      case WKI::kDataViewGetFloat64:
        result = DataViewGetter(decoder, args, DataViewOp::kGetFloat64);
        break;
      case WKI::kDataViewGetInt8:
        result = DataViewGetter(decoder, args, DataViewOp::kGetInt8);
        break;
      case WKI::kDataViewGetInt16:
        result = DataViewGetter(decoder, args, DataViewOp::kGetInt16);
        break;
      case WKI::kDataViewGetInt32:
        result = DataViewGetter(decoder, args, DataViewOp::kGetInt32);
        break;
      case WKI::kDataViewGetUint8:
        result = DataViewGetter(decoder, args, DataViewOp::kGetUint8);
        break;
      case WKI::kDataViewGetUint16:
        result = DataViewGetter(decoder, args, DataViewOp::kGetUint16);
        break;
      case WKI::kDataViewGetUint32:
        result = DataViewGetter(decoder, args, DataViewOp::kGetUint32);
        break;
      case WKI::kDataViewSetBigInt64:
        DataViewSetter(decoder, args, DataViewOp::kSetBigInt64);
        break;
      case WKI::kDataViewSetBigUint64:
        DataViewSetter(decoder, args, DataViewOp::kSetBigUint64);
        break;
      case WKI::kDataViewSetFloat32:
        DataViewSetter(decoder, args, DataViewOp::kSetFloat32);
        break;
      case WKI::kDataViewSetFloat64:
        DataViewSetter(decoder, args, DataViewOp::kSetFloat64);
        break;
      case WKI::kDataViewSetInt8:
        DataViewSetter(decoder, args, DataViewOp::kSetInt8);
        break;
      case WKI::kDataViewSetInt16:
        DataViewSetter(decoder, args, DataViewOp::kSetInt16);
        break;
      case WKI::kDataViewSetInt32:
        DataViewSetter(decoder, args, DataViewOp::kSetInt32);
        break;
      case WKI::kDataViewSetUint8:
        DataViewSetter(decoder, args, DataViewOp::kSetUint8);
        break;
      case WKI::kDataViewSetUint16:
        DataViewSetter(decoder, args, DataViewOp::kSetUint16);
        break;
      case WKI::kDataViewSetUint32:
        DataViewSetter(decoder, args, DataViewOp::kSetUint32);
        break;
    }
    if (v8_flags.trace_wasm_inlining) {
      PrintF("[function %d: call to %d is well-known %s]\n", func_index_, index,
             WellKnownImportName(imported_op));
    }
    assumptions_->RecordAssumption(index, imported_op);
    returns[0].op = result;
    return true;
  }

  void CallDirect(FullDecoder* decoder, const CallFunctionImmediate& imm,
                  const Value args[], Value returns[]) {
    feedback_slot_++;
    if (imm.index < decoder->module_->num_imported_functions) {
      if (HandleWellKnownImport(decoder, imm.index, args, returns)) {
        return;
      }
      auto [target, ref] = BuildImportedFunctionTargetAndRef(imm.index);
      BuildWasmCall(decoder, imm.sig, target, ref, args, returns);
    } else {
      // Locally defined function.
      if (inlining_enabled(decoder) &&
          should_inline(feedback_slot_,
                        decoder->module_->functions[imm.index].code.length())) {
        InlineWasmCall(decoder, imm.index, imm.sig, 0, args, returns);
      } else {
        V<WordPtr> callee =
            __ RelocatableConstant(imm.index, RelocInfo::WASM_CALL);
        BuildWasmCall(decoder, imm.sig, callee, instance_node_, args, returns);
      }
    }
  }

  void ReturnCall(FullDecoder* decoder, const CallFunctionImmediate& imm,
                  const Value args[]) {
    feedback_slot_++;
    auto [target, ref] =
        imm.index < decoder->module_->num_imported_functions
            ? BuildImportedFunctionTargetAndRef(imm.index)
            : std::pair{__ RelocatableConstant(imm.index, RelocInfo::WASM_CALL),
                        instance_node_};
    BuildWasmMaybeReturnCall(decoder, imm.sig, target, ref, args);
  }

  void CallIndirect(FullDecoder* decoder, const Value& index,
                    const CallIndirectImmediate& imm, const Value args[],
                    Value returns[]) {
    auto [target, ref] = BuildIndirectCallTargetAndRef(decoder, index.op, imm);
    BuildWasmCall(decoder, imm.sig, target, ref, args, returns);
  }

  void ReturnCallIndirect(FullDecoder* decoder, const Value& index,
                          const CallIndirectImmediate& imm,
                          const Value args[]) {
    auto [target, ref] = BuildIndirectCallTargetAndRef(decoder, index.op, imm);
    BuildWasmMaybeReturnCall(decoder, imm.sig, target, ref, args);
  }

  void CallRef(FullDecoder* decoder, const Value& func_ref,
               const FunctionSig* sig, uint32_t sig_index, const Value args[],
               Value returns[]) {
    feedback_slot_++;
    if (inlining_enabled(decoder) &&
        should_inline(feedback_slot_, std::numeric_limits<int>::max())) {
      V<FixedArray> internal_functions =
          LOAD_IMMUTABLE_INSTANCE_FIELD(instance_node_, WasmInternalFunctions,
                                        MemoryRepresentation::TaggedPointer());
      size_t return_count = sig->return_count();
      base::Vector<InliningTree*> feedback_cases =
          inlining_decisions_->function_calls()[feedback_slot_];
      std::vector<base::SmallVector<OpIndex, 2>> case_returns(return_count);
      base::SmallVector<TSBlock*, 5> case_blocks;
      for (size_t i = 0; i < feedback_cases.size() + 1; i++) {
        case_blocks.push_back(__ NewBlock());
      }
      TSBlock* merge = __ NewBlock();
      __ Goto(case_blocks[0]);
      for (size_t i = 0; i < feedback_cases.size(); i++) {
        __ Bind(case_blocks[i]);
        InliningTree* tree = feedback_cases[i];
        if (!tree || !tree->is_inlined()) {
          __ Goto(case_blocks[i + 1]);
          continue;
        }
        uint32_t inlined_index = tree->function_index();
        V<Tagged> inlined_func_ref =
            __ LoadFixedArrayElement(internal_functions, inlined_index);
        TSBlock* inline_block = __ NewBlock();
        __ Branch(
            {__ TaggedEqual(func_ref.op, inlined_func_ref), BranchHint::kTrue},
            inline_block, case_blocks[i + 1]);

        __ Bind(inline_block);
        base::SmallVector<Value, 2> direct_returns(return_count);
        InlineWasmCall(decoder, inlined_index, sig, static_cast<uint32_t>(i),
                       args, direct_returns.data());
        for (size_t ret = 0; ret < direct_returns.size(); ret++) {
          case_returns[ret].push_back(direct_returns[ret].op);
        }
        __ Goto(merge);
      }

      TSBlock* no_inline_block = case_blocks[case_blocks.size() - 1];
      __ Bind(no_inline_block);
      auto [target, ref] =
          BuildFunctionReferenceTargetAndRef(func_ref.op, func_ref.type);
      base::SmallVector<Value, 2> ref_returns(return_count);
      BuildWasmCall(decoder, sig, target, ref, args, ref_returns.data());
      for (size_t ret = 0; ret < ref_returns.size(); ret++) {
        case_returns[ret].push_back(ref_returns[ret].op);
      }
      __ Goto(merge);

      __ Bind(merge);
      for (size_t i = 0; i < case_returns.size(); i++) {
        returns[i].op = __ Phi(base::VectorOf(case_returns[i]),
                               RepresentationFor(sig->GetReturn(i)));
      }
    } else {
      auto [target, ref] =
          BuildFunctionReferenceTargetAndRef(func_ref.op, func_ref.type);
      BuildWasmCall(decoder, sig, target, ref, args, returns);
    }
  }

  void ReturnCallRef(FullDecoder* decoder, const Value& func_ref,
                     const FunctionSig* sig, uint32_t sig_index,
                     const Value args[]) {
    feedback_slot_++;
    auto [target, ref] =
        BuildFunctionReferenceTargetAndRef(func_ref.op, func_ref.type);
    BuildWasmMaybeReturnCall(decoder, sig, target, ref, args);
  }

  void BrOnNull(FullDecoder* decoder, const Value& ref_object, uint32_t depth,
                bool pass_null_along_branch, Value* result_on_fallthrough) {
    result_on_fallthrough->op = ref_object.op;
    IF (UNLIKELY(__ IsNull(ref_object.op, ref_object.type))) {
      BrOrRet(decoder, depth, pass_null_along_branch ? 0 : 1);
    }
    END_IF
  }

  void BrOnNonNull(FullDecoder* decoder, const Value& ref_object, Value* result,
                   uint32_t depth, bool /* drop_null_on_fallthrough */) {
    result->op = ref_object.op;
    IF_NOT (UNLIKELY(__ IsNull(ref_object.op, ref_object.type))) {
      BrOrRet(decoder, depth, 0);
    }
    END_IF
  }

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, const Value* args,
              Value* result) {
    switch (opcode) {
#define HANDLE_BINARY_OPCODE(kind)                                            \
  case kExpr##kind:                                                           \
    result->op =                                                              \
        __ Simd128Binop(args[0].op, args[1].op,                               \
                        compiler::turboshaft::Simd128BinopOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_BINARY_OPCODE(HANDLE_BINARY_OPCODE)
#undef HANDLE_BINARY_OPCODE

#define HANDLE_INVERSE_COMPARISON(wasm_kind, ts_kind)            \
  case kExpr##wasm_kind:                                         \
    result->op = __ Simd128Binop(                                \
        args[1].op, args[0].op,                                  \
        compiler::turboshaft::Simd128BinopOp::Kind::k##ts_kind); \
    break;

      HANDLE_INVERSE_COMPARISON(I8x16LtS, I8x16GtS)
      HANDLE_INVERSE_COMPARISON(I8x16LtU, I8x16GtU)
      HANDLE_INVERSE_COMPARISON(I8x16LeS, I8x16GeS)
      HANDLE_INVERSE_COMPARISON(I8x16LeU, I8x16GeU)

      HANDLE_INVERSE_COMPARISON(I16x8LtS, I16x8GtS)
      HANDLE_INVERSE_COMPARISON(I16x8LtU, I16x8GtU)
      HANDLE_INVERSE_COMPARISON(I16x8LeS, I16x8GeS)
      HANDLE_INVERSE_COMPARISON(I16x8LeU, I16x8GeU)

      HANDLE_INVERSE_COMPARISON(I32x4LtS, I32x4GtS)
      HANDLE_INVERSE_COMPARISON(I32x4LtU, I32x4GtU)
      HANDLE_INVERSE_COMPARISON(I32x4LeS, I32x4GeS)
      HANDLE_INVERSE_COMPARISON(I32x4LeU, I32x4GeU)

      HANDLE_INVERSE_COMPARISON(I64x2LtS, I64x2GtS)
      HANDLE_INVERSE_COMPARISON(I64x2LeS, I64x2GeS)

      HANDLE_INVERSE_COMPARISON(F32x4Gt, F32x4Lt)
      HANDLE_INVERSE_COMPARISON(F32x4Ge, F32x4Le)
      HANDLE_INVERSE_COMPARISON(F64x2Gt, F64x2Lt)
      HANDLE_INVERSE_COMPARISON(F64x2Ge, F64x2Le)

#undef HANDLE_INVERSE_COMPARISON

#define HANDLE_UNARY_NON_OPTIONAL_OPCODE(kind)                            \
  case kExpr##kind:                                                       \
    result->op = __ Simd128Unary(                                         \
        args[0].op, compiler::turboshaft::Simd128UnaryOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_UNARY_NON_OPTIONAL_OPCODE(
          HANDLE_UNARY_NON_OPTIONAL_OPCODE)
#undef HANDLE_UNARY_NON_OPTIONAL_OPCODE

#define HANDLE_UNARY_OPTIONAL_OPCODE(kind, feature, external_ref)           \
  case kExpr##kind:                                                         \
    if (SupportedOperations::feature()) {                                   \
      result->op = __ Simd128Unary(                                         \
          args[0].op, compiler::turboshaft::Simd128UnaryOp::Kind::k##kind); \
    } else {                                                                \
      result->op = CallCStackSlotToStackSlot(                               \
          args[0].op, ExternalReference::external_ref(),                    \
          MemoryRepresentation::Simd128());                                 \
    }                                                                       \
    break;
      HANDLE_UNARY_OPTIONAL_OPCODE(F32x4Ceil, float32_round_up, wasm_f32x4_ceil)
      HANDLE_UNARY_OPTIONAL_OPCODE(F32x4Floor, float32_round_down,
                                   wasm_f32x4_floor)
      HANDLE_UNARY_OPTIONAL_OPCODE(F32x4Trunc, float32_round_to_zero,
                                   wasm_f32x4_trunc)
      HANDLE_UNARY_OPTIONAL_OPCODE(F32x4NearestInt, float32_round_ties_even,
                                   wasm_f32x4_nearest_int)
      HANDLE_UNARY_OPTIONAL_OPCODE(F64x2Ceil, float64_round_up, wasm_f64x2_ceil)
      HANDLE_UNARY_OPTIONAL_OPCODE(F64x2Floor, float64_round_down,
                                   wasm_f64x2_floor)
      HANDLE_UNARY_OPTIONAL_OPCODE(F64x2Trunc, float64_round_to_zero,
                                   wasm_f64x2_trunc)
      HANDLE_UNARY_OPTIONAL_OPCODE(F64x2NearestInt, float64_round_ties_even,
                                   wasm_f64x2_nearest_int)
#undef HANDLE_UNARY_OPTIONAL_OPCODE

#define HANDLE_SHIFT_OPCODE(kind)                                             \
  case kExpr##kind:                                                           \
    result->op =                                                              \
        __ Simd128Shift(args[0].op, args[1].op,                               \
                        compiler::turboshaft::Simd128ShiftOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_SHIFT_OPCODE(HANDLE_SHIFT_OPCODE)
#undef HANDLE_SHIFT_OPCODE

#define HANDLE_TEST_OPCODE(kind)                                         \
  case kExpr##kind:                                                      \
    result->op = __ Simd128Test(                                         \
        args[0].op, compiler::turboshaft::Simd128TestOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_TEST_OPCODE(HANDLE_TEST_OPCODE)
#undef HANDLE_TEST_OPCODE

#define HANDLE_SPLAT_OPCODE(kind)                                         \
  case kExpr##kind##Splat:                                                \
    result->op = __ Simd128Splat(                                         \
        args[0].op, compiler::turboshaft::Simd128SplatOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_SPLAT_OPCODE(HANDLE_SPLAT_OPCODE)
#undef HANDLE_SPLAT_OPCODE

// Ternary mask operators put the mask as first input.
#define HANDLE_TERNARY_MASK_OPCODE(kind)                        \
  case kExpr##kind:                                             \
    result->op = __ Simd128Ternary(                             \
        args[2].op, args[0].op, args[1].op,                     \
        compiler::turboshaft::Simd128TernaryOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_TERNARY_MASK_OPCODE(HANDLE_TERNARY_MASK_OPCODE)
#undef HANDLE_TERNARY_MASK_OPCODE

#define HANDLE_TERNARY_OTHER_OPCODE(kind)                       \
  case kExpr##kind:                                             \
    result->op = __ Simd128Ternary(                             \
        args[0].op, args[1].op, args[2].op,                     \
        compiler::turboshaft::Simd128TernaryOp::Kind::k##kind); \
    break;
      FOREACH_SIMD_128_TERNARY_OTHER_OPCODE(HANDLE_TERNARY_OTHER_OPCODE)
#undef HANDLE_TERNARY_OTHER_OPCODE

      default:
        UNREACHABLE();
    }
  }

  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate& imm,
                  base::Vector<const Value> inputs, Value* result) {
    using compiler::turboshaft::Simd128ExtractLaneOp;
    using compiler::turboshaft::Simd128ReplaceLaneOp;
    switch (opcode) {
      case kExprI8x16ExtractLaneS:
        result->op = __ Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI8x16S, imm.lane);
        break;
      case kExprI8x16ExtractLaneU:
        result->op = __ Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI8x16U, imm.lane);
        break;
      case kExprI16x8ExtractLaneS:
        result->op = __ Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI16x8S, imm.lane);
        break;
      case kExprI16x8ExtractLaneU:
        result->op = __ Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI16x8U, imm.lane);
        break;
      case kExprI32x4ExtractLane:
        result->op = __ Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI32x4, imm.lane);
        break;
      case kExprI64x2ExtractLane:
        result->op = __ Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kI64x2, imm.lane);
        break;
      case kExprF32x4ExtractLane:
        result->op = __ Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kF32x4, imm.lane);
        break;
      case kExprF64x2ExtractLane:
        result->op = __ Simd128ExtractLane(
            inputs[0].op, Simd128ExtractLaneOp::Kind::kF64x2, imm.lane);
        break;
      case kExprI8x16ReplaceLane:
        result->op =
            __ Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                  Simd128ReplaceLaneOp::Kind::kI8x16, imm.lane);
        break;
      case kExprI16x8ReplaceLane:
        result->op =
            __ Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                  Simd128ReplaceLaneOp::Kind::kI16x8, imm.lane);
        break;
      case kExprI32x4ReplaceLane:
        result->op =
            __ Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                  Simd128ReplaceLaneOp::Kind::kI32x4, imm.lane);
        break;
      case kExprI64x2ReplaceLane:
        result->op =
            __ Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                  Simd128ReplaceLaneOp::Kind::kI64x2, imm.lane);
        break;
      case kExprF32x4ReplaceLane:
        result->op =
            __ Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                  Simd128ReplaceLaneOp::Kind::kF32x4, imm.lane);
        break;
      case kExprF64x2ReplaceLane:
        result->op =
            __ Simd128ReplaceLane(inputs[0].op, inputs[1].op,
                                  Simd128ReplaceLaneOp::Kind::kF64x2, imm.lane);
        break;
      default:
        UNREACHABLE();
    }
  }

  void Simd8x16ShuffleOp(FullDecoder* decoder, const Simd128Immediate& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {
    result->op = __ Simd128Shuffle(input0.op, input1.op, imm.value);
  }

  void Try(FullDecoder* decoder, Control* block) {
    block->false_or_loop_or_catch_block = NewBlockWithPhis(decoder, nullptr);
    block->merge_block = NewBlockWithPhis(decoder, block->br_merge());
  }

  void Throw(FullDecoder* decoder, const TagIndexImmediate& imm,
             const Value arg_values[]) {
    size_t count = imm.tag->sig->parameter_count();
    base::SmallVector<OpIndex, 8> values(count);
    for (size_t index = 0; index < count; index++) {
      values[index] = arg_values[index].op;
    }

    uint32_t encoded_size = WasmExceptionPackage::GetEncodedSize(imm.tag);

    V<FixedArray> values_array =
        CallBuiltinThroughJumptable(decoder, Builtin::kWasmAllocateFixedArray,
                                    {__ IntPtrConstant(encoded_size)});

    uint32_t index = 0;
    const wasm::WasmTagSig* sig = imm.tag->sig;

    // Encode the exception values in {values_array}.
    for (size_t i = 0; i < count; i++) {
      OpIndex value = values[i];
      switch (sig->GetParam(i).kind()) {
        case kF32:
          value = __ BitcastFloat32ToWord32(value);
          V8_FALLTHROUGH;
        case kI32:
          BuildEncodeException32BitValue(values_array, index, value);
          // We need 2 Smis to encode a 32-bit value.
          index += 2;
          break;
        case kF64:
          value = __ BitcastFloat64ToWord64(value);
          V8_FALLTHROUGH;
        case kI64: {
          OpIndex upper_half =
              __ TruncateWord64ToWord32(__ Word64ShiftRightLogical(value, 32));
          BuildEncodeException32BitValue(values_array, index, upper_half);
          index += 2;
          OpIndex lower_half = __ TruncateWord64ToWord32(value);
          BuildEncodeException32BitValue(values_array, index, lower_half);
          index += 2;
          break;
        }
        case wasm::kRef:
        case wasm::kRefNull:
        case wasm::kRtt:
          __ StoreFixedArrayElement(values_array, index, value,
                                    compiler::kFullWriteBarrier);
          index++;
          break;
        case kS128: {
          using Kind = compiler::turboshaft::Simd128ExtractLaneOp::Kind;
          BuildEncodeException32BitValue(
              values_array, index,
              __ Simd128ExtractLane(value, Kind::kI32x4, 0));
          index += 2;
          BuildEncodeException32BitValue(
              values_array, index,
              __ Simd128ExtractLane(value, Kind::kI32x4, 1));
          index += 2;
          BuildEncodeException32BitValue(
              values_array, index,
              __ Simd128ExtractLane(value, Kind::kI32x4, 2));
          index += 2;
          BuildEncodeException32BitValue(
              values_array, index,
              __ Simd128ExtractLane(value, Kind::kI32x4, 3));
          index += 2;
          break;
        }
        case kI8:
        case kI16:
        case kVoid:
        case kBottom:
          UNREACHABLE();
      }
    }

    V<FixedArray> instance_tags = LOAD_IMMUTABLE_INSTANCE_FIELD(
        instance_node_, TagsTable, MemoryRepresentation::TaggedPointer());
    auto tag = V<WasmTagObject>::Cast(
        __ LoadFixedArrayElement(instance_tags, imm.index));

    CallBuiltinThroughJumptable(decoder, Builtin::kWasmThrow,
                                {tag, values_array}, Operator::kNoProperties,
                                CheckForException::kCatchInThisFrame);
    __ Unreachable();
  }

  void Rethrow(FullDecoder* decoder, Control* block) {
    CallBuiltinThroughJumptable(decoder, Builtin::kWasmRethrow,
                                {block->exception}, Operator::kNoProperties,
                                CheckForException::kCatchInThisFrame);
    __ Unreachable();
  }

  // TODO(14108): Optimize in case of unreachable catch block?
  void CatchException(FullDecoder* decoder, const TagIndexImmediate& imm,
                      Control* block, base::Vector<Value> values) {
    BindBlockAndGeneratePhis(decoder, block->false_or_loop_or_catch_block,
                             nullptr, &block->exception);
    V<NativeContext> native_context = LOAD_IMMUTABLE_INSTANCE_FIELD(
        instance_node_, NativeContext, MemoryRepresentation::TaggedPointer());
    V<WasmTagObject> caught_tag = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmGetOwnProperty,
        {block->exception, LOAD_IMMUTABLE_ROOT(wasm_exception_tag_symbol),
         native_context});
    V<FixedArray> instance_tags = LOAD_IMMUTABLE_INSTANCE_FIELD(
        instance_node_, TagsTable, MemoryRepresentation::TaggedPointer());
    auto expected_tag = V<WasmTagObject>::Cast(
        __ LoadFixedArrayElement(instance_tags, imm.index));
    TSBlock* if_catch = __ NewBlock();
    TSBlock* if_no_catch = NewBlockWithPhis(decoder, nullptr);
    SetupControlFlowEdge(decoder, if_no_catch);

    // If the tags don't match we continue with the next tag by setting the
    // no-catch environment as the new {block->catch_block} here.
    block->false_or_loop_or_catch_block = if_no_catch;

    if (imm.tag->sig->parameter_count() == 1 &&
        imm.tag->sig->GetParam(0).is_reference_to(HeapType::kExtern)) {
      // Check for the special case where the tag is WebAssembly.JSTag and the
      // exception is not a WebAssembly.Exception. In this case the exception is
      // caught and pushed on the operand stack.
      // Only perform this check if the tag signature is the same as
      // the JSTag signature, i.e. a single externref or (ref extern), otherwise
      // we know statically that it cannot be the JSTag.
      V<Word32> caught_tag_undefined =
          __ TaggedEqual(caught_tag, LOAD_IMMUTABLE_ROOT(UndefinedValue));
      Label<Tagged> if_catch(&asm_);
      Label<> no_catch_merge(&asm_);

      IF (UNLIKELY(caught_tag_undefined)) {
        V<Tagged> tag_object = __ Load(
            native_context, LoadOp::Kind::TaggedBase(),
            MemoryRepresentation::TaggedPointer(),
            NativeContext::OffsetOfElementAt(Context::WASM_JS_TAG_INDEX));
        V<Tagged> js_tag = __ Load(tag_object, LoadOp::Kind::TaggedBase(),
                                   MemoryRepresentation::TaggedPointer(),
                                   WasmTagObject::kTagOffset);
        GOTO_IF(__ TaggedEqual(expected_tag, js_tag), if_catch,
                block->exception);
        GOTO(no_catch_merge);
      }
      ELSE {
        IF (__ TaggedEqual(caught_tag, expected_tag)) {
          UnpackWasmException(decoder, block->exception, values);
          GOTO(if_catch, values[0].op);
        }
        END_IF
        GOTO(no_catch_merge);
      }
      END_IF

      BIND(no_catch_merge);
      __ Goto(if_no_catch);

      BIND(if_catch, caught_exception);
      // The first unpacked value is the exception itself in the case of a JS
      // exception.
      values[0].op = caught_exception;
    } else {
      __ Branch(ConditionWithHint(__ TaggedEqual(caught_tag, expected_tag)),
                if_catch, if_no_catch);
      __ Bind(if_catch);
      UnpackWasmException(decoder, block->exception, values);
    }
  }

  // TODO(14108): Optimize in case of unreachable catch block?
  void Delegate(FullDecoder* decoder, uint32_t depth, Control* block) {
    BindBlockAndGeneratePhis(decoder, block->false_or_loop_or_catch_block,
                             nullptr, &block->exception);
    if (depth == decoder->control_depth() - 1) {
      // We just throw to the caller, no need to handle the exception in this
      // frame.
      CallBuiltinThroughJumptable(decoder, Builtin::kWasmRethrow,
                                  {block->exception});
      __ Unreachable();
    } else {
      DCHECK(decoder->control_at(depth)->is_try());
      TSBlock* target_catch =
          decoder->control_at(depth)->false_or_loop_or_catch_block;
      SetupControlFlowEdge(decoder, target_catch, 0, block->exception);
      __ Goto(target_catch);
    }
  }

  void CatchAll(FullDecoder* decoder, Control* block) {
    DCHECK(block->is_try_catchall() || block->is_try_catch());
    DCHECK_EQ(decoder->control_at(0), block);
    BindBlockAndGeneratePhis(decoder, block->false_or_loop_or_catch_block,
                             nullptr, &block->exception);
  }

  V<BigInt> BuildChangeInt64ToBigInt(V<Word64> input) {
    Builtin builtin =
        Is64() ? Builtin::kI64ToBigInt : Builtin::kI32PairToBigInt;
    V<WordPtr> target = __ RelocatableWasmBuiltinCallTarget(builtin);
    CallInterfaceDescriptor interface_descriptor =
        Builtins::CallInterfaceDescriptorFor(builtin);
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetStubCallDescriptor(
            __ graph_zone(),  // zone
            interface_descriptor,
            0,                         // stack parameter count
            CallDescriptor::kNoFlags,  // flags
            Operator::kNoProperties,   // properties
            StubCallMode::kCallWasmRuntimeStub);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo, __ graph_zone());
    if constexpr (Is64()) {
      return __ Call(target, {input}, ts_call_descriptor);
    }
    V<Word32> low_word = __ TruncateWord64ToWord32(input);
    V<Word32> high_word = __ TruncateWord64ToWord32(__ ShiftRightLogical(
        input, __ Word32Constant(32), WordRepresentation::Word64()));
    return __ Call(target, {low_word, high_word}, ts_call_descriptor);
  }

  void AtomicNotify(FullDecoder* decoder, const MemoryAccessImmediate& imm,
                    OpIndex index, OpIndex num_waiters_to_wake, Value* result) {
    V<WordPtr> converted_index;
    compiler::BoundsCheckResult bounds_check_result;
    std::tie(converted_index, bounds_check_result) = CheckBoundsAndAlignment(
        imm.memory, MemoryRepresentation::Int32(), index, imm.offset,
        decoder->position(), compiler::EnforceBoundsCheck::kNeedsBoundsCheck);

    OpIndex effective_offset = __ WordPtrAdd(converted_index, imm.offset);
    OpIndex addr = __ WordPtrAdd(MemStart(imm.mem_index), effective_offset);

    auto sig = FixedSizeSignature<MachineType>::Returns(MachineType::Int32())
                   .Params(MachineType::Pointer(), MachineType::Uint32());
    result->op = CallC(&sig, ExternalReference::wasm_atomic_notify(),
                       {addr, num_waiters_to_wake});
  }

  void AtomicWait(FullDecoder* decoder, WasmOpcode opcode,
                  const MemoryAccessImmediate& imm, OpIndex index,
                  OpIndex expected, V<Word64> timeout, Value* result) {
    V<WordPtr> converted_index;
    compiler::BoundsCheckResult bounds_check_result;
    std::tie(converted_index, bounds_check_result) = CheckBoundsAndAlignment(
        imm.memory,
        opcode == kExprI32AtomicWait ? MemoryRepresentation::Int32()
                                     : MemoryRepresentation::Int64(),
        index, imm.offset, decoder->position(),
        compiler::EnforceBoundsCheck::kNeedsBoundsCheck);

    OpIndex effective_offset = __ WordPtrAdd(converted_index, imm.offset);
    V<BigInt> bigint_timeout = BuildChangeInt64ToBigInt(timeout);

    if (opcode == kExprI32AtomicWait) {
      result->op = CallBuiltinThroughJumptable(
          decoder, Builtin::kWasmI32AtomicWait,
          {__ Word32Constant(imm.memory->index), effective_offset, expected,
           bigint_timeout});
      return;
    }
    DCHECK_EQ(opcode, kExprI64AtomicWait);
    V<BigInt> bigint_expected = BuildChangeInt64ToBigInt(expected);
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmI64AtomicWait,
        {__ Word32Constant(imm.memory->index), effective_offset,
         bigint_expected, bigint_timeout});
  }

  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode, const Value args[],
                const size_t argc, const MemoryAccessImmediate& imm,
                Value* result) {
    if (opcode == WasmOpcode::kExprAtomicNotify) {
      return AtomicNotify(decoder, imm, args[0].op, args[1].op, result);
    }
    if (opcode == WasmOpcode::kExprI32AtomicWait ||
        opcode == WasmOpcode::kExprI64AtomicWait) {
      return AtomicWait(decoder, opcode, imm, args[0].op, args[1].op,
                        args[2].op, result);
    }
    using Binop = compiler::turboshaft::AtomicRMWOp::BinOp;
    enum OpType { kBinop, kLoad, kStore };
    struct AtomicOpInfo {
      OpType op_type;
      // Initialize with a default value, to allow constexpr constructors.
      Binop bin_op = Binop::kAdd;
      RegisterRepresentation result_rep;
      MemoryRepresentation input_rep;

      constexpr AtomicOpInfo(Binop bin_op, RegisterRepresentation result_rep,
                             MemoryRepresentation input_rep)
          : op_type(kBinop),
            bin_op(bin_op),
            result_rep(result_rep),
            input_rep(input_rep) {}

      constexpr AtomicOpInfo(OpType op_type, RegisterRepresentation result_rep,
                             MemoryRepresentation input_rep)
          : op_type(op_type), result_rep(result_rep), input_rep(input_rep) {}

      static constexpr AtomicOpInfo Get(wasm::WasmOpcode opcode) {
        switch (opcode) {
#define CASE_BINOP(OPCODE, BINOP, RESULT, INPUT)                           \
  case WasmOpcode::kExpr##OPCODE:                                          \
    return AtomicOpInfo(Binop::k##BINOP, RegisterRepresentation::RESULT(), \
                        MemoryRepresentation::INPUT());
#define RMW_OPERATION(V)                                          \
  V(I32AtomicAdd, Add, Word32, Uint32)                            \
  V(I32AtomicAdd8U, Add, Word32, Uint8)                           \
  V(I32AtomicAdd16U, Add, Word32, Uint16)                         \
  V(I32AtomicSub, Sub, Word32, Uint32)                            \
  V(I32AtomicSub8U, Sub, Word32, Uint8)                           \
  V(I32AtomicSub16U, Sub, Word32, Uint16)                         \
  V(I32AtomicAnd, And, Word32, Uint32)                            \
  V(I32AtomicAnd8U, And, Word32, Uint8)                           \
  V(I32AtomicAnd16U, And, Word32, Uint16)                         \
  V(I32AtomicOr, Or, Word32, Uint32)                              \
  V(I32AtomicOr8U, Or, Word32, Uint8)                             \
  V(I32AtomicOr16U, Or, Word32, Uint16)                           \
  V(I32AtomicXor, Xor, Word32, Uint32)                            \
  V(I32AtomicXor8U, Xor, Word32, Uint8)                           \
  V(I32AtomicXor16U, Xor, Word32, Uint16)                         \
  V(I32AtomicExchange, Exchange, Word32, Uint32)                  \
  V(I32AtomicExchange8U, Exchange, Word32, Uint8)                 \
  V(I32AtomicExchange16U, Exchange, Word32, Uint16)               \
  V(I32AtomicCompareExchange, CompareExchange, Word32, Uint32)    \
  V(I32AtomicCompareExchange8U, CompareExchange, Word32, Uint8)   \
  V(I32AtomicCompareExchange16U, CompareExchange, Word32, Uint16) \
  V(I64AtomicAdd, Add, Word64, Uint64)                            \
  V(I64AtomicAdd8U, Add, Word64, Uint8)                           \
  V(I64AtomicAdd16U, Add, Word64, Uint16)                         \
  V(I64AtomicAdd32U, Add, Word64, Uint32)                         \
  V(I64AtomicSub, Sub, Word64, Uint64)                            \
  V(I64AtomicSub8U, Sub, Word64, Uint8)                           \
  V(I64AtomicSub16U, Sub, Word64, Uint16)                         \
  V(I64AtomicSub32U, Sub, Word64, Uint32)                         \
  V(I64AtomicAnd, And, Word64, Uint64)                            \
  V(I64AtomicAnd8U, And, Word64, Uint8)                           \
  V(I64AtomicAnd16U, And, Word64, Uint16)                         \
  V(I64AtomicAnd32U, And, Word64, Uint32)                         \
  V(I64AtomicOr, Or, Word64, Uint64)                              \
  V(I64AtomicOr8U, Or, Word64, Uint8)                             \
  V(I64AtomicOr16U, Or, Word64, Uint16)                           \
  V(I64AtomicOr32U, Or, Word64, Uint32)                           \
  V(I64AtomicXor, Xor, Word64, Uint64)                            \
  V(I64AtomicXor8U, Xor, Word64, Uint8)                           \
  V(I64AtomicXor16U, Xor, Word64, Uint16)                         \
  V(I64AtomicXor32U, Xor, Word64, Uint32)                         \
  V(I64AtomicExchange, Exchange, Word64, Uint64)                  \
  V(I64AtomicExchange8U, Exchange, Word64, Uint8)                 \
  V(I64AtomicExchange16U, Exchange, Word64, Uint16)               \
  V(I64AtomicExchange32U, Exchange, Word64, Uint32)               \
  V(I64AtomicCompareExchange, CompareExchange, Word64, Uint64)    \
  V(I64AtomicCompareExchange8U, CompareExchange, Word64, Uint8)   \
  V(I64AtomicCompareExchange16U, CompareExchange, Word64, Uint16) \
  V(I64AtomicCompareExchange32U, CompareExchange, Word64, Uint32)

          RMW_OPERATION(CASE_BINOP)
#undef RMW_OPERATION
#undef CASE
#define CASE_LOAD(OPCODE, RESULT, INPUT)                         \
  case WasmOpcode::kExpr##OPCODE:                                \
    return AtomicOpInfo(kLoad, RegisterRepresentation::RESULT(), \
                        MemoryRepresentation::INPUT());
#define LOAD_OPERATION(V)             \
  V(I32AtomicLoad, Word32, Uint32)    \
  V(I32AtomicLoad16U, Word32, Uint16) \
  V(I32AtomicLoad8U, Word32, Uint8)   \
  V(I64AtomicLoad, Word64, Uint64)    \
  V(I64AtomicLoad32U, Word64, Uint32) \
  V(I64AtomicLoad16U, Word64, Uint16) \
  V(I64AtomicLoad8U, Word64, Uint8)
          LOAD_OPERATION(CASE_LOAD)
#undef LOAD_OPERATION
#undef CASE_LOAD
#define CASE_STORE(OPCODE, INPUT, OUTPUT)                        \
  case WasmOpcode::kExpr##OPCODE:                                \
    return AtomicOpInfo(kStore, RegisterRepresentation::INPUT(), \
                        MemoryRepresentation::OUTPUT());
#define STORE_OPERATION(V)             \
  V(I32AtomicStore, Word32, Uint32)    \
  V(I32AtomicStore16U, Word32, Uint16) \
  V(I32AtomicStore8U, Word32, Uint8)   \
  V(I64AtomicStore, Word64, Uint64)    \
  V(I64AtomicStore32U, Word64, Uint32) \
  V(I64AtomicStore16U, Word64, Uint16) \
  V(I64AtomicStore8U, Word64, Uint8)
          STORE_OPERATION(CASE_STORE)
#undef STORE_OPERATION_OPERATION
#undef CASE_STORE
          default:
            UNREACHABLE();
        }
      }
    };

    AtomicOpInfo info = AtomicOpInfo::Get(opcode);

    V<WordPtr> index;
    compiler::BoundsCheckResult bounds_check_result;
    std::tie(index, bounds_check_result) = CheckBoundsAndAlignment(
        imm.memory, info.input_rep, args[0].op, imm.offset, decoder->position(),
        compiler::EnforceBoundsCheck::kCanOmitBoundsCheck);
    // MemoryAccessKind::kUnaligned is impossible due to explicit aligment
    // check.
    MemoryAccessKind access_kind =
        bounds_check_result == compiler::BoundsCheckResult::kTrapHandler
            ? MemoryAccessKind::kProtected
            : MemoryAccessKind::kNormal;

    if (info.op_type == kBinop) {
      if (info.bin_op == Binop::kCompareExchange) {
        result->op = __ AtomicCompareExchange(
            MemBuffer(imm.memory->index, imm.offset), index, args[1].op,
            args[2].op, info.result_rep, info.input_rep, access_kind);
        return;
      }
      result->op = __ AtomicRMW(MemBuffer(imm.memory->index, imm.offset), index,
                                args[1].op, info.bin_op, info.result_rep,
                                info.input_rep, access_kind);
      return;
    }
    if (info.op_type == kStore) {
      OpIndex value = args[1].op;
      if (info.result_rep == RegisterRepresentation::Word64() &&
          info.input_rep != MemoryRepresentation::Uint64()) {
        value = __ TruncateWord64ToWord32(value);
      }
      __ Store(MemBuffer(imm.memory->index, imm.offset), index, value,
               access_kind == MemoryAccessKind::kProtected
                   ? LoadOp::Kind::Protected().Atomic()
                   : LoadOp::Kind::RawAligned().Atomic(),
               info.input_rep, compiler::kNoWriteBarrier);
      return;
    }
    DCHECK_EQ(info.op_type, kLoad);
    result->op = __ Load(MemBuffer(imm.memory->index, imm.offset), index,
                         access_kind == MemoryAccessKind::kProtected
                             ? LoadOp::Kind::Protected().Atomic()
                             : LoadOp::Kind::RawAligned().Atomic(),
                         info.input_rep, info.result_rep);
  }

  void AtomicFence(FullDecoder* decoder) {
    __ MemoryBarrier(AtomicMemoryOrder::kSeqCst);
  }

  void MemoryInit(FullDecoder* decoder, const MemoryInitImmediate& imm,
                  const Value& dst, const Value& src, const Value& size) {
    V<WordPtr> dst_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(imm.memory.memory->is_memory64, dst.op);
    DCHECK_EQ(size.type, kWasmI32);
    V<Word32> result = CallCStackSlotToInt32(
        ExternalReference::wasm_memory_init(),
        {{__ BitcastTaggedToWord(instance_node_),
          MemoryRepresentation::PointerSized()},
         {__ Word32Constant(imm.memory.index), MemoryRepresentation::Int32()},
         {dst_uintptr, MemoryRepresentation::PointerSized()},
         {src.op, MemoryRepresentation::Int32()},
         {__ Word32Constant(imm.data_segment.index),
          MemoryRepresentation::Int32()},
         {size.op, MemoryRepresentation::Int32()}});
    __ TrapIfNot(result, OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
  }

  void MemoryCopy(FullDecoder* decoder, const MemoryCopyImmediate& imm,
                  const Value& dst, const Value& src, const Value& size) {
    bool is_memory_64 = imm.memory_src.memory->is_memory64;
    DCHECK_EQ(is_memory_64, imm.memory_dst.memory->is_memory64);
    V<WordPtr> dst_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, dst.op);
    V<WordPtr> src_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, src.op);
    V<WordPtr> size_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, size.op);
    V<Word32> result = CallCStackSlotToInt32(
        ExternalReference::wasm_memory_copy(),
        {{__ BitcastTaggedToWord(instance_node_),
          MemoryRepresentation::PointerSized()},
         {__ Word32Constant(imm.memory_dst.index),
          MemoryRepresentation::Int32()},
         {__ Word32Constant(imm.memory_src.index),
          MemoryRepresentation::Int32()},
         {dst_uintptr, MemoryRepresentation::PointerSized()},
         {src_uintptr, MemoryRepresentation::PointerSized()},
         {size_uintptr, MemoryRepresentation::PointerSized()}});
    __ TrapIfNot(result, OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
  }

  void MemoryFill(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                  const Value& dst, const Value& value, const Value& size) {
    bool is_memory_64 = imm.memory->is_memory64;
    V<WordPtr> dst_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, dst.op);
    V<WordPtr> size_uintptr =
        MemoryIndexToUintPtrOrOOBTrap(is_memory_64, size.op);
    V<Word32> result = CallCStackSlotToInt32(
        ExternalReference::wasm_memory_fill(),
        {{__ BitcastTaggedToWord(instance_node_),
          MemoryRepresentation::PointerSized()},
         {__ Word32Constant(imm.index), MemoryRepresentation::Int32()},
         {dst_uintptr, MemoryRepresentation::PointerSized()},
         {value.op, MemoryRepresentation::Int32()},
         {size_uintptr, MemoryRepresentation::PointerSized()}});

    __ TrapIfNot(result, OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
  }

  void DataDrop(FullDecoder* decoder, const IndexImmediate& imm) {
    V<FixedUInt32Array> data_segment_sizes =
        LOAD_IMMUTABLE_INSTANCE_FIELD(instance_node_, DataSegmentSizes,
                                      MemoryRepresentation::TaggedPointer());
    __ Store(data_segment_sizes, __ Word32Constant(0),
             StoreOp::Kind::TaggedBase(), MemoryRepresentation::Int32(),
             compiler::kNoWriteBarrier,
             FixedUInt32Array::kHeaderSize + imm.index * kUInt32Size);
  }

  void TableGet(FullDecoder* decoder, const Value& index, Value* result,
                const IndexImmediate& imm) {
    ValueType element_type = decoder->module_->tables[imm.index].type;
    auto stub = IsSubtypeOf(element_type, kWasmFuncRef, decoder->module_)
                    ? Builtin::kWasmTableGetFuncRef
                    : Builtin::kWasmTableGet;
    result->op = CallBuiltinThroughJumptable(
        decoder, stub, {__ IntPtrConstant(imm.index), index.op});
    result->op = AnnotateResultIfReference(result->op, element_type);
  }

  void TableSet(FullDecoder* decoder, const Value& index, const Value& value,
                const IndexImmediate& imm) {
    ValueType element_type = decoder->module_->tables[imm.index].type;
    auto stub = IsSubtypeOf(element_type, kWasmFuncRef, decoder->module_)
                    ? Builtin::kWasmTableSetFuncRef
                    : Builtin::kWasmTableSet;
    CallBuiltinThroughJumptable(
        decoder, stub, {__ IntPtrConstant(imm.index), index.op, value.op});
  }

  void TableInit(FullDecoder* decoder, const TableInitImmediate& imm,
                 const Value* args) {
    V<Word32> dst = args[0].op;
    V<Word32> src = args[1].op;
    V<Word32> size = args[2].op;
    CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmTableInit,
        {dst, src, size, __ NumberConstant(imm.table.index),
         __ NumberConstant(imm.element_segment.index)});
  }

  void TableCopy(FullDecoder* decoder, const TableCopyImmediate& imm,
                 const Value args[]) {
    V<Word32> dst = args[0].op;
    V<Word32> src = args[1].op;
    V<Word32> size = args[2].op;
    CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmTableCopy,
        {dst, src, size, __ NumberConstant(imm.table_dst.index),
         __ NumberConstant(imm.table_src.index)});
  }

  void TableGrow(FullDecoder* decoder, const IndexImmediate& imm,
                 const Value& value, const Value& delta, Value* result) {
    V<Smi> result_smi = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmTableGrow,
        {__ NumberConstant(imm.index), delta.op, value.op});
    result->op = __ UntagSmi(result_smi);
  }

  void TableFill(FullDecoder* decoder, const IndexImmediate& imm,
                 const Value& start, const Value& value, const Value& count) {
    CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmTableFill,
        {__ NumberConstant(imm.index), start.op, count.op, value.op});
  }

  void TableSize(FullDecoder* decoder, const IndexImmediate& imm,
                 Value* result) {
    V<FixedArray> tables = LOAD_IMMUTABLE_INSTANCE_FIELD(
        instance_node_, Tables, MemoryRepresentation::TaggedPointer());
    auto table =
        V<WasmTableObject>::Cast(__ LoadFixedArrayElement(tables, imm.index));
    V<Smi> size_smi = __ Load(table, LoadOp::Kind::TaggedBase(),
                              MemoryRepresentation::TaggedSigned(),
                              WasmTableObject::kCurrentLengthOffset);
    result->op = __ UntagSmi(size_smi);
  }

  void ElemDrop(FullDecoder* decoder, const IndexImmediate& imm) {
    V<FixedArray> elem_segments = LOAD_IMMUTABLE_INSTANCE_FIELD(
        instance_node_, ElementSegments, MemoryRepresentation::TaggedPointer());
    __ StoreFixedArrayElement(elem_segments, imm.index,
                              LOAD_ROOT(EmptyFixedArray),
                              compiler::kFullWriteBarrier);
  }

  void StructNew(FullDecoder* decoder, const StructIndexImmediate& imm,
                 const Value args[], Value* result) {
    uint32_t field_count = imm.struct_type->field_count();
    base::SmallVector<OpIndex, 8> args_vector(field_count);
    for (uint32_t i = 0; i < field_count; ++i) {
      args_vector[i] = args[i].op;
    }
    result->op = StructNewImpl(imm, args_vector.data());
  }

  void StructNewDefault(FullDecoder* decoder, const StructIndexImmediate& imm,
                        Value* result) {
    uint32_t field_count = imm.struct_type->field_count();
    base::SmallVector<OpIndex, 8> args(field_count);
    for (uint32_t i = 0; i < field_count; i++) {
      ValueType field_type = imm.struct_type->field(i);
      args[i] = DefaultValue(field_type);
    }
    result->op = StructNewImpl(imm, args.data());
  }

  void StructGet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate& field, bool is_signed, Value* result) {
    result->op = __ StructGet(
        struct_object.op, field.struct_imm.struct_type, field.struct_imm.index,
        field.field_imm.index, is_signed,
        struct_object.type.is_nullable() ? compiler::kWithNullCheck
                                         : compiler::kWithoutNullCheck);
  }

  void StructSet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate& field, const Value& field_value) {
    __ StructSet(struct_object.op, field_value.op, field.struct_imm.struct_type,
                 field.struct_imm.index, field.field_imm.index,
                 struct_object.type.is_nullable()
                     ? compiler::kWithNullCheck
                     : compiler::kWithoutNullCheck);
  }

  void ArrayNew(FullDecoder* decoder, const ArrayIndexImmediate& imm,
                const Value& length, const Value& initial_value,
                Value* result) {
    result->op = ArrayNewImpl(decoder, imm.index, imm.array_type, length.op,
                              initial_value.op);
  }

  void ArrayNewDefault(FullDecoder* decoder, const ArrayIndexImmediate& imm,
                       const Value& length, Value* result) {
    OpIndex initial_value = DefaultValue(imm.array_type->element_type());
    result->op = ArrayNewImpl(decoder, imm.index, imm.array_type, length.op,
                              initial_value);
  }

  void ArrayGet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate& imm, const Value& index,
                bool is_signed, Value* result) {
    BoundsCheckArray(array_obj.op, index.op, array_obj.type);
    result->op = __ ArrayGet(array_obj.op, index.op,
                             imm.array_type->element_type(), is_signed);
  }

  void ArraySet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate& imm, const Value& index,
                const Value& value) {
    BoundsCheckArray(array_obj.op, index.op, array_obj.type);
    __ ArraySet(array_obj.op, index.op, value.op,
                imm.array_type->element_type());
  }

  void ArrayLen(FullDecoder* decoder, const Value& array_obj, Value* result) {
    result->op =
        __ ArrayLength(array_obj.op, array_obj.type.is_nullable()
                                         ? compiler::kWithNullCheck
                                         : compiler::kWithoutNullCheck);
  }

  void ArrayCopy(FullDecoder* decoder, const Value& dst, const Value& dst_index,
                 const Value& src, const Value& src_index,
                 const ArrayIndexImmediate& src_imm, const Value& length) {
    BoundsCheckArrayWithLength(dst.op, dst_index.op, length.op,
                               dst.type.is_nullable()
                                   ? compiler::kWithNullCheck
                                   : compiler::kWithoutNullCheck);
    BoundsCheckArrayWithLength(src.op, src_index.op, length.op,
                               src.type.is_nullable()
                                   ? compiler::kWithNullCheck
                                   : compiler::kWithoutNullCheck);

    ValueType element_type = src_imm.array_type->element_type();

    Label<> end(&asm_);
    Label<> builtin(&asm_);
    Label<> reverse(&asm_);

    GOTO_IF(__ Word32Equal(length.op, 0), end);

    // Values determined by test/mjsunit/wasm/array-copy-benchmark.js on x64.
    int array_copy_max_loop_length;
    switch (element_type.kind()) {
      case wasm::kI32:
      case wasm::kI64:
      case wasm::kI8:
      case wasm::kI16:
        array_copy_max_loop_length = 20;
        break;
      case wasm::kF32:
      case wasm::kF64:
        array_copy_max_loop_length = 35;
        break;
      case wasm::kS128:
        array_copy_max_loop_length = 100;
        break;
      case wasm::kRtt:
      case wasm::kRef:
      case wasm::kRefNull:
        array_copy_max_loop_length = 15;
        break;
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }

    GOTO_IF(__ Uint32LessThan(array_copy_max_loop_length, length.op), builtin);
    V<Word32> src_end_index =
        __ Word32Sub(__ Word32Add(src_index.op, length.op), 1);
    GOTO_IF(__ Uint32LessThan(src_index.op, dst_index.op), reverse);

    {
      LoopLabel<Word32, Word32> loop_label(&asm_);
      GOTO(loop_label, src_index.op, dst_index.op);
      LOOP(loop_label, src_index_loop, dst_index_loop) {
        OpIndex value = __ ArrayGet(src.op, src_index_loop, element_type, true);
        __ ArraySet(dst.op, dst_index_loop, value, element_type);

        V<Word32> condition = __ Uint32LessThan(src_index_loop, src_end_index);
        IF (condition) {
          GOTO(loop_label, __ Word32Add(src_index_loop, 1),
               __ Word32Add(dst_index_loop, 1));
        }
        END_IF

        GOTO(end);
      }
    }

    {
      BIND(reverse);
      V<Word32> dst_end_index =
          __ Word32Sub(__ Word32Add(dst_index.op, length.op), 1);
      LoopLabel<Word32, Word32> loop_label(&asm_);
      GOTO(loop_label, src_end_index, dst_end_index);
      LOOP(loop_label, src_index_loop, dst_index_loop) {
        OpIndex value = __ ArrayGet(src.op, src_index_loop, element_type, true);
        __ ArraySet(dst.op, dst_index_loop, value, element_type);

        V<Word32> condition = __ Uint32LessThan(src_index.op, src_index_loop);
        IF (condition) {
          GOTO(loop_label, __ Word32Sub(src_index_loop, 1),
               __ Word32Sub(dst_index_loop, 1));
        }
        END_IF

        GOTO(end);
      }
    }

    {
      BIND(builtin);
      MachineType arg_types[]{
          MachineType::TaggedPointer(), MachineType::TaggedPointer(),
          MachineType::Uint32(),        MachineType::TaggedPointer(),
          MachineType::Uint32(),        MachineType::Uint32()};
      MachineSignature sig(0, 6, arg_types);

      CallC(&sig, ExternalReference::wasm_array_copy(),
            {instance_node_, dst.op, dst_index.op, src.op, src_index.op,
             length.op});
      GOTO(end);
    }

    BIND(end);
  }

  void ArrayFill(FullDecoder* decoder, ArrayIndexImmediate& imm,
                 const Value& array, const Value& index, const Value& value,
                 const Value& length) {
    const bool emit_write_barrier =
        imm.array_type->element_type().is_reference();
    BoundsCheckArrayWithLength(array.op, index.op, length.op,
                               array.type.is_nullable()
                                   ? compiler::kWithNullCheck
                                   : compiler::kWithoutNullCheck);
    ArrayFillImpl(array.op, index.op, value.op, length.op, imm.array_type,
                  emit_write_barrier);
  }

  void ArrayNewFixed(FullDecoder* decoder, const ArrayIndexImmediate& array_imm,
                     const IndexImmediate& length_imm, const Value elements[],
                     Value* result) {
    const wasm::ArrayType* type = array_imm.array_type;
    wasm::ValueType element_type = type->element_type();
    int element_count = length_imm.index;
    // Initialize the array header.
    V<Map> rtt = __ RttCanon(instance_node_, array_imm.index);
    V<HeapObject> array = __ WasmAllocateArray(rtt, element_count, type);
    // Initialize all elements.
    for (int i = 0; i < element_count; i++) {
      __ ArraySet(array, __ Word32Constant(i), elements[i].op, element_type);
    }
    result->op = array;
  }

  void ArrayNewSegment(FullDecoder* decoder,
                       const ArrayIndexImmediate& array_imm,
                       const IndexImmediate& segment_imm, const Value& offset,
                       const Value& length, Value* result) {
    bool is_element = array_imm.array_type->element_type().is_reference();
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmArrayNewSegment,
        {__ Word32Constant(segment_imm.index), offset.op, length.op,
         __ SmiConstant(Smi::FromInt(is_element ? 1 : 0)),
         __ RttCanon(instance_node_, array_imm.index)});
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void ArrayInitSegment(FullDecoder* decoder,
                        const ArrayIndexImmediate& array_imm,
                        const IndexImmediate& segment_imm, const Value& array,
                        const Value& array_index, const Value& segment_offset,
                        const Value& length) {
    bool is_element = array_imm.array_type->element_type().is_reference();
    CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmArrayInitSegment,
        {array_index.op, segment_offset.op, length.op,
         __ SmiConstant(Smi::FromInt(segment_imm.index)),
         __ SmiConstant(Smi::FromInt(is_element ? 1 : 0)), array.op});
  }

  void RefI31(FullDecoder* decoder, const Value& input, Value* result) {
    if constexpr (SmiValuesAre31Bits()) {
      V<Word32> shifted =
          __ Word32ShiftLeft(input.op, kSmiTagSize + kSmiShiftSize);
      if constexpr (Is64()) {
        // The uppermost bits don't matter.
        result->op = __ BitcastWord32ToWord64(shifted);
      } else {
        result->op = shifted;
      }
    } else {
      // Set the topmost bit to sign-extend the second bit. This way,
      // interpretation in JS (if this value escapes there) will be the same as
      // i31.get_s.
      V<WordPtr> input_wordptr = __ ChangeUint32ToUintPtr(input.op);
      result->op = __ WordPtrShiftRightArithmetic(
          __ WordPtrShiftLeft(input_wordptr, kSmiShiftSize + kSmiTagSize + 1),
          1);
    }
  }

  void I31GetS(FullDecoder* decoder, const Value& input, Value* result) {
    V<Tagged> input_non_null = NullCheck(input);
    if constexpr (SmiValuesAre31Bits()) {
      result->op = __ Word32ShiftRightArithmeticShiftOutZeros(
          __ TruncateWordPtrToWord32(__ BitcastTaggedToWord(input_non_null)),
          kSmiTagSize + kSmiShiftSize);
    } else {
      // Topmost bit is already sign-extended.
      result->op = __ TruncateWordPtrToWord32(
          __ WordPtrShiftRightArithmeticShiftOutZeros(
              __ BitcastTaggedToWord(input_non_null),
              kSmiTagSize + kSmiShiftSize));
    }
  }

  void I31GetU(FullDecoder* decoder, const Value& input, Value* result) {
    V<Tagged> input_non_null = NullCheck(input);
    if constexpr (SmiValuesAre31Bits()) {
      result->op = __ Word32ShiftRightLogical(
          __ TruncateWordPtrToWord32(__ BitcastTaggedToWord(input_non_null)),
          kSmiTagSize + kSmiShiftSize);
    } else {
      // Topmost bit is sign-extended, remove it.
      result->op = __ TruncateWordPtrToWord32(__ WordPtrShiftRightLogical(
          __ WordPtrShiftLeft(__ BitcastTaggedToWord(input_non_null), 1),
          kSmiTagSize + kSmiShiftSize + 1));
    }
  }

  void RefTest(FullDecoder* decoder, uint32_t ref_index, const Value& object,
               Value* result, bool null_succeeds) {
    V<Map> rtt = __ RttCanon(instance_node_, ref_index);
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         ref_index, null_succeeds ? kNullable : kNonNullable)};
    result->op = __ WasmTypeCheck(object.op, rtt, config);
  }

  void RefTestAbstract(FullDecoder* decoder, const Value& object, HeapType type,
                       Value* result, bool null_succeeds) {
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         type, null_succeeds ? kNullable : kNonNullable)};
    V<Map> rtt = OpIndex::Invalid();
    result->op = __ WasmTypeCheck(object.op, rtt, config);
  }

  void RefCast(FullDecoder* decoder, uint32_t ref_index, const Value& object,
               Value* result, bool null_succeeds) {
    if (v8_flags.experimental_wasm_assume_ref_cast_succeeds) {
      // TODO(14108): Implement type guards.
      Forward(decoder, object, result);
      return;
    }
    V<Map> rtt = __ RttCanon(instance_node_, ref_index);
    DCHECK_EQ(result->type.is_nullable(), null_succeeds);
    compiler::WasmTypeCheckConfig config{object.type, result->type};
    result->op = __ WasmTypeCast(object.op, rtt, config);
  }

  void RefCastAbstract(FullDecoder* decoder, const Value& object, HeapType type,
                       Value* result, bool null_succeeds) {
    if (v8_flags.experimental_wasm_assume_ref_cast_succeeds) {
      // TODO(14108): Implement type guards.
      Forward(decoder, object, result);
      return;
    }
    // TODO(jkummerow): {type} is redundant.
    DCHECK_IMPLIES(null_succeeds, result->type.is_nullable());
    DCHECK_EQ(type, result->type.heap_type());
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         type, null_succeeds ? kNullable : kNonNullable)};
    V<Map> rtt = OpIndex::Invalid();
    result->op = __ WasmTypeCast(object.op, rtt, config);
  }

  void BrOnCast(FullDecoder* decoder, uint32_t ref_index, const Value& object,
                Value* value_on_branch, uint32_t br_depth, bool null_succeeds) {
    V<Map> rtt = __ RttCanon(instance_node_, ref_index);
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         ref_index, null_succeeds ? kNullable : kNonNullable)};
    return BrOnCastImpl(decoder, rtt, config, object, value_on_branch, br_depth,
                        null_succeeds);
  }

  void BrOnCastAbstract(FullDecoder* decoder, const Value& object,
                        HeapType type, Value* value_on_branch,
                        uint32_t br_depth, bool null_succeeds) {
    V<Map> rtt = OpIndex::Invalid();
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         type, null_succeeds ? kNullable : kNonNullable)};
    return BrOnCastImpl(decoder, rtt, config, object, value_on_branch, br_depth,
                        null_succeeds);
  }

  void BrOnCastFail(FullDecoder* decoder, uint32_t ref_index,
                    const Value& object, Value* value_on_fallthrough,
                    uint32_t br_depth, bool null_succeeds) {
    V<Map> rtt = __ RttCanon(instance_node_, ref_index);
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         ref_index, null_succeeds ? kNullable : kNonNullable)};
    return BrOnCastFailImpl(decoder, rtt, config, object, value_on_fallthrough,
                            br_depth, null_succeeds);
  }

  void BrOnCastFailAbstract(FullDecoder* decoder, const Value& object,
                            HeapType type, Value* value_on_fallthrough,
                            uint32_t br_depth, bool null_succeeds) {
    V<Map> rtt = OpIndex::Invalid();
    compiler::WasmTypeCheckConfig config{
        object.type, ValueType::RefMaybeNull(
                         type, null_succeeds ? kNullable : kNonNullable)};
    return BrOnCastFailImpl(decoder, rtt, config, object, value_on_fallthrough,
                            br_depth, null_succeeds);
  }

  void StringNewWtf8(FullDecoder* decoder, const MemoryIndexImmediate& memory,
                     const unibrow::Utf8Variant variant, const Value& offset,
                     const Value& size, Value* result) {
    V<Object> memory_smi = __ SmiConstant(Smi::FromInt(memory.index));
    V<Object> variant_smi =
        __ SmiConstant(Smi::FromInt(static_cast<int>(variant)));
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringNewWtf8,
        {offset.op, size.op, memory_smi, variant_smi},
        Operator::kNoDeopt | Operator::kNoThrow);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  // TODO(jkummerow): This check would be more elegant if we made
  // {ArrayNewSegment} a high-level node that's lowered later.
  bool IsArrayNewSegment(V<Object> array) {
    const CallOp* call = __ output_graph().Get(array).TryCast<CallOp>();
    if (call == nullptr) return false;
    int64_t stub_id{};
    if (!OperationMatcher(__ output_graph())
             .MatchWasmStubCallConstant(call->callee(), &stub_id)) {
      return false;
    }
    DCHECK_LT(stub_id, static_cast<int64_t>(Builtin::kFirstBytecodeHandler));
    return stub_id == static_cast<int64_t>(Builtin::kWasmArrayNewSegment);
  }

  V<Tagged> StringNewWtf8ArrayImpl(FullDecoder* decoder,
                                   const unibrow::Utf8Variant variant,
                                   const Value& array, const Value& start,
                                   const Value& end) {
    // Special case: shortcut a sequence "array from data segment" + "string
    // from wtf8 array" to directly create a string from the segment.
    V<Tagged> call;
    if (IsArrayNewSegment(array.op)) {
      // We can only pass 3 untagged parameters to the builtin (on 32-bit
      // platforms). The segment index is easy to tag: if it validated, it must
      // be in Smi range.
      const Operation& array_new = __ output_graph().Get(array.op);
      OpIndex segment_index = array_new.input(1);
      int32_t index_val;
      OperationMatcher(__ output_graph())
          .MatchIntegralWord32Constant(segment_index, &index_val);
      V<Object> index_smi = __ SmiConstant(Smi::FromInt(index_val));
      // Arbitrary choice for the second tagged parameter: the segment offset.
      OpIndex segment_offset = array_new.input(2);
      __ TrapIfNot(
          __ Uint32LessThan(segment_offset, __ Word32Constant(Smi::kMaxValue)),
          OpIndex::Invalid(), TrapId::kTrapDataSegmentOutOfBounds);
      V<Object> offset_smi = __ TagSmi(segment_offset);
      OpIndex segment_length = array_new.input(3);
      call = CallBuiltinThroughJumptable(
          decoder, Builtin::kWasmStringFromDataSegment,
          {segment_length, start.op, end.op, index_smi, offset_smi},
          Operator::kNoDeopt | Operator::kNoThrow);
    } else {
      // Regular path if the shortcut wasn't taken.
      call = CallBuiltinThroughJumptable(
          decoder, Builtin::kWasmStringNewWtf8Array,
          {start.op, end.op, NullCheck(array),
           __ SmiConstant(Smi::FromInt(static_cast<int32_t>(variant)))},
          Operator::kNoDeopt | Operator::kNoThrow);
    }
    bool null_on_invalid = variant == unibrow::Utf8Variant::kUtf8NoTrap;
    return __ AnnotateWasmType(
        call, null_on_invalid ? kWasmStringRef : kWasmRefString);
  }

  void StringNewWtf8Array(FullDecoder* decoder,
                          const unibrow::Utf8Variant variant,
                          const Value& array, const Value& start,
                          const Value& end, Value* result) {
    result->op = StringNewWtf8ArrayImpl(decoder, variant, array, start, end);
  }

  void StringNewWtf16(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                      const Value& offset, const Value& size, Value* result) {
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringNewWtf16,
        {__ Word32Constant(imm.index), offset.op, size.op},
        Operator::kNoDeopt | Operator::kNoThrow);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void StringNewWtf16Array(FullDecoder* decoder, const Value& array,
                           const Value& start, const Value& end,
                           Value* result) {
    result->op =
        CallBuiltinThroughJumptable(decoder, Builtin::kWasmStringNewWtf16Array,
                                    {NullCheck(array), start.op, end.op},
                                    Operator::kNoDeopt | Operator::kNoThrow);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void StringConst(FullDecoder* decoder, const StringConstImmediate& imm,
                   Value* result) {
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringConst, {__ Word32Constant(imm.index)},
        Operator::kNoDeopt | Operator::kNoThrow);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void StringMeasureWtf8(FullDecoder* decoder,
                         const unibrow::Utf8Variant variant, const Value& str,
                         Value* result) {
    Builtin builtin;
    switch (variant) {
      case unibrow::Utf8Variant::kUtf8:
        builtin = Builtin::kWasmStringMeasureUtf8;
        break;
      case unibrow::Utf8Variant::kLossyUtf8:
      case unibrow::Utf8Variant::kWtf8:
        builtin = Builtin::kWasmStringMeasureWtf8;
        break;
      case unibrow::Utf8Variant::kUtf8NoTrap:
        UNREACHABLE();
    }
    result->op = CallBuiltinThroughJumptable(decoder, builtin, {NullCheck(str)},
                                             Operator::kEliminatable);
  }

  void StringMeasureWtf16(FullDecoder* decoder, const Value& str,
                          Value* result) {
    result->op = __ Load(NullCheck(str), LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::Uint32(), String::kLengthOffset);
  }

  void StringEncodeWtf8(FullDecoder* decoder,
                        const MemoryIndexImmediate& memory,
                        const unibrow::Utf8Variant variant, const Value& str,
                        const Value& offset, Value* result) {
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringEncodeWtf8,
        {NullCheck(str), offset.op, __ SmiConstant(Smi::FromInt(memory.index)),
         __ SmiConstant(Smi::FromInt(static_cast<int32_t>(variant)))},
        Operator::kNoDeopt | Operator::kNoThrow);
  }

  void StringEncodeWtf8Array(FullDecoder* decoder,
                             const unibrow::Utf8Variant variant,
                             const Value& str, const Value& array,
                             const Value& start, Value* result) {
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringEncodeWtf8Array,
        {NullCheck(str), NullCheck(array), start.op,
         __ SmiConstant(Smi::FromInt(static_cast<int32_t>(variant)))},
        Operator::kNoDeopt | Operator::kNoThrow);
  }

  void StringEncodeWtf16(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                         const Value& str, const Value& offset, Value* result) {
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringEncodeWtf16,
        {NullCheck(str), offset.op,
         __ SmiConstant(Smi::FromInt(static_cast<int32_t>(imm.index)))},
        Operator::kNoDeopt | Operator::kNoThrow);
  }

  void StringEncodeWtf16Array(FullDecoder* decoder, const Value& str,
                              const Value& array, const Value& start,
                              Value* result) {
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringEncodeWtf16Array,
        {NullCheck(str), NullCheck(array), start.op},
        Operator::kNoDeopt | Operator::kNoThrow);
  }

  void StringConcat(FullDecoder* decoder, const Value& head, const Value& tail,
                    Value* result) {
    V<HeapObject> native_context = LOAD_IMMUTABLE_INSTANCE_FIELD(
        instance_node_, NativeContext, MemoryRepresentation::TaggedPointer());
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kStringAdd_CheckNone,
        {NullCheck(head), NullCheck(tail), native_context},
        Operator::kNoDeopt | Operator::kNoThrow);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  V<Word32> StringEqImpl(FullDecoder* decoder, V<Tagged> a, V<Tagged> b,
                         ValueType a_type, ValueType b_type) {
    Label<Word32> done(&asm_);
    // Covers "identical string pointer" and "both are null" cases.
    GOTO_IF(__ TaggedEqual(a, b), done, __ Word32Constant(1));
    if (a_type.is_nullable()) {
      GOTO_IF(__ IsNull(a, a_type), done, __ Word32Constant(0));
    }
    if (b_type.is_nullable()) {
      GOTO_IF(__ IsNull(b, b_type), done, __ Word32Constant(0));
    }
    // TODO(jkummerow): Call Builtin::kStringEqual directly.
    GOTO(done, CallBuiltinThroughJumptable(decoder, Builtin::kWasmStringEqual,
                                           {a, b}, Operator::kEliminatable));
    BIND(done, eq_result);
    return eq_result;
  }

  void StringEq(FullDecoder* decoder, const Value& a, const Value& b,
                Value* result) {
    result->op = StringEqImpl(decoder, a.op, b.op, a.type, b.type);
  }

  void StringIsUSVSequence(FullDecoder* decoder, const Value& str,
                           Value* result) {
    result->op =
        CallBuiltinThroughJumptable(decoder, Builtin::kWasmStringIsUSVSequence,
                                    {NullCheck(str)}, Operator::kEliminatable);
  }

  void StringAsWtf8(FullDecoder* decoder, const Value& str, Value* result) {
    result->op =
        CallBuiltinThroughJumptable(decoder, Builtin::kWasmStringAsWtf8,
                                    {NullCheck(str)}, Operator::kEliminatable);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void StringViewWtf8Advance(FullDecoder* decoder, const Value& view,
                             const Value& pos, const Value& bytes,
                             Value* result) {
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringViewWtf8Advance,
        {NullCheck(view), pos.op, bytes.op}, Operator::kEliminatable);
  }

  void StringViewWtf8Encode(FullDecoder* decoder,
                            const MemoryIndexImmediate& memory,
                            const unibrow::Utf8Variant variant,
                            const Value& view, const Value& addr,
                            const Value& pos, const Value& bytes,
                            Value* next_pos, Value* bytes_written) {
    OpIndex result = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringViewWtf8Encode,
        {addr.op, pos.op, bytes.op, NullCheck(view),
         __ SmiConstant(Smi::FromInt(memory.index)),
         __ SmiConstant(Smi::FromInt(static_cast<int32_t>(variant)))},
        Operator::kNoDeopt | Operator::kNoThrow);
    next_pos->op = __ Projection(result, 0, RepresentationFor(next_pos->type));
    bytes_written->op =
        __ Projection(result, 1, RepresentationFor(bytes_written->type));
  }

  void StringViewWtf8Slice(FullDecoder* decoder, const Value& view,
                           const Value& start, const Value& end,
                           Value* result) {
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringViewWtf8Slice,
        {NullCheck(view), start.op, end.op}, Operator::kEliminatable);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void StringAsWtf16(FullDecoder* decoder, const Value& str, Value* result) {
    result->op = __ StringAsWtf16(NullCheck(str));
  }

  V<Word32> GetCodeUnitImpl(FullDecoder* decoder, V<Tagged> string,
                            V<Word32> offset) {
    OpIndex prepare = __ StringPrepareForGetCodeUnit(string);
    V<Tagged> base =
        __ Projection(prepare, 0, RegisterRepresentation::Tagged());
    V<WordPtr> base_offset =
        __ Projection(prepare, 1, RegisterRepresentation::PointerSized());
    V<Word32> charwidth_shift =
        __ Projection(prepare, 2, RegisterRepresentation::Word32());

    // Bounds check.
    V<Word32> length = __ template LoadField<Word32>(
        string, compiler::AccessBuilder::ForStringLength());
    __ TrapIfNot(__ Uint32LessThan(offset, length), OpIndex::Invalid(),
                 TrapId::kTrapStringOffsetOutOfBounds);

    Label<> onebyte(&asm_);
    // TODO(14108): The bailout label should be deferred.
    Label<> bailout(&asm_);
    Label<Word32> done(&asm_);
    GOTO_IF(
        __ Word32Equal(charwidth_shift, compiler::kCharWidthBailoutSentinel),
        bailout);
    GOTO_IF(__ Word32Equal(charwidth_shift, 0), onebyte);

    // Two-byte.
    V<WordPtr> object_offset = __ WordPtrAdd(
        __ WordPtrMul(__ ChangeInt32ToIntPtr(offset), 2), base_offset);
    // Bitcast the tagged to a wordptr as the offset already contains the
    // kHeapObjectTag handling. Furthermore, in case of external strings the
    // tagged value is a smi 0, which doesn't really encode a tagged load.
    V<WordPtr> base_ptr = __ BitcastTaggedToWord(base);
    V<Word32> result_value =
        __ Load(base_ptr, object_offset, LoadOp::Kind::RawAligned().Immutable(),
                MemoryRepresentation::Uint16());
    GOTO(done, result_value);

    // One-byte.
    BIND(onebyte);
    object_offset = __ WordPtrAdd(__ ChangeInt32ToIntPtr(offset), base_offset);
    // Bitcast the tagged to a wordptr as the offset already contains the
    // kHeapObjectTag handling. Furthermore, in case of external strings the
    // tagged value is a smi 0, which doesn't really encode a tagged load.
    base_ptr = __ BitcastTaggedToWord(base);
    result_value =
        __ Load(base_ptr, object_offset, LoadOp::Kind::RawAligned().Immutable(),
                MemoryRepresentation::Uint8());
    GOTO(done, result_value);

    BIND(bailout);
    GOTO(done, CallBuiltinThroughJumptable(
                   decoder, Builtin::kWasmStringViewWtf16GetCodeUnit,
                   {string, offset}, Operator::kPure));

    BIND(done, final_result);
    // Make sure the original string is kept alive as long as we're operating
    // on pointers extracted from it (otherwise e.g. external strings' resources
    // might get freed prematurely).
    __ Retain(string);
    return final_result;
  }

  void StringViewWtf16GetCodeUnit(FullDecoder* decoder, const Value& view,
                                  const Value& pos, Value* result) {
    result->op = GetCodeUnitImpl(decoder, NullCheck(view), pos.op);
  }

  V<Word32> StringCodePointAt(FullDecoder* decoder, V<Tagged> string,
                              V<Word32> offset) {
    OpIndex prepare = __ StringPrepareForGetCodeUnit(string);
    V<Tagged> base =
        __ Projection(prepare, 0, RegisterRepresentation::Tagged());
    V<WordPtr> base_offset =
        __ Projection(prepare, 1, RegisterRepresentation::PointerSized());
    V<Word32> charwidth_shift =
        __ Projection(prepare, 2, RegisterRepresentation::Word32());

    // Bounds check.
    V<Word32> length = __ template LoadField<Word32>(
        string, compiler::AccessBuilder::ForStringLength());
    __ TrapIfNot(__ Uint32LessThan(offset, length), OpIndex::Invalid(),
                 TrapId::kTrapStringOffsetOutOfBounds);

    Label<> onebyte(&asm_);
    Label<> bailout(&asm_);
    Label<Word32> done(&asm_);
    GOTO_IF(
        __ Word32Equal(charwidth_shift, compiler::kCharWidthBailoutSentinel),
        bailout);
    GOTO_IF(__ Word32Equal(charwidth_shift, 0), onebyte);

    // Two-byte.
    V<WordPtr> object_offset = __ WordPtrAdd(
        __ WordPtrMul(__ ChangeInt32ToIntPtr(offset), 2), base_offset);
    // Bitcast the tagged to a wordptr as the offset already contains the
    // kHeapObjectTag handling. Furthermore, in case of external strings the
    // tagged value is a smi 0, which doesn't really encode a tagged load.
    V<WordPtr> base_ptr = __ BitcastTaggedToWord(base);
    V<Word32> lead =
        __ Load(base_ptr, object_offset, LoadOp::Kind::RawAligned().Immutable(),
                MemoryRepresentation::Uint16());
    V<Word32> is_lead_surrogate =
        __ Word32Equal(__ Word32BitwiseAnd(lead, 0xFC00), 0xD800);
    GOTO_IF_NOT(is_lead_surrogate, done, lead);
    V<Word32> trail_offset = __ Word32Add(offset, 1);
    GOTO_IF_NOT(__ Uint32LessThan(trail_offset, length), done, lead);
    V<Word32> trail = __ Load(
        base_ptr, __ WordPtrAdd(object_offset, __ IntPtrConstant(2)),
        LoadOp::Kind::RawAligned().Immutable(), MemoryRepresentation::Uint16());
    V<Word32> is_trail_surrogate =
        __ Word32Equal(__ Word32BitwiseAnd(trail, 0xFC00), 0xDC00);
    GOTO_IF_NOT(is_trail_surrogate, done, lead);
    V<Word32> surrogate_bias =
        __ Word32Constant(0x10000 - (0xD800 << 10) - 0xDC00);
    V<Word32> result = __ Word32Add(__ Word32ShiftLeft(lead, 10),
                                    __ Word32Add(trail, surrogate_bias));
    GOTO(done, result);

    // One-byte.
    BIND(onebyte);
    object_offset = __ WordPtrAdd(__ ChangeInt32ToIntPtr(offset), base_offset);
    // Bitcast the tagged to a wordptr as the offset already contains the
    // kHeapObjectTag handling. Furthermore, in case of external strings the
    // tagged value is a smi 0, which doesn't really encode a tagged load.
    base_ptr = __ BitcastTaggedToWord(base);
    result =
        __ Load(base_ptr, object_offset, LoadOp::Kind::RawAligned().Immutable(),
                MemoryRepresentation::Uint8());
    GOTO(done, result);

    BIND(bailout);
    GOTO(done,
         CallBuiltinThroughJumptable(decoder, Builtin::kWasmStringCodePointAt,
                                     {string, offset}, Operator::kPure));

    BIND(done, final_result);
    // Make sure the original string is kept alive as long as we're operating
    // on pointers extracted from it (otherwise e.g. external strings' resources
    // might get freed prematurely).
    __ Retain(string);
    return final_result;
  }

  void StringViewWtf16Encode(FullDecoder* decoder,
                             const MemoryIndexImmediate& imm, const Value& view,
                             const Value& offset, const Value& pos,
                             const Value& codeunits, Value* result) {
    V<Tagged> string = NullCheck(view);
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringViewWtf16Encode,
        {offset.op, pos.op, codeunits.op, string,
         __ SmiConstant(Smi::FromInt(imm.index))},
        Operator::kNoDeopt | Operator::kNoThrow);
  }

  void StringViewWtf16Slice(FullDecoder* decoder, const Value& view,
                            const Value& start, const Value& end,
                            Value* result) {
    V<Tagged> string = NullCheck(view);
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringViewWtf16Slice, {string, start.op, end.op},
        Operator::kEliminatable);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void StringAsIter(FullDecoder* decoder, const Value& str, Value* result) {
    V<Tagged> string = NullCheck(str);
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringAsIter, {string}, Operator::kEliminatable);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void StringViewIterNext(FullDecoder* decoder, const Value& view,
                          Value* result) {
    V<Tagged> string = NullCheck(view);
    result->op =
        CallBuiltinThroughJumptable(decoder, Builtin::kWasmStringViewIterNext,
                                    {string}, Operator::kEliminatable);
  }

  void StringViewIterAdvance(FullDecoder* decoder, const Value& view,
                             const Value& codepoints, Value* result) {
    V<Tagged> string = NullCheck(view);
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringViewIterAdvance, {string, codepoints.op},
        Operator::kEliminatable);
  }

  void StringViewIterRewind(FullDecoder* decoder, const Value& view,
                            const Value& codepoints, Value* result) {
    V<Tagged> string = NullCheck(view);
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringViewIterRewind, {string, codepoints.op},
        Operator::kEliminatable);
  }

  void StringViewIterSlice(FullDecoder* decoder, const Value& view,
                           const Value& codepoints, Value* result) {
    V<Tagged> string = NullCheck(view);
    result->op = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmStringViewIterSlice, {string, codepoints.op},
        Operator::kEliminatable);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void StringCompare(FullDecoder* decoder, const Value& lhs, const Value& rhs,
                     Value* result) {
    V<Tagged> lhs_val = NullCheck(lhs);
    V<Tagged> rhs_val = NullCheck(rhs);
    result->op = __ UntagSmi(CallBuiltinThroughJumptable(
        decoder, Builtin::kStringCompare, {lhs_val, rhs_val},
        Operator::kEliminatable));
  }

  void StringFromCodePoint(FullDecoder* decoder, const Value& code_point,
                           Value* result) {
    result->op =
        CallBuiltinThroughJumptable(decoder, Builtin::kWasmStringFromCodePoint,
                                    {code_point.op}, Operator::kEliminatable);
    result->op = __ AnnotateWasmType(result->op, result->type);
  }

  void StringHash(FullDecoder* decoder, const Value& string, Value* result) {
    V<Tagged> string_val = NullCheck(string);

    Label<> runtime_label(&Asm());
    Label<Word32> end_label(&Asm());

    V<Word32> raw_hash =
        __ Load(string_val, LoadOp::Kind::TaggedBase(),
                MemoryRepresentation::Int32(), Name::kRawHashFieldOffset);
    V<Word32> hash_not_computed_mask =
        __ Word32Constant(static_cast<int32_t>(Name::kHashNotComputedMask));
    static_assert(Name::HashFieldTypeBits::kShift == 0);
    V<Word32> hash_not_computed =
        __ Word32BitwiseAnd(raw_hash, hash_not_computed_mask);
    GOTO_IF(hash_not_computed, runtime_label);

    // Fast path if hash is already computed: Decode raw hash value.
    static_assert(Name::HashBits::kLastUsedBit == kBitsPerInt - 1);
    V<Word32> hash = __ Word32ShiftRightLogical(
        raw_hash, static_cast<int32_t>(Name::HashBits::kShift));
    GOTO(end_label, hash);

    BIND(runtime_label);
    V<Word32> hash_runtime =
        CallBuiltinThroughJumptable(decoder, Builtin::kWasmStringHash,
                                    {string_val}, Operator::kEliminatable);
    GOTO(end_label, hash_runtime);

    BIND(end_label, hash_val);
    result->op = hash_val;
  }

  void Forward(FullDecoder* decoder, const Value& from, Value* to) {
    to->op = from.op;
  }

  bool did_bailout() { return did_bailout_; }

 private:
  // Holds phi inputs for a specific block. These include SSA values as well as
  // stack merge values.
  struct BlockPhis {
    // The first vector corresponds to all inputs of the first phi etc.
    std::vector<std::vector<OpIndex>> phi_inputs;
    std::vector<ValueType> phi_types;
    std::vector<OpIndex> incoming_exception;

    explicit BlockPhis(int total_arity)
        : phi_inputs(total_arity), phi_types(total_arity) {}
    BlockPhis() : BlockPhis(0) {}
  };

  enum class CheckForException { kNo, kCatchInThisFrame, kCatchInParentFrame };

  void Bailout(FullDecoder* decoder) {
    decoder->errorf("Unsupported Turboshaft operation: %s",
                    decoder->SafeOpcodeNameAt(decoder->pc()));
    did_bailout_ = true;
  }

  // Perform a null check if the input type is nullable.
  V<Tagged> NullCheck(const Value& value,
                      TrapId trap_id = TrapId::kTrapNullDereference) {
    OpIndex not_null_value = value.op;
    if (value.type.is_nullable()) {
      not_null_value = __ AssertNotNull(value.op, value.type, trap_id);
    }
    return not_null_value;
  }

  // Creates a new block, initializes a {BlockPhis} for it, and registers it
  // with block_phis_. We pass a {merge} only if we later need to recover values
  // for that merge.
  TSBlock* NewBlockWithPhis(FullDecoder* decoder, Merge<Value>* merge) {
    TSBlock* block = __ NewBlock();
    BlockPhis block_phis(decoder->num_locals() +
                         (merge != nullptr ? merge->arity : 0));
    for (uint32_t i = 0; i < decoder->num_locals(); i++) {
      block_phis.phi_types[i] = decoder->local_type(i);
    }
    if (merge != nullptr) {
      for (uint32_t i = 0; i < merge->arity; i++) {
        block_phis.phi_types[decoder->num_locals() + i] = (*merge)[i].type;
      }
    }
    block_phis_.emplace(block, std::move(block_phis));
    return block;
  }

  // Sets up a control flow edge from the current SSA environment and a stack to
  // {block}. The stack is {stack_values} if present, otherwise the current
  // decoder stack.
  void SetupControlFlowEdge(FullDecoder* decoder, TSBlock* block,
                            uint32_t drop_values = 0,
                            V<Tagged> exception = OpIndex::Invalid(),
                            Merge<Value>* stack_values = nullptr) {
    if (__ current_block() == nullptr) return;
    // It is guaranteed that this element exists.
    BlockPhis& phis_for_block = block_phis_.find(block)->second;
    uint32_t merge_arity =
        static_cast<uint32_t>(phis_for_block.phi_inputs.size()) -
        decoder->num_locals();
    for (size_t i = 0; i < ssa_env_.size(); i++) {
      phis_for_block.phi_inputs[i].emplace_back(ssa_env_[i]);
    }
    // We never drop values from an explicit merge.
    DCHECK_IMPLIES(stack_values != nullptr, drop_values == 0);
    Value* stack_base = merge_arity == 0 ? nullptr
                        : stack_values != nullptr
                            ? &(*stack_values)[0]
                            : decoder->stack_value(merge_arity + drop_values);
    for (size_t i = 0; i < merge_arity; i++) {
      DCHECK(stack_base[i].op.valid());
      phis_for_block.phi_inputs[decoder->num_locals() + i].emplace_back(
          stack_base[i].op);
    }
    if (exception.valid()) {
      phis_for_block.incoming_exception.push_back(exception);
    }
  }

  OpIndex MaybePhi(std::vector<OpIndex>& elements, ValueType type) {
    if (elements.empty()) return OpIndex::Invalid();
    for (size_t i = 1; i < elements.size(); i++) {
      if (elements[i] != elements[0]) {
        return __ Phi(base::VectorOf(elements), RepresentationFor(type));
      }
    }
    return elements[0];
  }

  // Binds a block, initializes phis for its SSA environment from its entry in
  // {block_phis_}, and sets values to its {merge} (if available) from the
  // its entry in {block_phis_}.
  void BindBlockAndGeneratePhis(FullDecoder* decoder, TSBlock* tsblock,
                                Merge<Value>* merge,
                                OpIndex* exception = nullptr) {
    __ Bind(tsblock);
    BlockPhis& block_phis = block_phis_.at(tsblock);
    for (uint32_t i = 0; i < decoder->num_locals(); i++) {
      ssa_env_[i] = MaybePhi(block_phis.phi_inputs[i], block_phis.phi_types[i]);
    }
    DCHECK_EQ(decoder->num_locals() + (merge != nullptr ? merge->arity : 0),
              block_phis.phi_inputs.size());
    if (merge != nullptr) {
      for (uint32_t i = 0; i < merge->arity; i++) {
        (*merge)[i].op =
            MaybePhi(block_phis.phi_inputs[decoder->num_locals() + i],
                     block_phis.phi_types[decoder->num_locals() + i]);
      }
    }
    DCHECK_IMPLIES(exception == nullptr, block_phis.incoming_exception.empty());
    if (exception != nullptr && !exception->valid()) {
      *exception = MaybePhi(block_phis.incoming_exception, kWasmExternRef);
    }
    block_phis_.erase(tsblock);
  }

  OpIndex DefaultValue(ValueType type) {
    switch (type.kind()) {
      case kI8:
      case kI16:
      case kI32:
        return __ Word32Constant(int32_t{0});
      case kI64:
        return __ Word64Constant(int64_t{0});
      case kF32:
        return __ Float32Constant(0.0f);
      case kF64:
        return __ Float64Constant(0.0);
      case kRefNull:
        return __ Null(type);
      case kS128: {
        uint8_t value[kSimd128Size] = {};
        return __ Simd128Constant(value);
      }
      case kVoid:
      case kRtt:
      case kRef:
      case kBottom:
        UNREACHABLE();
    }
  }

  RegisterRepresentation RepresentationFor(ValueType type) {
    switch (type.kind()) {
      case kI8:
      case kI16:
      case kI32:
        return RegisterRepresentation::Word32();
      case kI64:
        return RegisterRepresentation::Word64();
      case kF32:
        return RegisterRepresentation::Float32();
      case kF64:
        return RegisterRepresentation::Float64();
      case kRefNull:
      case kRef:
        return RegisterRepresentation::Tagged();
      case kS128:
        return RegisterRepresentation::Simd128();
      case kVoid:
      case kRtt:
      case kBottom:
        UNREACHABLE();
    }
  }

  V<Word64> ExtractTruncationProjections(OpIndex truncated) {
    V<Word64> result =
        __ Projection(truncated, 0, RegisterRepresentation::Word64());
    V<Word32> check =
        __ Projection(truncated, 1, RegisterRepresentation::Word32());
    __ TrapIf(__ Word32Equal(check, 0), OpIndex::Invalid(),
              TrapId::kTrapFloatUnrepresentable);
    return result;
  }

  std::pair<OpIndex, V<Word32>> BuildCCallForFloatConversion(
      OpIndex arg, MemoryRepresentation float_type,
      ExternalReference ccall_ref) {
    uint8_t slot_size = MemoryRepresentation::Int64().SizeInBytes();
    V<WordPtr> stack_slot = __ StackSlot(slot_size, slot_size);
    __ Store(stack_slot, arg, StoreOp::Kind::RawAligned(), float_type,
             compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Int32(), MachineType::Pointer()};
    MachineSignature sig(1, 1, reps);
    V<Word32> overflow = CallC(&sig, ccall_ref, stack_slot);
    return {stack_slot, overflow};
  }

  OpIndex BuildCcallConvertFloat(OpIndex arg, MemoryRepresentation float_type,
                                 ExternalReference ccall_ref) {
    auto [stack_slot, overflow] =
        BuildCCallForFloatConversion(arg, float_type, ccall_ref);
    __ TrapIf(__ Word32Equal(overflow, 0), OpIndex::Invalid(),
              compiler::TrapId::kTrapFloatUnrepresentable);
    MemoryRepresentation int64 = MemoryRepresentation::Int64();
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(), int64);
  }

  OpIndex BuildCcallConvertFloatSat(OpIndex arg,
                                    MemoryRepresentation float_type,
                                    ExternalReference ccall_ref,
                                    bool is_signed) {
    MemoryRepresentation int64 = MemoryRepresentation::Int64();
    uint8_t slot_size = int64.SizeInBytes();
    V<WordPtr> stack_slot = __ StackSlot(slot_size, slot_size);
    __ Store(stack_slot, arg, StoreOp::Kind::RawAligned(), float_type,
             compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Pointer()};
    MachineSignature sig(0, 1, reps);
    CallC(&sig, ccall_ref, stack_slot);
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(), int64);
  }

  OpIndex BuildIntToFloatConversionInstruction(
      OpIndex input, ExternalReference ccall_ref,
      MemoryRepresentation input_representation,
      MemoryRepresentation result_representation) {
    uint8_t slot_size = std::max(input_representation.SizeInBytes(),
                                 result_representation.SizeInBytes());
    V<WordPtr> stack_slot = __ StackSlot(slot_size, slot_size);
    __ Store(stack_slot, input, StoreOp::Kind::RawAligned(),
             input_representation, compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Pointer()};
    MachineSignature sig(0, 1, reps);
    CallC(&sig, ccall_ref, stack_slot);
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(),
                   result_representation);
  }

  OpIndex BuildDiv64Call(OpIndex lhs, OpIndex rhs, ExternalReference ccall_ref,
                         wasm::TrapId trap_zero) {
    MemoryRepresentation int64_rep = MemoryRepresentation::Int64();
    V<WordPtr> stack_slot =
        __ StackSlot(2 * int64_rep.SizeInBytes(), int64_rep.SizeInBytes());
    __ Store(stack_slot, lhs, StoreOp::Kind::RawAligned(), int64_rep,
             compiler::WriteBarrierKind::kNoWriteBarrier);
    __ Store(stack_slot, rhs, StoreOp::Kind::RawAligned(), int64_rep,
             compiler::WriteBarrierKind::kNoWriteBarrier,
             int64_rep.SizeInBytes());

    MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
    MachineSignature sig(1, 1, sig_types);
    OpIndex rc = CallC(&sig, ccall_ref, stack_slot);
    __ TrapIf(__ Word32Equal(rc, 0), OpIndex::Invalid(), trap_zero);
    __ TrapIf(__ Word32Equal(rc, -1), OpIndex::Invalid(),
              TrapId::kTrapDivUnrepresentable);
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(), int64_rep);
  }

  OpIndex UnOpImpl(WasmOpcode opcode, OpIndex arg,
                   ValueType input_type /* for ref.is_null only*/) {
    switch (opcode) {
      case kExprI32Eqz:
        return __ Word32Equal(arg, 0);
      case kExprF32Abs:
        return __ Float32Abs(arg);
      case kExprF32Neg:
        return __ Float32Negate(arg);
      case kExprF32Sqrt:
        return __ Float32Sqrt(arg);
      case kExprF64Abs:
        return __ Float64Abs(arg);
      case kExprF64Neg:
        return __ Float64Negate(arg);
      case kExprF64Sqrt:
        return __ Float64Sqrt(arg);
      case kExprI32SConvertF32: {
        V<Float32> truncated = UnOpImpl(kExprF32Trunc, arg, kWasmF32);
        V<Word32> result = __ TruncateFloat32ToInt32OverflowToMin(truncated);
        V<Float32> converted_back = __ ChangeInt32ToFloat32(result);
        __ TrapIf(__ Word32Equal(__ Float32Equal(converted_back, truncated), 0),
                  OpIndex::Invalid(), TrapId::kTrapFloatUnrepresentable);
        return result;
      }
      case kExprI32UConvertF32: {
        V<Float32> truncated = UnOpImpl(kExprF32Trunc, arg, kWasmF32);
        V<Word32> result = __ TruncateFloat32ToUint32OverflowToMin(truncated);
        V<Float32> converted_back = __ ChangeUint32ToFloat32(result);
        __ TrapIf(__ Word32Equal(__ Float32Equal(converted_back, truncated), 0),
                  OpIndex::Invalid(), TrapId::kTrapFloatUnrepresentable);
        return result;
      }
      case kExprI32SConvertF64: {
        V<Float64> truncated = UnOpImpl(kExprF64Trunc, arg, kWasmF64);
        V<Word32> result =
            __ TruncateFloat64ToInt32OverflowUndefined(truncated);
        V<Float64> converted_back = __ ChangeInt32ToFloat64(result);
        __ TrapIf(__ Word32Equal(__ Float64Equal(converted_back, truncated), 0),
                  OpIndex::Invalid(), TrapId::kTrapFloatUnrepresentable);
        return result;
      }
      case kExprI32UConvertF64: {
        V<Float64> truncated = UnOpImpl(kExprF64Trunc, arg, kWasmF64);
        V<Word32> result = __ TruncateFloat64ToUint32OverflowToMin(truncated);
        V<Float64> converted_back = __ ChangeUint32ToFloat64(result);
        __ TrapIf(__ Word32Equal(__ Float64Equal(converted_back, truncated), 0),
                  OpIndex::Invalid(), TrapId::kTrapFloatUnrepresentable);
        return result;
      }
      case kExprI64SConvertF32:
        return Is64() ? ExtractTruncationProjections(
                            __ TryTruncateFloat32ToInt64(arg))
                      : BuildCcallConvertFloat(
                            arg, MemoryRepresentation::Float32(),
                            ExternalReference::wasm_float32_to_int64());
      case kExprI64UConvertF32:
        return Is64() ? ExtractTruncationProjections(
                            __ TryTruncateFloat32ToUint64(arg))
                      : BuildCcallConvertFloat(
                            arg, MemoryRepresentation::Float32(),
                            ExternalReference::wasm_float32_to_uint64());
      case kExprI64SConvertF64:
        return Is64() ? ExtractTruncationProjections(
                            __ TryTruncateFloat64ToInt64(arg))
                      : BuildCcallConvertFloat(
                            arg, MemoryRepresentation::Float64(),
                            ExternalReference::wasm_float64_to_int64());
      case kExprI64UConvertF64:
        return Is64() ? ExtractTruncationProjections(
                            __ TryTruncateFloat64ToUint64(arg))
                      : BuildCcallConvertFloat(
                            arg, MemoryRepresentation::Float64(),
                            ExternalReference::wasm_float64_to_uint64());
      case kExprF64SConvertI32:
        return __ ChangeInt32ToFloat64(arg);
      case kExprF64UConvertI32:
        return __ ChangeUint32ToFloat64(arg);
      case kExprF32SConvertI32:
        return __ ChangeInt32ToFloat32(arg);
      case kExprF32UConvertI32:
        return __ ChangeUint32ToFloat32(arg);
      case kExprI32SConvertSatF32: {
        V<Float32> truncated = UnOpImpl(kExprF32Trunc, arg, kWasmF32);
        V<Word32> converted =
            __ TruncateFloat32ToInt32OverflowUndefined(truncated);
        V<Float32> converted_back = __ ChangeInt32ToFloat32(converted);

        Label<Word32> done(&asm_);

        IF (LIKELY(__ Float32Equal(truncated, converted_back))) {
          GOTO(done, converted);
        }
        ELSE {
          // Overflow.
          IF (__ Float32Equal(arg, arg)) {
            // Not NaN.
            IF (__ Float32LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done,
                   __ Word32Constant(std::numeric_limits<int32_t>::min()));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   __ Word32Constant(std::numeric_limits<int32_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, __ Word32Constant(0));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI32UConvertSatF32: {
        V<Float32> truncated = UnOpImpl(kExprF32Trunc, arg, kWasmF32);
        V<Word32> converted =
            __ TruncateFloat32ToUint32OverflowUndefined(truncated);
        V<Float32> converted_back = __ ChangeUint32ToFloat32(converted);

        Label<Word32> done(&asm_);

        IF (LIKELY(__ Float32Equal(truncated, converted_back))) {
          GOTO(done, converted);
        }
        ELSE {
          // Overflow.
          IF (__ Float32Equal(arg, arg)) {
            // Not NaN.
            IF (__ Float32LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done, __ Word32Constant(0));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   __ Word32Constant(std::numeric_limits<uint32_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, __ Word32Constant(0));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI32SConvertSatF64: {
        V<Float64> truncated = UnOpImpl(kExprF64Trunc, arg, kWasmF64);
        V<Word32> converted =
            __ TruncateFloat64ToInt32OverflowUndefined(truncated);
        V<Float64> converted_back = __ ChangeInt32ToFloat64(converted);

        Label<Word32> done(&asm_);

        IF (LIKELY(__ Float64Equal(truncated, converted_back))) {
          GOTO(done, converted);
        }
        ELSE {
          // Overflow.
          IF (__ Float64Equal(arg, arg)) {
            // Not NaN.
            IF (__ Float64LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done,
                   __ Word32Constant(std::numeric_limits<int32_t>::min()));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   __ Word32Constant(std::numeric_limits<int32_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, __ Word32Constant(0));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI32UConvertSatF64: {
        V<Float64> truncated = UnOpImpl(kExprF64Trunc, arg, kWasmF64);
        V<Word32> converted =
            __ TruncateFloat64ToUint32OverflowUndefined(truncated);
        V<Float64> converted_back = __ ChangeUint32ToFloat64(converted);

        Label<Word32> done(&asm_);

        IF (LIKELY(__ Float64Equal(truncated, converted_back))) {
          GOTO(done, converted);
        }
        ELSE {
          // Overflow.
          IF (__ Float64Equal(arg, arg)) {
            // Not NaN.
            IF (__ Float64LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done, __ Word32Constant(0));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   __ Word32Constant(std::numeric_limits<uint32_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, __ Word32Constant(0));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI64SConvertSatF32: {
        if constexpr (!Is64()) {
          bool is_signed = true;
          return BuildCcallConvertFloatSat(
              arg, MemoryRepresentation::Float32(),
              ExternalReference::wasm_float32_to_int64_sat(), is_signed);
        }
        V<Word64> converted = __ TryTruncateFloat32ToInt64(arg);
        Label<compiler::turboshaft::Word64> done(&asm_);

        if (SupportedOperations::sat_conversion_is_safe()) {
          return __ Projection(converted, 0, RegisterRepresentation::Word64());
        }
        IF (LIKELY(__ Projection(converted, 1,
                                 RegisterRepresentation::Word32()))) {
          GOTO(done,
               __ Projection(converted, 0, RegisterRepresentation::Word64()));
        }
        ELSE {
          // Overflow.
          IF (__ Float32Equal(arg, arg)) {
            // Not NaN.
            IF (__ Float32LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done,
                   __ Word64Constant(std::numeric_limits<int64_t>::min()));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   __ Word64Constant(std::numeric_limits<int64_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, __ Word64Constant(int64_t{0}));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI64UConvertSatF32: {
        if constexpr (!Is64()) {
          bool is_signed = false;
          return BuildCcallConvertFloatSat(
              arg, MemoryRepresentation::Float32(),
              ExternalReference::wasm_float32_to_uint64_sat(), is_signed);
        }
        V<Word64> converted = __ TryTruncateFloat32ToUint64(arg);
        Label<compiler::turboshaft::Word64> done(&asm_);

        if (SupportedOperations::sat_conversion_is_safe()) {
          return __ Projection(converted, 0, RegisterRepresentation::Word64());
        }

        IF (LIKELY(__ Projection(converted, 1,
                                 RegisterRepresentation::Word32()))) {
          GOTO(done,
               __ Projection(converted, 0, RegisterRepresentation::Word64()));
        }
        ELSE {
          // Overflow.
          IF (__ Float32Equal(arg, arg)) {
            // Not NaN.
            IF (__ Float32LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done, __ Word64Constant(int64_t{0}));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   __ Word64Constant(std::numeric_limits<uint64_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, __ Word64Constant(int64_t{0}));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI64SConvertSatF64: {
        if constexpr (!Is64()) {
          bool is_signed = true;
          return BuildCcallConvertFloatSat(
              arg, MemoryRepresentation::Float64(),
              ExternalReference::wasm_float64_to_int64_sat(), is_signed);
        }
        V<Word64> converted = __ TryTruncateFloat64ToInt64(arg);
        Label<compiler::turboshaft::Word64> done(&asm_);

        if (SupportedOperations::sat_conversion_is_safe()) {
          return __ Projection(converted, 0, RegisterRepresentation::Word64());
        }

        IF (LIKELY(__ Projection(converted, 1,
                                 RegisterRepresentation::Word32()))) {
          GOTO(done,
               __ Projection(converted, 0, RegisterRepresentation::Word64()));
        }
        ELSE {
          // Overflow.
          IF (__ Float64Equal(arg, arg)) {
            // Not NaN.
            IF (__ Float64LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done,
                   __ Word64Constant(std::numeric_limits<int64_t>::min()));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   __ Word64Constant(std::numeric_limits<int64_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, __ Word64Constant(int64_t{0}));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprI64UConvertSatF64: {
        if constexpr (!Is64()) {
          bool is_signed = false;
          return BuildCcallConvertFloatSat(
              arg, MemoryRepresentation::Float64(),
              ExternalReference::wasm_float64_to_uint64_sat(), is_signed);
        }
        V<Word64> converted = __ TryTruncateFloat64ToUint64(arg);
        Label<compiler::turboshaft::Word64> done(&asm_);

        if (SupportedOperations::sat_conversion_is_safe()) {
          return __ Projection(converted, 0, RegisterRepresentation::Word64());
        }

        IF (LIKELY(__ Projection(converted, 1,
                                 RegisterRepresentation::Word32()))) {
          GOTO(done,
               __ Projection(converted, 0, RegisterRepresentation::Word64()));
        }
        ELSE {
          // Overflow.
          IF (__ Float64Equal(arg, arg)) {
            // Not NaN.
            IF (__ Float64LessThan(arg, 0)) {
              // Negative arg.
              GOTO(done, __ Word64Constant(int64_t{0}));
            }
            ELSE {
              // Positive arg.
              GOTO(done,
                   __ Word64Constant(std::numeric_limits<uint64_t>::max()));
            }
            END_IF
          }
          ELSE {
            // NaN.
            GOTO(done, __ Word64Constant(int64_t{0}));
          }
          END_IF
        }
        END_IF
        BIND(done, result);

        return result;
      }
      case kExprF32ConvertF64:
        return __ ChangeFloat64ToFloat32(arg);
      case kExprF64ConvertF32:
        return __ ChangeFloat32ToFloat64(arg);
      case kExprF32ReinterpretI32:
        return __ BitcastWord32ToFloat32(arg);
      case kExprI32ReinterpretF32:
        return __ BitcastFloat32ToWord32(arg);
      case kExprI32Clz:
        return __ Word32CountLeadingZeros(arg);
      case kExprI32Ctz:
        if (SupportedOperations::word32_ctz()) {
          return __ Word32CountTrailingZeros(arg);
        } else {
          // TODO(14108): Use reverse_bits if supported.
          return CallCStackSlotToInt32(arg,
                                       ExternalReference::wasm_word32_ctz(),
                                       MemoryRepresentation::Int32());
        }
      case kExprI32Popcnt:
        if (SupportedOperations::word32_popcnt()) {
          return __ Word32PopCount(arg);
        } else {
          return CallCStackSlotToInt32(arg,
                                       ExternalReference::wasm_word32_popcnt(),
                                       MemoryRepresentation::Int32());
        }
      case kExprF32Floor:
        if (SupportedOperations::float32_round_down()) {
          return __ Float32RoundDown(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f32_floor(),
                                           MemoryRepresentation::Float32());
        }
      case kExprF32Ceil:
        if (SupportedOperations::float32_round_up()) {
          return __ Float32RoundUp(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f32_ceil(),
                                           MemoryRepresentation::Float32());
        }
      case kExprF32Trunc:
        if (SupportedOperations::float32_round_to_zero()) {
          return __ Float32RoundToZero(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f32_trunc(),
                                           MemoryRepresentation::Float32());
        }
      case kExprF32NearestInt:
        if (SupportedOperations::float32_round_ties_even()) {
          return __ Float32RoundTiesEven(arg);
        } else {
          return CallCStackSlotToStackSlot(
              arg, ExternalReference::wasm_f32_nearest_int(),
              MemoryRepresentation::Float32());
        }
      case kExprF64Floor:
        if (SupportedOperations::float64_round_down()) {
          return __ Float64RoundDown(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f64_floor(),
                                           MemoryRepresentation::Float64());
        }
      case kExprF64Ceil:
        if (SupportedOperations::float64_round_up()) {
          return __ Float64RoundUp(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f64_ceil(),
                                           MemoryRepresentation::Float64());
        }
      case kExprF64Trunc:
        if (SupportedOperations::float64_round_to_zero()) {
          return __ Float64RoundToZero(arg);
        } else {
          return CallCStackSlotToStackSlot(arg,
                                           ExternalReference::wasm_f64_trunc(),
                                           MemoryRepresentation::Float64());
        }
      case kExprF64NearestInt:
        if (SupportedOperations::float64_round_ties_even()) {
          return __ Float64RoundTiesEven(arg);
        } else {
          return CallCStackSlotToStackSlot(
              arg, ExternalReference::wasm_f64_nearest_int(),
              MemoryRepresentation::Float64());
        }
      case kExprF64Acos:
        return CallCStackSlotToStackSlot(
            arg, ExternalReference::f64_acos_wrapper_function(),
            MemoryRepresentation::Float64());
      case kExprF64Asin:
        return CallCStackSlotToStackSlot(
            arg, ExternalReference::f64_asin_wrapper_function(),
            MemoryRepresentation::Float64());
      case kExprF64Atan:
        return __ Float64Atan(arg);
      case kExprF64Cos:
        return __ Float64Cos(arg);
      case kExprF64Sin:
        return __ Float64Sin(arg);
      case kExprF64Tan:
        return __ Float64Tan(arg);
      case kExprF64Exp:
        return __ Float64Exp(arg);
      case kExprF64Log:
        return __ Float64Log(arg);
      case kExprI32ConvertI64:
        return __ TruncateWord64ToWord32(arg);
      case kExprI64SConvertI32:
        return __ ChangeInt32ToInt64(arg);
      case kExprI64UConvertI32:
        return __ ChangeUint32ToUint64(arg);
      case kExprF64ReinterpretI64:
        return __ BitcastWord64ToFloat64(arg);
      case kExprI64ReinterpretF64:
        return __ BitcastFloat64ToWord64(arg);
      case kExprI64Clz:
        return __ Word64CountLeadingZeros(arg);
      case kExprI64Ctz:
        if (SupportedOperations::word64_ctz() ||
            (!Is64() && SupportedOperations::word32_ctz())) {
          return __ Word64CountTrailingZeros(arg);
        } else {
          // TODO(14108): Use reverse_bits if supported.
          return __ ChangeUint32ToUint64(
              CallCStackSlotToInt32(arg, ExternalReference::wasm_word64_ctz(),
                                    MemoryRepresentation::Int64()));
        }
      case kExprI64Popcnt:
        if (SupportedOperations::word64_popcnt() ||
            (!Is64() && SupportedOperations::word32_popcnt())) {
          return __ Word64PopCount(arg);
        } else {
          return __ ChangeUint32ToUint64(CallCStackSlotToInt32(
              arg, ExternalReference::wasm_word64_popcnt(),
              MemoryRepresentation::Int64()));
        }
      case kExprI64Eqz:
        return __ Word64Equal(arg, 0);
      case kExprF32SConvertI64:
        if constexpr (!Is64()) {
          return BuildIntToFloatConversionInstruction(
              arg, ExternalReference::wasm_int64_to_float32(),
              MemoryRepresentation::Int64(), MemoryRepresentation::Float32());
        }
        return __ ChangeInt64ToFloat32(arg);
      case kExprF32UConvertI64:
        if constexpr (!Is64()) {
          return BuildIntToFloatConversionInstruction(
              arg, ExternalReference::wasm_uint64_to_float32(),
              MemoryRepresentation::Uint64(), MemoryRepresentation::Float32());
        }
        return __ ChangeUint64ToFloat32(arg);
      case kExprF64SConvertI64:
        if constexpr (!Is64()) {
          return BuildIntToFloatConversionInstruction(
              arg, ExternalReference::wasm_int64_to_float64(),
              MemoryRepresentation::Int64(), MemoryRepresentation::Float64());
        }
        return __ ChangeInt64ToFloat64(arg);
      case kExprF64UConvertI64:
        if constexpr (!Is64()) {
          return BuildIntToFloatConversionInstruction(
              arg, ExternalReference::wasm_uint64_to_float64(),
              MemoryRepresentation::Uint64(), MemoryRepresentation::Float64());
        }
        return __ ChangeUint64ToFloat64(arg);
      case kExprI32SExtendI8:
        return __ Word32SignExtend8(arg);
      case kExprI32SExtendI16:
        return __ Word32SignExtend16(arg);
      case kExprI64SExtendI8:
        return __ Word64SignExtend8(arg);
      case kExprI64SExtendI16:
        return __ Word64SignExtend16(arg);
      case kExprI64SExtendI32:
        // TODO(14108): Is this correct?
        return __ ChangeInt32ToInt64(__ TruncateWord64ToWord32(arg));
      case kExprRefIsNull:
        return __ IsNull(arg, input_type);
      case kExprI32AsmjsLoadMem8S:
        return AsmjsLoadMem(arg, MemoryRepresentation::Int8());
      case kExprI32AsmjsLoadMem8U:
        return AsmjsLoadMem(arg, MemoryRepresentation::Uint8());
      case kExprI32AsmjsLoadMem16S:
        return AsmjsLoadMem(arg, MemoryRepresentation::Int16());
      case kExprI32AsmjsLoadMem16U:
        return AsmjsLoadMem(arg, MemoryRepresentation::Uint16());
      case kExprI32AsmjsLoadMem:
        return AsmjsLoadMem(arg, MemoryRepresentation::Int32());
      case kExprF32AsmjsLoadMem:
        return AsmjsLoadMem(arg, MemoryRepresentation::Float32());
      case kExprF64AsmjsLoadMem:
        return AsmjsLoadMem(arg, MemoryRepresentation::Float64());
      case kExprI32AsmjsSConvertF32:
      case kExprI32AsmjsUConvertF32:
        return __ JSTruncateFloat64ToWord32(__ ChangeFloat32ToFloat64(arg));
      case kExprI32AsmjsSConvertF64:
      case kExprI32AsmjsUConvertF64:
        return __ JSTruncateFloat64ToWord32(arg);
      case kExprRefAsNonNull:
        // We abuse ref.as_non_null, which isn't otherwise used in this switch,
        // as a sentinel for the negation of ref.is_null.
        return __ Word32Equal(__ IsNull(arg, input_type), 0);
      case kExprAnyConvertExtern:
        return __ AnyConvertExtern(arg);
      case kExprExternConvertAny:
        return __ ExternConvertAny(arg);
      default:
        UNREACHABLE();
    }
  }

  // TODO(14108): Implement 64-bit divisions on 32-bit platforms.
  OpIndex BinOpImpl(WasmOpcode opcode, OpIndex lhs, OpIndex rhs) {
    switch (opcode) {
      case kExprI32Add:
        return __ Word32Add(lhs, rhs);
      case kExprI32Sub:
        return __ Word32Sub(lhs, rhs);
      case kExprI32Mul:
        return __ Word32Mul(lhs, rhs);
      case kExprI32DivS: {
        __ TrapIf(__ Word32Equal(rhs, 0), OpIndex::Invalid(),
                  TrapId::kTrapDivByZero);
        V<Word32> unrepresentable_condition = __ Word32BitwiseAnd(
            __ Word32Equal(rhs, -1), __ Word32Equal(lhs, kMinInt));
        __ TrapIf(unrepresentable_condition, OpIndex::Invalid(),
                  TrapId::kTrapDivUnrepresentable);
        return __ Int32Div(lhs, rhs);
      }
      case kExprI32DivU:
        __ TrapIf(__ Word32Equal(rhs, 0), OpIndex::Invalid(),
                  TrapId::kTrapDivByZero);
        return __ Uint32Div(lhs, rhs);
      case kExprI32RemS: {
        __ TrapIf(__ Word32Equal(rhs, 0), OpIndex::Invalid(),
                  TrapId::kTrapRemByZero);
        Label<Word32> done(&asm_);
        IF (UNLIKELY(__ Word32Equal(rhs, -1))) {
          GOTO(done, __ Word32Constant(0));
        }
        ELSE {
          GOTO(done, __ Int32Mod(lhs, rhs));
        }
        END_IF;

        BIND(done, result);
        return result;
      }
      case kExprI32RemU:
        __ TrapIf(__ Word32Equal(rhs, 0), OpIndex::Invalid(),
                  TrapId::kTrapRemByZero);
        return __ Uint32Mod(lhs, rhs);
      case kExprI32And:
        return __ Word32BitwiseAnd(lhs, rhs);
      case kExprI32Ior:
        return __ Word32BitwiseOr(lhs, rhs);
      case kExprI32Xor:
        return __ Word32BitwiseXor(lhs, rhs);
      case kExprI32Shl:
        // If possible, the bitwise-and gets optimized away later.
        return __ Word32ShiftLeft(lhs, __ Word32BitwiseAnd(rhs, 0x1f));
      case kExprI32ShrS:
        return __ Word32ShiftRightArithmetic(lhs,
                                             __ Word32BitwiseAnd(rhs, 0x1f));
      case kExprI32ShrU:
        return __ Word32ShiftRightLogical(lhs, __ Word32BitwiseAnd(rhs, 0x1f));
      case kExprI32Ror:
        return __ Word32RotateRight(lhs, __ Word32BitwiseAnd(rhs, 0x1f));
      case kExprI32Rol:
        if (SupportedOperations::word32_rol()) {
          return __ Word32RotateLeft(lhs, __ Word32BitwiseAnd(rhs, 0x1f));
        } else {
          return __ Word32RotateRight(
              lhs, __ Word32Sub(32, __ Word32BitwiseAnd(rhs, 0x1f)));
        }
      case kExprI32Eq:
        return __ Word32Equal(lhs, rhs);
      case kExprI32Ne:
        return __ Word32Equal(__ Word32Equal(lhs, rhs), 0);
      case kExprI32LtS:
        return __ Int32LessThan(lhs, rhs);
      case kExprI32LeS:
        return __ Int32LessThanOrEqual(lhs, rhs);
      case kExprI32LtU:
        return __ Uint32LessThan(lhs, rhs);
      case kExprI32LeU:
        return __ Uint32LessThanOrEqual(lhs, rhs);
      case kExprI32GtS:
        return __ Int32LessThan(rhs, lhs);
      case kExprI32GeS:
        return __ Int32LessThanOrEqual(rhs, lhs);
      case kExprI32GtU:
        return __ Uint32LessThan(rhs, lhs);
      case kExprI32GeU:
        return __ Uint32LessThanOrEqual(rhs, lhs);
      case kExprI64Add:
        return __ Word64Add(lhs, rhs);
      case kExprI64Sub:
        return __ Word64Sub(lhs, rhs);
      case kExprI64Mul:
        return __ Word64Mul(lhs, rhs);
      case kExprI64DivS: {
        if constexpr (!Is64()) {
          return BuildDiv64Call(lhs, rhs, ExternalReference::wasm_int64_div(),
                                wasm::TrapId::kTrapDivByZero);
        }
        __ TrapIf(__ Word64Equal(rhs, 0), OpIndex::Invalid(),
                  TrapId::kTrapDivByZero);
        V<Word32> unrepresentable_condition = __ Word32BitwiseAnd(
            __ Word64Equal(rhs, -1),
            __ Word64Equal(lhs, std::numeric_limits<int64_t>::min()));
        __ TrapIf(unrepresentable_condition, OpIndex::Invalid(),
                  TrapId::kTrapDivUnrepresentable);
        return __ Int64Div(lhs, rhs);
      }
      case kExprI64DivU:
        if constexpr (!Is64()) {
          return BuildDiv64Call(lhs, rhs, ExternalReference::wasm_uint64_div(),
                                wasm::TrapId::kTrapDivByZero);
        }
        __ TrapIf(__ Word64Equal(rhs, 0), OpIndex::Invalid(),
                  TrapId::kTrapDivByZero);
        return __ Uint64Div(lhs, rhs);
      case kExprI64RemS: {
        if constexpr (!Is64()) {
          return BuildDiv64Call(lhs, rhs, ExternalReference::wasm_int64_mod(),
                                wasm::TrapId::kTrapRemByZero);
        }
        __ TrapIf(__ Word64Equal(rhs, 0), OpIndex::Invalid(),
                  TrapId::kTrapRemByZero);
        Label<Word64> done(&asm_);
        IF (UNLIKELY(__ Word64Equal(rhs, -1))) {
          GOTO(done, __ Word64Constant(int64_t{0}));
        }
        ELSE {
          GOTO(done, __ Int64Mod(lhs, rhs));
        }
        END_IF;

        BIND(done, result);
        return result;
      }
      case kExprI64RemU:
        if constexpr (!Is64()) {
          return BuildDiv64Call(lhs, rhs, ExternalReference::wasm_uint64_mod(),
                                wasm::TrapId::kTrapRemByZero);
        }
        __ TrapIf(__ Word64Equal(rhs, 0), OpIndex::Invalid(),
                  TrapId::kTrapRemByZero);
        return __ Uint64Mod(lhs, rhs);
      case kExprI64And:
        return __ Word64BitwiseAnd(lhs, rhs);
      case kExprI64Ior:
        return __ Word64BitwiseOr(lhs, rhs);
      case kExprI64Xor:
        return __ Word64BitwiseXor(lhs, rhs);
      case kExprI64Shl:
        // If possible, the bitwise-and gets optimized away later.
        return __ Word64ShiftLeft(
            lhs, __ Word32BitwiseAnd(__ TruncateWord64ToWord32(rhs), 0x3f));
      case kExprI64ShrS:
        return __ Word64ShiftRightArithmetic(
            lhs, __ Word32BitwiseAnd(__ TruncateWord64ToWord32(rhs), 0x3f));
      case kExprI64ShrU:
        return __ Word64ShiftRightLogical(
            lhs, __ Word32BitwiseAnd(__ TruncateWord64ToWord32(rhs), 0x3f));
      case kExprI64Ror:
        return __ Word64RotateRight(
            lhs, __ Word32BitwiseAnd(__ TruncateWord64ToWord32(rhs), 0x3f));
      case kExprI64Rol:
        if (SupportedOperations::word64_rol()) {
          return __ Word64RotateLeft(
              lhs, __ Word32BitwiseAnd(__ TruncateWord64ToWord32(rhs), 0x3f));
        } else {
          return __ Word64RotateRight(
              lhs, __ Word32BitwiseAnd(
                       __ Word32Sub(64, __ TruncateWord64ToWord32(rhs)), 0x3f));
        }
      case kExprI64Eq:
        return __ Word64Equal(lhs, rhs);
      case kExprI64Ne:
        return __ Word32Equal(__ Word64Equal(lhs, rhs), 0);
      case kExprI64LtS:
        return __ Int64LessThan(lhs, rhs);
      case kExprI64LeS:
        return __ Int64LessThanOrEqual(lhs, rhs);
      case kExprI64LtU:
        return __ Uint64LessThan(lhs, rhs);
      case kExprI64LeU:
        return __ Uint64LessThanOrEqual(lhs, rhs);
      case kExprI64GtS:
        return __ Int64LessThan(rhs, lhs);
      case kExprI64GeS:
        return __ Int64LessThanOrEqual(rhs, lhs);
      case kExprI64GtU:
        return __ Uint64LessThan(rhs, lhs);
      case kExprI64GeU:
        return __ Uint64LessThanOrEqual(rhs, lhs);
      case kExprF32CopySign: {
        V<Word32> lhs_without_sign =
            __ Word32BitwiseAnd(__ BitcastFloat32ToWord32(lhs), 0x7fffffff);
        V<Word32> rhs_sign =
            __ Word32BitwiseAnd(__ BitcastFloat32ToWord32(rhs), 0x80000000);
        return __ BitcastWord32ToFloat32(
            __ Word32BitwiseOr(lhs_without_sign, rhs_sign));
      }
      case kExprF32Add:
        return __ Float32Add(lhs, rhs);
      case kExprF32Sub:
        return __ Float32Sub(lhs, rhs);
      case kExprF32Mul:
        return __ Float32Mul(lhs, rhs);
      case kExprF32Div:
        return __ Float32Div(lhs, rhs);
      case kExprF32Eq:
        return __ Float32Equal(lhs, rhs);
      case kExprF32Ne:
        return __ Word32Equal(__ Float32Equal(lhs, rhs), 0);
      case kExprF32Lt:
        return __ Float32LessThan(lhs, rhs);
      case kExprF32Le:
        return __ Float32LessThanOrEqual(lhs, rhs);
      case kExprF32Gt:
        return __ Float32LessThan(rhs, lhs);
      case kExprF32Ge:
        return __ Float32LessThanOrEqual(rhs, lhs);
      case kExprF32Min:
        return __ Float32Min(rhs, lhs);
      case kExprF32Max:
        return __ Float32Max(rhs, lhs);
      case kExprF64CopySign: {
        V<Word64> lhs_without_sign = __ Word64BitwiseAnd(
            __ BitcastFloat64ToWord64(lhs), 0x7fffffffffffffff);
        V<Word64> rhs_sign = __ Word64BitwiseAnd(__ BitcastFloat64ToWord64(rhs),
                                                 0x8000000000000000);
        return __ BitcastWord64ToFloat64(
            __ Word64BitwiseOr(lhs_without_sign, rhs_sign));
      }
      case kExprF64Add:
        return __ Float64Add(lhs, rhs);
      case kExprF64Sub:
        return __ Float64Sub(lhs, rhs);
      case kExprF64Mul:
        return __ Float64Mul(lhs, rhs);
      case kExprF64Div:
        return __ Float64Div(lhs, rhs);
      case kExprF64Eq:
        return __ Float64Equal(lhs, rhs);
      case kExprF64Ne:
        return __ Word32Equal(__ Float64Equal(lhs, rhs), 0);
      case kExprF64Lt:
        return __ Float64LessThan(lhs, rhs);
      case kExprF64Le:
        return __ Float64LessThanOrEqual(lhs, rhs);
      case kExprF64Gt:
        return __ Float64LessThan(rhs, lhs);
      case kExprF64Ge:
        return __ Float64LessThanOrEqual(rhs, lhs);
      case kExprF64Min:
        return __ Float64Min(lhs, rhs);
      case kExprF64Max:
        return __ Float64Max(lhs, rhs);
      case kExprF64Pow:
        return __ Float64Power(lhs, rhs);
      case kExprF64Atan2:
        return __ Float64Atan2(lhs, rhs);
      case kExprF64Mod:
        return CallCStackSlotToStackSlot(
            lhs, rhs, ExternalReference::f64_mod_wrapper_function(),
            MemoryRepresentation::Float64());
      case kExprRefEq:
        return __ TaggedEqual(lhs, rhs);
      case kExprI32AsmjsDivS: {
        // asmjs semantics return 0 when dividing by 0.
        if (SupportedOperations::int32_div_is_safe()) {
          return __ Int32Div(lhs, rhs);
        }
        Label<Word32> done(&asm_);
        IF (UNLIKELY(__ Word32Equal(rhs, 0))) {
          GOTO(done, __ Word32Constant(0));
        }
        ELSE {
          IF (UNLIKELY(__ Word32Equal(rhs, -1))) {
            GOTO(done, __ Word32Sub(0, lhs));
          }
          ELSE {
            GOTO(done, __ Int32Div(lhs, rhs));
          }
          END_IF
        }
        END_IF
        BIND(done, result);
        return result;
      }
      case kExprI32AsmjsDivU: {
        // asmjs semantics return 0 when dividing by 0.
        if (SupportedOperations::uint32_div_is_safe()) {
          return __ Uint32Div(lhs, rhs);
        }
        Label<Word32> done(&asm_);
        IF (UNLIKELY(__ Word32Equal(rhs, 0))) {
          GOTO(done, __ Word32Constant(0));
        }
        ELSE {
          GOTO(done, __ Uint32Div(lhs, rhs));
        }
        END_IF
        BIND(done, result);
        return result;
      }
      case kExprI32AsmjsRemS: {
        Label<Word32> done(&asm_);
        IF (UNLIKELY(__ Word32Equal(rhs, 0))) {
          GOTO(done, __ Word32Constant(0));
        }
        ELSE {
          IF (UNLIKELY(__ Word32Equal(rhs, -1))) {
            GOTO(done, __ Word32Constant(0));
          }
          ELSE {
            GOTO(done, __ Int32Mod(lhs, rhs));
          }
          END_IF
        }
        END_IF
        BIND(done, result);
        return result;
      }
      case kExprI32AsmjsRemU: {
        // asmjs semantics return 0 for mod with 0.
        Label<Word32> done(&asm_);
        IF (UNLIKELY(__ Word32Equal(rhs, 0))) {
          GOTO(done, __ Word32Constant(0));
        }
        ELSE {
          GOTO(done, __ Uint32Mod(lhs, rhs));
        }
        END_IF
        BIND(done, result);
        return result;
      }
      case kExprI32AsmjsStoreMem8:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Int8());
        return rhs;
      case kExprI32AsmjsStoreMem16:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Int16());
        return rhs;
      case kExprI32AsmjsStoreMem:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Int32());
        return rhs;
      case kExprF32AsmjsStoreMem:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Float32());
        return rhs;
      case kExprF64AsmjsStoreMem:
        AsmjsStoreMem(lhs, rhs, MemoryRepresentation::Float64());
        return rhs;
      default:
        UNREACHABLE();
    }
  }

  std::pair<V<WordPtr>, compiler::BoundsCheckResult> CheckBoundsAndAlignment(
      const wasm::WasmMemory* memory, MemoryRepresentation repr, OpIndex index,
      uintptr_t offset, wasm::WasmCodePosition position,
      compiler::EnforceBoundsCheck enforce_check) {
    // Atomic operations need bounds checks until the backend can emit protected
    // loads.
    compiler::BoundsCheckResult bounds_check_result;
    V<WordPtr> converted_index;
    std::tie(converted_index, bounds_check_result) =
        BoundsCheckMem(memory, repr, index, offset, enforce_check);

    const uintptr_t align_mask = repr.SizeInBytes() - 1;

    // TODO(14108): Optimize constant index as per wasm-compiler.cc.

    // Unlike regular memory accesses, atomic memory accesses should trap if
    // the effective offset is misaligned.
    // TODO(wasm): this addition is redundant with one inserted by {MemBuffer}.
    OpIndex effective_offset =
        __ WordPtrAdd(MemBuffer(memory->index, offset), converted_index);

    V<Word32> cond = __ TruncateWordPtrToWord32(
        __ WordPtrBitwiseAnd(effective_offset, __ IntPtrConstant(align_mask)));
    __ TrapIfNot(__ Word32Equal(cond, __ Word32Constant(0)), OpIndex::Invalid(),
                 TrapId::kTrapUnalignedAccess);
    return {converted_index, bounds_check_result};
  }

  std::pair<V<WordPtr>, compiler::BoundsCheckResult> BoundsCheckMem(
      const wasm::WasmMemory* memory, MemoryRepresentation repr, OpIndex index,
      uintptr_t offset, compiler::EnforceBoundsCheck enforce_bounds_check) {
    // The function body decoder already validated that the access is not
    // statically OOB.
    DCHECK(base::IsInBounds(offset, static_cast<uintptr_t>(repr.SizeInBytes()),
                            memory->max_memory_size));

    // Convert the index to uintptr.
    if (!memory->is_memory64) {
      index = __ ChangeUint32ToUintPtr(index);
    } else if (kSystemPointerSize == kInt32Size) {
      // In memory64 mode on 32-bit systems, the upper 32 bits need to be zero
      // to succeed the bounds check.
      DCHECK_NE(kTrapHandler, memory->bounds_checks);
      if (memory->bounds_checks == wasm::kExplicitBoundsChecks) {
        V<Word32> high_word =
            __ TruncateWord64ToWord32(__ Word64ShiftRightLogical(index, 32));
        __ TrapIf(high_word, OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
      }
      // Truncate index to 32-bit.
      index = __ TruncateWord64ToWord32(index);
    }

    //  If no bounds checks should be performed (for testing), just return the
    // converted index and assume it to be in-bounds.
    if (memory->bounds_checks == wasm::kNoBoundsChecks) {
      return {index, compiler::BoundsCheckResult::kInBounds};
    }

    // TODO(14108): Optimize constant index as per wasm-compiler.cc.

    if (memory->bounds_checks == kTrapHandler &&
        enforce_bounds_check ==
            compiler::EnforceBoundsCheck::kCanOmitBoundsCheck) {
      return {index, compiler::BoundsCheckResult::kTrapHandler};
    }

    uintptr_t end_offset = offset + repr.SizeInBytes() - 1u;

    V<WordPtr> memory_size = MemSize(memory->index);
    if (end_offset > memory->min_memory_size) {
      // The end offset is larger than the smallest memory.
      // Dynamically check the end offset against the dynamic memory size.
      __ TrapIfNot(
          __ UintPtrLessThan(__ UintPtrConstant(end_offset), memory_size),
          OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
    }

    // This produces a positive number since {end_offset <= min_size <=
    // mem_size}.
    V<WordPtr> effective_size = __ WordPtrSub(memory_size, end_offset);
    __ TrapIfNot(__ UintPtrLessThan(index, effective_size), OpIndex::Invalid(),
                 TrapId::kTrapMemOutOfBounds);
    return {index, compiler::BoundsCheckResult::kDynamicallyChecked};
  }

  V<WordPtr> MemStart(uint32_t index) {
    if (index == 0) {
      return LOAD_INSTANCE_FIELD(instance_node_, Memory0Start,
                                 kMaybeSandboxedPointer);
    } else {
      V<ByteArray> instance_memories =
          LOAD_IMMUTABLE_INSTANCE_FIELD(instance_node_, MemoryBasesAndSizes,
                                        MemoryRepresentation::TaggedPointer());
      return __ Load(instance_memories, LoadOp::Kind::TaggedBase(),
                     kMaybeSandboxedPointer,
                     ByteArray::kHeaderSize +
                         2 * index * kMaybeSandboxedPointer.SizeInBytes());
    }
  }

  V<WordPtr> MemBuffer(uint32_t mem_index, uintptr_t offset) {
    V<WordPtr> mem_start = MemStart(mem_index);
    if (offset == 0) return mem_start;
    return __ WordPtrAdd(mem_start, offset);
  }

  V<WordPtr> MemSize(uint32_t index) {
    if (index == 0) {
      return LOAD_INSTANCE_FIELD(instance_node_, Memory0Size,
                                 MemoryRepresentation::PointerSized());
    } else {
      V<ByteArray> instance_memories =
          LOAD_IMMUTABLE_INSTANCE_FIELD(instance_node_, MemoryBasesAndSizes,
                                        MemoryRepresentation::TaggedPointer());
      return __ Load(
          instance_memories, LoadOp::Kind::TaggedBase(),
          MemoryRepresentation::PointerSized(),
          ByteArray::kHeaderSize + (2 * index + 1) * kSystemPointerSize);
    }
  }

  LoadOp::Kind GetMemoryAccessKind(
      MemoryRepresentation repr,
      compiler::BoundsCheckResult bounds_check_result) {
    LoadOp::Kind result;
    if (bounds_check_result == compiler::BoundsCheckResult::kTrapHandler) {
      DCHECK(repr == MemoryRepresentation::Int8() ||
             repr == MemoryRepresentation::Uint8() ||
             SupportedOperations::IsUnalignedLoadSupported(repr));
      result = LoadOp::Kind::Protected();
    } else if (repr != MemoryRepresentation::Int8() &&
               repr != MemoryRepresentation::Uint8() &&
               !SupportedOperations::IsUnalignedLoadSupported(repr)) {
      result = LoadOp::Kind::RawUnaligned();
    } else {
      result = LoadOp::Kind::RawAligned();
    }
    return result.NotAlwaysCanonicallyAccessed();
  }

  void TraceMemoryOperation(bool is_store, MemoryRepresentation repr,
                            V<WordPtr> index, uintptr_t offset) {
    int kAlign = 4;  // Ensure that the LSB is 0, like a Smi.
    V<WordPtr> info = __ StackSlot(sizeof(MemoryTracingInfo), kAlign);
    V<WordPtr> effective_offset = __ WordPtrAdd(index, offset);
    __ Store(info, effective_offset, StoreOp::Kind::RawAligned(),
             MemoryRepresentation::PointerSized(), compiler::kNoWriteBarrier,
             offsetof(MemoryTracingInfo, offset));
    __ Store(info, __ Word32Constant(is_store ? 1 : 0),
             StoreOp::Kind::RawAligned(), MemoryRepresentation::Uint8(),
             compiler::kNoWriteBarrier, offsetof(MemoryTracingInfo, is_store));
    V<Word32> rep_as_int = __ Word32Constant(
        static_cast<int>(repr.ToMachineType().representation()));
    __ Store(info, rep_as_int, StoreOp::Kind::RawAligned(),
             MemoryRepresentation::Uint8(), compiler::kNoWriteBarrier,
             offsetof(MemoryTracingInfo, mem_rep));
    CallRuntime(Runtime::kWasmTraceMemory, {info});
  }

  void StackCheck(StackCheckOp::CheckKind kind) {
    if (V8_UNLIKELY(!v8_flags.wasm_stack_checks)) return;
    __ StackCheck(StackCheckOp::CheckOrigin::kFromWasm, kind);
  }

  std::pair<V<WordPtr>, V<HeapObject>> BuildImportedFunctionTargetAndRef(
      uint32_t function_index) {
    // Imported function.
    V<WordPtr> func_index = __ IntPtrConstant(function_index);
    V<FixedArray> imported_function_refs =
        LOAD_IMMUTABLE_INSTANCE_FIELD(instance_node_, ImportedFunctionRefs,
                                      MemoryRepresentation::TaggedPointer());
    auto ref = V<HeapObject>::Cast(
        __ LoadFixedArrayElement(imported_function_refs, func_index));
    V<FixedAddressArray> imported_targets =
        LOAD_IMMUTABLE_INSTANCE_FIELD(instance_node_, ImportedFunctionTargets,
                                      MemoryRepresentation::TaggedPointer());
    V<WordPtr> target =
        __ Load(imported_targets, func_index, LoadOp::Kind::TaggedBase(),
                MemoryRepresentation::PointerSized(),
                FixedAddressArray::kHeaderSize, kSystemPointerSizeLog2);
    return {target, ref};
  }

  std::pair<V<WordPtr>, V<HeapObject>> BuildIndirectCallTargetAndRef(
      FullDecoder* decoder, OpIndex index, CallIndirectImmediate imm) {
    uint32_t table_index = imm.table_imm.index;
    const WasmTable& table = decoder->module_->tables[table_index];
    V<WordPtr> index_intptr = __ ChangeInt32ToIntPtr(index);
    uint32_t sig_index = imm.sig_imm.index;

    /* Step 1: Load the indirect function tables for this table. */
    bool needs_dynamic_size =
        !(table.has_maximum_size && table.maximum_size == table.initial_size);
    V<Word32> ift_size;
    V<ByteArray> ift_sig_ids;
    V<ExternalPointerArray> ift_targets;
    V<FixedArray> ift_refs;
    if (table_index == 0) {
      ift_size =
          needs_dynamic_size
              ? LOAD_INSTANCE_FIELD(instance_node_, IndirectFunctionTableSize,
                                    MemoryRepresentation::Uint32())
              : __ Word32Constant(table.initial_size);
      ift_sig_ids =
          LOAD_INSTANCE_FIELD(instance_node_, IndirectFunctionTableSigIds,
                              MemoryRepresentation::TaggedPointer());
      ift_targets =
          LOAD_INSTANCE_FIELD(instance_node_, IndirectFunctionTableTargets,
                              MemoryRepresentation::TaggedPointer());
      ift_refs = LOAD_INSTANCE_FIELD(instance_node_, IndirectFunctionTableRefs,
                                     MemoryRepresentation::TaggedPointer());
    } else {
      V<FixedArray> ift_tables =
          LOAD_IMMUTABLE_INSTANCE_FIELD(instance_node_, IndirectFunctionTables,
                                        MemoryRepresentation::TaggedPointer());
      OpIndex ift_table = __ LoadFixedArrayElement(ift_tables, table_index);
      ift_size = needs_dynamic_size
                     ? __ Load(ift_table, LoadOp::Kind::TaggedBase(),
                               MemoryRepresentation::Uint32(),
                               WasmIndirectFunctionTable::kSizeOffset)
                     : __ Word32Constant(table.initial_size);
      ift_sig_ids = __ Load(ift_table, LoadOp::Kind::TaggedBase(),
                            MemoryRepresentation::TaggedPointer(),
                            WasmIndirectFunctionTable::kSigIdsOffset);
      ift_targets = __ Load(ift_table, LoadOp::Kind::TaggedBase(),
                            MemoryRepresentation::TaggedPointer(),
                            WasmIndirectFunctionTable::kTargetsOffset);
      ift_refs = __ Load(ift_table, LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::TaggedPointer(),
                         WasmIndirectFunctionTable::kRefsOffset);
    }

    /* Step 2: Bounds check against the table size. */
    __ TrapIfNot(__ Uint32LessThan(index, ift_size), OpIndex::Invalid(),
                 TrapId::kTrapTableOutOfBounds);

    /* Step 3: Check the canonical real signature against the canonical declared
     * signature. */
    bool needs_type_check =
        !EquivalentTypes(table.type.AsNonNull(), ValueType::Ref(sig_index),
                         decoder->module_, decoder->module_);
    bool needs_null_check = table.type.is_nullable();

    if (needs_type_check) {
      V<WordPtr> isorecursive_canonical_types = LOAD_IMMUTABLE_INSTANCE_FIELD(
          instance_node_, IsorecursiveCanonicalTypes,
          MemoryRepresentation::PointerSized());
      V<Word32> expected_sig_id =
          __ Load(isorecursive_canonical_types, LoadOp::Kind::RawAligned(),
                  MemoryRepresentation::Uint32(), sig_index * kUInt32Size);
      V<Word32> loaded_sig =
          __ Load(ift_sig_ids, index_intptr, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::Uint32(), ByteArray::kHeaderSize,
                  2 /* kInt32SizeLog2 */);
      V<Word32> sigs_match = __ Word32Equal(expected_sig_id, loaded_sig);
      if (decoder->enabled_.has_gc() &&
          !decoder->module_->types[sig_index].is_final) {
        // In this case, a full type check is needed.
        Label<> end(&asm_);

        // First, check if signatures happen to match exactly.
        GOTO_IF(sigs_match, end);

        if (needs_null_check) {
          // Trap on null element.
          __ TrapIf(__ Word32Equal(loaded_sig, -1), OpIndex::Invalid(),
                    TrapId::kTrapFuncSigMismatch);
        }

        V<Map> formal_rtt = __ RttCanon(instance_node_, sig_index);
        int rtt_depth = GetSubtypingDepth(decoder->module_, sig_index);
        DCHECK_GE(rtt_depth, 0);

        // Since we have the canonical index of the real rtt, we have to load it
        // from the isolate rtt-array (which is canonically indexed). Since this
        // reference is weak, we have to promote it to a strong reference.
        // Note: The reference cannot have been cleared: Since the loaded_sig
        // corresponds to a function of the same canonical type, that function
        // will have kept the type alive.
        V<WeakArrayList> rtts = LOAD_ROOT(WasmCanonicalRtts);
        V<Tagged> weak_rtt = __ Load(
            rtts, __ ChangeInt32ToIntPtr(loaded_sig),
            LoadOp::Kind::TaggedBase(), MemoryRepresentation::TaggedPointer(),
            WeakArrayList::kHeaderSize, kTaggedSizeLog2);
        V<Map> real_rtt = V<Map>::Cast(__ WordPtrBitwiseAnd(
            __ BitcastTaggedToWord(weak_rtt), ~kWeakHeapObjectMask));
        V<WasmTypeInfo> type_info =
            __ Load(real_rtt, LoadOp::Kind::TaggedBase(),
                    MemoryRepresentation::TaggedPointer(),
                    Map::kConstructorOrBackPointerOrNativeContextOffset);
        // If the depth of the rtt is known to be less than the minimum
        // supertype array length, we can access the supertype without
        // bounds-checking the supertype array.
        if (static_cast<uint32_t>(rtt_depth) >=
            wasm::kMinimumSupertypeArraySize) {
          V<Word32> supertypes_length =
              __ UntagSmi(__ Load(type_info, LoadOp::Kind::TaggedBase(),
                                  MemoryRepresentation::TaggedSigned(),
                                  WasmTypeInfo::kSupertypesLengthOffset));
          __ TrapIfNot(__ Uint32LessThan(rtt_depth, supertypes_length),
                       OpIndex::Invalid(), TrapId::kTrapFuncSigMismatch);
        }
        V<Map> maybe_match =
            __ Load(type_info, LoadOp::Kind::TaggedBase(),
                    MemoryRepresentation::TaggedPointer(),
                    WasmTypeInfo::kSupertypesOffset + kTaggedSize * rtt_depth);
        __ TrapIfNot(__ TaggedEqual(maybe_match, formal_rtt),
                     OpIndex::Invalid(), TrapId::kTrapFuncSigMismatch);
        GOTO(end);
        BIND(end);
      } else {
        // In this case, signatures must match exactly.
        __ TrapIfNot(sigs_match, OpIndex::Invalid(),
                     TrapId::kTrapFuncSigMismatch);
      }
    } else if (needs_null_check) {
      V<Word32> loaded_sig =
          __ Load(ift_sig_ids, index_intptr, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::Uint32(), ByteArray::kHeaderSize,
                  2 /* kInt32SizeLog2 */);
      __ TrapIf(__ Word32Equal(-1, loaded_sig), OpIndex::Invalid(),
                TrapId::kTrapFuncSigMismatch);
    }

    /* Step 4: Extract ref and target. */
#ifdef V8_ENABLE_SANDBOX
    V<Word32> external_pointer_handle = __ Load(
        ift_targets, index_intptr, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::Uint32(), ExternalPointerArray::kHeaderSize, 2);
    V<WordPtr> target = __ DecodeExternalPointer(
        external_pointer_handle, kWasmIndirectFunctionTargetTag);
#else
    V<WordPtr> target =
        __ Load(ift_targets, index_intptr, LoadOp::Kind::TaggedBase(),
                MemoryRepresentation::PointerSized(),
                ExternalPointerArray::kHeaderSize, kSystemPointerSizeLog2);
#endif
    auto ref =
        V<HeapObject>::Cast(__ LoadFixedArrayElement(ift_refs, index_intptr));
    return {target, ref};
  }

  std::pair<V<WordPtr>, V<HeapObject>> BuildFunctionReferenceTargetAndRef(
      V<HeapObject> func_ref, ValueType type) {
    if (type.is_nullable() &&
        null_check_strategy_ == compiler::NullCheckStrategy::kExplicit) {
      func_ref = V<HeapObject>::Cast(
          __ AssertNotNull(func_ref, type, TrapId::kTrapNullDereference));
    }

    V<HeapObject> ref =
        type.is_nullable() && null_check_strategy_ ==
                                  compiler::NullCheckStrategy::kTrapHandler
            ? __ Load(func_ref, LoadOp::Kind::TrapOnNull(),
                      MemoryRepresentation::TaggedPointer(),
                      WasmInternalFunction::kRefOffset)
            : __ Load(func_ref, LoadOp::Kind::TaggedBase(),
                      MemoryRepresentation::TaggedPointer(),
                      WasmInternalFunction::kRefOffset);

#ifdef V8_ENABLE_SANDBOX
    V<Word32> target_handle = __ Load(func_ref, LoadOp::Kind::TaggedBase(),
                                      MemoryRepresentation::Uint32(),
                                      WasmInternalFunction::kCallTargetOffset);
    V<WordPtr> target = __ DecodeExternalPointer(
        target_handle, kWasmInternalFunctionCallTargetTag);
#else
    V<WordPtr> target = __ Load(func_ref, LoadOp::Kind::TaggedBase(),
                                MemoryRepresentation::PointerSized(),
                                WasmInternalFunction::kCallTargetOffset);
#endif
    Label<WordPtr> done(&asm_);
    // For wasm functions, we have a cached handle to a pointer to off-heap
    // code. For WasmJSFunctions we used not to be able to cache this handle
    // because the code was on-heap, so we needed to fetch it every time from
    // the on-heap Code object. However now, when code pointer sandboxing is on,
    // the on-heap Code object also has a handle to off-heap code, so we could
    // probably also cache that somehow.
    // TODO(manoskouk): Figure out how to improve the situation.
    IF (UNLIKELY(__ WordPtrEqual(target, 0))) {
      V<Code> wrapper_code = __ Load(func_ref, LoadOp::Kind::TaggedBase(),
                                     MemoryRepresentation::TaggedPointer(),
                                     WasmInternalFunction::kCodeOffset);
#ifdef V8_ENABLE_SANDBOX
      V<Word32> call_target_handle = __ Load(
          wrapper_code, LoadOp::Kind::TaggedBase(),
          MemoryRepresentation::Uint32(), Code::kSelfIndirectPointerOffset);
      V<WordPtr> call_target =
          BuildDecodeExternalCodePointer(call_target_handle);
#else
      V<WordPtr> call_target = __ Load(wrapper_code, LoadOp::Kind::TaggedBase(),
                                       MemoryRepresentation::PointerSized(),
                                       Code::kInstructionStartOffset);
#endif
      GOTO(done, call_target);
    }
    ELSE {
      GOTO(done, target);
    }
    END_IF

    BIND(done, final_target);

    return {final_target, ref};
  }

  OpIndex AnnotateResultIfReference(OpIndex result, wasm::ValueType type) {
    return type.is_object_reference() ? __ AnnotateWasmType(result, type)
                                      : result;
  }

  void BuildWasmCall(FullDecoder* decoder, const FunctionSig* sig,
                     V<WordPtr> callee, V<HeapObject> ref, const Value args[],
                     Value returns[],
                     CheckForException check_for_exception =
                         CheckForException::kCatchInThisFrame) {
    const TSCallDescriptor* descriptor = TSCallDescriptor::Create(
        compiler::GetWasmCallDescriptor(__ graph_zone(), sig),
        compiler::CanThrow::kYes, __ graph_zone());

    std::vector<OpIndex> arg_indices(sig->parameter_count() + 1);
    arg_indices[0] = ref;
    for (uint32_t i = 0; i < sig->parameter_count(); i++) {
      arg_indices[i + 1] = args[i].op;
    }

    OpIndex call =
        CallAndMaybeCatchException(decoder, callee, base::VectorOf(arg_indices),
                                   descriptor, check_for_exception);

    if (sig->return_count() == 1) {
      returns[0].op = AnnotateResultIfReference(call, sig->GetReturn(0));
    } else if (sig->return_count() > 1) {
      for (uint32_t i = 0; i < sig->return_count(); i++) {
        wasm::ValueType type = sig->GetReturn(i);
        returns[i].op = AnnotateResultIfReference(
            __ Projection(call, i, RepresentationFor(type)), type);
      }
    }
  }

  void BuildWasmMaybeReturnCall(FullDecoder* decoder, const FunctionSig* sig,
                                V<WordPtr> callee, V<HeapObject> ref,
                                const Value args[]) {
    if (mode_ == kRegular) {
      const TSCallDescriptor* descriptor = TSCallDescriptor::Create(
          compiler::GetWasmCallDescriptor(__ graph_zone(), sig),
          compiler::CanThrow::kYes, __ graph_zone());

      base::SmallVector<OpIndex, 8> arg_indices(sig->parameter_count() + 1);
      arg_indices[0] = ref;
      for (uint32_t i = 0; i < sig->parameter_count(); i++) {
        arg_indices[i + 1] = args[i].op;
      }
      __ TailCall(callee, base::VectorOf(arg_indices), descriptor);
    } else {
      if (__ generating_unreachable_operations()) return;
      // This is a tail call in the inlinee. Transform it into a regular call,
      // and return the return values to the caller.
      // TODO(14108): This can remain a tail call if the inlined call is also a
      // tail call.
      base::SmallVector<Value, 8> returns(sig->return_count());
      // Since an exception in a tail call cannot be caught in this frame, we
      // should only catch exceptions in the generated call if this is a
      // recursively inlined function, and the parent frame provides a handler.
      BuildWasmCall(decoder, sig, callee, ref, args, returns.data(),
                    CheckForException::kCatchInParentFrame);
      for (size_t i = 0; i < sig->return_count(); i++) {
        return_phis_.phi_inputs[i].push_back(returns[i].op);
      }
      __ Goto(return_block_);
    }
  }

  OpIndex CallBuiltinThroughJumptable(
      FullDecoder* decoder, Builtin builtin, base::Vector<const OpIndex> args,
      Operator::Properties properties = Operator::kNoProperties,
      CheckForException check_for_exception = CheckForException::kNo) {
    DCHECK_NE(check_for_exception, CheckForException::kCatchInParentFrame);
    CallInterfaceDescriptor interface_descriptor =
        Builtins::CallInterfaceDescriptorFor(builtin);
    // TODO(14108): We should set properties like `Operator::kEliminatable`
    // where applicable.
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetStubCallDescriptor(
            __ graph_zone(), interface_descriptor,
            interface_descriptor.GetStackParameterCount(),
            CallDescriptor::kNoFlags, properties,
            StubCallMode::kCallWasmRuntimeStub);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kYes, __ graph_zone());
    V<WordPtr> call_target = __ RelocatableWasmBuiltinCallTarget(builtin);
    return CallAndMaybeCatchException(decoder, call_target, args,
                                      ts_call_descriptor, check_for_exception);
  }

  OpIndex CallBuiltinThroughJumptable(
      FullDecoder* decoder, Builtin builtin,
      std::initializer_list<OpIndex> args,
      Operator::Properties properties = Operator::kNoProperties,
      CheckForException check_for_exception = CheckForException::kNo) {
    return CallBuiltinThroughJumptable(decoder, builtin, base::VectorOf(args),
                                       properties, check_for_exception);
  }

  void MaybeSetPositionToParent(OpIndex call,
                                CheckForException check_for_exception) {
    // For tail calls that we transform to regular calls, we need to set the
    // call's position to that of the inlined call node to get correct stack
    // traces.
    if (check_for_exception == CheckForException::kCatchInParentFrame) {
      __ output_graph().operation_origins()[call] = WasmPositionToOpIndex(
          parent_position_.ScriptOffset(), parent_position_.InliningId() == -1
                                               ? kNoInliningId
                                               : parent_position_.InliningId());
    }
  }

  OpIndex CallAndMaybeCatchException(FullDecoder* decoder, V<WordPtr> callee,
                                     base::Vector<const OpIndex> args,
                                     const TSCallDescriptor* descriptor,
                                     CheckForException check_for_exception) {
    if (check_for_exception == CheckForException::kNo) {
      return __ Call(callee, OpIndex::Invalid(), args, descriptor);
    }
    bool handled_in_this_frame =
        decoder->current_catch() != -1 &&
        check_for_exception == CheckForException::kCatchInThisFrame;
    if (!handled_in_this_frame && mode_ != kInlinedWithCatch) {
      OpIndex call = __ Call(callee, OpIndex::Invalid(), args, descriptor);
      MaybeSetPositionToParent(call, check_for_exception);
      return call;
    }

    TSBlock* catch_block;
    if (handled_in_this_frame) {
      Control* current_catch =
          decoder->control_at(decoder->control_depth_of_current_catch());
      catch_block = current_catch->false_or_loop_or_catch_block;
    } else {
      DCHECK_EQ(mode_, kInlinedWithCatch);
      catch_block = return_catch_block_;
    }
    TSBlock* success_block = __ NewBlock();
    TSBlock* exception_block = __ NewBlock();
    OpIndex call;
    {
      Assembler::CatchScope scope(asm_, exception_block);

      call = __ Call(callee, OpIndex::Invalid(), args, descriptor);
      __ Goto(success_block);
    }

    __ Bind(exception_block);
    OpIndex exception = __ CatchBlockBegin();
    if (handled_in_this_frame) {
      SetupControlFlowEdge(decoder, catch_block, 0, exception);
    } else {
      DCHECK_EQ(mode_, kInlinedWithCatch);
      if (exception.valid()) return_exception_phis_.push_back(exception);
    }
    __ Goto(catch_block);

    __ Bind(success_block);

    MaybeSetPositionToParent(call, check_for_exception);

    return call;
  }

  OpIndex CallRuntime(Runtime::FunctionId f,
                      std::initializer_list<const OpIndex> args) {
    const Runtime::Function* fun = Runtime::FunctionForId(f);
    OpIndex isolate_root = __ LoadRootRegister();
    DCHECK_EQ(1, fun->result_size);
    int builtin_slot_offset = IsolateData::BuiltinSlotOffset(
        Builtin::kCEntry_Return1_ArgvOnStack_NoBuiltinExit);
    OpIndex centry_stub =
        __ Load(isolate_root, LoadOp::Kind::RawAligned(),
                MemoryRepresentation::PointerSized(), builtin_slot_offset);
    base::SmallVector<OpIndex, 8> centry_args;
    for (OpIndex arg : args) centry_args.emplace_back(arg);
    centry_args.emplace_back(__ ExternalConstant(ExternalReference::Create(f)));
    centry_args.emplace_back(__ Word32Constant(fun->nargs));
    centry_args.emplace_back(__ NoContextConstant());  // js_context
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetRuntimeCallDescriptor(
            __ graph_zone(), f, fun->nargs, Operator::kNoProperties,
            CallDescriptor::kNoFlags);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kYes, __ graph_zone());
    return __ Call(centry_stub, OpIndex::Invalid(), base::VectorOf(centry_args),
                   ts_call_descriptor);
  }

  OpIndex CallC(const MachineSignature* sig, ExternalReference ref,
                std::initializer_list<OpIndex> args) {
    DCHECK_LE(sig->return_count(), 1);
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetSimplifiedCDescriptor(__ graph_zone(), sig);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo, __ graph_zone());
    return __ Call(__ ExternalConstant(ref), OpIndex::Invalid(),
                   base::VectorOf(args), ts_call_descriptor);
  }

  OpIndex CallC(const MachineSignature* sig, ExternalReference ref,
                OpIndex arg) {
    return CallC(sig, ref, {arg});
  }

  OpIndex CallCStackSlotToInt32(OpIndex arg, ExternalReference ref,
                                MemoryRepresentation arg_type) {
    OpIndex stack_slot_param =
        __ StackSlot(arg_type.SizeInBytes(), arg_type.SizeInBytes());
    __ Store(stack_slot_param, arg, StoreOp::Kind::RawAligned(), arg_type,
             compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Int32(), MachineType::Pointer()};
    MachineSignature sig(1, 1, reps);
    return CallC(&sig, ref, stack_slot_param);
  }

  V<Word32> CallCStackSlotToInt32(
      ExternalReference ref,
      std::initializer_list<std::pair<OpIndex, MemoryRepresentation>> args) {
    int slot_size = 0;
    for (auto arg : args) slot_size += arg.second.SizeInBytes();
    // Since we are storing the arguments unaligned anyway, we do not need
    // alignment > 0.
    V<WordPtr> stack_slot_param = __ StackSlot(slot_size, 0);
    int offset = 0;
    for (auto arg : args) {
      __ Store(stack_slot_param, arg.first, StoreOp::Kind::RawUnaligned(),
               arg.second, compiler::WriteBarrierKind::kNoWriteBarrier, offset);
      offset += arg.second.SizeInBytes();
    }
    MachineType reps[]{MachineType::Int32(), MachineType::Pointer()};
    MachineSignature sig(1, 1, reps);
    return CallC(&sig, ref, stack_slot_param);
  }

  OpIndex CallCStackSlotToStackSlot(OpIndex arg, ExternalReference ref,
                                    MemoryRepresentation arg_type) {
    V<WordPtr> stack_slot =
        __ StackSlot(arg_type.SizeInBytes(), arg_type.SizeInBytes());
    __ Store(stack_slot, arg, StoreOp::Kind::RawAligned(), arg_type,
             compiler::WriteBarrierKind::kNoWriteBarrier);
    MachineType reps[]{MachineType::Pointer()};
    MachineSignature sig(0, 1, reps);
    CallC(&sig, ref, stack_slot);
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(), arg_type);
  }

  OpIndex CallCStackSlotToStackSlot(OpIndex arg0, OpIndex arg1,
                                    ExternalReference ref,
                                    MemoryRepresentation arg_type) {
    V<WordPtr> stack_slot =
        __ StackSlot(2 * arg_type.SizeInBytes(), arg_type.SizeInBytes());
    __ Store(stack_slot, arg0, StoreOp::Kind::RawAligned(), arg_type,
             compiler::WriteBarrierKind::kNoWriteBarrier);
    __ Store(stack_slot, arg1, StoreOp::Kind::RawAligned(), arg_type,
             compiler::WriteBarrierKind::kNoWriteBarrier,
             arg_type.SizeInBytes());
    MachineType reps[]{MachineType::Pointer()};
    MachineSignature sig(0, 1, reps);
    CallC(&sig, ref, stack_slot);
    return __ Load(stack_slot, LoadOp::Kind::RawAligned(), arg_type);
  }

  V<WordPtr> MemoryIndexToUintPtrOrOOBTrap(bool memory_is_64, OpIndex index) {
    if (!memory_is_64) return __ ChangeUint32ToUintPtr(index);
    if (kSystemPointerSize == kInt64Size) return index;
    __ TrapIf(__ TruncateWord64ToWord32(__ Word64ShiftRightLogical(index, 32)),
              OpIndex::Invalid(), TrapId::kTrapMemOutOfBounds);
    return OpIndex(__ TruncateWord64ToWord32(index));
  }

  V<Smi> ChangeUint31ToSmi(V<Word32> value) {
    if constexpr (COMPRESS_POINTERS_BOOL) {
      return V<Smi>::Cast(
          __ Word32ShiftLeft(value, kSmiShiftSize + kSmiTagSize));
    } else {
      return V<Smi>::Cast(__ WordPtrShiftLeft(__ ChangeUint32ToUintPtr(value),
                                              kSmiShiftSize + kSmiTagSize));
    }
  }

  V<Word32> ChangeSmiToUint32(V<Smi> value) {
    if constexpr (COMPRESS_POINTERS_BOOL) {
      return __ Word32ShiftRightLogical(V<Word32>::Cast(value),
                                        kSmiShiftSize + kSmiTagSize);
    } else {
      return __ TruncateWordPtrToWord32(__ WordPtrShiftRightLogical(
          V<WordPtr>::Cast(value), kSmiShiftSize + kSmiTagSize));
    }
  }

  V<WordPtr> BuildDecodeExternalCodePointer(V<Word32> handle) {
#ifdef V8_ENABLE_SANDBOX
    V<Word32> index =
        __ Word32ShiftRightLogical(handle, kCodePointerHandleShift);
    V<WordPtr> offset = __ ChangeUint32ToUintPtr(
        __ Word32ShiftLeft(index, kCodePointerTableEntrySizeLog2));
    V<WordPtr> table =
        __ ExternalConstant(ExternalReference::code_pointer_table_address());
    return __ Load(table, offset, LoadOp::Kind::RawAligned(),
                   MemoryRepresentation::PointerSized());
#else
    UNREACHABLE();
#endif
  }

  void BuildEncodeException32BitValue(V<FixedArray> values_array,
                                      uint32_t index, V<Word32> value) {
    V<Smi> upper_half =
        ChangeUint31ToSmi(__ Word32ShiftRightLogical(value, 16));
    __ StoreFixedArrayElement(values_array, index, upper_half,
                              compiler::kNoWriteBarrier);
    V<Smi> lower_half = ChangeUint31ToSmi(__ Word32BitwiseAnd(value, 0xffffu));
    __ StoreFixedArrayElement(values_array, index + 1, lower_half,
                              compiler::kNoWriteBarrier);
  }

  V<Word32> BuildDecodeException32BitValue(V<FixedArray> exception_values_array,
                                           int index) {
    V<Word32> upper_half = __ Word32ShiftLeft(
        ChangeSmiToUint32(V<Smi>::Cast(
            __ LoadFixedArrayElement(exception_values_array, index))),
        16);
    V<Word32> lower_half = ChangeSmiToUint32(V<Smi>::Cast(
        __ LoadFixedArrayElement(exception_values_array, index + 1)));
    return __ Word32BitwiseOr(upper_half, lower_half);
  }

  V<Word64> BuildDecodeException64BitValue(V<FixedArray> exception_values_array,
                                           int index) {
    V<Word64> upper_half = __ Word64ShiftLeft(
        __ ChangeUint32ToUint64(
            BuildDecodeException32BitValue(exception_values_array, index)),
        32);
    V<Word64> lower_half = __ ChangeUint32ToUint64(
        BuildDecodeException32BitValue(exception_values_array, index + 2));
    return __ Word64BitwiseOr(upper_half, lower_half);
  }

  void UnpackWasmException(FullDecoder* decoder, V<Tagged> exception,
                           base::Vector<Value> values) {
    V<FixedArray> exception_values_array = CallBuiltinThroughJumptable(
        decoder, Builtin::kWasmGetOwnProperty,
        {exception, LOAD_IMMUTABLE_ROOT(wasm_exception_values_symbol),
         LOAD_IMMUTABLE_INSTANCE_FIELD(instance_node_, NativeContext,
                                       MemoryRepresentation::TaggedPointer())});

    int index = 0;
    for (Value& value : values) {
      switch (value.type.kind()) {
        case kI32:
          value.op =
              BuildDecodeException32BitValue(exception_values_array, index);
          index += 2;
          break;
        case kI64:
          value.op =
              BuildDecodeException64BitValue(exception_values_array, index);
          index += 4;
          break;
        case kF32:
          value.op = __ BitcastWord32ToFloat32(
              BuildDecodeException32BitValue(exception_values_array, index));
          index += 2;
          break;
        case kF64:
          value.op = __ BitcastWord64ToFloat64(
              BuildDecodeException64BitValue(exception_values_array, index));
          index += 4;
          break;
        case kS128: {
          value.op = __ Simd128Splat(
              BuildDecodeException32BitValue(exception_values_array, index),
              compiler::turboshaft::Simd128SplatOp::Kind::kI32x4);
          index += 2;
          using Kind = compiler::turboshaft::Simd128ReplaceLaneOp::Kind;
          value.op = __ Simd128ReplaceLane(
              value.op,
              BuildDecodeException32BitValue(exception_values_array, index),
              Kind::kI32x4, 1);
          index += 2;
          value.op = __ Simd128ReplaceLane(
              value.op,
              BuildDecodeException32BitValue(exception_values_array, index),
              Kind::kI32x4, 2);
          index += 2;
          value.op = __ Simd128ReplaceLane(
              value.op,
              BuildDecodeException32BitValue(exception_values_array, index),
              Kind::kI32x4, 3);
          index += 2;
          break;
        }
        case kRtt:
        case kRef:
        case kRefNull:
          value.op = __ LoadFixedArrayElement(exception_values_array, index);
          index++;
          break;
        case kI8:
        case kI16:
        case kVoid:
        case kBottom:
          UNREACHABLE();
      }
    }
  }

  void AsmjsStoreMem(V<Word32> index, OpIndex value,
                     MemoryRepresentation repr) {
    // Since asmjs does not support unaligned accesses, we can bounds-check
    // ignoring the access size.
    IF (LIKELY(
            __ Uint32LessThan(index, __ TruncateWordPtrToWord32(MemSize(0))))) {
      __ Store(MemStart(0), __ ChangeUint32ToUintPtr(index), value,
               StoreOp::Kind::RawAligned(), repr, compiler::kNoWriteBarrier, 0);
    }
    END_IF
  }

  OpIndex AsmjsLoadMem(V<Word32> index, MemoryRepresentation repr) {
    // Since asmjs does not support unaligned accesses, we can bounds-check
    // ignoring the access size.
    Variable result = __ NewVariable(repr.ToRegisterRepresentation());

    IF (LIKELY(
            __ Uint32LessThan(index, __ TruncateWordPtrToWord32(MemSize(0))))) {
      __ SetVariable(result, __ Load(MemStart(0), __ ChangeInt32ToIntPtr(index),
                                     LoadOp::Kind::RawAligned(), repr));
    }
    ELSE {
      switch (repr) {
        case MemoryRepresentation::Int8():
        case MemoryRepresentation::Int16():
        case MemoryRepresentation::Int32():
        case MemoryRepresentation::Uint8():
        case MemoryRepresentation::Uint16():
        case MemoryRepresentation::Uint32():
          __ SetVariable(result, __ Word32Constant(0));
          break;
        case MemoryRepresentation::Float32():
          __ SetVariable(result, __ Float32Constant(
                                     std::numeric_limits<float>::quiet_NaN()));
          break;
        case MemoryRepresentation::Float64():
          __ SetVariable(result, __ Float64Constant(
                                     std::numeric_limits<double>::quiet_NaN()));
          break;
        default:
          UNREACHABLE();
      }
    }
    END_IF

    OpIndex result_op = __ GetVariable(result);
    __ SetVariable(result, OpIndex::Invalid());
    return result_op;
  }

  void BoundsCheckArray(V<HeapObject> array, V<Word32> index,
                        ValueType array_type) {
    if (V8_UNLIKELY(v8_flags.experimental_wasm_skip_bounds_checks)) {
      if (array_type.is_nullable()) {
        __ AssertNotNull(array, array_type, TrapId::kTrapNullDereference);
      }
    } else {
      OpIndex length = __ ArrayLength(array, array_type.is_nullable()
                                                 ? compiler::kWithNullCheck
                                                 : compiler::kWithoutNullCheck);
      __ TrapIfNot(__ Uint32LessThan(index, length), OpIndex::Invalid(),
                   TrapId::kTrapArrayOutOfBounds);
    }
  }

  void BoundsCheckArrayWithLength(V<HeapObject> array, V<Word32> index,
                                  V<Word32> length,
                                  compiler::CheckForNull null_check) {
    if (V8_UNLIKELY(v8_flags.experimental_wasm_skip_bounds_checks)) return;
    V<Word32> array_length = __ ArrayLength(array, null_check);
    V<Word32> range_end = __ Word32Add(index, length);
    V<Word32> range_valid = __ Word32BitwiseAnd(
        // OOB if (index + length > array.len).
        __ Uint32LessThanOrEqual(range_end, array_length),
        // OOB if (index + length) overflows.
        __ Uint32LessThanOrEqual(index, range_end));
    __ TrapIfNot(range_valid, OpIndex::Invalid(),
                 TrapId::kTrapArrayOutOfBounds);
  }

  void BrOnCastImpl(FullDecoder* decoder, V<Map> rtt,
                    compiler::WasmTypeCheckConfig config, const Value& object,
                    Value* value_on_branch, uint32_t br_depth,
                    bool null_succeeds) {
    OpIndex cast_succeeds = __ WasmTypeCheck(object.op, rtt, config);
    IF (cast_succeeds) {
      // Narrow type for the successful cast target branch.
      Forward(decoder, object, value_on_branch);
      BrOrRet(decoder, br_depth, 0);
    }
    END_IF
    // Note: Differently to below for br_on_cast_fail, we do not Forward
    // the value here to perform a TypeGuard. It can't be done here due to
    // asymmetric decoder code. A Forward here would be popped from the stack
    // and ignored by the decoder. Therefore the decoder has to call Forward
    // itself.
  }

  void BrOnCastFailImpl(FullDecoder* decoder, V<Map> rtt,
                        compiler::WasmTypeCheckConfig config,
                        const Value& object, Value* value_on_fallthrough,
                        uint32_t br_depth, bool null_succeeds) {
    OpIndex cast_succeeds = __ WasmTypeCheck(object.op, rtt, config);
    IF (__ Word32Equal(cast_succeeds, 0)) {
      // It is necessary in case of {null_succeeds} to forward the value.
      // This will add a TypeGuard to the non-null type (as in this case the
      // object is non-nullable).
      Forward(decoder, object, decoder->stack_value(1));
      BrOrRet(decoder, br_depth, 0);
    }
    END_IF
    // Narrow type for the successful cast fallthrough branch.
    Forward(decoder, object, value_on_fallthrough);
  }

  V<HeapObject> ArrayNewImpl(FullDecoder* decoder, uint32_t index,
                             const ArrayType* array_type, OpIndex length,
                             OpIndex initial_value) {
    // Initialize the array header.
    V<Map> rtt = __ RttCanon(instance_node_, index);
    V<HeapObject> array = __ WasmAllocateArray(rtt, length, array_type);
    // Initialize the elements.
    ArrayFillImpl(array, __ Word32Constant(0), initial_value, length,
                  array_type, false);
    return array;
  }

  V<HeapObject> StructNewImpl(const StructIndexImmediate& imm, OpIndex args[]) {
    V<Map> rtt = __ RttCanon(instance_node_, imm.index);

    V<HeapObject> struct_value = __ WasmAllocateStruct(rtt, imm.struct_type);
    for (uint32_t i = 0; i < imm.struct_type->field_count(); ++i) {
      __ StructSet(struct_value, args[i], imm.struct_type, imm.index, i,
                   compiler::kWithoutNullCheck);
    }
    // If this assert fails then initialization of padding field might be
    // necessary.
    static_assert(Heap::kMinObjectSizeInTaggedWords == 2 &&
                      WasmStruct::kHeaderSize == 2 * kTaggedSize,
                  "empty struct might require initialization of padding field");
    return struct_value;
  }

  bool IsSimd128ZeroConstant(OpIndex op) {
    DCHECK_IMPLIES(!op.valid(), __ generating_unreachable_operations());
    if (__ generating_unreachable_operations()) return false;
    const Simd128ConstantOp* s128_op =
        __ output_graph().Get(op).TryCast<Simd128ConstantOp>();
    return s128_op && s128_op->IsZero();
  }

  void ArrayFillImpl(V<HeapObject> array, OpIndex index, OpIndex value,
                     OpIndex length, const wasm::ArrayType* type,
                     bool emit_write_barrier) {
    wasm::ValueType element_type = type->element_type();

    // Initialize the array. Use an external function for large arrays with
    // null/number initializer. Use a loop for small arrays and reference arrays
    // with a non-null initial value.
    Label<> done(&asm_);
    LoopLabel<Word32> loop(&asm_);

    // The builtin cannot handle s128 values other than 0.
    if (!(element_type == wasm::kWasmS128 && !IsSimd128ZeroConstant(value))) {
      constexpr uint32_t kArrayNewMinimumSizeForMemSet = 16;
      GOTO_IF(__ Uint32LessThan(
                  length, __ Word32Constant(kArrayNewMinimumSizeForMemSet)),
              loop, index);
      OpIndex stack_slot = StoreInInt64StackSlot(value, element_type);
      MachineType arg_types[]{
          MachineType::TaggedPointer(), MachineType::Uint32(),
          MachineType::Uint32(),        MachineType::Uint32(),
          MachineType::Uint32(),        MachineType::Pointer()};
      MachineSignature sig(0, 6, arg_types);
      CallC(
          &sig, ExternalReference::wasm_array_fill(),
          {array, index, length, __ Word32Constant(emit_write_barrier ? 1 : 0),
           __ Word32Constant(element_type.raw_bit_field()), stack_slot});
      GOTO(done);
    } else {
      GOTO(loop, index);
    }
    LOOP(loop, current_index) {
      V<Word32> check =
          __ Uint32LessThan(current_index, __ Word32Add(index, length));
      GOTO_IF_NOT(check, done);
      __ ArraySet(array, current_index, value, type->element_type());
      current_index = __ Word32Add(current_index, 1);
      GOTO(loop, current_index);
    }
    BIND(done);
  }

  V<WordPtr> StoreInInt64StackSlot(OpIndex value, wasm::ValueType type) {
    OpIndex value_int64;
    switch (type.kind()) {
      case wasm::kI32:
      case wasm::kI8:
      case wasm::kI16:
        value_int64 = __ ChangeInt32ToInt64(value);
        break;
      case wasm::kI64:
        value_int64 = value;
        break;
      case wasm::kS128:
        // We can only get here if {value} is the constant 0.
        DCHECK(__ output_graph().Get(value).Cast<Simd128ConstantOp>().IsZero());
        value_int64 = __ Word64Constant(uint64_t{0});
        break;
      case wasm::kF32:
        value_int64 = __ ChangeUint32ToUint64(__ BitcastFloat32ToWord32(value));
        break;
      case wasm::kF64:
        value_int64 = __ BitcastFloat64ToWord64(value);
        break;
      case wasm::kRefNull:
      case wasm::kRef:
        value_int64 = kTaggedSize == 4 ? __ ChangeInt32ToInt64(value) : value;
        break;
      case wasm::kRtt:
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }

    MemoryRepresentation int64_rep = MemoryRepresentation::Int64();
    V<WordPtr> stack_slot =
        __ StackSlot(int64_rep.SizeInBytes(), int64_rep.SizeInBytes());
    __ Store(stack_slot, value_int64, StoreOp::Kind::RawAligned(), int64_rep,
             compiler::WriteBarrierKind::kNoWriteBarrier);
    return stack_slot;
  }

  void InlineWasmCall(FullDecoder* decoder, uint32_t func_index,
                      const FunctionSig* sig, uint32_t feedback_case,
                      const Value args[], Value returns[]) {
    const WasmFunction& inlinee = decoder->module_->functions[func_index];

    std::vector<OpIndex> inlinee_args(inlinee.sig->parameter_count() + 1);
    inlinee_args[0] = instance_node_;
    for (size_t i = 0; i < inlinee.sig->parameter_count(); i++) {
      inlinee_args[i + 1] = args[i].op;
    }

    base::Vector<const uint8_t> function_bytes =
        wire_bytes_->GetCode(inlinee.code);

    const wasm::FunctionBody inlinee_body{inlinee.sig, inlinee.code.offset(),
                                          function_bytes.begin(),
                                          function_bytes.end()};

    // If the inlinee was not validated before, do that now.
    if (V8_UNLIKELY(!decoder->module_->function_was_validated(func_index))) {
      if (ValidateFunctionBody(decoder->zone_, decoder->enabled_,
                               decoder->module_, decoder->detected_,
                               inlinee_body)
              .failed()) {
        // At this point we cannot easily raise a compilation error any more.
        // Since this situation is highly unlikely though, we just ignore this
        // inlinee, emit a regular call, and move on. The same validation error
        // will be triggered again when actually compiling the invalid function.
        V<WordPtr> callee =
            __ RelocatableConstant(func_index, RelocInfo::WASM_CALL);
        BuildWasmCall(decoder, sig, callee, instance_node_, args, returns);
        return;
      }
      decoder->module_->set_function_validated(func_index);
    }

    Mode inlinee_mode =
        mode_ != kInlinedWithCatch && decoder->current_catch() == -1
            ? kInlinedUnhandled
            : kInlinedWithCatch;
    // TODO(14108): If this is nested inlined, can we forward the callers's
    // catch block instead?
    TSBlock* callee_catch_block =
        inlinee_mode == kInlinedUnhandled ? nullptr : __ NewBlock();
    TSBlock* callee_return_block = __ NewBlock();

    WasmFullDecoder<Decoder::FullValidationTag,
                    TurboshaftGraphBuildingInterface>
        inlinee_decoder(decoder->zone_, decoder->module_, decoder->enabled_,
                        decoder->detected_, inlinee_body, asm_, assumptions_,
                        inlining_positions_, func_index, wire_bytes_,
                        &inlinee_args, callee_return_block, callee_catch_block);
    size_t inlining_id = inlining_positions_->size();
    SourcePosition call_position = SourcePosition(
        decoder->position(), inlining_id_ == kNoInliningId ? -1 : inlining_id_);
    inlining_positions_->push_back(
        {static_cast<int>(func_index), call_position});
    inlinee_decoder.interface().set_inlining_id(
        static_cast<uint8_t>(inlining_id));
    inlinee_decoder.interface().set_parent_position(call_position);
    if (v8_flags.liftoff) {
      if (inlining_decisions_ && inlining_decisions_->feedback_found()) {
        inlinee_decoder.interface().set_inlining_decisions(
            inlining_decisions_
                ->function_calls()[feedback_slot_][feedback_case]);
      }
    } else {
      no_liftoff_inlining_budget_ -= inlinee.code.length();
      inlinee_decoder.interface().set_no_liftoff_inlining_budget(
          no_liftoff_inlining_budget_);
    }
    inlinee_decoder.Decode();
    // Turboshaft runs with validation, but the function should already be
    // validated, so graph building must always succeed, unless we bailed out.
    DCHECK_IMPLIES(!inlinee_decoder.ok(),
                   inlinee_decoder.interface().did_bailout());

    DCHECK_IMPLIES(inlinee_mode == kInlinedUnhandled,
                   inlinee_decoder.interface().return_exception_phis().empty());

    if (inlinee_mode == kInlinedWithCatch &&
        !inlinee_decoder.interface().return_exception_phis().empty()) {
      // We need to handle exceptions in the inlined call.
      __ Bind(callee_catch_block);
      OpIndex exception = MaybePhi(
          inlinee_decoder.interface().return_exception_phis(), kWasmExternRef);
      bool handled_in_this_frame = decoder->current_catch() != -1;
      TSBlock* catch_block;
      if (handled_in_this_frame) {
        Control* current_catch =
            decoder->control_at(decoder->control_depth_of_current_catch());
        catch_block = current_catch->false_or_loop_or_catch_block;
      } else {
        DCHECK_EQ(mode_, kInlinedWithCatch);
        catch_block = return_catch_block_;
      }
      if (handled_in_this_frame) {
        SetupControlFlowEdge(decoder, catch_block, 0, exception);
      } else {
        if (exception.valid()) return_exception_phis_.emplace_back(exception);
      }
      __ Goto(catch_block);
    }

    __ Bind(callee_return_block);
    BlockPhis& return_phis = inlinee_decoder.interface().return_phis();
    for (size_t i = 0; i < inlinee.sig->return_count(); i++) {
      returns[i].op =
          MaybePhi(return_phis.phi_inputs[i], return_phis.phi_types[i]);
    }

    if (!v8_flags.liftoff) {
      set_no_liftoff_inlining_budget(
          inlinee_decoder.interface().no_liftoff_inlining_budget());
    }
  }

  TrapId GetTrapIdForTrap(wasm::TrapReason reason) {
    switch (reason) {
#define TRAPREASON_TO_TRAPID(name)                                 \
  case wasm::k##name:                                              \
    static_assert(static_cast<int>(TrapId::k##name) ==             \
                      static_cast<int>(Builtin::kThrowWasm##name), \
                  "trap id mismatch");                             \
    return TrapId::k##name;
      FOREACH_WASM_TRAPREASON(TRAPREASON_TO_TRAPID)
#undef TRAPREASON_TO_TRAPID
      default:
        UNREACHABLE();
    }
  }

  // We need this shift so that resulting OpIndex offsets are multiples of
  // `sizeof(OperationStorageSlot)`.
  static constexpr int kPositionFieldShift = 3;
  static_assert(sizeof(compiler::turboshaft::OperationStorageSlot) ==
                1 << kPositionFieldShift);
  static constexpr int kPositionFieldSize = 23;
  static_assert(kV8MaxWasmFunctionSize < (1 << kPositionFieldSize));
  static constexpr int kInliningIdFieldSize = 6;
  static constexpr uint8_t kNoInliningId = 63;
  static_assert((1 << kInliningIdFieldSize) - 1 == kNoInliningId);
  // We need to assign inlining_ids to inlined nodes.
  static_assert(kNoInliningId > InliningTree::kMaxInlinedCount);

  // We encode the wasm code position and the inlining index in an OpIndex
  // stored in the output graph's node origins.
  using PositionField =
      base::BitField<WasmCodePosition, kPositionFieldShift, kPositionFieldSize>;
  using InliningIdField = PositionField::Next<uint8_t, kInliningIdFieldSize>;

  OpIndex WasmPositionToOpIndex(WasmCodePosition position, int inlining_id) {
    return OpIndex(PositionField::encode(position) |
                   InliningIdField::encode(inlining_id));
  }

  SourcePosition OpIndexToSourcePosition(OpIndex index) {
    DCHECK(index.valid());
    uint8_t inlining_id = InliningIdField::decode(index.offset());
    return SourcePosition(PositionField::decode(index.offset()),
                          inlining_id == kNoInliningId ? -1 : inlining_id);
  }

  BranchHint GetBranchHint(FullDecoder* decoder) {
    WasmBranchHint hint =
        branch_hints_ ? branch_hints_->GetHintFor(decoder->pc_relative_offset())
                      : WasmBranchHint::kNoHint;
    switch (hint) {
      case WasmBranchHint::kNoHint:
        return BranchHint::kNone;
      case WasmBranchHint::kUnlikely:
        return BranchHint::kFalse;
      case WasmBranchHint::kLikely:
        return BranchHint::kTrue;
    }
  }

  Assembler& Asm() { return asm_; }

  bool inlining_enabled(FullDecoder* decoder) {
    return decoder->enabled_.has_inlining() || decoder->module_->is_wasm_gc;
  }

  bool should_inline(int feedback_slot, int size) {
    if (v8_flags.liftoff) {
      if (inlining_decisions_ && inlining_decisions_->feedback_found()) {
        DCHECK_GT(inlining_decisions_->function_calls().size(), feedback_slot);
        // We should inline if at least one case for this feedback slot needs
        // to be inlined.
        for (InliningTree* tree :
             inlining_decisions_->function_calls()[feedback_slot]) {
          if (tree && tree->is_inlined()) return true;
        }
        return false;
      } else {
        return false;
      }
    } else {
      // We check the flag here because we want the ability to force inlining
      // off in unit tests, whereas {inlining_enabled()} turns it on for all
      // WasmGC modules.
      return v8_flags.experimental_wasm_inlining &&
             size < no_liftoff_inlining_budget_ &&
             inlining_positions_->size() < InliningTree::kMaxInlinedCount;
    }
  }

  void set_inlining_decisions(InliningTree* inlining_decisions) {
    inlining_decisions_ = inlining_decisions;
  }

  BlockPhis& return_phis() { return return_phis_; }
  std::vector<OpIndex>& return_exception_phis() {
    return return_exception_phis_;
  }
  void set_inlining_id(uint8_t inlining_id) {
    DCHECK_NE(inlining_id, kNoInliningId);
    inlining_id_ = inlining_id;
  }
  void set_parent_position(SourcePosition position) {
    parent_position_ = position;
  }
  int no_liftoff_inlining_budget() { return no_liftoff_inlining_budget_; }
  void set_no_liftoff_inlining_budget(int no_liftoff_inlining_budget) {
    no_liftoff_inlining_budget_ = no_liftoff_inlining_budget;
  }

  Mode mode_;
  V<WasmInstanceObject> instance_node_;
  std::unordered_map<TSBlock*, BlockPhis> block_phis_;
  Assembler& asm_;
  AssumptionsJournal* assumptions_;
  ZoneVector<WasmInliningPosition>* inlining_positions_;
  uint8_t inlining_id_ = kNoInliningId;
  std::vector<OpIndex> ssa_env_;
  bool did_bailout_ = false;
  compiler::NullCheckStrategy null_check_strategy_ =
      trap_handler::IsTrapHandlerEnabled() && V8_STATIC_ROOTS_BOOL
          ? compiler::NullCheckStrategy::kTrapHandler
          : compiler::NullCheckStrategy::kExplicit;
  int func_index_;
  const WireBytesStorage* wire_bytes_;
  const BranchHintMap* branch_hints_ = nullptr;
  InliningTree* inlining_decisions_ = nullptr;
  int feedback_slot_ = -1;
  // Inlining budget in case of --no-liftoff.
  int no_liftoff_inlining_budget_ = 0;

  /* Used for inlining modes */
  // Contains real parameters for this inlined function, including the instance.
  // Used only in StartFunction();
  std::vector<OpIndex>* real_parameters_;
  // The block where this function returns its values (passed by the caller).
  TSBlock* return_block_ = nullptr;
  // The return values for this function. The caller will reconstruct each one
  // with a Phi.
  BlockPhis return_phis_;
  // The block where exceptions from this function are caught (passed by the
  // caller).
  TSBlock* return_catch_block_ = nullptr;
  // The exception values for this function (will be reconstructed in the caller
  // with a Phi).
  std::vector<OpIndex> return_exception_phis_;
  // The position of the call that is being inlined.
  SourcePosition parent_position_;
};

V8_EXPORT_PRIVATE bool BuildTSGraph(
    AccountingAllocator* allocator, WasmFeatures enabled,
    const WasmModule* module, WasmFeatures* detected, Graph& graph,
    const FunctionBody& func_body, const WireBytesStorage* wire_bytes,
    compiler::NodeOriginTable* node_origins, AssumptionsJournal* assumptions,
    ZoneVector<WasmInliningPosition>* inlining_positions, int func_index) {
  Zone zone(allocator, ZONE_NAME);
  Assembler assembler(graph, graph, &zone, node_origins);
  WasmFullDecoder<Decoder::FullValidationTag, TurboshaftGraphBuildingInterface>
      decoder(&zone, module, enabled, detected, func_body, assembler,
              assumptions, inlining_positions, func_index, wire_bytes);
  decoder.Decode();
  // Turboshaft runs with validation, but the function should already be
  // validated, so graph building must always succeed, unless we bailed out.
  DCHECK_IMPLIES(!decoder.ok(), decoder.interface().did_bailout());
  return decoder.ok();
}

#undef LOAD_IMMUTABLE_INSTANCE_FIELD
#undef LOAD_INSTANCE_FIELD
#undef LOAD_ROOT
#undef LOAD_IMMUTABLE_ROOT
#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::wasm
