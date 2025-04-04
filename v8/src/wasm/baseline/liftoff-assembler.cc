// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-assembler.h"

#include <sstream>

#include "src/base/optional.h"
#include "src/base/platform/memory.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/utils/ostreams.h"
#include "src/wasm/baseline/liftoff-register.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

using VarState = LiftoffAssembler::VarState;
using ValueKindSig = LiftoffAssembler::ValueKindSig;

constexpr ValueKind LiftoffAssembler::kIntPtrKind;
constexpr ValueKind LiftoffAssembler::kSmiKind;

namespace {

class StackTransferRecipe {
  struct RegisterMove {
    LiftoffRegister src;
    ValueKind kind;
    constexpr RegisterMove(LiftoffRegister src, ValueKind kind)
        : src(src), kind(kind) {}
  };

  struct RegisterLoad {
    enum LoadKind : uint8_t {
      kNop,           // no-op, used for high fp of a fp pair.
      kConstant,      // load a constant value into a register.
      kStack,         // fill a register from a stack slot.
      kLowHalfStack,  // fill a register from the low half of a stack slot.
      kHighHalfStack  // fill a register from the high half of a stack slot.
    };

    LoadKind load_kind;
    ValueKind kind;
    int32_t value;  // i32 constant value or stack offset, depending on kind.

    // Named constructors.
    static RegisterLoad Const(WasmValue constant) {
      if (constant.type().kind() == kI32) {
        return {kConstant, kI32, constant.to_i32()};
      }
      DCHECK_EQ(kI64, constant.type().kind());
      int32_t i32_const = static_cast<int32_t>(constant.to_i64());
      DCHECK_EQ(constant.to_i64(), i32_const);
      return {kConstant, kI64, i32_const};
    }
    static RegisterLoad Stack(int32_t offset, ValueKind kind) {
      return {kStack, kind, offset};
    }
    static RegisterLoad HalfStack(int32_t offset, RegPairHalf half) {
      return {half == kLowWord ? kLowHalfStack : kHighHalfStack, kI32, offset};
    }
    static RegisterLoad Nop() {
      // ValueKind does not matter.
      return {kNop, kI32, 0};
    }

   private:
    RegisterLoad(LoadKind load_kind, ValueKind kind, int32_t value)
        : load_kind(load_kind), kind(kind), value(value) {}
  };

 public:
  explicit StackTransferRecipe(LiftoffAssembler* wasm_asm) : asm_(wasm_asm) {}
  StackTransferRecipe(const StackTransferRecipe&) = delete;
  StackTransferRecipe& operator=(const StackTransferRecipe&) = delete;
  V8_INLINE ~StackTransferRecipe() { Execute(); }

  V8_INLINE void Execute() {
    // First, execute register moves. Then load constants and stack values into
    // registers.
    if (!move_dst_regs_.is_empty()) ExecuteMoves();
    DCHECK(move_dst_regs_.is_empty());
    if (!load_dst_regs_.is_empty()) ExecuteLoads();
    DCHECK(load_dst_regs_.is_empty());
    // Tell the compiler that the StackTransferRecipe is empty after this, so it
    // can eliminate a second {Execute} in the destructor.
    bool all_done = move_dst_regs_.is_empty() && load_dst_regs_.is_empty();
    V8_ASSUME(all_done);
    USE(all_done);
  }

  V8_INLINE void Transfer(const VarState& dst, const VarState& src) {
    DCHECK(CompatibleStackSlotTypes(dst.kind(), src.kind()));
    if (dst.is_stack()) {
      if (V8_UNLIKELY(!(src.is_stack() && src.offset() == dst.offset()))) {
        TransferToStack(dst.offset(), src);
      }
    } else if (dst.is_reg()) {
      LoadIntoRegister(dst.reg(), src);
    } else {
      DCHECK(dst.is_const());
      DCHECK_EQ(dst.i32_const(), src.i32_const());
    }
  }

  void TransferToStack(int dst_offset, const VarState& src) {
    switch (src.loc()) {
      case VarState::kStack:
        if (src.offset() != dst_offset) {
          asm_->MoveStackValue(dst_offset, src.offset(), src.kind());
        }
        break;
      case VarState::kRegister:
        asm_->Spill(dst_offset, src.reg(), src.kind());
        break;
      case VarState::kIntConst:
        asm_->Spill(dst_offset, src.constant());
        break;
    }
  }

  V8_INLINE void LoadIntoRegister(LiftoffRegister dst,
                                  const LiftoffAssembler::VarState& src) {
    if (src.is_reg()) {
      DCHECK_EQ(dst.reg_class(), src.reg_class());
      if (dst != src.reg()) MoveRegister(dst, src.reg(), src.kind());
    } else if (src.is_stack()) {
      LoadStackSlot(dst, src.offset(), src.kind());
    } else {
      DCHECK(src.is_const());
      LoadConstant(dst, src.constant());
    }
  }

  void LoadI64HalfIntoRegister(LiftoffRegister dst,
                               const LiftoffAssembler::VarState& src,
                               RegPairHalf half) {
    // Use CHECK such that the remaining code is statically dead if
    // {kNeedI64RegPair} is false.
    CHECK(kNeedI64RegPair);
    DCHECK_EQ(kI64, src.kind());
    switch (src.loc()) {
      case VarState::kStack:
        LoadI64HalfStackSlot(dst, src.offset(), half);
        break;
      case VarState::kRegister: {
        LiftoffRegister src_half =
            half == kLowWord ? src.reg().low() : src.reg().high();
        if (dst != src_half) MoveRegister(dst, src_half, kI32);
        break;
      }
      case VarState::kIntConst:
        int32_t value = src.i32_const();
        // The high word is the sign extension of the low word.
        if (half == kHighWord) value = value >> 31;
        LoadConstant(dst, WasmValue(value));
        break;
    }
  }

  void MoveRegister(LiftoffRegister dst, LiftoffRegister src, ValueKind kind) {
    DCHECK_NE(dst, src);
    DCHECK_EQ(dst.reg_class(), src.reg_class());
    DCHECK_EQ(reg_class_for(kind), src.reg_class());
    if (src.is_gp_pair()) {
      DCHECK_EQ(kI64, kind);
      if (dst.low() != src.low()) MoveRegister(dst.low(), src.low(), kI32);
      if (dst.high() != src.high()) MoveRegister(dst.high(), src.high(), kI32);
      return;
    }
    if (src.is_fp_pair()) {
      DCHECK_EQ(kS128, kind);
      if (dst.low() != src.low()) {
        MoveRegister(dst.low(), src.low(), kF64);
        MoveRegister(dst.high(), src.high(), kF64);
      }
      return;
    }
    if (move_dst_regs_.has(dst)) {
      DCHECK_EQ(register_move(dst)->src, src);
      // Non-fp registers can only occur with the exact same type.
      DCHECK_IMPLIES(!dst.is_fp(), register_move(dst)->kind == kind);
      // It can happen that one fp register holds both the f32 zero and the f64
      // zero, as the initial value for local variables. Move the value as f64
      // in that case.
      if (kind == kF64) register_move(dst)->kind = kF64;
      return;
    }
    move_dst_regs_.set(dst);
    ++*src_reg_use_count(src);
    *register_move(dst) = {src, kind};
  }

  void LoadConstant(LiftoffRegister dst, WasmValue value) {
    DCHECK(!load_dst_regs_.has(dst));
    load_dst_regs_.set(dst);
    if (dst.is_gp_pair()) {
      DCHECK_EQ(kI64, value.type().kind());
      int64_t i64 = value.to_i64();
      *register_load(dst.low()) =
          RegisterLoad::Const(WasmValue(static_cast<int32_t>(i64)));
      *register_load(dst.high()) =
          RegisterLoad::Const(WasmValue(static_cast<int32_t>(i64 >> 32)));
    } else {
      *register_load(dst) = RegisterLoad::Const(value);
    }
  }

  void LoadStackSlot(LiftoffRegister dst, uint32_t stack_offset,
                     ValueKind kind) {
    if (load_dst_regs_.has(dst)) {
      // It can happen that we spilled the same register to different stack
      // slots, and then we reload them later into the same dst register.
      // In that case, it is enough to load one of the stack slots.
      return;
    }
    load_dst_regs_.set(dst);
    if (dst.is_gp_pair()) {
      DCHECK_EQ(kI64, kind);
      *register_load(dst.low()) =
          RegisterLoad::HalfStack(stack_offset, kLowWord);
      *register_load(dst.high()) =
          RegisterLoad::HalfStack(stack_offset, kHighWord);
    } else if (dst.is_fp_pair()) {
      DCHECK_EQ(kS128, kind);
      // Only need register_load for low_gp since we load 128 bits at one go.
      // Both low and high need to be set in load_dst_regs_ but when iterating
      // over it, both low and high will be cleared, so we won't load twice.
      *register_load(dst.low()) = RegisterLoad::Stack(stack_offset, kind);
      *register_load(dst.high()) = RegisterLoad::Nop();
    } else {
      *register_load(dst) = RegisterLoad::Stack(stack_offset, kind);
    }
  }

  void LoadI64HalfStackSlot(LiftoffRegister dst, int offset, RegPairHalf half) {
    if (load_dst_regs_.has(dst)) {
      // It can happen that we spilled the same register to different stack
      // slots, and then we reload them later into the same dst register.
      // In that case, it is enough to load one of the stack slots.
      return;
    }
    load_dst_regs_.set(dst);
    *register_load(dst) = RegisterLoad::HalfStack(offset, half);
  }

 private:
  using MovesStorage =
      std::aligned_storage<kAfterMaxLiftoffRegCode * sizeof(RegisterMove),
                           alignof(RegisterMove)>::type;
  using LoadsStorage =
      std::aligned_storage<kAfterMaxLiftoffRegCode * sizeof(RegisterLoad),
                           alignof(RegisterLoad)>::type;

  ASSERT_TRIVIALLY_COPYABLE(RegisterMove);
  ASSERT_TRIVIALLY_COPYABLE(RegisterLoad);

  MovesStorage register_moves_;  // uninitialized
  LoadsStorage register_loads_;  // uninitialized
  int src_reg_use_count_[kAfterMaxLiftoffRegCode] = {0};
  LiftoffRegList move_dst_regs_;
  LiftoffRegList load_dst_regs_;
  LiftoffAssembler* const asm_;
  // Cache the last spill offset in case we need to spill for resolving move
  // cycles.
  int last_spill_offset_ = asm_->TopSpillOffset();

  RegisterMove* register_move(LiftoffRegister reg) {
    return reinterpret_cast<RegisterMove*>(&register_moves_) +
           reg.liftoff_code();
  }
  RegisterLoad* register_load(LiftoffRegister reg) {
    return reinterpret_cast<RegisterLoad*>(&register_loads_) +
           reg.liftoff_code();
  }
  int* src_reg_use_count(LiftoffRegister reg) {
    return src_reg_use_count_ + reg.liftoff_code();
  }

  void ExecuteMove(LiftoffRegister dst) {
    RegisterMove* move = register_move(dst);
    DCHECK_EQ(0, *src_reg_use_count(dst));
    asm_->Move(dst, move->src, move->kind);
    ClearExecutedMove(dst);
  }

  void ClearExecutedMove(LiftoffRegister dst) {
    DCHECK(move_dst_regs_.has(dst));
    move_dst_regs_.clear(dst);
    RegisterMove* move = register_move(dst);
    DCHECK_LT(0, *src_reg_use_count(move->src));
    if (--*src_reg_use_count(move->src)) return;
    // src count dropped to zero. If this is a destination register, execute
    // that move now.
    if (!move_dst_regs_.has(move->src)) return;
    ExecuteMove(move->src);
  }

  V8_NOINLINE V8_PRESERVE_MOST void ExecuteMoves() {
    // Execute all moves whose {dst} is not being used as src in another move.
    // If any src count drops to zero, also (transitively) execute the
    // corresponding move to that register.
    for (LiftoffRegister dst : move_dst_regs_) {
      // Check if already handled via transitivity in {ClearExecutedMove}.
      if (!move_dst_regs_.has(dst)) continue;
      if (*src_reg_use_count(dst)) continue;
      ExecuteMove(dst);
    }

    // All remaining moves are parts of a cycle. Just spill the first one, then
    // process all remaining moves in that cycle. Repeat for all cycles.
    while (!move_dst_regs_.is_empty()) {
      // TODO(clemensb): Use an unused register if available.
      LiftoffRegister dst = move_dst_regs_.GetFirstRegSet();
      RegisterMove* move = register_move(dst);
      last_spill_offset_ += LiftoffAssembler::SlotSizeForType(move->kind);
      LiftoffRegister spill_reg = move->src;
      asm_->Spill(last_spill_offset_, spill_reg, move->kind);
      // Remember to reload into the destination register later.
      LoadStackSlot(dst, last_spill_offset_, move->kind);
      ClearExecutedMove(dst);
    }
  }

  V8_NOINLINE V8_PRESERVE_MOST void ExecuteLoads() {
    for (LiftoffRegister dst : load_dst_regs_) {
      RegisterLoad* load = register_load(dst);
      switch (load->load_kind) {
        case RegisterLoad::kNop:
          break;
        case RegisterLoad::kConstant:
          asm_->LoadConstant(dst, load->kind == kI64
                                      ? WasmValue(int64_t{load->value})
                                      : WasmValue(int32_t{load->value}));
          break;
        case RegisterLoad::kStack:
          if (kNeedS128RegPair && load->kind == kS128) {
            asm_->Fill(LiftoffRegister::ForFpPair(dst.fp()), load->value,
                       load->kind);
          } else {
            asm_->Fill(dst, load->value, load->kind);
          }
          break;
        case RegisterLoad::kLowHalfStack:
          // Half of a register pair, {dst} must be a gp register.
          asm_->FillI64Half(dst.gp(), load->value, kLowWord);
          break;
        case RegisterLoad::kHighHalfStack:
          // Half of a register pair, {dst} must be a gp register.
          asm_->FillI64Half(dst.gp(), load->value, kHighWord);
          break;
      }
    }
    load_dst_regs_ = {};
  }
};

class RegisterReuseMap {
 public:
  void Add(LiftoffRegister src, LiftoffRegister dst) {
    if (auto previous = Lookup(src)) {
      DCHECK_EQ(previous, dst);
      return;
    }
    map_.emplace_back(src);
    map_.emplace_back(dst);
  }

  base::Optional<LiftoffRegister> Lookup(LiftoffRegister src) {
    for (auto it = map_.begin(), end = map_.end(); it != end; it += 2) {
      if (it->is_gp_pair() == src.is_gp_pair() &&
          it->is_fp_pair() == src.is_fp_pair() && *it == src)
        return *(it + 1);
    }
    return {};
  }

 private:
  // {map_} holds pairs of <src, dst>.
  base::SmallVector<LiftoffRegister, 8> map_;
};

enum MergeKeepStackSlots : bool {
  kKeepStackSlots = true,
  kTurnStackSlotsIntoRegisters = false
};
enum MergeAllowConstants : bool {
  kConstantsAllowed = true,
  kConstantsNotAllowed = false
};
enum MergeAllowRegisters : bool {
  kRegistersAllowed = true,
  kRegistersNotAllowed = false
};
enum ReuseRegisters : bool {
  kReuseRegisters = true,
  kNoReuseRegisters = false
};
// {InitMergeRegion} is a helper used by {MergeIntoNewState} to initialize
// a part of the target stack ([target, target+count]) from [source,
// source+count]. The parameters specify how to initialize the part. The goal is
// to set up the region such that later merges (via {MergeStackWith} /
// {MergeFullStackWith} can successfully transfer their values to this new
// state.
void InitMergeRegion(LiftoffAssembler::CacheState* state,
                     const VarState* source, VarState* target, uint32_t count,
                     MergeKeepStackSlots keep_stack_slots,
                     MergeAllowConstants allow_constants,
                     MergeAllowRegisters allow_registers,
                     ReuseRegisters reuse_registers, LiftoffRegList used_regs,
                     int new_stack_offset, StackTransferRecipe& transfers) {
  RegisterReuseMap register_reuse_map;
  for (const VarState* source_end = source + count; source < source_end;
       ++source, ++target) {
    if (source->is_stack() && keep_stack_slots) {
      *target = *source;
      // If {new_stack_offset} is set, we want to recompute stack offsets for
      // the region we are initializing such that they are contiguous. If
      // {new_stack_offset} is zero (which is an illegal stack offset), we just
      // keep the source offsets.
      if (new_stack_offset) {
        new_stack_offset =
            LiftoffAssembler::NextSpillOffset(source->kind(), new_stack_offset);
        if (new_stack_offset != source->offset()) {
          target->set_offset(new_stack_offset);
          transfers.TransferToStack(new_stack_offset, *source);
        }
      }
      continue;
    }
    if (source->is_const() && allow_constants) {
      *target = *source;
      DCHECK(!new_stack_offset);
      continue;
    }
    base::Optional<LiftoffRegister> reg;
    bool needs_reg_transfer = true;
    if (allow_registers) {
      // First try: Keep the same register, if it's free.
      if (source->is_reg() && state->is_free(source->reg())) {
        reg = source->reg();
        needs_reg_transfer = false;
      }
      // Second try: Use the same register we used before (if we reuse
      // registers).
      if (!reg && reuse_registers) {
        reg = register_reuse_map.Lookup(source->reg());
      }
      // Third try: Use any free register.
      RegClass rc = reg_class_for(source->kind());
      if (!reg && state->has_unused_register(rc, used_regs)) {
        reg = state->unused_register(rc, used_regs);
      }
    }
    // See above: Recompute the stack offset if requested.
    int target_offset = source->offset();
    if (new_stack_offset) {
      new_stack_offset =
          LiftoffAssembler::NextSpillOffset(source->kind(), new_stack_offset);
      target_offset = new_stack_offset;
    }
    if (reg) {
      if (needs_reg_transfer) transfers.LoadIntoRegister(*reg, *source);
      if (reuse_registers) register_reuse_map.Add(source->reg(), *reg);
      state->inc_used(*reg);
      *target = VarState(source->kind(), *reg, target_offset);
    } else {
      // No free register; make this a stack slot.
      *target = VarState(source->kind(), target_offset);
      transfers.TransferToStack(target_offset, *source);
    }
  }
}

}  // namespace

LiftoffAssembler::CacheState LiftoffAssembler::MergeIntoNewState(
    uint32_t num_locals, uint32_t arity, uint32_t stack_depth) {
  CacheState target{zone()};

  // The source state looks like this:
  // |------locals------|---(stack prefix)---|--(discarded)--|----merge----|
  //  <-- num_locals --> <-- stack_depth  -->                 <-- arity -->
  //
  // We compute the following target state from it:
  // |------locals------|---(stack prefix)----|----merge----|
  //  <-- num_locals --> <-- stack_depth   --> <-- arity -->
  //
  // The target state will have dropped the "(discarded)" region, and the
  // "locals" and "merge" regions have been modified to avoid any constants and
  // avoid duplicate register uses. This ensures that later merges can
  // successfully transfer into the target state.
  // The "stack prefix" region will be identical for any source that merges into
  // that state.

  if (cache_state_.cached_instance != no_reg) {
    target.SetInstanceCacheRegister(cache_state_.cached_instance);
  }

  if (cache_state_.cached_mem_start != no_reg) {
    target.SetMemStartCacheRegister(cache_state_.cached_mem_start);
  }

  uint32_t target_height = num_locals + stack_depth + arity;

  target.stack_state.resize_no_init(target_height);

  const VarState* source_begin = cache_state_.stack_state.data();
  VarState* target_begin = target.stack_state.data();

  // Compute the starts of the different regions, for source and target (see
  // pictograms above).
  const VarState* locals_source = source_begin;
  const VarState* stack_prefix_source = source_begin + num_locals;
  const VarState* discarded_source = stack_prefix_source + stack_depth;
  const VarState* merge_source = cache_state_.stack_state.end() - arity;
  VarState* locals_target = target_begin;
  VarState* stack_prefix_target = target_begin + num_locals;
  VarState* merge_target = target_begin + num_locals + stack_depth;

  // Try to keep locals and the merge region in their registers. Registers used
  // multiple times need to be copied to another free register. Compute the list
  // of used registers.
  LiftoffRegList used_regs;
  for (auto& src : base::VectorOf(locals_source, num_locals)) {
    if (src.is_reg()) used_regs.set(src.reg());
  }
  // If there is more than one operand in the merge region, a stack-to-stack
  // move can interfere with a register reload, which would not be handled
  // correctly by the StackTransferRecipe. To avoid this, spill all registers in
  // this region.
  MergeAllowRegisters allow_registers =
      arity <= 1 ? kRegistersAllowed : kRegistersNotAllowed;
  if (allow_registers) {
    for (auto& src : base::VectorOf(merge_source, arity)) {
      if (src.is_reg()) used_regs.set(src.reg());
    }
  }

  StackTransferRecipe transfers(this);

  // The merge region is often empty, hence check for this before doing any
  // work (even though not needed for correctness).
  if (arity) {
    // Initialize the merge region. If this region moves, try to turn stack
    // slots into registers since we need to load the value anyways.
    MergeKeepStackSlots keep_merge_stack_slots =
        target_height == cache_state_.stack_height()
            ? kKeepStackSlots
            : kTurnStackSlotsIntoRegisters;
    // Shift spill offsets down to keep slots contiguous. We place the merge
    // region right after the "stack prefix", if it exists.
    int merge_region_stack_offset = discarded_source == source_begin
                                        ? StaticStackFrameSize()
                                        : discarded_source[-1].offset();
    InitMergeRegion(&target, merge_source, merge_target, arity,
                    keep_merge_stack_slots, kConstantsNotAllowed,
                    allow_registers, kNoReuseRegisters, used_regs,
                    merge_region_stack_offset, transfers);
  }

  // Initialize the locals region. Here, stack slots stay stack slots (because
  // they do not move). Try to keep register in registers, but avoid duplicates.
  if (num_locals) {
    InitMergeRegion(&target, locals_source, locals_target, num_locals,
                    kKeepStackSlots, kConstantsNotAllowed, kRegistersAllowed,
                    kNoReuseRegisters, used_regs, 0, transfers);
  }
  // Consistency check: All the {used_regs} are really in use now.
  DCHECK_EQ(used_regs, target.used_registers & used_regs);

  // Last, initialize the "stack prefix" region. Here, constants are allowed,
  // but registers which are already used for the merge region or locals must be
  // moved to other registers or spilled. If a register appears twice in the
  // source region, ensure to use the same register twice in the target region.
  if (stack_depth) {
    InitMergeRegion(&target, stack_prefix_source, stack_prefix_target,
                    stack_depth, kKeepStackSlots, kConstantsAllowed,
                    kRegistersAllowed, kReuseRegisters, used_regs, 0,
                    transfers);
  }

  return target;
}

void LiftoffAssembler::CacheState::Steal(CacheState& source) {
  // Just use the move assignment operator.
  *this = std::move(source);
}

void LiftoffAssembler::CacheState::Split(const CacheState& source) {
  // Call the private copy assignment operator.
  *this = source;
}

namespace {
int GetSafepointIndexForStackSlot(const VarState& slot) {
  // index = 0 is for the stack slot at 'fp + kFixedFrameSizeAboveFp -
  // kSystemPointerSize', the location of the current stack slot is 'fp -
  // slot.offset()'. The index we need is therefore '(fp +
  // kFixedFrameSizeAboveFp - kSystemPointerSize) - (fp - slot.offset())' =
  // 'slot.offset() + kFixedFrameSizeAboveFp - kSystemPointerSize'.
  // Concretely, the index of the first stack slot is '4'.
  return (slot.offset() + StandardFrameConstants::kFixedFrameSizeAboveFp -
          kSystemPointerSize) /
         kSystemPointerSize;
}
}  // namespace

void LiftoffAssembler::CacheState::GetTaggedSlotsForOOLCode(
    ZoneVector<int>* slots, LiftoffRegList* spills,
    SpillLocation spill_location) {
  for (const auto& slot : stack_state) {
    if (!is_reference(slot.kind())) continue;

    if (spill_location == SpillLocation::kTopOfStack && slot.is_reg()) {
      // Registers get spilled just before the call to the runtime. In {spills}
      // we store which of the spilled registers contain references, so that we
      // can add the spill slots to the safepoint.
      spills->set(slot.reg());
      continue;
    }
    DCHECK_IMPLIES(slot.is_reg(), spill_location == SpillLocation::kStackSlots);

    slots->push_back(GetSafepointIndexForStackSlot(slot));
  }
}

void LiftoffAssembler::CacheState::DefineSafepoint(
    SafepointTableBuilder::Safepoint& safepoint) {
  // Go in reversed order to set the higher bits first; this avoids cost for
  // growing the underlying bitvector.
  for (const auto& slot : base::Reversed(stack_state)) {
    if (is_reference(slot.kind())) {
      DCHECK(slot.is_stack());
      safepoint.DefineTaggedStackSlot(GetSafepointIndexForStackSlot(slot));
    }
  }
}

void LiftoffAssembler::CacheState::DefineSafepointWithCalleeSavedRegisters(
    SafepointTableBuilder::Safepoint& safepoint) {
  for (const auto& slot : stack_state) {
    if (!is_reference(slot.kind())) continue;
    if (slot.is_stack()) {
      safepoint.DefineTaggedStackSlot(GetSafepointIndexForStackSlot(slot));
    } else {
      DCHECK(slot.is_reg());
      safepoint.DefineTaggedRegister(slot.reg().gp().code());
    }
  }
  if (cached_instance != no_reg) {
    safepoint.DefineTaggedRegister(cached_instance.code());
  }
}

int LiftoffAssembler::GetTotalFrameSlotCountForGC() const {
  // The GC does not care about the actual number of spill slots, just about
  // the number of references that could be there in the spilling area. Note
  // that the offset of the first spill slot is kSystemPointerSize and not
  // '0'. Therefore we don't have to add '+1' here.
  return (max_used_spill_offset_ +
          StandardFrameConstants::kFixedFrameSizeAboveFp +
          ool_spill_space_size_) /
         kSystemPointerSize;
}

namespace {

AssemblerOptions DefaultLiftoffOptions() { return AssemblerOptions{}; }

}  // namespace

LiftoffAssembler::LiftoffAssembler(Zone* zone,
                                   std::unique_ptr<AssemblerBuffer> buffer)
    : MacroAssembler(nullptr, DefaultLiftoffOptions(), CodeObjectRequired::kNo,
                     std::move(buffer)),
      cache_state_(zone) {
  set_abort_hard(true);  // Avoid calls to Abort.
}

LiftoffAssembler::~LiftoffAssembler() {
  if (num_locals_ > kInlineLocalKinds) {
    base::Free(more_local_kinds_);
  }
}

LiftoffRegister LiftoffAssembler::LoadToRegister_Slow(VarState slot,
                                                      LiftoffRegList pinned) {
  DCHECK(!slot.is_reg());
  LiftoffRegister reg = GetUnusedRegister(reg_class_for(slot.kind()), pinned);
  LoadToFixedRegister(slot, reg);
  return reg;
}

LiftoffRegister LiftoffAssembler::LoadI64HalfIntoRegister(VarState slot,
                                                          RegPairHalf half) {
  if (slot.is_reg()) {
    return half == kLowWord ? slot.reg().low() : slot.reg().high();
  }
  LiftoffRegister dst = GetUnusedRegister(kGpReg, {});
  if (slot.is_stack()) {
    FillI64Half(dst.gp(), slot.offset(), half);
    return dst;
  }
  DCHECK(slot.is_const());
  int32_t half_word =
      static_cast<int32_t>(half == kLowWord ? slot.constant().to_i64()
                                            : slot.constant().to_i64() >> 32);
  LoadConstant(dst, WasmValue(half_word));
  return dst;
}

LiftoffRegister LiftoffAssembler::PeekToRegister(int index,
                                                 LiftoffRegList pinned) {
  DCHECK_LT(index, cache_state_.stack_state.size());
  VarState& slot = cache_state_.stack_state.end()[-1 - index];
  if (V8_LIKELY(slot.is_reg())) return slot.reg();
  LiftoffRegister reg = LoadToRegister(slot, pinned);
  cache_state_.inc_used(reg);
  slot.MakeRegister(reg);
  return reg;
}

void LiftoffAssembler::DropValues(int count) {
  DCHECK_GE(cache_state_.stack_state.size(), count);
  for (VarState& slot :
       base::VectorOf(cache_state_.stack_state.end() - count, count)) {
    if (slot.is_reg()) {
      cache_state_.dec_used(slot.reg());
    }
  }
  cache_state_.stack_state.pop_back(count);
}

void LiftoffAssembler::DropExceptionValueAtOffset(int offset) {
  auto* dropped = cache_state_.stack_state.begin() + offset;
  if (dropped->is_reg()) {
    cache_state_.dec_used(dropped->reg());
  }
  // Compute the stack offset that the remaining slots are based on.
  int stack_offset =
      offset == 0 ? StaticStackFrameSize() : dropped[-1].offset();
  // Move remaining slots down.
  for (VarState *slot = dropped, *end = cache_state_.stack_state.end() - 1;
       slot != end; ++slot) {
    *slot = *(slot + 1);
    stack_offset = NextSpillOffset(slot->kind(), stack_offset);
    // Padding could allow us to exit early.
    if (slot->offset() == stack_offset) break;
    if (slot->is_stack()) {
      MoveStackValue(stack_offset, slot->offset(), slot->kind());
    }
    slot->set_offset(stack_offset);
  }
  cache_state_.stack_state.pop_back();
}

void LiftoffAssembler::PrepareLoopArgs(int num) {
  for (int i = 0; i < num; ++i) {
    VarState& slot = cache_state_.stack_state.end()[-1 - i];
    if (slot.is_stack()) continue;
    RegClass rc = reg_class_for(slot.kind());
    if (slot.is_reg()) {
      if (cache_state_.get_use_count(slot.reg()) > 1) {
        // If the register is used more than once, we cannot use it for the
        // merge. Move it to an unused register instead.
        LiftoffRegList pinned;
        pinned.set(slot.reg());
        LiftoffRegister dst_reg = GetUnusedRegister(rc, pinned);
        Move(dst_reg, slot.reg(), slot.kind());
        cache_state_.dec_used(slot.reg());
        cache_state_.inc_used(dst_reg);
        slot.MakeRegister(dst_reg);
      }
      continue;
    }
    LiftoffRegister reg = GetUnusedRegister(rc, {});
    LoadConstant(reg, slot.constant());
    slot.MakeRegister(reg);
    cache_state_.inc_used(reg);
  }
}

void LiftoffAssembler::PrepareForBranch(uint32_t arity, LiftoffRegList pinned) {
  VarState* stack_base = cache_state_.stack_state.data();
  for (auto slots :
       {base::VectorOf(stack_base + cache_state_.stack_state.size() - arity,
                       arity),
        base::VectorOf(stack_base, num_locals())}) {
    for (VarState& slot : slots) {
      if (slot.is_reg()) {
        // Registers used more than once can't be used for merges.
        if (cache_state_.get_use_count(slot.reg()) > 1) {
          RegClass rc = reg_class_for(slot.kind());
          if (cache_state_.has_unused_register(rc, pinned)) {
            LiftoffRegister dst_reg = cache_state_.unused_register(rc, pinned);
            Move(dst_reg, slot.reg(), slot.kind());
            cache_state_.inc_used(dst_reg);
            cache_state_.dec_used(slot.reg());
            slot.MakeRegister(dst_reg);
          } else {
            Spill(slot.offset(), slot.reg(), slot.kind());
            cache_state_.dec_used(slot.reg());
            slot.MakeStack();
          }
        }
        continue;
      }
      // Materialize constants.
      if (!slot.is_const()) continue;
      RegClass rc = reg_class_for(slot.kind());
      if (cache_state_.has_unused_register(rc, pinned)) {
        LiftoffRegister reg = cache_state_.unused_register(rc, pinned);
        LoadConstant(reg, slot.constant());
        cache_state_.inc_used(reg);
        slot.MakeRegister(reg);
      } else {
        Spill(slot.offset(), slot.constant());
        slot.MakeStack();
      }
    }
  }
}

#ifdef DEBUG
namespace {
bool SlotInterference(const VarState& a, const VarState& b) {
  return a.is_stack() && b.is_stack() &&
         b.offset() > a.offset() - value_kind_size(a.kind()) &&
         b.offset() - value_kind_size(b.kind()) < a.offset();
}

bool SlotInterference(const VarState& a, base::Vector<const VarState> v) {
  // Check the first 16 entries in {v}, then increase the step size to avoid
  // quadratic runtime on huge stacks. This logic checks 41 of the first 100
  // slots, 77 of the first 1000 and 115 of the first 10000.
  for (size_t idx = 0, end = v.size(); idx < end; idx += 1 + idx / 16) {
    if (SlotInterference(a, v[idx])) return true;
  }
  return false;
}
}  // namespace
#endif

void LiftoffAssembler::MergeFullStackWith(CacheState& target) {
  DCHECK_EQ(cache_state_.stack_height(), target.stack_height());
  // TODO(clemensb): Reuse the same StackTransferRecipe object to save some
  // allocations.
  StackTransferRecipe transfers(this);
  for (uint32_t i = 0, e = cache_state_.stack_height(); i < e; ++i) {
    transfers.Transfer(target.stack_state[i], cache_state_.stack_state[i]);
    DCHECK(!SlotInterference(target.stack_state[i],
                             base::VectorOf(cache_state_.stack_state) + i + 1));
  }

  // Full stack merging is only done for forward jumps, so we can just clear the
  // cache registers at the target in case of mismatch.
  if (cache_state_.cached_instance != target.cached_instance) {
    target.ClearCachedInstanceRegister();
  }
  if (cache_state_.cached_mem_start != target.cached_mem_start) {
    target.ClearCachedMemStartRegister();
  }
}

void LiftoffAssembler::MergeStackWith(CacheState& target, uint32_t arity,
                                      JumpDirection jump_direction) {
  // Before: ----------------|----- (discarded) ----|--- arity ---|
  //                         ^target_stack_height   ^stack_base   ^stack_height
  // After:  ----|-- arity --|
  //             ^           ^target_stack_height
  //             ^target_stack_base
  uint32_t stack_height = cache_state_.stack_height();
  uint32_t target_stack_height = target.stack_height();
  DCHECK_LE(target_stack_height, stack_height);
  DCHECK_LE(arity, target_stack_height);
  uint32_t stack_base = stack_height - arity;
  uint32_t target_stack_base = target_stack_height - arity;
  StackTransferRecipe transfers(this);
  for (uint32_t i = 0; i < target_stack_base; ++i) {
    transfers.Transfer(target.stack_state[i], cache_state_.stack_state[i]);
    DCHECK(!SlotInterference(
        target.stack_state[i],
        base::VectorOf(cache_state_.stack_state.data() + i + 1,
                       target_stack_base - i - 1)));
    DCHECK(!SlotInterference(
        target.stack_state[i],
        base::VectorOf(cache_state_.stack_state.data() + stack_base, arity)));
  }
  for (uint32_t i = 0; i < arity; ++i) {
    transfers.Transfer(target.stack_state[target_stack_base + i],
                       cache_state_.stack_state[stack_base + i]);
    DCHECK(!SlotInterference(
        target.stack_state[target_stack_base + i],
        base::VectorOf(cache_state_.stack_state.data() + stack_base + i + 1,
                       arity - i - 1)));
  }

  // Check whether the cached instance and/or memory start need to be moved to
  // another register. Register moves are executed as part of the
  // {StackTransferRecipe}. Remember whether the register content has to be
  // reloaded after executing the stack transfers.
  bool reload_instance = false;
  bool reload_mem_start = false;
  for (auto tuple :
       {std::make_tuple(&reload_instance, cache_state_.cached_instance,
                        &target.cached_instance),
        std::make_tuple(&reload_mem_start, cache_state_.cached_mem_start,
                        &target.cached_mem_start)}) {
    bool* reload = std::get<0>(tuple);
    Register src_reg = std::get<1>(tuple);
    Register* dst_reg = std::get<2>(tuple);
    // If the registers match, or the destination has no cache register, nothing
    // needs to be done.
    if (src_reg == *dst_reg || *dst_reg == no_reg) continue;
    // On forward jumps, just reset the cached register in the target state.
    if (jump_direction == kForwardJump) {
      target.ClearCacheRegister(dst_reg);
    } else if (src_reg != no_reg) {
      // If the source has the content but in the wrong register, execute a
      // register move as part of the stack transfer.
      transfers.MoveRegister(LiftoffRegister{*dst_reg},
                             LiftoffRegister{src_reg}, kIntPtrKind);
    } else {
      // Otherwise (the source state has no cached content), we reload later.
      *reload = true;
    }
  }

  // Now execute stack transfers and register moves/loads.
  transfers.Execute();

  if (reload_instance) {
    LoadInstanceFromFrame(target.cached_instance);
  }
  if (reload_mem_start) {
    // {target.cached_instance} already got restored above, so we can use it
    // if it exists.
    Register instance = target.cached_instance;
    if (instance == no_reg) {
      // We don't have the instance available yet. Store it into the target
      // mem_start, so that we can load the mem_start from there.
      instance = target.cached_mem_start;
      LoadInstanceFromFrame(instance);
    }
    LoadFromInstance(
        target.cached_mem_start, instance,
        ObjectAccess::ToTagged(WasmInstanceObject::kMemoryStartOffset),
        sizeof(size_t));
#ifdef V8_ENABLE_SANDBOX
    DecodeSandboxedPointer(target.cached_mem_start);
#endif
  }
}

void LiftoffAssembler::Spill(VarState* slot) {
  switch (slot->loc()) {
    case VarState::kStack:
      return;
    case VarState::kRegister:
      Spill(slot->offset(), slot->reg(), slot->kind());
      cache_state_.dec_used(slot->reg());
      break;
    case VarState::kIntConst:
      Spill(slot->offset(), slot->constant());
      break;
  }
  slot->MakeStack();
}

void LiftoffAssembler::SpillLocals() {
  for (uint32_t i = 0; i < num_locals_; ++i) {
    Spill(&cache_state_.stack_state[i]);
  }
}

void LiftoffAssembler::SpillAllRegisters() {
  for (uint32_t i = 0, e = cache_state_.stack_height(); i < e; ++i) {
    auto& slot = cache_state_.stack_state[i];
    if (!slot.is_reg()) continue;
    Spill(slot.offset(), slot.reg(), slot.kind());
    slot.MakeStack();
  }
  cache_state_.ClearAllCacheRegisters();
  cache_state_.reset_used_registers();
}

void LiftoffAssembler::ClearRegister(
    Register reg, std::initializer_list<Register*> possible_uses,
    LiftoffRegList pinned) {
  if (reg == cache_state()->cached_instance) {
    cache_state()->ClearCachedInstanceRegister();
    // We can return immediately. The instance is only used to load information
    // at the beginning of an instruction when values don't have to be in
    // specific registers yet. Therefore the instance should never be one of the
    // {possible_uses}.
    for (Register* use : possible_uses) {
      USE(use);
      DCHECK_NE(reg, *use);
    }
    return;
  } else if (reg == cache_state()->cached_mem_start) {
    cache_state()->ClearCachedMemStartRegister();
    // The memory start may be among the {possible_uses}, e.g. for an atomic
    // compare exchange. Therefore it is necessary to iterate over the
    // {possible_uses} below, and we cannot return early.
  } else if (cache_state()->is_used(LiftoffRegister(reg))) {
    SpillRegister(LiftoffRegister(reg));
  }
  Register replacement = no_reg;
  for (Register* use : possible_uses) {
    if (reg != *use) continue;
    if (replacement == no_reg) {
      replacement = GetUnusedRegister(kGpReg, pinned).gp();
      Move(replacement, reg, kIntPtrKind);
    }
    // We cannot leave this loop early. There may be multiple uses of {reg}.
    *use = replacement;
  }
}

namespace {
void PrepareStackTransfers(const ValueKindSig* sig,
                           compiler::CallDescriptor* call_descriptor,
                           const VarState* slots,
                           LiftoffStackSlots* stack_slots,
                           StackTransferRecipe* stack_transfers,
                           LiftoffRegList* param_regs) {
  // Process parameters backwards, to reduce the amount of Slot sorting for
  // the most common case - a normal Wasm Call. Slots will be mostly unsorted
  // in the Builtin call case.
  uint32_t call_desc_input_idx =
      static_cast<uint32_t>(call_descriptor->InputCount());
  uint32_t num_params = static_cast<uint32_t>(sig->parameter_count());
  for (uint32_t i = num_params; i > 0; --i) {
    const uint32_t param = i - 1;
    ValueKind kind = sig->GetParam(param);
    const bool is_gp_pair = kNeedI64RegPair && kind == kI64;
    const int num_lowered_params = is_gp_pair ? 2 : 1;
    const VarState& slot = slots[param];
    DCHECK(CompatibleStackSlotTypes(slot.kind(), kind));
    // Process both halfs of a register pair separately, because they are passed
    // as separate parameters. One or both of them could end up on the stack.
    for (int lowered_idx = 0; lowered_idx < num_lowered_params; ++lowered_idx) {
      const RegPairHalf half =
          is_gp_pair && lowered_idx == 0 ? kHighWord : kLowWord;
      --call_desc_input_idx;
      compiler::LinkageLocation loc =
          call_descriptor->GetInputLocation(call_desc_input_idx);
      if (loc.IsRegister()) {
        DCHECK(!loc.IsAnyRegister());
        RegClass rc = is_gp_pair ? kGpReg : reg_class_for(kind);
        int reg_code = loc.AsRegister();
        LiftoffRegister reg =
            LiftoffRegister::from_external_code(rc, kind, reg_code);
        param_regs->set(reg);
        if (is_gp_pair) {
          stack_transfers->LoadI64HalfIntoRegister(reg, slot, half);
        } else {
          stack_transfers->LoadIntoRegister(reg, slot);
        }
      } else {
        DCHECK(loc.IsCallerFrameSlot());
        int param_offset = -loc.GetLocation() - 1;
        stack_slots->Add(slot, slot.offset(), half, param_offset);
      }
    }
  }
}

}  // namespace

void LiftoffAssembler::PrepareBuiltinCall(
    const ValueKindSig* sig, compiler::CallDescriptor* call_descriptor,
    std::initializer_list<VarState> params) {
  LiftoffStackSlots stack_slots(this);
  StackTransferRecipe stack_transfers(this);
  LiftoffRegList param_regs;
  PrepareStackTransfers(sig, call_descriptor, params.begin(), &stack_slots,
                        &stack_transfers, &param_regs);
  SpillAllRegisters();
  int param_slots = static_cast<int>(call_descriptor->ParameterSlotCount());
  if (param_slots > 0) {
    stack_slots.Construct(param_slots);
  }
  // Execute the stack transfers before filling the instance register.
  stack_transfers.Execute();

  // Reset register use counters.
  cache_state_.reset_used_registers();
}

void LiftoffAssembler::PrepareCall(const ValueKindSig* sig,
                                   compiler::CallDescriptor* call_descriptor,
                                   Register* target, Register target_instance) {
  uint32_t num_params = static_cast<uint32_t>(sig->parameter_count());

  LiftoffStackSlots stack_slots(this);
  StackTransferRecipe stack_transfers(this);
  LiftoffRegList param_regs;

  // Move the target instance (if supplied) into the correct instance register.
  Register instance_reg = wasm::kGpParamRegisters[0];
  // Check that the call descriptor agrees. Input 0 is the call target, 1 is the
  // instance.
  DCHECK_EQ(
      instance_reg,
      Register::from_code(call_descriptor->GetInputLocation(1).AsRegister()));
  param_regs.set(instance_reg);
  if (target_instance == no_reg) target_instance = cache_state_.cached_instance;
  if (target_instance != no_reg && target_instance != instance_reg) {
    stack_transfers.MoveRegister(LiftoffRegister(instance_reg),
                                 LiftoffRegister(target_instance), kIntPtrKind);
  }

  int param_slots = static_cast<int>(call_descriptor->ParameterSlotCount());
  if (num_params) {
    uint32_t param_base = cache_state_.stack_height() - num_params;
    PrepareStackTransfers(sig, call_descriptor,
                          &cache_state_.stack_state[param_base], &stack_slots,
                          &stack_transfers, &param_regs);
  }

  // If the target register overlaps with a parameter register, then move the
  // target to another free register, or spill to the stack.
  if (target && param_regs.has(LiftoffRegister(*target))) {
    // Try to find another free register.
    LiftoffRegList free_regs = kGpCacheRegList.MaskOut(param_regs);
    if (!free_regs.is_empty()) {
      LiftoffRegister new_target = free_regs.GetFirstRegSet();
      stack_transfers.MoveRegister(new_target, LiftoffRegister(*target),
                                   kIntPtrKind);
      *target = new_target.gp();
    } else {
      stack_slots.Add(VarState(kIntPtrKind, LiftoffRegister(*target), 0),
                      param_slots);
      param_slots++;
      *target = no_reg;
    }
  }

  // After figuring out all register and stack moves, drop the parameter slots
  // from the stack.
  DropValues(num_params);

  // Spill all remaining cache slots.
  cache_state_.ClearAllCacheRegisters();
  // Iterate backwards, spilling register slots until all registers are free.
  if (!cache_state_.used_registers.is_empty()) {
    for (auto* slot = cache_state_.stack_state.end() - 1;; --slot) {
      DCHECK_LE(cache_state_.stack_state.begin(), slot);
      if (!slot->is_reg()) continue;
      Spill(slot->offset(), slot->reg(), slot->kind());
      cache_state_.dec_used(slot->reg());
      slot->MakeStack();
      if (cache_state_.used_registers.is_empty()) break;
    }
  }
  // All slots are either spilled on the stack, or hold constants now.
  DCHECK(std::all_of(
      cache_state_.stack_state.begin(), cache_state_.stack_state.end(),
      [](const VarState& slot) { return slot.is_stack() || slot.is_const(); }));

  if (param_slots > 0) {
    stack_slots.Construct(param_slots);
  }
  // Execute the stack transfers before filling the instance register.
  stack_transfers.Execute();

  // Reload the instance from the stack if we do not have it in a register.
  if (target_instance == no_reg) {
    LoadInstanceFromFrame(instance_reg);
  }
}

void LiftoffAssembler::FinishCall(const ValueKindSig* sig,
                                  compiler::CallDescriptor* call_descriptor) {
  int call_desc_return_idx = 0;
  for (ValueKind return_kind : sig->returns()) {
    DCHECK_LT(call_desc_return_idx, call_descriptor->ReturnCount());
    const bool needs_gp_pair = needs_gp_reg_pair(return_kind);
    const int num_lowered_params = 1 + needs_gp_pair;
    const ValueKind lowered_kind = needs_gp_pair ? kI32 : return_kind;
    const RegClass rc = reg_class_for(lowered_kind);
    // Initialize to anything, will be set in the loop and used afterwards.
    LiftoffRegister reg_pair[2] = {kGpCacheRegList.GetFirstRegSet(),
                                   kGpCacheRegList.GetFirstRegSet()};
    LiftoffRegList pinned;
    for (int pair_idx = 0; pair_idx < num_lowered_params; ++pair_idx) {
      compiler::LinkageLocation loc =
          call_descriptor->GetReturnLocation(call_desc_return_idx++);
      if (loc.IsRegister()) {
        DCHECK(!loc.IsAnyRegister());
        reg_pair[pair_idx] = LiftoffRegister::from_external_code(
            rc, lowered_kind, loc.AsRegister());
      } else {
        DCHECK(loc.IsCallerFrameSlot());
        reg_pair[pair_idx] = GetUnusedRegister(rc, pinned);
        // Get slot offset relative to the stack pointer.
        int offset = call_descriptor->GetOffsetToReturns();
        int return_slot = -loc.GetLocation() - offset - 1;
        LoadReturnStackSlot(reg_pair[pair_idx],
                            return_slot * kSystemPointerSize, lowered_kind);
      }
      if (pair_idx == 0) {
        pinned.set(reg_pair[0]);
      }
    }
    if (num_lowered_params == 1) {
      PushRegister(return_kind, reg_pair[0]);
    } else {
      PushRegister(return_kind, LiftoffRegister::ForPair(reg_pair[0].gp(),
                                                         reg_pair[1].gp()));
    }
  }
  int return_slots = static_cast<int>(call_descriptor->ReturnSlotCount());
  RecordUsedSpillOffset(TopSpillOffset() + return_slots * kSystemPointerSize);
}

void LiftoffAssembler::Move(LiftoffRegister dst, LiftoffRegister src,
                            ValueKind kind) {
  DCHECK_EQ(dst.reg_class(), src.reg_class());
  DCHECK_NE(dst, src);
  if (kNeedI64RegPair && dst.is_gp_pair()) {
    // Use the {StackTransferRecipe} to move pairs, as the registers in the
    // pairs might overlap.
    StackTransferRecipe(this).MoveRegister(dst, src, kind);
  } else if (kNeedS128RegPair && dst.is_fp_pair()) {
    // Calling low_fp is fine, Move will automatically check the kind and
    // convert this FP to its SIMD register, and use a SIMD move.
    Move(dst.low_fp(), src.low_fp(), kind);
  } else if (dst.is_gp()) {
    Move(dst.gp(), src.gp(), kind);
  } else {
    Move(dst.fp(), src.fp(), kind);
  }
}

void LiftoffAssembler::ParallelRegisterMove(
    base::Vector<const ParallelRegisterMoveTuple> tuples) {
  StackTransferRecipe stack_transfers(this);
  for (auto tuple : tuples) {
    if (tuple.dst == tuple.src) continue;
    stack_transfers.MoveRegister(tuple.dst, tuple.src, tuple.kind);
  }
}

void LiftoffAssembler::MoveToReturnLocations(
    const FunctionSig* sig, compiler::CallDescriptor* descriptor) {
  DCHECK_LT(0, sig->return_count());
  if (V8_UNLIKELY(sig->return_count() > 1)) {
    MoveToReturnLocationsMultiReturn(sig, descriptor);
    return;
  }

  ValueKind return_kind = sig->GetReturn(0).kind();
  // Defaults to a gp reg, will be set below if return kind is not gp.
  LiftoffRegister return_reg = LiftoffRegister(kGpReturnRegisters[0]);

  if (needs_gp_reg_pair(return_kind)) {
    return_reg =
        LiftoffRegister::ForPair(kGpReturnRegisters[0], kGpReturnRegisters[1]);
  } else if (needs_fp_reg_pair(return_kind)) {
    return_reg = LiftoffRegister::ForFpPair(kFpReturnRegisters[0]);
  } else if (reg_class_for(return_kind) == kFpReg) {
    return_reg = LiftoffRegister(kFpReturnRegisters[0]);
  } else {
    DCHECK_EQ(kGpReg, reg_class_for(return_kind));
  }
  VarState& slot = cache_state_.stack_state.back();
  if (V8_LIKELY(slot.is_reg())) {
    if (slot.reg() != return_reg) {
      Move(return_reg, slot.reg(), slot.kind());
    }
  } else {
    LoadToFixedRegister(cache_state_.stack_state.back(), return_reg);
  }
}

void LiftoffAssembler::MoveToReturnLocationsMultiReturn(
    const FunctionSig* sig, compiler::CallDescriptor* descriptor) {
  DCHECK_LT(1, sig->return_count());
  StackTransferRecipe stack_transfers(this);

  // We sometimes allocate a register to perform stack-to-stack moves, which can
  // cause a spill in the cache state. Conservatively save and restore the
  // original state in case it is needed after the current instruction
  // (conditional branch).
  CacheState saved_state{zone()};
#if DEBUG
  uint32_t saved_state_frozenness = cache_state_.frozen;
  cache_state_.frozen = 0;
#endif
  saved_state.Split(*cache_state());
  int call_desc_return_idx = 0;
  DCHECK_LE(sig->return_count(), cache_state_.stack_height());
  VarState* slots = cache_state_.stack_state.end() - sig->return_count();
  // Fill return frame slots first to ensure that all potential spills happen
  // before we prepare the stack transfers.
  for (size_t i = 0; i < sig->return_count(); ++i) {
    ValueKind return_kind = sig->GetReturn(i).kind();
    bool needs_gp_pair = needs_gp_reg_pair(return_kind);
    int num_lowered_params = 1 + needs_gp_pair;
    for (int pair_idx = 0; pair_idx < num_lowered_params; ++pair_idx) {
      compiler::LinkageLocation loc =
          descriptor->GetReturnLocation(call_desc_return_idx++);
      if (loc.IsCallerFrameSlot()) {
        RegPairHalf half = pair_idx == 0 ? kLowWord : kHighWord;
        VarState& slot = slots[i];
        LiftoffRegister reg = needs_gp_pair
                                  ? LoadI64HalfIntoRegister(slot, half)
                                  : LoadToRegister(slot, {});
        ValueKind lowered_kind = needs_gp_pair ? kI32 : return_kind;
        StoreCallerFrameSlot(reg, -loc.AsCallerFrameSlot(), lowered_kind);
      }
    }
  }
  // Prepare and execute stack transfers.
  call_desc_return_idx = 0;
  for (size_t i = 0; i < sig->return_count(); ++i) {
    ValueKind return_kind = sig->GetReturn(i).kind();
    bool needs_gp_pair = needs_gp_reg_pair(return_kind);
    int num_lowered_params = 1 + needs_gp_pair;
    for (int pair_idx = 0; pair_idx < num_lowered_params; ++pair_idx) {
      RegPairHalf half = pair_idx == 0 ? kLowWord : kHighWord;
      compiler::LinkageLocation loc =
          descriptor->GetReturnLocation(call_desc_return_idx++);
      if (loc.IsRegister()) {
        DCHECK(!loc.IsAnyRegister());
        int reg_code = loc.AsRegister();
        ValueKind lowered_kind = needs_gp_pair ? kI32 : return_kind;
        RegClass rc = reg_class_for(lowered_kind);
        LiftoffRegister reg =
            LiftoffRegister::from_external_code(rc, return_kind, reg_code);
        VarState& slot = slots[i];
        if (needs_gp_pair) {
          stack_transfers.LoadI64HalfIntoRegister(reg, slot, half);
        } else {
          stack_transfers.LoadIntoRegister(reg, slot);
        }
      }
    }
  }
  cache_state()->Steal(saved_state);
#if DEBUG
  cache_state_.frozen = saved_state_frozenness;
#endif
}

#if DEBUG
void LiftoffRegList::Print() const {
  std::ostringstream os;
  os << *this << "\n";
  PrintF("%s", os.str().c_str());
}
#endif

#ifdef ENABLE_SLOW_DCHECKS
bool LiftoffAssembler::ValidateCacheState() const {
  uint32_t register_use_count[kAfterMaxLiftoffRegCode] = {0};
  LiftoffRegList used_regs;
  int offset = StaticStackFrameSize();
  for (const VarState& var : cache_state_.stack_state) {
    // Check for continuous stack offsets.
    offset = NextSpillOffset(var.kind(), offset);
    DCHECK_EQ(offset, var.offset());
    if (!var.is_reg()) continue;
    LiftoffRegister reg = var.reg();
    if ((kNeedI64RegPair || kNeedS128RegPair) && reg.is_pair()) {
      ++register_use_count[reg.low().liftoff_code()];
      ++register_use_count[reg.high().liftoff_code()];
    } else {
      ++register_use_count[reg.liftoff_code()];
    }
    used_regs.set(reg);
  }
  for (Register cache_reg :
       {cache_state_.cached_instance, cache_state_.cached_mem_start}) {
    if (cache_reg != no_reg) {
      DCHECK(!used_regs.has(cache_reg));
      int liftoff_code = LiftoffRegister{cache_reg}.liftoff_code();
      used_regs.set(cache_reg);
      DCHECK_EQ(0, register_use_count[liftoff_code]);
      register_use_count[liftoff_code] = 1;
    }
  }
  bool valid = memcmp(register_use_count, cache_state_.register_use_count,
                      sizeof(register_use_count)) == 0 &&
               used_regs == cache_state_.used_registers;
  if (valid) return true;
  std::ostringstream os;
  os << "Error in LiftoffAssembler::ValidateCacheState().\n";
  os << "expected: used_regs " << used_regs << ", counts "
     << PrintCollection(register_use_count) << "\n";
  os << "found:    used_regs " << cache_state_.used_registers << ", counts "
     << PrintCollection(cache_state_.register_use_count) << "\n";
  os << "Use --trace-wasm-decoder and --trace-liftoff to debug.";
  FATAL("%s", os.str().c_str());
}
#endif

LiftoffRegister LiftoffAssembler::SpillOneRegister(LiftoffRegList candidates) {
  // Before spilling a regular stack slot, try to drop a "volatile" register
  // (used for caching the memory start or the instance itself). Those can be
  // reloaded without requiring a spill here.
  if (cache_state_.has_volatile_register(candidates)) {
    return cache_state_.take_volatile_register(candidates);
  }

  LiftoffRegister spilled_reg = cache_state_.GetNextSpillReg(candidates);
  SpillRegister(spilled_reg);
  return spilled_reg;
}

LiftoffRegister LiftoffAssembler::SpillAdjacentFpRegisters(
    LiftoffRegList pinned) {
  // We end up in this call only when:
  // [1] kNeedS128RegPair, and
  // [2] there are no pair of adjacent FP registers that are free
  CHECK(kNeedS128RegPair);
  DCHECK(!kFpCacheRegList.MaskOut(pinned)
              .MaskOut(cache_state_.used_registers)
              .HasAdjacentFpRegsSet());

  // Special logic, if the top fp register is even, we might hit a case of an
  // invalid register in case 2.
  LiftoffRegister last_fp = kFpCacheRegList.GetLastRegSet();
  if (last_fp.fp().code() % 2 == 0) {
    pinned.set(last_fp);
  }
  // If half of an adjacent pair is pinned, consider the whole pair pinned.
  // Otherwise the code below would potentially spill the pinned register
  // (after first spilling the unpinned half of the pair).
  pinned = pinned.SpreadSetBitsToAdjacentFpRegs();

  // We can try to optimize the spilling here:
  // 1. Try to get a free fp register, either:
  //  a. This register is already free, or
  //  b. it had to be spilled.
  // 2. If 1a, the adjacent register is used (invariant [2]), spill it.
  // 3. If 1b, check the adjacent register:
  //  a. If free, done!
  //  b. If used, spill it.
  // We spill one register in 2 and 3a, and two registers in 3b.

  LiftoffRegister first_reg = GetUnusedRegister(kFpReg, pinned);
  LiftoffRegister second_reg = first_reg, low_reg = first_reg;

  if (first_reg.fp().code() % 2 == 0) {
    second_reg =
        LiftoffRegister::from_liftoff_code(first_reg.liftoff_code() + 1);
  } else {
    second_reg =
        LiftoffRegister::from_liftoff_code(first_reg.liftoff_code() - 1);
    low_reg = second_reg;
  }

  if (cache_state_.is_used(second_reg)) {
    SpillRegister(second_reg);
  }

  return low_reg;
}

void LiftoffAssembler::SpillRegister(LiftoffRegister reg) {
  DCHECK(!cache_state_.frozen);
  int remaining_uses = cache_state_.get_use_count(reg);
  DCHECK_LT(0, remaining_uses);
  for (uint32_t idx = cache_state_.stack_height() - 1;; --idx) {
    DCHECK_GT(cache_state_.stack_height(), idx);
    auto* slot = &cache_state_.stack_state[idx];
    if (!slot->is_reg() || !slot->reg().overlaps(reg)) continue;
    if (slot->reg().is_pair()) {
      // Make sure to decrement *both* registers in a pair, because the
      // {clear_used} call below only clears one of them.
      cache_state_.dec_used(slot->reg().low());
      cache_state_.dec_used(slot->reg().high());
      cache_state_.last_spilled_regs.set(slot->reg().low());
      cache_state_.last_spilled_regs.set(slot->reg().high());
    }
    Spill(slot->offset(), slot->reg(), slot->kind());
    slot->MakeStack();
    if (--remaining_uses == 0) break;
  }
  cache_state_.clear_used(reg);
  cache_state_.last_spilled_regs.set(reg);
}

void LiftoffAssembler::set_num_locals(uint32_t num_locals) {
  DCHECK_EQ(0, num_locals_);  // only call this once.
  num_locals_ = num_locals;
  if (num_locals > kInlineLocalKinds) {
    more_local_kinds_ = reinterpret_cast<ValueKind*>(
        base::Malloc(num_locals * sizeof(ValueKind)));
    DCHECK_NOT_NULL(more_local_kinds_);
  }
}

std::ostream& operator<<(std::ostream& os, VarState slot) {
  os << name(slot.kind()) << ":";
  switch (slot.loc()) {
    case VarState::kStack:
      return os << "s0x" << std::hex << slot.offset() << std::dec;
    case VarState::kRegister:
      return os << slot.reg();
    case VarState::kIntConst:
      return os << "c" << slot.i32_const();
  }
  UNREACHABLE();
}

#if DEBUG
bool CompatibleStackSlotTypes(ValueKind a, ValueKind b) {
  // Since Liftoff doesn't do accurate type tracking (e.g. on loop back edges,
  // ref.as_non_null/br_on_cast results), we only care that pointer types stay
  // amongst pointer types. It's fine if ref/ref null overwrite each other.
  return a == b || (is_object_reference(a) && is_object_reference(b));
}
#endif

}  // namespace wasm
}  // namespace internal
}  // namespace v8
