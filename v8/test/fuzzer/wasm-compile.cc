// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>

#include "src/base/macros.h"
#include "src/base/v8-fallthrough.h"
#include "src/execution/isolate.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

namespace v8::internal::wasm::fuzzer {

namespace {

constexpr int kMaxArrays = 4;
constexpr int kMaxStructs = 4;
constexpr int kMaxStructFields = 4;
constexpr int kMaxFunctions = 4;
constexpr int kMaxGlobals = 64;
constexpr int kMaxParameters = 15;
constexpr int kMaxReturns = 15;
constexpr int kMaxExceptions = 4;
constexpr int kMaxTableSize = 32;
constexpr int kMaxTables = 4;
constexpr int kMaxArraySize = 20;
constexpr int kMaxPassiveDataSegments = 2;
constexpr uint32_t kMaxRecursionDepth = 64;

class DataRange {
  // data_ is used for general random values for fuzzing.
  base::Vector<const uint8_t> data_;
  // The RNG is used for generating random values (i32.consts etc.) for which
  // the quality of the input is less important.
  base::RandomNumberGenerator rng_;

 public:
  explicit DataRange(base::Vector<const uint8_t> data, int64_t seed = -1)
      : data_(data), rng_(seed == -1 ? get<int64_t>() : seed) {}
  DataRange(const DataRange&) = delete;
  DataRange& operator=(const DataRange&) = delete;

  // Don't accidentally pass DataRange by value. This will reuse bytes and might
  // lead to OOM because the end might not be reached.
  // Define move constructor and move assignment, disallow copy constructor and
  // copy assignment (below).
  DataRange(DataRange&& other) V8_NOEXCEPT : data_(other.data_),
                                             rng_(other.rng_) {
    other.data_ = {};
  }
  DataRange& operator=(DataRange&& other) V8_NOEXCEPT {
    data_ = other.data_;
    rng_ = other.rng_;
    other.data_ = {};
    return *this;
  }

  size_t size() const { return data_.size(); }

  DataRange split() {
    // As we might split many times, only use 2 bytes if the data size is large.
    uint16_t random_choice = data_.size() > std::numeric_limits<uint8_t>::max()
                                 ? get<uint16_t>()
                                 : get<uint8_t>();
    uint16_t num_bytes = random_choice % std::max(size_t{1}, data_.size());
    int64_t new_seed = rng_.initial_seed() ^ rng_.NextInt64();
    DataRange split(data_.SubVector(0, num_bytes), new_seed);
    data_ += num_bytes;
    return split;
  }

  template <typename T, size_t max_bytes = sizeof(T)>
  T getPseudoRandom() {
    static_assert(!std::is_same<T, bool>::value, "bool needs special handling");
    static_assert(max_bytes <= sizeof(T));
    // Special handling for signed integers: Calling getPseudoRandom<int32_t, 1>
    // () should be equal to getPseudoRandom<int8_t>(). (The NextBytes() below
    // does not achieve that due to depending on endianness and either never
    // generating negative values or filling in the highest significant bits
    // which would be unexpected).
    if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
      switch (max_bytes) {
        case 1:
          return static_cast<int8_t>(getPseudoRandom<uint8_t>());
        case 2:
          return static_cast<int16_t>(getPseudoRandom<uint16_t>());
        case 4:
          return static_cast<int32_t>(getPseudoRandom<uint32_t>());
        default:
          return static_cast<T>(
              getPseudoRandom<std::make_unsigned_t<T>, max_bytes>());
      }
    }

    T result{};
    rng_.NextBytes(&result, max_bytes);
    return result;
  }

  template <typename T>
  T get() {
    // Bool needs special handling (see template specialization below).
    static_assert(!std::is_same<T, bool>::value, "bool needs special handling");

    // We want to support the case where we have less than sizeof(T) bytes
    // remaining in the slice. We'll just use what we have, so we get a bit of
    // randomness when there are still some bytes left. If size == 0, get<T>()
    // returns the type's value-initialized value.
    const size_t num_bytes = std::min(sizeof(T), data_.size());
    T result{};
    memcpy(&result, data_.begin(), num_bytes);
    data_ += num_bytes;
    return result;
  }
};

// Explicit specialization must be defined outside of class body.
template <>
bool DataRange::get() {
  // The general implementation above is not instantiable for bool, as that
  // would cause undefinied behaviour when memcpy'ing random bytes to the
  // bool. This can result in different observable side effects when invoking
  // get<bool> between debug and release version, which eventually makes the
  // code output different as well as raising various unrecoverable errors on
  // runtime.
  // Hence we specialize get<bool> to consume a full byte and use the least
  // significant bit only (0 == false, 1 == true).
  return get<uint8_t>() % 2;
}

enum NumericTypes { kIncludeNumericTypes, kExcludeNumericTypes };
enum PackedTypes { kIncludePackedTypes, kExcludePackedTypes };
enum Generics {
  kAlwaysIncludeAllGenerics,
  kExcludeSomeGenericsWhenTypeIsNonNullable
};

ValueType GetValueTypeHelper(DataRange* data, uint32_t num_nullable_types,
                             uint32_t num_non_nullable_types,
                             NumericTypes include_numeric_types,
                             PackedTypes include_packed_types,
                             Generics include_generics) {
  std::vector<ValueType> types;
  // Non wasm-gc types.
  if (include_numeric_types == kIncludeNumericTypes) {
    types.insert(types.end(),
                 {kWasmI32, kWasmI64, kWasmF32, kWasmF64, kWasmS128});
    if (include_packed_types == kIncludePackedTypes) {
      types.insert(types.end(), {kWasmI8, kWasmI16});
    }
  }
  // Decide if the return type will be nullable or not.
  const bool nullable = data->get<bool>();

  types.insert(types.end(), {kWasmI31Ref, kWasmFuncRef});
  if (nullable) {
    types.insert(types.end(),
                 {kWasmNullRef, kWasmNullExternRef, kWasmNullFuncRef});
  }
  if (nullable || include_generics == kAlwaysIncludeAllGenerics) {
    types.insert(types.end(), {kWasmStructRef, kWasmArrayRef, kWasmAnyRef,
                               kWasmEqRef, kWasmExternRef});
  }

  // The last index of user-defined types allowed is different based on the
  // nullability of the output.
  const uint32_t num_user_defined_types =
      nullable ? num_nullable_types : num_non_nullable_types;

  // Conceptually, user-defined types are added to the end of the list. Pick a
  // random one among them.
  uint32_t id = data->get<uint8_t>() % (types.size() + num_user_defined_types);

  Nullability nullability = nullable ? kNullable : kNonNullable;

  if (id >= types.size()) {
    // Return user-defined type.
    return ValueType::RefMaybeNull(id - static_cast<uint32_t>(types.size()),
                                   nullability);
  }
  // If returning a reference type, fix its nullability according to {nullable}.
  if (types[id].is_reference()) {
    return ValueType::RefMaybeNull(types[id].heap_type(), nullability);
  }
  // Otherwise, just return the picked type.
  return types[id];
}

ValueType GetValueType(DataRange* data, uint32_t num_types) {
  return GetValueTypeHelper(data, num_types, num_types, kIncludeNumericTypes,
                            kExcludePackedTypes, kAlwaysIncludeAllGenerics);
}

void GeneratePassiveDataSegment(DataRange* range, WasmModuleBuilder* builder) {
  int length = range->get<uint8_t>() % 65;
  ZoneVector<uint8_t> data(length, builder->zone());
  for (int i = 0; i < length; ++i) {
    data[i] = range->getPseudoRandom<uint8_t>();
  }
  builder->AddPassiveDataSegment(data.data(),
                                 static_cast<uint32_t>(data.size()));
}

uint32_t GenerateRefTypeElementSegment(DataRange* range,
                                       WasmModuleBuilder* builder,
                                       ValueType element_type) {
  DCHECK(element_type.is_object_reference());
  DCHECK(element_type.has_index());
  WasmModuleBuilder::WasmElemSegment segment(
      builder->zone(), element_type, false,
      WasmInitExpr::RefNullConst(element_type.heap_representation()));
  size_t element_count = range->get<uint8_t>() % 11;
  for (size_t i = 0; i < element_count; ++i) {
    segment.entries.emplace_back(
        WasmModuleBuilder::WasmElemSegment::Entry::kRefNullEntry,
        element_type.ref_index());
  }
  return builder->AddElementSegment(std::move(segment));
}

class WasmGenerator {
  template <WasmOpcode Op, ValueKind... Args>
  void op(DataRange* data) {
    Generate<Args...>(data);
    builder_->Emit(Op);
  }

  class V8_NODISCARD BlockScope {
   public:
    BlockScope(WasmGenerator* gen, WasmOpcode block_type,
               base::Vector<const ValueType> param_types,
               base::Vector<const ValueType> result_types,
               base::Vector<const ValueType> br_types, bool emit_end = true)
        : gen_(gen), emit_end_(emit_end) {
      gen->blocks_.emplace_back(br_types.begin(), br_types.end());
      gen->builder_->EmitByte(block_type);

      if (param_types.size() == 0 && result_types.size() == 0) {
        gen->builder_->EmitValueType(kWasmVoid);
        return;
      }
      if (param_types.size() == 0 && result_types.size() == 1) {
        gen->builder_->EmitValueType(result_types[0]);
        return;
      }
      // Multi-value block.
      Zone* zone = gen->builder_->builder()->zone();
      FunctionSig::Builder builder(zone, result_types.size(),
                                   param_types.size());
      for (auto& type : param_types) {
        DCHECK_NE(type, kWasmVoid);
        builder.AddParam(type);
      }
      for (auto& type : result_types) {
        DCHECK_NE(type, kWasmVoid);
        builder.AddReturn(type);
      }
      FunctionSig* sig = builder.Build();
      int sig_id = gen->builder_->builder()->AddSignature(
          sig, v8_flags.wasm_final_types);
      gen->builder_->EmitI32V(sig_id);
    }

    ~BlockScope() {
      if (emit_end_) gen_->builder_->Emit(kExprEnd);
      gen_->blocks_.pop_back();
    }

   private:
    WasmGenerator* const gen_;
    bool emit_end_;
  };

  void block(base::Vector<const ValueType> param_types,
             base::Vector<const ValueType> return_types, DataRange* data) {
    BlockScope block_scope(this, kExprBlock, param_types, return_types,
                           return_types);
    ConsumeAndGenerate(param_types, return_types, data);
  }

  template <ValueKind T>
  void block(DataRange* data) {
    block({}, base::VectorOf({ValueType::Primitive(T)}), data);
  }

  void loop(base::Vector<const ValueType> param_types,
            base::Vector<const ValueType> return_types, DataRange* data) {
    BlockScope block_scope(this, kExprLoop, param_types, return_types,
                           param_types);
    ConsumeAndGenerate(param_types, return_types, data);
  }

  template <ValueKind T>
  void loop(DataRange* data) {
    loop({}, base::VectorOf({ValueType::Primitive(T)}), data);
  }

  enum IfType { kIf, kIfElse };

  void if_(base::Vector<const ValueType> param_types,
           base::Vector<const ValueType> return_types, IfType type,
           DataRange* data) {
    // One-armed "if" are only valid if the input and output types are the same.
    DCHECK_IMPLIES(type == kIf, param_types == return_types);
    Generate(kWasmI32, data);
    BlockScope block_scope(this, kExprIf, param_types, return_types,
                           return_types);
    ConsumeAndGenerate(param_types, return_types, data);
    if (type == kIfElse) {
      builder_->Emit(kExprElse);
      ConsumeAndGenerate(param_types, return_types, data);
    }
  }

  template <ValueKind T, IfType type>
  void if_(DataRange* data) {
    static_assert(T == kVoid || type == kIfElse,
                  "if without else cannot produce a value");
    if_({},
        T == kVoid ? base::Vector<ValueType>{}
                   : base::VectorOf({ValueType::Primitive(T)}),
        type, data);
  }

  void try_block_helper(ValueType return_type, DataRange* data) {
    bool has_catch_all = data->get<bool>();
    uint8_t num_catch =
        data->get<uint8_t>() % (builder_->builder()->NumExceptions() + 1);
    bool is_delegate = num_catch == 0 && !has_catch_all && data->get<bool>();
    // Allow one more target than there are enclosing try blocks, for delegating
    // to the caller.

    base::Vector<const ValueType> return_type_vec =
        return_type.kind() == kVoid ? base::Vector<ValueType>{}
                                    : base::VectorOf(&return_type, 1);
    BlockScope block_scope(this, kExprTry, {}, return_type_vec, return_type_vec,
                           !is_delegate);
    int control_depth = static_cast<int>(blocks_.size()) - 1;
    Generate(return_type, data);
    catch_blocks_.push_back(control_depth);
    for (int i = 0; i < num_catch; ++i) {
      const FunctionSig* exception_type =
          builder_->builder()->GetExceptionType(i);
      auto exception_type_vec =
          base::VectorOf(exception_type->parameters().begin(),
                         exception_type->parameter_count());
      builder_->EmitWithU32V(kExprCatch, i);
      ConsumeAndGenerate(exception_type_vec, return_type_vec, data);
    }
    if (has_catch_all) {
      builder_->Emit(kExprCatchAll);
      Generate(return_type, data);
    }
    if (is_delegate) {
      // The delegate target depth does not include the current try block,
      // because 'delegate' closes this scope. However it is still in the
      // {blocks_} list, so remove one to get the correct size.
      int delegate_depth = data->get<uint8_t>() % (blocks_.size() - 1);
      builder_->EmitWithU32V(kExprDelegate, delegate_depth);
    }
    catch_blocks_.pop_back();
  }

  template <ValueKind T>
  void try_block(DataRange* data) {
    try_block_helper(ValueType::Primitive(T), data);
  }

  void any_block(base::Vector<const ValueType> param_types,
                 base::Vector<const ValueType> return_types, DataRange* data) {
    uint8_t block_type = data->get<uint8_t>() % 4;
    switch (block_type) {
      case 0:
        block(param_types, return_types, data);
        return;
      case 1:
        loop(param_types, return_types, data);
        return;
      case 2:
        if (param_types == return_types) {
          if_({}, {}, kIf, data);
          return;
        }
        V8_FALLTHROUGH;
      case 3:
        if_(param_types, return_types, kIfElse, data);
        return;
    }
  }

  void br(DataRange* data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint8_t>() % blocks_.size();
    const auto break_types = blocks_[target_block];

    Generate(base::VectorOf(break_types), data);
    builder_->EmitWithI32V(
        kExprBr, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
  }

  template <ValueKind wanted_kind>
  void br_if(DataRange* data) {
    // There is always at least the block representing the function body.
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint8_t>() % blocks_.size();
    const auto break_types = base::VectorOf(blocks_[target_block]);

    Generate(break_types, data);
    Generate(kWasmI32, data);
    builder_->EmitWithI32V(
        kExprBrIf, static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
    ConsumeAndGenerate(
        break_types,
        wanted_kind == kVoid
            ? base::Vector<ValueType>{}
            : base::VectorOf({ValueType::Primitive(wanted_kind)}),
        data);
  }

  template <ValueKind wanted_kind>
  void br_on_null(DataRange* data) {
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint8_t>() % blocks_.size();
    const auto break_types = base::VectorOf(blocks_[target_block]);
    Generate(break_types, data);
    GenerateRef(data);
    builder_->EmitWithI32V(
        kExprBrOnNull,
        static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
    builder_->Emit(kExprDrop);
    ConsumeAndGenerate(
        break_types,
        wanted_kind == kVoid
            ? base::Vector<ValueType>{}
            : base::VectorOf({ValueType::Primitive(wanted_kind)}),
        data);
  }

  template <ValueKind wanted_kind>
  void br_on_non_null(DataRange* data) {
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint8_t>() % blocks_.size();
    const auto break_types = base::VectorOf(blocks_[target_block]);
    if (break_types.empty() ||
        !break_types[break_types.size() - 1].is_reference()) {
      // Invalid break_types for br_on_non_null.
      Generate<wanted_kind>(data);
      return;
    }
    Generate(break_types, data);
    builder_->EmitWithI32V(
        kExprBrOnNonNull,
        static_cast<uint32_t>(blocks_.size()) - 1 - target_block);
    ConsumeAndGenerate(
        base::VectorOf(break_types.data(), break_types.size() - 1),
        wanted_kind == kVoid
            ? base::Vector<ValueType>{}
            : base::VectorOf({ValueType::Primitive(wanted_kind)}),
        data);
  }

  void br_table(ValueType result_type, DataRange* data) {
    const uint8_t block_count = 1 + data->get<uint8_t>() % 8;
    // Generate the block entries.
    uint16_t entry_bits =
        block_count > 4 ? data->get<uint16_t>() : data->get<uint8_t>();
    for (size_t i = 0; i < block_count; ++i) {
      builder_->Emit(kExprBlock);
      builder_->EmitValueType(result_type);
      blocks_.emplace_back();
      if (result_type != kWasmVoid) {
        blocks_.back().push_back(result_type);
      }
      // There can be additional instructions in each block.
      // Only generate it with a 25% chance as it's otherwise quite unlikely to
      // have enough random bytes left for the br_table instruction.
      if ((entry_bits & 3) == 3) {
        Generate(kWasmVoid, data);
      }
      entry_bits >>= 2;
    }
    // Generate the br_table.
    Generate(result_type, data);
    Generate(kWasmI32, data);
    builder_->Emit(kExprBrTable);
    uint32_t entry_count = 1 + data->get<uint8_t>() % 8;
    builder_->EmitU32V(entry_count);
    for (size_t i = 0; i < entry_count + 1; ++i) {
      builder_->EmitU32V(data->get<uint8_t>() % block_count);
    }
    // Generate the block ends.
    uint8_t exit_bits = result_type == kWasmVoid ? 0 : data->get<uint8_t>();
    for (size_t i = 0; i < block_count; ++i) {
      if (exit_bits & 1) {
        // Drop and generate new value.
        builder_->Emit(kExprDrop);
        Generate(result_type, data);
      }
      exit_bits >>= 1;
      builder_->Emit(kExprEnd);
      blocks_.pop_back();
    }
  }

  template <ValueKind wanted_kind>
  void br_table(DataRange* data) {
    br_table(
        wanted_kind == kVoid ? kWasmVoid : ValueType::Primitive(wanted_kind),
        data);
  }

  void return_op(DataRange* data) {
    auto returns = builder_->signature()->returns();
    Generate(base::VectorOf(returns.begin(), returns.size()), data);
    builder_->Emit(kExprReturn);
  }

  // TODO(eholk): make this function constexpr once gcc supports it
  static uint8_t max_alignment(WasmOpcode memop) {
    switch (memop) {
      case kExprS128LoadMem:
      case kExprS128StoreMem:
        return 4;
      case kExprI64LoadMem:
      case kExprF64LoadMem:
      case kExprI64StoreMem:
      case kExprF64StoreMem:
      case kExprI64AtomicStore:
      case kExprI64AtomicLoad:
      case kExprI64AtomicAdd:
      case kExprI64AtomicSub:
      case kExprI64AtomicAnd:
      case kExprI64AtomicOr:
      case kExprI64AtomicXor:
      case kExprI64AtomicExchange:
      case kExprI64AtomicCompareExchange:
      case kExprS128Load8x8S:
      case kExprS128Load8x8U:
      case kExprS128Load16x4S:
      case kExprS128Load16x4U:
      case kExprS128Load32x2S:
      case kExprS128Load32x2U:
      case kExprS128Load64Splat:
      case kExprS128Load64Zero:
        return 3;
      case kExprI32LoadMem:
      case kExprI64LoadMem32S:
      case kExprI64LoadMem32U:
      case kExprF32LoadMem:
      case kExprI32StoreMem:
      case kExprI64StoreMem32:
      case kExprF32StoreMem:
      case kExprI32AtomicStore:
      case kExprI64AtomicStore32U:
      case kExprI32AtomicLoad:
      case kExprI64AtomicLoad32U:
      case kExprI32AtomicAdd:
      case kExprI32AtomicSub:
      case kExprI32AtomicAnd:
      case kExprI32AtomicOr:
      case kExprI32AtomicXor:
      case kExprI32AtomicExchange:
      case kExprI32AtomicCompareExchange:
      case kExprI64AtomicAdd32U:
      case kExprI64AtomicSub32U:
      case kExprI64AtomicAnd32U:
      case kExprI64AtomicOr32U:
      case kExprI64AtomicXor32U:
      case kExprI64AtomicExchange32U:
      case kExprI64AtomicCompareExchange32U:
      case kExprS128Load32Splat:
      case kExprS128Load32Zero:
        return 2;
      case kExprI32LoadMem16S:
      case kExprI32LoadMem16U:
      case kExprI64LoadMem16S:
      case kExprI64LoadMem16U:
      case kExprI32StoreMem16:
      case kExprI64StoreMem16:
      case kExprI32AtomicStore16U:
      case kExprI64AtomicStore16U:
      case kExprI32AtomicLoad16U:
      case kExprI64AtomicLoad16U:
      case kExprI32AtomicAdd16U:
      case kExprI32AtomicSub16U:
      case kExprI32AtomicAnd16U:
      case kExprI32AtomicOr16U:
      case kExprI32AtomicXor16U:
      case kExprI32AtomicExchange16U:
      case kExprI32AtomicCompareExchange16U:
      case kExprI64AtomicAdd16U:
      case kExprI64AtomicSub16U:
      case kExprI64AtomicAnd16U:
      case kExprI64AtomicOr16U:
      case kExprI64AtomicXor16U:
      case kExprI64AtomicExchange16U:
      case kExprI64AtomicCompareExchange16U:
      case kExprS128Load16Splat:
        return 1;
      case kExprI32LoadMem8S:
      case kExprI32LoadMem8U:
      case kExprI64LoadMem8S:
      case kExprI64LoadMem8U:
      case kExprI32StoreMem8:
      case kExprI64StoreMem8:
      case kExprI32AtomicStore8U:
      case kExprI64AtomicStore8U:
      case kExprI32AtomicLoad8U:
      case kExprI64AtomicLoad8U:
      case kExprI32AtomicAdd8U:
      case kExprI32AtomicSub8U:
      case kExprI32AtomicAnd8U:
      case kExprI32AtomicOr8U:
      case kExprI32AtomicXor8U:
      case kExprI32AtomicExchange8U:
      case kExprI32AtomicCompareExchange8U:
      case kExprI64AtomicAdd8U:
      case kExprI64AtomicSub8U:
      case kExprI64AtomicAnd8U:
      case kExprI64AtomicOr8U:
      case kExprI64AtomicXor8U:
      case kExprI64AtomicExchange8U:
      case kExprI64AtomicCompareExchange8U:
      case kExprS128Load8Splat:
        return 0;
      default:
        return 0;
    }
  }

  template <WasmOpcode memory_op, ValueKind... arg_kinds>
  void memop(DataRange* data) {
    const uint8_t align =
        data->getPseudoRandom<uint8_t>() % (max_alignment(memory_op) + 1);
    uint32_t offset = data->get<uint16_t>();
    // With a 1/256 chance generate potentially very large offsets.
    if ((offset & 0xff) == 0xff) {
      offset = data->getPseudoRandom<uint32_t>();
    }

    // Generate the index and the arguments, if any.
    Generate<kI32, arg_kinds...>(data);

    if (WasmOpcodes::IsPrefixOpcode(static_cast<WasmOpcode>(memory_op >> 8))) {
      DCHECK(memory_op >> 8 == kAtomicPrefix || memory_op >> 8 == kSimdPrefix);
      builder_->EmitWithPrefix(memory_op);
    } else {
      builder_->Emit(memory_op);
    }
    builder_->EmitU32V(align);
    builder_->EmitU32V(offset);
  }

  template <WasmOpcode Op, ValueKind... Args>
  void atomic_op(DataRange* data) {
    const uint8_t align =
        data->getPseudoRandom<uint8_t>() % (max_alignment(Op) + 1);
    uint32_t offset = data->get<uint16_t>();
    // With a 1/256 chance generate potentially very large offsets.
    if ((offset & 0xff) == 0xff) {
      offset = data->getPseudoRandom<uint32_t>();
    }

    Generate<Args...>(data);
    builder_->EmitWithPrefix(Op);

    builder_->EmitU32V(align);
    builder_->EmitU32V(offset);
  }

  template <WasmOpcode Op, ValueKind... Args>
  void op_with_prefix(DataRange* data) {
    Generate<Args...>(data);
    builder_->EmitWithPrefix(Op);
  }

  void simd_const(DataRange* data) {
    builder_->EmitWithPrefix(kExprS128Const);
    for (int i = 0; i < kSimd128Size; i++) {
      builder_->EmitByte(data->getPseudoRandom<uint8_t>());
    }
  }

  template <WasmOpcode Op, int lanes, ValueKind... Args>
  void simd_lane_op(DataRange* data) {
    Generate<Args...>(data);
    builder_->EmitWithPrefix(Op);
    builder_->EmitByte(data->get<uint8_t>() % lanes);
  }

  template <WasmOpcode Op, int lanes, ValueKind... Args>
  void simd_lane_memop(DataRange* data) {
    // Simd load/store instructions that have a lane immediate.
    memop<Op, Args...>(data);
    builder_->EmitByte(data->get<uint8_t>() % lanes);
  }

  void simd_shuffle(DataRange* data) {
    Generate<kS128, kS128>(data);
    builder_->EmitWithPrefix(kExprI8x16Shuffle);
    for (int i = 0; i < kSimd128Size; i++) {
      builder_->EmitByte(static_cast<uint8_t>(data->get<uint8_t>() % 32));
    }
  }

  void drop(DataRange* data) {
    Generate(GetValueType(data, static_cast<uint32_t>(functions_.size()) +
                                    num_structs_ + num_arrays_),
             data);
    builder_->Emit(kExprDrop);
  }

  enum CallKind { kCallDirect, kCallIndirect, kCallRef };

  template <ValueKind wanted_kind>
  void call(DataRange* data) {
    call(data, ValueType::Primitive(wanted_kind), kCallDirect);
  }

  template <ValueKind wanted_kind>
  void call_indirect(DataRange* data) {
    call(data, ValueType::Primitive(wanted_kind), kCallIndirect);
  }

  template <ValueKind wanted_kind>
  void call_ref(DataRange* data) {
    call(data, ValueType::Primitive(wanted_kind), kCallRef);
  }

  void Convert(ValueType src, ValueType dst) {
    auto idx = [](ValueType t) -> int {
      switch (t.kind()) {
        case kI32:
          return 0;
        case kI64:
          return 1;
        case kF32:
          return 2;
        case kF64:
          return 3;
        default:
          UNREACHABLE();
      }
    };
    static constexpr WasmOpcode kConvertOpcodes[] = {
        // {i32, i64, f32, f64} -> i32
        kExprNop, kExprI32ConvertI64, kExprI32SConvertF32, kExprI32SConvertF64,
        // {i32, i64, f32, f64} -> i64
        kExprI64SConvertI32, kExprNop, kExprI64SConvertF32, kExprI64SConvertF64,
        // {i32, i64, f32, f64} -> f32
        kExprF32SConvertI32, kExprF32SConvertI64, kExprNop, kExprF32ConvertF64,
        // {i32, i64, f32, f64} -> f64
        kExprF64SConvertI32, kExprF64SConvertI64, kExprF64ConvertF32, kExprNop};
    int arr_idx = idx(dst) << 2 | idx(src);
    builder_->Emit(kConvertOpcodes[arr_idx]);
  }

  int choose_function_table_index(DataRange* data) {
    int table_count = builder_->builder()->NumTables();
    int start = data->get<uint8_t>() % table_count;
    for (int i = 0; i < table_count; ++i) {
      int index = (start + i) % table_count;
      if (builder_->builder()->GetTableType(index).is_reference_to(
              HeapType::kFunc)) {
        return index;
      }
    }
    FATAL("No funcref table found; table index 0 is expected to be funcref");
  }

  void call(DataRange* data, ValueType wanted_kind, CallKind call_kind) {
    uint8_t random_byte = data->get<uint8_t>();
    int func_index = random_byte % functions_.size();
    uint32_t sig_index = functions_[func_index];
    const FunctionSig* sig = builder_->builder()->GetSignature(sig_index);
    // Generate arguments.
    for (size_t i = 0; i < sig->parameter_count(); ++i) {
      Generate(sig->GetParam(i), data);
    }
    // Emit call.
    // If the return types of the callee happen to match the return types of the
    // caller, generate a tail call.
    bool use_return_call = random_byte > 127;
    if (use_return_call &&
        std::equal(sig->returns().begin(), sig->returns().end(),
                   builder_->signature()->returns().begin(),
                   builder_->signature()->returns().end())) {
      if (call_kind == kCallDirect) {
        builder_->EmitWithU32V(kExprReturnCall, func_index);
      } else if (call_kind == kCallIndirect) {
        // This will not trap because table[func_index] always contains function
        // func_index.
        builder_->EmitI32Const(func_index);
        builder_->EmitWithU32V(kExprReturnCallIndirect, sig_index);
        builder_->EmitByte(choose_function_table_index(data));  // Table index.
      } else {
        GenerateRef(HeapType(sig_index), data);
        builder_->EmitWithU32V(kExprReturnCallRef, sig_index);
      }
      return;
    } else {
      if (call_kind == kCallDirect) {
        builder_->EmitWithU32V(kExprCallFunction, func_index);
      } else if (call_kind == kCallIndirect) {
        // This will not trap because table[func_index] always contains function
        // func_index.
        builder_->EmitI32Const(func_index);
        builder_->EmitWithU32V(kExprCallIndirect, sig_index);
        builder_->EmitByte(choose_function_table_index(data));  // Table index.
      } else {
        GenerateRef(HeapType(sig_index), data);
        builder_->EmitWithU32V(kExprCallRef, sig_index);
      }
    }
    if (sig->return_count() == 0 && wanted_kind != kWasmVoid) {
      // The call did not generate a value. Thus just generate it here.
      Generate(wanted_kind, data);
      return;
    }
    if (wanted_kind == kWasmVoid) {
      // The call did generate values, but we did not want one.
      for (size_t i = 0; i < sig->return_count(); ++i) {
        builder_->Emit(kExprDrop);
      }
      return;
    }
    auto return_types =
        base::VectorOf(sig->returns().begin(), sig->return_count());
    auto wanted_types =
        base::VectorOf(&wanted_kind, wanted_kind == kWasmVoid ? 0 : 1);
    ConsumeAndGenerate(return_types, wanted_types, data);
  }

  struct Var {
    uint32_t index;
    ValueType type = kWasmVoid;
    Var() = default;
    Var(uint32_t index, ValueType type) : index(index), type(type) {}
    bool is_valid() const { return type != kWasmVoid; }
  };

  Var GetRandomLocal(DataRange* data) {
    uint32_t num_params =
        static_cast<uint32_t>(builder_->signature()->parameter_count());
    uint32_t num_locals = static_cast<uint32_t>(locals_.size());
    if (num_params + num_locals == 0) return {};
    uint32_t index = data->get<uint8_t>() % (num_params + num_locals);
    ValueType type = index < num_params ? builder_->signature()->GetParam(index)
                                        : locals_[index - num_params];
    return {index, type};
  }

  constexpr static bool is_convertible_kind(ValueKind kind) {
    return kind == kI32 || kind == kI64 || kind == kF32 || kind == kF64;
  }

  template <ValueKind wanted_kind>
  void local_op(DataRange* data, WasmOpcode opcode) {
    static_assert(wanted_kind == kVoid || is_convertible_kind(wanted_kind));
    Var local = GetRandomLocal(data);
    // If there are no locals and no parameters, just generate any value (if a
    // value is needed), or do nothing.
    if (!local.is_valid() || !is_convertible_kind(local.type.kind())) {
      if (wanted_kind == kVoid) return;
      return Generate<wanted_kind>(data);
    }

    if (opcode != kExprLocalGet) Generate(local.type, data);
    builder_->EmitWithU32V(opcode, local.index);
    if (wanted_kind != kVoid && local.type.kind() != wanted_kind) {
      Convert(local.type, ValueType::Primitive(wanted_kind));
    }
  }

  template <ValueKind wanted_kind>
  void get_local(DataRange* data) {
    static_assert(wanted_kind != kVoid, "illegal type");
    local_op<wanted_kind>(data, kExprLocalGet);
  }

  void set_local(DataRange* data) { local_op<kVoid>(data, kExprLocalSet); }

  template <ValueKind wanted_kind>
  void tee_local(DataRange* data) {
    local_op<wanted_kind>(data, kExprLocalTee);
  }

  template <size_t num_bytes>
  void i32_const(DataRange* data) {
    builder_->EmitI32Const(data->getPseudoRandom<int32_t, num_bytes>());
  }

  template <size_t num_bytes>
  void i64_const(DataRange* data) {
    builder_->EmitI64Const(data->getPseudoRandom<int64_t, num_bytes>());
  }

  Var GetRandomGlobal(DataRange* data, bool ensure_mutable) {
    uint32_t index;
    if (ensure_mutable) {
      if (mutable_globals_.empty()) return {};
      index = mutable_globals_[data->get<uint8_t>() % mutable_globals_.size()];
    } else {
      if (globals_.empty()) return {};
      index = data->get<uint8_t>() % globals_.size();
    }
    ValueType type = globals_[index];
    return {index, type};
  }

  template <ValueKind wanted_kind>
  void global_op(DataRange* data) {
    static_assert(wanted_kind == kVoid || is_convertible_kind(wanted_kind));
    constexpr bool is_set = wanted_kind == kVoid;
    Var global = GetRandomGlobal(data, is_set);
    // If there are no globals, just generate any value (if a value is needed),
    // or do nothing.
    if (!global.is_valid() || !is_convertible_kind(global.type.kind())) {
      if (wanted_kind == kVoid) return;
      return Generate<wanted_kind>(data);
    }

    if (is_set) Generate(global.type, data);
    builder_->EmitWithU32V(is_set ? kExprGlobalSet : kExprGlobalGet,
                           global.index);
    if (!is_set && global.type.kind() != wanted_kind) {
      Convert(global.type, ValueType::Primitive(wanted_kind));
    }
  }

  template <ValueKind wanted_kind>
  void get_global(DataRange* data) {
    static_assert(wanted_kind != kVoid, "illegal type");
    global_op<wanted_kind>(data);
  }

  template <ValueKind select_kind>
  void select_with_type(DataRange* data) {
    static_assert(select_kind != kVoid, "illegal kind for select");
    Generate<select_kind, select_kind, kI32>(data);
    // num_types is always 1.
    uint8_t num_types = 1;
    builder_->EmitWithU8U8(kExprSelectWithType, num_types,
                           ValueType::Primitive(select_kind).value_type_code());
  }

  void set_global(DataRange* data) { global_op<kVoid>(data); }

  void throw_or_rethrow(DataRange* data) {
    bool rethrow = data->get<bool>();
    if (rethrow && !catch_blocks_.empty()) {
      int control_depth = static_cast<int>(blocks_.size() - 1);
      int catch_index =
          data->get<uint8_t>() % static_cast<int>(catch_blocks_.size());
      builder_->EmitWithU32V(kExprRethrow,
                             control_depth - catch_blocks_[catch_index]);
    } else {
      int tag = data->get<uint8_t>() % builder_->builder()->NumExceptions();
      const FunctionSig* exception_sig =
          builder_->builder()->GetExceptionType(tag);
      base::Vector<const ValueType> exception_types(
          exception_sig->parameters().begin(),
          exception_sig->parameter_count());
      Generate(exception_types, data);
      builder_->EmitWithU32V(kExprThrow, tag);
    }
  }

  template <ValueKind... Types>
  void sequence(DataRange* data) {
    Generate<Types...>(data);
  }

  void current_memory(DataRange* data) {
    builder_->EmitWithU8(kExprMemorySize, 0);
  }

  void grow_memory(DataRange* data);

  void ref_null(HeapType type, DataRange* data) {
    builder_->EmitWithI32V(kExprRefNull, type.code());
  }

  bool get_local_ref(HeapType type, DataRange* data, Nullability nullable) {
    Var local = GetRandomLocal(data);
    // TODO(14034): Ideally we would check for subtyping here over type
    // equality, but we don't have a module.
    if (local.is_valid() && local.type.is_object_reference() &&
        local.type.heap_type() == type &&
        (local.type.is_nullable()
             ? nullable == kNullable  // We check for nullability-subtyping
             : locals_initialized_    // If the local is not nullable, we cannot
                                      // use it during locals initialization
         )) {
      builder_->EmitWithU32V(kExprLocalGet, local.index);
      return true;
    }

    return false;
  }

  bool new_object(HeapType type, DataRange* data, Nullability nullable) {
    DCHECK(type.is_index());

    uint32_t index = type.ref_index();
    bool new_default = data->get<bool>();

    if (builder_->builder()->IsStructType(index)) {
      const StructType* struct_gen = builder_->builder()->GetStructType(index);
      int field_count = struct_gen->field_count();
      bool can_be_defaultable = std::all_of(
          struct_gen->fields().begin(), struct_gen->fields().end(),
          [](ValueType type) -> bool { return type.is_defaultable(); });

      if (new_default && can_be_defaultable) {
        builder_->EmitWithPrefix(kExprStructNewDefault);
        builder_->EmitU32V(index);
      } else {
        for (int i = 0; i < field_count; i++) {
          Generate(struct_gen->field(i).Unpacked(), data);
        }
        builder_->EmitWithPrefix(kExprStructNew);
        builder_->EmitU32V(index);
      }
    } else if (builder_->builder()->IsArrayType(index)) {
      ValueType element_type =
          builder_->builder()->GetArrayType(index)->element_type();
      bool can_be_defaultable = element_type.is_defaultable();
      WasmOpcode array_new_op[] = {
          kExprArrayNew,        kExprArrayNewFixed,
          kExprArrayNewData,    kExprArrayNewElem,
          kExprArrayNewDefault,  // default op has to be at the end of the list.
      };
      size_t op_size = arraysize(array_new_op);
      if (!can_be_defaultable) --op_size;
      switch (array_new_op[data->get<uint8_t>() % op_size]) {
        case kExprArrayNewElem:
        case kExprArrayNewData: {
          // This is more restrictive than it has to be.
          // TODO(14034): Also support nonnullable and non-index reference
          // types.
          if (element_type.is_reference() && element_type.is_nullable() &&
              element_type.has_index()) {
            // Add a new element segment with the corresponding type.
            uint32_t element_segment = GenerateRefTypeElementSegment(
                data, builder_->builder(), element_type);
            // Generate offset, length.
            // TODO(14034): Change the distribution here to make it more likely
            // that the numbers are in range.
            Generate(base::VectorOf({kWasmI32, kWasmI32}), data);
            // Generate array.new_elem instruction.
            builder_->EmitWithPrefix(kExprArrayNewElem);
            builder_->EmitU32V(index);
            builder_->EmitU32V(element_segment);
            break;
          } else if (!element_type.is_reference()) {
            // Lazily create a data segment if the module doesn't have one yet.
            if (builder_->builder()->NumDataSegments() == 0) {
              GeneratePassiveDataSegment(data, builder_->builder());
            }
            int data_index =
                data->get<uint8_t>() % builder_->builder()->NumDataSegments();
            // Generate offset, length.
            Generate(base::VectorOf({kWasmI32, kWasmI32}), data);
            builder_->EmitWithPrefix(kExprArrayNewData);
            builder_->EmitU32V(index);
            builder_->EmitU32V(data_index);
            break;
          }
          V8_FALLTHROUGH;  // To array.new.
        }
        case kExprArrayNew:
          Generate(element_type.Unpacked(), data);
          Generate(kWasmI32, data);
          builder_->EmitI32Const(kMaxArraySize);
          builder_->Emit(kExprI32RemS);
          builder_->EmitWithPrefix(kExprArrayNew);
          builder_->EmitU32V(index);
          break;
        case kExprArrayNewFixed: {
          size_t element_count =
              std::min(static_cast<size_t>(data->get<uint8_t>()), data->size());
          for (size_t i = 0; i < element_count; ++i) {
            Generate(element_type.Unpacked(), data);
          }
          builder_->EmitWithPrefix(kExprArrayNewFixed);
          builder_->EmitU32V(index);
          builder_->EmitU32V(static_cast<uint32_t>(element_count));
          break;
        }
        case kExprArrayNewDefault:
          Generate(kWasmI32, data);
          builder_->EmitI32Const(kMaxArraySize);
          builder_->Emit(kExprI32RemS);
          builder_->EmitWithPrefix(kExprArrayNewDefault);
          builder_->EmitU32V(index);
          break;
        default:
          FATAL("Unimplemented opcode");
      }
    } else {
      // Map the type index to a function index.
      // TODO(11954. 7748): Once we have type canonicalization, choose a random
      // function from among those matching the signature (consider function
      // subtyping?).
      uint32_t func_index = index - (num_arrays_ + num_structs_);
      DCHECK_EQ(builder_->builder()->GetSignature(index),
                builder_->builder()->GetFunction(func_index)->signature());
      builder_->EmitWithU32V(kExprRefFunc, func_index);
    }

    return true;
  }

  template <ValueKind wanted_kind>
  void table_op(std::vector<ValueType> types, DataRange* data,
                WasmOpcode opcode) {
    DCHECK(opcode == kExprTableSet || opcode == kExprTableSize ||
           opcode == kExprTableGrow || opcode == kExprTableFill);
    int num_tables = builder_->builder()->NumTables();
    DCHECK_GT(num_tables, 0);
    int index = data->get<uint8_t>() % num_tables;
    for (size_t i = 0; i < types.size(); i++) {
      // When passing the reftype by default kWasmFuncRef is used.
      // Then the type is changed according to its table type.
      if (types[i] == kWasmFuncRef) {
        types[i] = builder_->builder()->GetTableType(index);
      }
    }
    Generate(base::VectorOf(types), data);
    if (opcode == kExprTableSet) {
      builder_->Emit(opcode);
    } else {
      builder_->EmitWithPrefix(opcode);
    }
    builder_->EmitU32V(index);
  }

  bool table_get(HeapType type, DataRange* data, Nullability nullable) {
    ValueType needed_type = ValueType::RefMaybeNull(type, nullable);
    int table_count = builder_->builder()->NumTables();
    ZoneVector<uint32_t> table(builder_->builder()->zone());
    for (int i = 0; i < table_count; i++) {
      if (builder_->builder()->GetTableType(i) == needed_type) {
        table.push_back(i);
      }
    }
    if (table.empty()) {
      return false;
    }
    int index = data->get<uint8_t>() % static_cast<int>(table.size());
    Generate(kWasmI32, data);
    builder_->Emit(kExprTableGet);
    builder_->EmitU32V(table[index]);
    return true;
  }

  void table_set(DataRange* data) {
    table_op<kVoid>({kWasmI32, kWasmFuncRef}, data, kExprTableSet);
  }
  void table_size(DataRange* data) { table_op<kI32>({}, data, kExprTableSize); }
  void table_grow(DataRange* data) {
    table_op<kI32>({kWasmFuncRef, kWasmI32}, data, kExprTableGrow);
  }
  void table_fill(DataRange* data) {
    table_op<kVoid>({kWasmI32, kWasmFuncRef, kWasmI32}, data, kExprTableFill);
  }
  void table_copy(DataRange* data) {
    ValueType needed_type = data->get<bool>() ? kWasmFuncRef : kWasmExternRef;
    int table_count = builder_->builder()->NumTables();
    ZoneVector<uint32_t> table(builder_->builder()->zone());
    for (int i = 0; i < table_count; i++) {
      if (builder_->builder()->GetTableType(i) == needed_type) {
        table.push_back(i);
      }
    }
    if (table.empty()) {
      return;
    }
    int first_index = data->get<uint8_t>() % static_cast<int>(table.size());
    int second_index = data->get<uint8_t>() % static_cast<int>(table.size());
    Generate(kWasmI32, data);
    Generate(kWasmI32, data);
    Generate(kWasmI32, data);
    builder_->EmitWithPrefix(kExprTableCopy);
    builder_->EmitU32V(table[first_index]);
    builder_->EmitU32V(table[second_index]);
  }

  bool array_get_helper(ValueType value_type, DataRange* data) {
    WasmModuleBuilder* builder = builder_->builder();
    ZoneVector<uint32_t> array_indices(builder->zone());

    for (uint32_t i = num_structs_; i < num_arrays_ + num_structs_; i++) {
      DCHECK(builder->IsArrayType(i));
      if (builder->GetArrayType(i)->element_type().Unpacked() == value_type) {
        array_indices.push_back(i);
      }
    }

    if (!array_indices.empty()) {
      int index = data->get<uint8_t>() % static_cast<int>(array_indices.size());
      GenerateRef(HeapType(array_indices[index]), data, kNullable);
      Generate(kWasmI32, data);
      if (builder->GetArrayType(array_indices[index])
              ->element_type()
              .is_packed()) {
        builder_->EmitWithPrefix(data->get<bool>() ? kExprArrayGetS
                                                   : kExprArrayGetU);

      } else {
        builder_->EmitWithPrefix(kExprArrayGet);
      }
      builder_->EmitU32V(array_indices[index]);
      return true;
    }

    return false;
  }

  template <ValueKind wanted_kind>
  void array_get(DataRange* data) {
    bool got_array_value =
        array_get_helper(ValueType::Primitive(wanted_kind), data);
    if (!got_array_value) {
      Generate<wanted_kind>(data);
    }
  }
  bool array_get_ref(HeapType type, DataRange* data, Nullability nullable) {
    ValueType needed_type = ValueType::RefMaybeNull(type, nullable);
    return array_get_helper(needed_type, data);
  }

  void i31_get(DataRange* data) {
    GenerateRef(HeapType(HeapType::kI31), data);
    if (data->get<bool>()) {
      builder_->EmitWithPrefix(kExprI31GetS);
    } else {
      builder_->EmitWithPrefix(kExprI31GetU);
    }
  }

  void array_len(DataRange* data) {
    if (num_arrays_ == 0) {
      Generate(kWasmI32, data);
      return;
    }
    GenerateRef(HeapType(HeapType::kArray), data);
    builder_->EmitWithPrefix(kExprArrayLen);
  }

  void array_copy(DataRange* data) {
    if (num_arrays_ == 0) {
      return;
    }
    // TODO(14034): The source element type only has to be a subtype of the
    // destination element type. Currently this only generates copy from same
    // typed arrays.
    int array_index = (data->get<uint8_t>() % num_arrays_) + num_structs_;
    DCHECK(builder_->builder()->IsArrayType(array_index));
    GenerateRef(HeapType(array_index), data);  // destination
    Generate(kWasmI32, data);                  // destination index
    GenerateRef(HeapType(array_index), data);  // source
    Generate(kWasmI32, data);                  // source index
    Generate(kWasmI32, data);                  // length
    builder_->EmitWithPrefix(kExprArrayCopy);
    builder_->EmitU32V(array_index);  // destination array type index
    builder_->EmitU32V(array_index);  // source array type index
  }

  void array_fill(DataRange* data) {
    if (num_arrays_ == 0) {
      return;
    }
    int array_index = (data->get<uint8_t>() % num_arrays_) + num_structs_;
    DCHECK(builder_->builder()->IsArrayType(array_index));
    ValueType element_type = builder_->builder()
                                 ->GetArrayType(array_index)
                                 ->element_type()
                                 .Unpacked();
    GenerateRef(HeapType(array_index), data);  // array
    Generate(kWasmI32, data);                  // offset
    Generate(element_type, data);              // value
    Generate(kWasmI32, data);                  // length
    builder_->EmitWithPrefix(kExprArrayFill);
    builder_->EmitU32V(array_index);
  }

  void array_init_data(DataRange* data) {
    if (num_arrays_ == 0) {
      return;
    }
    int array_index = (data->get<uint8_t>() % num_arrays_) + num_structs_;
    DCHECK(builder_->builder()->IsArrayType(array_index));
    const ArrayType* array_type =
        builder_->builder()->GetArrayType(array_index);
    DCHECK(array_type->mutability());
    ValueType element_type = array_type->element_type().Unpacked();
    if (element_type.is_reference()) {
      return;
    }
    if (builder_->builder()->NumDataSegments() == 0) {
      GeneratePassiveDataSegment(data, builder_->builder());
    }

    int data_index =
        data->get<uint8_t>() % builder_->builder()->NumDataSegments();
    // Generate array, index, data_offset, length.
    Generate(base::VectorOf({ValueType::RefNull(array_index), kWasmI32,
                             kWasmI32, kWasmI32}),
             data);
    builder_->EmitWithPrefix(kExprArrayInitData);
    builder_->EmitU32V(array_index);
    builder_->EmitU32V(data_index);
  }

  void array_init_elem(DataRange* data) {
    if (num_arrays_ == 0) {
      return;
    }
    int array_index = (data->get<uint8_t>() % num_arrays_) + num_structs_;
    DCHECK(builder_->builder()->IsArrayType(array_index));
    const ArrayType* array_type =
        builder_->builder()->GetArrayType(array_index);
    DCHECK(array_type->mutability());
    ValueType element_type = array_type->element_type().Unpacked();
    // This is more restrictive than it has to be.
    // TODO(14034): Also support nonnullable and non-index reference
    // types.
    if (!element_type.is_reference() || element_type.is_non_nullable() ||
        !element_type.has_index()) {
      return;
    }
    // Add a new element segment with the corresponding type.
    uint32_t element_segment =
        GenerateRefTypeElementSegment(data, builder_->builder(), element_type);
    // Generate array, index, elem_offset, length.
    // TODO(14034): Change the distribution here to make it more likely
    // that the numbers are in range.
    Generate(base::VectorOf({ValueType::RefNull(array_index), kWasmI32,
                             kWasmI32, kWasmI32}),
             data);
    // Generate array.new_elem instruction.
    builder_->EmitWithPrefix(kExprArrayInitElem);
    builder_->EmitU32V(array_index);
    builder_->EmitU32V(element_segment);
  }

  void array_set(DataRange* data) {
    WasmModuleBuilder* builder = builder_->builder();
    ZoneVector<uint32_t> array_indices(builder->zone());
    for (uint32_t i = num_structs_; i < num_arrays_ + num_structs_; i++) {
      DCHECK(builder->IsArrayType(i));
      if (builder->GetArrayType(i)->mutability()) {
        array_indices.push_back(i);
      }
    }

    if (array_indices.empty()) {
      return;
    }

    int index = data->get<uint8_t>() % static_cast<int>(array_indices.size());
    GenerateRef(HeapType(array_indices[index]), data);
    Generate(kWasmI32, data);
    Generate(
        builder->GetArrayType(array_indices[index])->element_type().Unpacked(),
        data);
    builder_->EmitWithPrefix(kExprArraySet);
    builder_->EmitU32V(array_indices[index]);
  }

  bool struct_get_helper(ValueType value_type, DataRange* data) {
    WasmModuleBuilder* builder = builder_->builder();
    ZoneVector<uint32_t> field_index(builder->zone());
    ZoneVector<uint32_t> struct_index(builder->zone());
    for (uint32_t i = 0; i < num_structs_; i++) {
      DCHECK(builder->IsStructType(i));
      int field_count = builder->GetStructType(i)->field_count();
      for (int index = 0; index < field_count; index++) {
        // TODO(14034): This should be a subtype check!
        if (builder->GetStructType(i)->field(index) == value_type) {
          field_index.push_back(index);
          struct_index.push_back(i);
        }
      }
    }
    if (!field_index.empty()) {
      int index = data->get<uint8_t>() % static_cast<int>(field_index.size());
      GenerateRef(HeapType(struct_index[index]), data, kNullable);
      if (builder->GetStructType(struct_index[index])
              ->field(field_index[index])
              .is_packed()) {
        builder_->EmitWithPrefix(data->get<bool>() ? kExprStructGetS
                                                   : kExprStructGetU);
      } else {
        builder_->EmitWithPrefix(kExprStructGet);
      }
      builder_->EmitU32V(struct_index[index]);
      builder_->EmitU32V(field_index[index]);
      return true;
    }
    return false;
  }

  template <ValueKind wanted_kind>
  void struct_get(DataRange* data) {
    bool got_struct_value =
        struct_get_helper(ValueType::Primitive(wanted_kind), data);
    if (!got_struct_value) {
      Generate<wanted_kind>(data);
    }
  }

  bool struct_get_ref(HeapType type, DataRange* data, Nullability nullable) {
    ValueType needed_type = ValueType::RefMaybeNull(type, nullable);
    return struct_get_helper(needed_type, data);
  }

  bool ref_cast(HeapType type, DataRange* data, Nullability nullable) {
    HeapType input_type = top_type(type);
    GenerateRef(input_type, data);
    builder_->EmitWithPrefix(nullable ? kExprRefCastNull : kExprRefCast);
    builder_->EmitI32V(type.code());
    return true;  // It always produces the desired result type.
  }

  HeapType top_type(HeapType type) {
    switch (type.representation()) {
      case HeapType::kAny:
      case HeapType::kEq:
      case HeapType::kArray:
      case HeapType::kStruct:
      case HeapType::kI31:
      case HeapType::kNone:
        return HeapType(HeapType::kAny);
      case HeapType::kExtern:
      case HeapType::kNoExtern:
        return HeapType(HeapType::kExtern);
      case HeapType::kFunc:
      case HeapType::kNoFunc:
        return HeapType(HeapType::kFunc);
      default:
        DCHECK(type.is_index());
        if (builder_->builder()->IsSignature(type.ref_index())) {
          return HeapType(HeapType::kFunc);
        }
        DCHECK(builder_->builder()->IsStructType(type.ref_index()) ||
               builder_->builder()->IsArrayType(type.ref_index()));
        return HeapType(HeapType::kAny);
    }
  }

  HeapType choose_sub_type(HeapType type, DataRange* data) {
    switch (type.representation()) {
      case HeapType::kAny: {
        constexpr HeapType::Representation generic_types[] = {
            HeapType::kAny,    HeapType::kEq,  HeapType::kArray,
            HeapType::kStruct, HeapType::kI31, HeapType::kNone,
        };
        const int type_count = num_arrays_ + num_structs_;
        const int choice =
            data->get<uint8_t>() % (type_count + arraysize(generic_types));
        return choice >= type_count
                   ? HeapType(generic_types[choice - type_count])
                   : HeapType(choice);
      }
      case HeapType::kEq: {
        constexpr HeapType::Representation generic_types[] = {
            HeapType::kEq,  HeapType::kArray, HeapType::kStruct,
            HeapType::kI31, HeapType::kNone,
        };
        const int type_count = num_arrays_ + num_structs_;
        const int choice =
            data->get<uint8_t>() % (type_count + arraysize(generic_types));
        return choice >= type_count
                   ? HeapType(generic_types[choice - type_count])
                   : HeapType(choice);
      }
      case HeapType::kStruct: {
        constexpr HeapType::Representation generic_types[] = {
            HeapType::kStruct,
            HeapType::kNone,
        };
        const int type_count = num_structs_;
        const int choice =
            data->get<uint8_t>() % (type_count + arraysize(generic_types));
        return choice >= type_count
                   ? HeapType(generic_types[choice - type_count])
                   : HeapType(choice);
      }
      case HeapType::kArray: {
        constexpr HeapType::Representation generic_types[] = {
            HeapType::kArray,
            HeapType::kNone,
        };
        const int type_count = num_arrays_;
        const int choice =
            data->get<uint8_t>() % (type_count + arraysize(generic_types));
        return choice >= type_count
                   ? HeapType(generic_types[choice - type_count])
                   : HeapType(choice + num_structs_);
      }
      case HeapType::kFunc: {
        constexpr HeapType::Representation generic_types[] = {
            HeapType::kFunc, HeapType::kNoFunc};
        const int type_count = static_cast<int>(functions_.size());
        const int choice =
            data->get<uint8_t>() % (type_count + arraysize(generic_types));
        return choice >= type_count
                   ? HeapType(generic_types[choice - type_count])
                   : HeapType(functions_[choice]);
      }
      case HeapType::kExtern:
        return HeapType(data->get<bool>() ? HeapType::kExtern
                                          : HeapType::kNoExtern);
      default:
        if (!type.is_index()) {
          // No logic implemented to find a sub-type.
          return type;
        }
        // Collect all (direct) sub types.
        // TODO(14034): Also collect indirect sub types.
        std::vector<uint32_t> subtypes;
        uint32_t type_count = builder_->builder()->NumTypes();
        for (uint32_t i = 0; i < type_count; ++i) {
          if (builder_->builder()->GetSuperType(i) == type.ref_index()) {
            subtypes.push_back(i);
          }
        }
        return subtypes.empty()
                   ? type  // no downcast possible
                   : HeapType(subtypes[data->get<uint8_t>() % subtypes.size()]);
    }
  }

  bool br_on_cast(HeapType type, DataRange* data, Nullability nullable) {
    DCHECK(!blocks_.empty());
    const uint32_t target_block = data->get<uint8_t>() % blocks_.size();
    const uint32_t block_index =
        static_cast<uint32_t>(blocks_.size()) - 1 - target_block;
    const auto break_types = base::VectorOf(blocks_[target_block]);
    if (break_types.empty()) {
      return false;
    }
    ValueType break_type = break_types[break_types.size() - 1];
    if (!break_type.is_reference()) {
      return false;
    }

    Generate(base::VectorOf(break_types.data(), break_types.size() - 1), data);
    if (data->get<bool>()) {
      // br_on_cast
      HeapType source_type = top_type(break_type.heap_type());
      const bool source_is_nullable = data->get<bool>();
      GenerateRef(source_type, data,
                  source_is_nullable ? kNullable : kNonNullable);
      const bool target_is_nullable =
          source_is_nullable && break_type.is_nullable() && data->get<bool>();
      builder_->EmitWithPrefix(kExprBrOnCastGeneric);
      builder_->EmitU32V(source_is_nullable + (target_is_nullable << 1));
      builder_->EmitU32V(block_index);
      builder_->EmitI32V(source_type.code());             // source type
      builder_->EmitI32V(break_type.heap_type().code());  // target type
      // Fallthrough: Generate the actually desired ref type.
      ConsumeAndGenerate(break_types, {}, data);
      GenerateRef(type, data, nullable);
    } else {
      // br_on_cast_fail
      HeapType source_type = break_type.heap_type();
      const bool source_is_nullable = data->get<bool>();
      GenerateRef(source_type, data,
                  source_is_nullable ? kNullable : kNonNullable);
      const bool target_is_nullable =
          source_is_nullable &&
          (!break_type.is_nullable() || data->get<bool>());
      HeapType target_type = choose_sub_type(source_type, data);

      builder_->EmitWithPrefix(kExprBrOnCastFailGeneric);
      builder_->EmitU32V(source_is_nullable + (target_is_nullable << 1));
      builder_->EmitU32V(block_index);
      builder_->EmitI32V(source_type.code());
      builder_->EmitI32V(target_type.code());
      // Fallthrough: Generate the actually desired ref type.
      ConsumeAndGenerate(break_types, {}, data);
      GenerateRef(type, data, nullable);
    }
    return true;
  }

  bool any_convert_extern(HeapType type, DataRange* data,
                          Nullability nullable) {
    if (type.representation() != HeapType::kAny) {
      return false;
    }
    GenerateRef(HeapType(HeapType::kExtern), data);
    builder_->EmitWithPrefix(kExprAnyConvertExtern);
    if (nullable == kNonNullable) {
      builder_->Emit(kExprRefAsNonNull);
    }
    return true;
  }

  bool ref_as_non_null(HeapType type, DataRange* data, Nullability nullable) {
    GenerateRef(type, data, kNullable);
    builder_->Emit(kExprRefAsNonNull);
    return true;
  }

  void struct_set(DataRange* data) {
    WasmModuleBuilder* builder = builder_->builder();
    if (num_structs_ > 0) {
      int struct_index = data->get<uint8_t>() % num_structs_;
      DCHECK(builder->IsStructType(struct_index));
      const StructType* struct_type = builder->GetStructType(struct_index);
      ZoneVector<uint32_t> field_indices(builder->zone());
      for (uint32_t i = 0; i < struct_type->field_count(); i++) {
        if (struct_type->mutability(i)) {
          field_indices.push_back(i);
        }
      }
      if (field_indices.empty()) {
        return;
      }
      int field_index =
          field_indices[data->get<uint8_t>() % field_indices.size()];
      GenerateRef(HeapType(struct_index), data);
      Generate(struct_type->field(field_index).Unpacked(), data);
      builder_->EmitWithPrefix(kExprStructSet);
      builder_->EmitU32V(struct_index);
      builder_->EmitU32V(field_index);
    }
  }

  void ref_is_null(DataRange* data) {
    GenerateRef(HeapType(HeapType::kAny), data);
    builder_->Emit(kExprRefIsNull);
  }

  template <WasmOpcode opcode>
  void ref_test(DataRange* data) {
    GenerateRef(HeapType(HeapType::kAny), data);
    constexpr int generic_types[] = {kAnyRefCode,    kEqRefCode, kArrayRefCode,
                                     kStructRefCode, kNoneCode,  kI31RefCode};
    int num_types = num_structs_ + num_arrays_;
    int num_all_types = num_types + arraysize(generic_types);
    int type_choice = data->get<uint8_t>() % num_all_types;
    builder_->EmitWithPrefix(opcode);
    if (type_choice < num_types) {
      builder_->EmitU32V(type_choice);
    } else {
      builder_->EmitU32V(generic_types[type_choice - num_types]);
    }
  }

  void ref_eq(DataRange* data) {
    GenerateRef(HeapType(HeapType::kEq), data);
    GenerateRef(HeapType(HeapType::kEq), data);
    builder_->Emit(kExprRefEq);
  }

  using GenerateFn = void (WasmGenerator::*const)(DataRange*);
  using GenerateFnWithHeap = bool (WasmGenerator::*const)(HeapType, DataRange*,
                                                          Nullability);

  template <size_t N>
  void GenerateOneOf(GenerateFn (&alternatives)[N], DataRange* data) {
    static_assert(N < std::numeric_limits<uint8_t>::max(),
                  "Too many alternatives. Use a bigger type if needed.");
    const auto which = data->get<uint8_t>();

    GenerateFn alternate = alternatives[which % N];
    (this->*alternate)(data);
  }

  // Returns true if it had succesfully generated the reference
  // and false otherwise.
  template <size_t N>
  bool GenerateOneOf(GenerateFnWithHeap (&alternatives)[N], HeapType type,
                     DataRange* data, Nullability nullability) {
    static_assert(N < std::numeric_limits<uint8_t>::max(),
                  "Too many alternatives. Use a bigger type if needed.");

    int index = data->get<uint8_t>() % (N + 1);

    if (nullability && index == N) {
      ref_null(type, data);
      return true;
    }

    for (int i = index; i < static_cast<int>(N); i++) {
      if ((this->*alternatives[i])(type, data, nullability)) {
        return true;
      }
    }

    for (int i = 0; i < index; i++) {
      if ((this->*alternatives[i])(type, data, nullability)) {
        return true;
      }
    }

    if (nullability == kNullable) {
      ref_null(type, data);
      return true;
    }

    return false;
  }

  struct GeneratorRecursionScope {
    explicit GeneratorRecursionScope(WasmGenerator* gen) : gen(gen) {
      ++gen->recursion_depth;
      DCHECK_LE(gen->recursion_depth, kMaxRecursionDepth);
    }
    ~GeneratorRecursionScope() {
      DCHECK_GT(gen->recursion_depth, 0);
      --gen->recursion_depth;
    }
    WasmGenerator* gen;
  };

 public:
  WasmGenerator(WasmFunctionBuilder* fn, const std::vector<uint32_t>& functions,
                const std::vector<ValueType>& globals,
                const std::vector<uint8_t>& mutable_globals,
                uint32_t num_structs, uint32_t num_arrays, DataRange* data)
      : builder_(fn),
        functions_(functions),
        globals_(globals),
        mutable_globals_(mutable_globals),
        num_structs_(num_structs),
        num_arrays_(num_arrays),
        locals_initialized_(false) {
    const FunctionSig* sig = fn->signature();
    blocks_.emplace_back();
    for (size_t i = 0; i < sig->return_count(); ++i) {
      blocks_.back().push_back(sig->GetReturn(i));
    }
    constexpr uint32_t kMaxLocals = 32;
    locals_.resize(data->get<uint8_t>() % kMaxLocals);
    uint32_t num_types =
        static_cast<uint32_t>(functions_.size()) + num_structs_ + num_arrays_;
    for (ValueType& local : locals_) {
      local = GetValueType(data, num_types);
      fn->AddLocal(local);
    }
  }

  void Generate(ValueType type, DataRange* data);

  template <ValueKind T>
  void Generate(DataRange* data);

  template <ValueKind T1, ValueKind T2, ValueKind... Ts>
  void Generate(DataRange* data) {
    // TODO(clemensb): Implement a more even split.
    // TODO(mliedtke): Instead of splitting we should probably "reserve" amount
    // x for the first part, any reserved but potentially unused random bytes
    // should then carry over instead of throwing them away which heavily
    // reduces the amount of actually used random input bytes.
    auto first_data = data->split();
    Generate<T1>(&first_data);
    Generate<T2, Ts...>(data);
  }

  void GenerateRef(DataRange* data);

  void GenerateRef(HeapType type, DataRange* data,
                   Nullability nullability = kNullable);

  std::vector<ValueType> GenerateTypes(DataRange* data);
  void Generate(base::Vector<const ValueType> types, DataRange* data);
  void ConsumeAndGenerate(base::Vector<const ValueType> parameter_types,
                          base::Vector<const ValueType> return_types,
                          DataRange* data);
  bool HasSimd() { return has_simd_; }

  void InitializeNonDefaultableLocals(DataRange* data) {
    for (uint32_t i = 0; i < locals_.size(); i++) {
      if (!locals_[i].is_defaultable()) {
        GenerateRef(locals_[i].heap_type(), data, kNonNullable);
        builder_->EmitWithU32V(
            kExprLocalSet, i + static_cast<uint32_t>(
                                   builder_->signature()->parameter_count()));
      }
    }
    locals_initialized_ = true;
  }

 private:
  WasmFunctionBuilder* builder_;
  std::vector<std::vector<ValueType>> blocks_;
  const std::vector<uint32_t>& functions_;
  std::vector<ValueType> locals_;
  std::vector<ValueType> globals_;
  std::vector<uint8_t> mutable_globals_;  // indexes into {globals_}.
  uint32_t recursion_depth = 0;
  std::vector<int> catch_blocks_;
  bool has_simd_;
  uint32_t num_structs_;
  uint32_t num_arrays_;
  bool locals_initialized_;

  bool recursion_limit_reached() {
    return recursion_depth >= kMaxRecursionDepth;
  }
};

template <>
void WasmGenerator::block<kVoid>(DataRange* data) {
  block({}, {}, data);
}

template <>
void WasmGenerator::loop<kVoid>(DataRange* data) {
  loop({}, {}, data);
}

template <>
void WasmGenerator::Generate<kVoid>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() == 0) return;

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::sequence<kVoid, kVoid>,
      &WasmGenerator::sequence<kVoid, kVoid, kVoid, kVoid>,
      &WasmGenerator::sequence<kVoid, kVoid, kVoid, kVoid, kVoid, kVoid, kVoid,
                               kVoid>,
      &WasmGenerator::block<kVoid>,
      &WasmGenerator::loop<kVoid>,
      &WasmGenerator::if_<kVoid, kIf>,
      &WasmGenerator::if_<kVoid, kIfElse>,
      &WasmGenerator::br,
      &WasmGenerator::br_if<kVoid>,
      &WasmGenerator::br_on_null<kVoid>,
      &WasmGenerator::br_on_non_null<kVoid>,
      &WasmGenerator::br_table<kVoid>,
      &WasmGenerator::return_op,

      &WasmGenerator::memop<kExprI32StoreMem, kI32>,
      &WasmGenerator::memop<kExprI32StoreMem8, kI32>,
      &WasmGenerator::memop<kExprI32StoreMem16, kI32>,
      &WasmGenerator::memop<kExprI64StoreMem, kI64>,
      &WasmGenerator::memop<kExprI64StoreMem8, kI64>,
      &WasmGenerator::memop<kExprI64StoreMem16, kI64>,
      &WasmGenerator::memop<kExprI64StoreMem32, kI64>,
      &WasmGenerator::memop<kExprF32StoreMem, kF32>,
      &WasmGenerator::memop<kExprF64StoreMem, kF64>,
      &WasmGenerator::memop<kExprI32AtomicStore, kI32>,
      &WasmGenerator::memop<kExprI32AtomicStore8U, kI32>,
      &WasmGenerator::memop<kExprI32AtomicStore16U, kI32>,
      &WasmGenerator::memop<kExprI64AtomicStore, kI64>,
      &WasmGenerator::memop<kExprI64AtomicStore8U, kI64>,
      &WasmGenerator::memop<kExprI64AtomicStore16U, kI64>,
      &WasmGenerator::memop<kExprI64AtomicStore32U, kI64>,
      &WasmGenerator::memop<kExprS128StoreMem, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Store8Lane, 16, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Store16Lane, 8, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Store32Lane, 4, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Store64Lane, 2, kS128>,

      &WasmGenerator::drop,

      &WasmGenerator::call<kVoid>,
      &WasmGenerator::call_indirect<kVoid>,
      &WasmGenerator::call_ref<kVoid>,

      &WasmGenerator::set_local,
      &WasmGenerator::set_global,
      &WasmGenerator::throw_or_rethrow,
      &WasmGenerator::try_block<kVoid>,

      &WasmGenerator::struct_set,
      &WasmGenerator::array_set,
      &WasmGenerator::array_copy,
      &WasmGenerator::array_fill,
      &WasmGenerator::array_init_data,
      &WasmGenerator::array_init_elem,

      &WasmGenerator::table_set,
      &WasmGenerator::table_fill,
      &WasmGenerator::table_copy};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kI32>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= 1) {
    builder_->EmitI32Const(data->getPseudoRandom<uint32_t>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::i32_const<1>,
      &WasmGenerator::i32_const<2>,
      &WasmGenerator::i32_const<3>,
      &WasmGenerator::i32_const<4>,

      &WasmGenerator::sequence<kI32, kVoid>,
      &WasmGenerator::sequence<kVoid, kI32>,
      &WasmGenerator::sequence<kVoid, kI32, kVoid>,

      &WasmGenerator::op<kExprI32Eqz, kI32>,
      &WasmGenerator::op<kExprI32Eq, kI32, kI32>,
      &WasmGenerator::op<kExprI32Ne, kI32, kI32>,
      &WasmGenerator::op<kExprI32LtS, kI32, kI32>,
      &WasmGenerator::op<kExprI32LtU, kI32, kI32>,
      &WasmGenerator::op<kExprI32GeS, kI32, kI32>,
      &WasmGenerator::op<kExprI32GeU, kI32, kI32>,

      &WasmGenerator::op<kExprI64Eqz, kI64>,
      &WasmGenerator::op<kExprI64Eq, kI64, kI64>,
      &WasmGenerator::op<kExprI64Ne, kI64, kI64>,
      &WasmGenerator::op<kExprI64LtS, kI64, kI64>,
      &WasmGenerator::op<kExprI64LtU, kI64, kI64>,
      &WasmGenerator::op<kExprI64GeS, kI64, kI64>,
      &WasmGenerator::op<kExprI64GeU, kI64, kI64>,

      &WasmGenerator::op<kExprF32Eq, kF32, kF32>,
      &WasmGenerator::op<kExprF32Ne, kF32, kF32>,
      &WasmGenerator::op<kExprF32Lt, kF32, kF32>,
      &WasmGenerator::op<kExprF32Ge, kF32, kF32>,

      &WasmGenerator::op<kExprF64Eq, kF64, kF64>,
      &WasmGenerator::op<kExprF64Ne, kF64, kF64>,
      &WasmGenerator::op<kExprF64Lt, kF64, kF64>,
      &WasmGenerator::op<kExprF64Ge, kF64, kF64>,

      &WasmGenerator::op<kExprI32Add, kI32, kI32>,
      &WasmGenerator::op<kExprI32Sub, kI32, kI32>,
      &WasmGenerator::op<kExprI32Mul, kI32, kI32>,

      &WasmGenerator::op<kExprI32DivS, kI32, kI32>,
      &WasmGenerator::op<kExprI32DivU, kI32, kI32>,
      &WasmGenerator::op<kExprI32RemS, kI32, kI32>,
      &WasmGenerator::op<kExprI32RemU, kI32, kI32>,

      &WasmGenerator::op<kExprI32And, kI32, kI32>,
      &WasmGenerator::op<kExprI32Ior, kI32, kI32>,
      &WasmGenerator::op<kExprI32Xor, kI32, kI32>,
      &WasmGenerator::op<kExprI32Shl, kI32, kI32>,
      &WasmGenerator::op<kExprI32ShrU, kI32, kI32>,
      &WasmGenerator::op<kExprI32ShrS, kI32, kI32>,
      &WasmGenerator::op<kExprI32Ror, kI32, kI32>,
      &WasmGenerator::op<kExprI32Rol, kI32, kI32>,

      &WasmGenerator::op<kExprI32Clz, kI32>,
      &WasmGenerator::op<kExprI32Ctz, kI32>,
      &WasmGenerator::op<kExprI32Popcnt, kI32>,

      &WasmGenerator::op<kExprI32ConvertI64, kI64>,
      &WasmGenerator::op<kExprI32SConvertF32, kF32>,
      &WasmGenerator::op<kExprI32UConvertF32, kF32>,
      &WasmGenerator::op<kExprI32SConvertF64, kF64>,
      &WasmGenerator::op<kExprI32UConvertF64, kF64>,
      &WasmGenerator::op<kExprI32ReinterpretF32, kF32>,

      &WasmGenerator::op_with_prefix<kExprI32SConvertSatF32, kF32>,
      &WasmGenerator::op_with_prefix<kExprI32UConvertSatF32, kF32>,
      &WasmGenerator::op_with_prefix<kExprI32SConvertSatF64, kF64>,
      &WasmGenerator::op_with_prefix<kExprI32UConvertSatF64, kF64>,

      &WasmGenerator::block<kI32>,
      &WasmGenerator::loop<kI32>,
      &WasmGenerator::if_<kI32, kIfElse>,
      &WasmGenerator::br_if<kI32>,
      &WasmGenerator::br_on_null<kI32>,
      &WasmGenerator::br_on_non_null<kI32>,
      &WasmGenerator::br_table<kI32>,

      &WasmGenerator::memop<kExprI32LoadMem>,
      &WasmGenerator::memop<kExprI32LoadMem8S>,
      &WasmGenerator::memop<kExprI32LoadMem8U>,
      &WasmGenerator::memop<kExprI32LoadMem16S>,
      &WasmGenerator::memop<kExprI32LoadMem16U>,
      &WasmGenerator::memop<kExprI32AtomicLoad>,
      &WasmGenerator::memop<kExprI32AtomicLoad8U>,
      &WasmGenerator::memop<kExprI32AtomicLoad16U>,

      &WasmGenerator::atomic_op<kExprI32AtomicAdd, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicSub, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAnd, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicOr, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicXor, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicExchange, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicCompareExchange, kI32, kI32,
                                kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAdd8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicSub8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAnd8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicOr8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicXor8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicExchange8U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicCompareExchange8U, kI32, kI32,
                                kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAdd16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicSub16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicAnd16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicOr16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicXor16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicExchange16U, kI32, kI32>,
      &WasmGenerator::atomic_op<kExprI32AtomicCompareExchange16U, kI32, kI32,
                                kI32>,

      &WasmGenerator::op_with_prefix<kExprV128AnyTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16AllTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16BitMask, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8AllTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8BitMask, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4AllTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4BitMask, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2AllTrue, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2BitMask, kS128>,
      &WasmGenerator::simd_lane_op<kExprI8x16ExtractLaneS, 16, kS128>,
      &WasmGenerator::simd_lane_op<kExprI8x16ExtractLaneU, 16, kS128>,
      &WasmGenerator::simd_lane_op<kExprI16x8ExtractLaneS, 8, kS128>,
      &WasmGenerator::simd_lane_op<kExprI16x8ExtractLaneU, 8, kS128>,
      &WasmGenerator::simd_lane_op<kExprI32x4ExtractLane, 4, kS128>,

      &WasmGenerator::current_memory,
      &WasmGenerator::grow_memory,

      &WasmGenerator::get_local<kI32>,
      &WasmGenerator::tee_local<kI32>,
      &WasmGenerator::get_global<kI32>,
      &WasmGenerator::op<kExprSelect, kI32, kI32, kI32>,
      &WasmGenerator::select_with_type<kI32>,

      &WasmGenerator::call<kI32>,
      &WasmGenerator::call_indirect<kI32>,
      &WasmGenerator::call_ref<kI32>,
      &WasmGenerator::try_block<kI32>,

      &WasmGenerator::i31_get,

      &WasmGenerator::struct_get<kI32>,
      &WasmGenerator::array_get<kI32>,
      &WasmGenerator::array_len,

      &WasmGenerator::ref_is_null,
      &WasmGenerator::ref_eq,
      &WasmGenerator::ref_test<kExprRefTest>,
      &WasmGenerator::ref_test<kExprRefTestNull>,

      &WasmGenerator::table_size,
      &WasmGenerator::table_grow};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kI64>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= 1) {
    builder_->EmitI64Const(data->getPseudoRandom<int64_t>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::i64_const<1>,
      &WasmGenerator::i64_const<2>,
      &WasmGenerator::i64_const<3>,
      &WasmGenerator::i64_const<4>,
      &WasmGenerator::i64_const<5>,
      &WasmGenerator::i64_const<6>,
      &WasmGenerator::i64_const<7>,
      &WasmGenerator::i64_const<8>,

      &WasmGenerator::sequence<kI64, kVoid>,
      &WasmGenerator::sequence<kVoid, kI64>,
      &WasmGenerator::sequence<kVoid, kI64, kVoid>,

      &WasmGenerator::op<kExprI64Add, kI64, kI64>,
      &WasmGenerator::op<kExprI64Sub, kI64, kI64>,
      &WasmGenerator::op<kExprI64Mul, kI64, kI64>,

      &WasmGenerator::op<kExprI64DivS, kI64, kI64>,
      &WasmGenerator::op<kExprI64DivU, kI64, kI64>,
      &WasmGenerator::op<kExprI64RemS, kI64, kI64>,
      &WasmGenerator::op<kExprI64RemU, kI64, kI64>,

      &WasmGenerator::op<kExprI64And, kI64, kI64>,
      &WasmGenerator::op<kExprI64Ior, kI64, kI64>,
      &WasmGenerator::op<kExprI64Xor, kI64, kI64>,
      &WasmGenerator::op<kExprI64Shl, kI64, kI64>,
      &WasmGenerator::op<kExprI64ShrU, kI64, kI64>,
      &WasmGenerator::op<kExprI64ShrS, kI64, kI64>,
      &WasmGenerator::op<kExprI64Ror, kI64, kI64>,
      &WasmGenerator::op<kExprI64Rol, kI64, kI64>,

      &WasmGenerator::op<kExprI64Clz, kI64>,
      &WasmGenerator::op<kExprI64Ctz, kI64>,
      &WasmGenerator::op<kExprI64Popcnt, kI64>,

      &WasmGenerator::op_with_prefix<kExprI64SConvertSatF32, kF32>,
      &WasmGenerator::op_with_prefix<kExprI64UConvertSatF32, kF32>,
      &WasmGenerator::op_with_prefix<kExprI64SConvertSatF64, kF64>,
      &WasmGenerator::op_with_prefix<kExprI64UConvertSatF64, kF64>,

      &WasmGenerator::block<kI64>,
      &WasmGenerator::loop<kI64>,
      &WasmGenerator::if_<kI64, kIfElse>,
      &WasmGenerator::br_if<kI64>,
      &WasmGenerator::br_on_null<kI64>,
      &WasmGenerator::br_on_non_null<kI64>,
      &WasmGenerator::br_table<kI64>,

      &WasmGenerator::memop<kExprI64LoadMem>,
      &WasmGenerator::memop<kExprI64LoadMem8S>,
      &WasmGenerator::memop<kExprI64LoadMem8U>,
      &WasmGenerator::memop<kExprI64LoadMem16S>,
      &WasmGenerator::memop<kExprI64LoadMem16U>,
      &WasmGenerator::memop<kExprI64LoadMem32S>,
      &WasmGenerator::memop<kExprI64LoadMem32U>,
      &WasmGenerator::memop<kExprI64AtomicLoad>,
      &WasmGenerator::memop<kExprI64AtomicLoad8U>,
      &WasmGenerator::memop<kExprI64AtomicLoad16U>,
      &WasmGenerator::memop<kExprI64AtomicLoad32U>,

      &WasmGenerator::atomic_op<kExprI64AtomicAdd, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange, kI32, kI64,
                                kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAdd8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange8U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange8U, kI32, kI64,
                                kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAdd16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange16U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange16U, kI32, kI64,
                                kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAdd32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicSub32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicAnd32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicOr32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicXor32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicExchange32U, kI32, kI64>,
      &WasmGenerator::atomic_op<kExprI64AtomicCompareExchange32U, kI32, kI64,
                                kI64>,

      &WasmGenerator::simd_lane_op<kExprI64x2ExtractLane, 2, kS128>,

      &WasmGenerator::get_local<kI64>,
      &WasmGenerator::tee_local<kI64>,
      &WasmGenerator::get_global<kI64>,
      &WasmGenerator::op<kExprSelect, kI64, kI64, kI32>,
      &WasmGenerator::select_with_type<kI64>,

      &WasmGenerator::call<kI64>,
      &WasmGenerator::call_indirect<kI64>,
      &WasmGenerator::call_ref<kI64>,
      &WasmGenerator::try_block<kI64>,

      &WasmGenerator::struct_get<kI64>,
      &WasmGenerator::array_get<kI64>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kF32>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= sizeof(float)) {
    builder_->EmitF32Const(data->getPseudoRandom<float>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::sequence<kF32, kVoid>,
      &WasmGenerator::sequence<kVoid, kF32>,
      &WasmGenerator::sequence<kVoid, kF32, kVoid>,

      &WasmGenerator::op<kExprF32Abs, kF32>,
      &WasmGenerator::op<kExprF32Neg, kF32>,
      &WasmGenerator::op<kExprF32Ceil, kF32>,
      &WasmGenerator::op<kExprF32Floor, kF32>,
      &WasmGenerator::op<kExprF32Trunc, kF32>,
      &WasmGenerator::op<kExprF32NearestInt, kF32>,
      &WasmGenerator::op<kExprF32Sqrt, kF32>,
      &WasmGenerator::op<kExprF32Add, kF32, kF32>,
      &WasmGenerator::op<kExprF32Sub, kF32, kF32>,
      &WasmGenerator::op<kExprF32Mul, kF32, kF32>,
      &WasmGenerator::op<kExprF32Div, kF32, kF32>,
      &WasmGenerator::op<kExprF32Min, kF32, kF32>,
      &WasmGenerator::op<kExprF32Max, kF32, kF32>,
      &WasmGenerator::op<kExprF32CopySign, kF32, kF32>,

      &WasmGenerator::op<kExprF32SConvertI32, kI32>,
      &WasmGenerator::op<kExprF32UConvertI32, kI32>,
      &WasmGenerator::op<kExprF32SConvertI64, kI64>,
      &WasmGenerator::op<kExprF32UConvertI64, kI64>,
      &WasmGenerator::op<kExprF32ConvertF64, kF64>,
      &WasmGenerator::op<kExprF32ReinterpretI32, kI32>,

      &WasmGenerator::block<kF32>,
      &WasmGenerator::loop<kF32>,
      &WasmGenerator::if_<kF32, kIfElse>,
      &WasmGenerator::br_if<kF32>,
      &WasmGenerator::br_on_null<kF32>,
      &WasmGenerator::br_on_non_null<kF32>,
      &WasmGenerator::br_table<kF32>,

      &WasmGenerator::memop<kExprF32LoadMem>,

      &WasmGenerator::simd_lane_op<kExprF32x4ExtractLane, 4, kS128>,

      &WasmGenerator::get_local<kF32>,
      &WasmGenerator::tee_local<kF32>,
      &WasmGenerator::get_global<kF32>,
      &WasmGenerator::op<kExprSelect, kF32, kF32, kI32>,
      &WasmGenerator::select_with_type<kF32>,

      &WasmGenerator::call<kF32>,
      &WasmGenerator::call_indirect<kF32>,
      &WasmGenerator::call_ref<kF32>,
      &WasmGenerator::try_block<kF32>,

      &WasmGenerator::struct_get<kF32>,
      &WasmGenerator::array_get<kF32>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kF64>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  if (recursion_limit_reached() || data->size() <= sizeof(double)) {
    builder_->EmitF64Const(data->getPseudoRandom<double>());
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::sequence<kF64, kVoid>,
      &WasmGenerator::sequence<kVoid, kF64>,
      &WasmGenerator::sequence<kVoid, kF64, kVoid>,

      &WasmGenerator::op<kExprF64Abs, kF64>,
      &WasmGenerator::op<kExprF64Neg, kF64>,
      &WasmGenerator::op<kExprF64Ceil, kF64>,
      &WasmGenerator::op<kExprF64Floor, kF64>,
      &WasmGenerator::op<kExprF64Trunc, kF64>,
      &WasmGenerator::op<kExprF64NearestInt, kF64>,
      &WasmGenerator::op<kExprF64Sqrt, kF64>,
      &WasmGenerator::op<kExprF64Add, kF64, kF64>,
      &WasmGenerator::op<kExprF64Sub, kF64, kF64>,
      &WasmGenerator::op<kExprF64Mul, kF64, kF64>,
      &WasmGenerator::op<kExprF64Div, kF64, kF64>,
      &WasmGenerator::op<kExprF64Min, kF64, kF64>,
      &WasmGenerator::op<kExprF64Max, kF64, kF64>,
      &WasmGenerator::op<kExprF64CopySign, kF64, kF64>,

      &WasmGenerator::op<kExprF64SConvertI32, kI32>,
      &WasmGenerator::op<kExprF64UConvertI32, kI32>,
      &WasmGenerator::op<kExprF64SConvertI64, kI64>,
      &WasmGenerator::op<kExprF64UConvertI64, kI64>,
      &WasmGenerator::op<kExprF64ConvertF32, kF32>,
      &WasmGenerator::op<kExprF64ReinterpretI64, kI64>,

      &WasmGenerator::block<kF64>,
      &WasmGenerator::loop<kF64>,
      &WasmGenerator::if_<kF64, kIfElse>,
      &WasmGenerator::br_if<kF64>,
      &WasmGenerator::br_on_null<kF64>,
      &WasmGenerator::br_on_non_null<kF64>,
      &WasmGenerator::br_table<kF64>,

      &WasmGenerator::memop<kExprF64LoadMem>,

      &WasmGenerator::simd_lane_op<kExprF64x2ExtractLane, 2, kS128>,

      &WasmGenerator::get_local<kF64>,
      &WasmGenerator::tee_local<kF64>,
      &WasmGenerator::get_global<kF64>,
      &WasmGenerator::op<kExprSelect, kF64, kF64, kI32>,
      &WasmGenerator::select_with_type<kF64>,

      &WasmGenerator::call<kF64>,
      &WasmGenerator::call_indirect<kF64>,
      &WasmGenerator::call_ref<kF64>,
      &WasmGenerator::try_block<kF64>,

      &WasmGenerator::struct_get<kF64>,
      &WasmGenerator::array_get<kF64>};

  GenerateOneOf(alternatives, data);
}

template <>
void WasmGenerator::Generate<kS128>(DataRange* data) {
  GeneratorRecursionScope rec_scope(this);
  has_simd_ = true;
  if (recursion_limit_reached() || data->size() <= sizeof(int32_t)) {
    // TODO(v8:8460): v128.const is not implemented yet, and we need a way to
    // "bottom-out", so use a splat to generate this.
    builder_->EmitI32Const(0);
    builder_->EmitWithPrefix(kExprI8x16Splat);
    return;
  }

  constexpr GenerateFn alternatives[] = {
      &WasmGenerator::simd_const,
      &WasmGenerator::simd_lane_op<kExprI8x16ReplaceLane, 16, kS128, kI32>,
      &WasmGenerator::simd_lane_op<kExprI16x8ReplaceLane, 8, kS128, kI32>,
      &WasmGenerator::simd_lane_op<kExprI32x4ReplaceLane, 4, kS128, kI32>,
      &WasmGenerator::simd_lane_op<kExprI64x2ReplaceLane, 2, kS128, kI64>,
      &WasmGenerator::simd_lane_op<kExprF32x4ReplaceLane, 4, kS128, kF32>,
      &WasmGenerator::simd_lane_op<kExprF64x2ReplaceLane, 2, kS128, kF64>,

      &WasmGenerator::op_with_prefix<kExprI8x16Splat, kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16LeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16GeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Shl, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16ShrS, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16ShrU, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI8x16Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16AddSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16AddSatU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16SubSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16SubSatU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MinS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MinU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MaxS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16MaxU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16RoundingAverageU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16Popcnt, kS128>,

      &WasmGenerator::op_with_prefix<kExprI16x8Splat, kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8LeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8GeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Shl, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8ShrS, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8ShrU, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI16x8Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8AddSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8AddSatU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SubSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SubSatU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MinS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MinU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MaxS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8MaxU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8RoundingAverageU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtMulLowI8x16S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtMulLowI8x16U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtMulHighI8x16S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtMulHighI8x16U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8Q15MulRSatS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtAddPairwiseI8x16S, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8ExtAddPairwiseI8x16U, kS128>,

      &WasmGenerator::op_with_prefix<kExprI32x4Splat, kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GtU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4LeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4GeU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Shl, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4ShrS, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4ShrU, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI32x4Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MinS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MinU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MaxS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4MaxU, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4DotI16x8S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtMulLowI16x8S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtMulLowI16x8U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtMulHighI16x8S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtMulHighI16x8U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtAddPairwiseI16x8S, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4ExtAddPairwiseI16x8U, kS128>,

      &WasmGenerator::op_with_prefix<kExprI64x2Splat, kI64>,
      &WasmGenerator::op_with_prefix<kExprI64x2Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2LtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2GtS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2LeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2GeS, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Shl, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI64x2ShrS, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI64x2ShrU, kS128, kI32>,
      &WasmGenerator::op_with_prefix<kExprI64x2Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2ExtMulLowI32x4S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2ExtMulLowI32x4U, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2ExtMulHighI32x4S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2ExtMulHighI32x4U, kS128, kS128>,

      &WasmGenerator::op_with_prefix<kExprF32x4Splat, kF32>,
      &WasmGenerator::op_with_prefix<kExprF32x4Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Lt, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Gt, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Le, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Ge, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Sqrt, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Div, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Min, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Max, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Pmin, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Pmax, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Ceil, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Floor, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Trunc, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4NearestInt, kS128>,

      &WasmGenerator::op_with_prefix<kExprF64x2Splat, kF64>,
      &WasmGenerator::op_with_prefix<kExprF64x2Eq, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Ne, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Lt, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Gt, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Le, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Ge, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Abs, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Neg, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Sqrt, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Add, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Sub, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Mul, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Div, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Min, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Max, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Pmin, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Pmax, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Ceil, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Floor, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Trunc, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2NearestInt, kS128>,

      &WasmGenerator::op_with_prefix<kExprF64x2PromoteLowF32x4, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2ConvertLowI32x4S, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2ConvertLowI32x4U, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4DemoteF64x2Zero, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4TruncSatF64x2SZero, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4TruncSatF64x2UZero, kS128>,

      &WasmGenerator::op_with_prefix<kExprI64x2SConvertI32x4Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2SConvertI32x4High, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2UConvertI32x4Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2UConvertI32x4High, kS128>,

      &WasmGenerator::op_with_prefix<kExprI32x4SConvertF32x4, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4UConvertF32x4, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4SConvertI32x4, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4UConvertI32x4, kS128>,

      &WasmGenerator::op_with_prefix<kExprI8x16SConvertI16x8, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16UConvertI16x8, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SConvertI32x4, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8UConvertI32x4, kS128, kS128>,

      &WasmGenerator::op_with_prefix<kExprI16x8SConvertI8x16Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8SConvertI8x16High, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8UConvertI8x16Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8UConvertI8x16High, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4SConvertI16x8Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4SConvertI16x8High, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4UConvertI16x8Low, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4UConvertI16x8High, kS128>,

      &WasmGenerator::op_with_prefix<kExprS128Not, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128And, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128AndNot, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128Or, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128Xor, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprS128Select, kS128, kS128, kS128>,

      &WasmGenerator::simd_shuffle,
      &WasmGenerator::op_with_prefix<kExprI8x16Swizzle, kS128, kS128>,

      &WasmGenerator::memop<kExprS128LoadMem>,
      &WasmGenerator::memop<kExprS128Load8x8S>,
      &WasmGenerator::memop<kExprS128Load8x8U>,
      &WasmGenerator::memop<kExprS128Load16x4S>,
      &WasmGenerator::memop<kExprS128Load16x4U>,
      &WasmGenerator::memop<kExprS128Load32x2S>,
      &WasmGenerator::memop<kExprS128Load32x2U>,
      &WasmGenerator::memop<kExprS128Load8Splat>,
      &WasmGenerator::memop<kExprS128Load16Splat>,
      &WasmGenerator::memop<kExprS128Load32Splat>,
      &WasmGenerator::memop<kExprS128Load64Splat>,
      &WasmGenerator::memop<kExprS128Load32Zero>,
      &WasmGenerator::memop<kExprS128Load64Zero>,
      &WasmGenerator::simd_lane_memop<kExprS128Load8Lane, 16, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Load16Lane, 8, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Load32Lane, 4, kS128>,
      &WasmGenerator::simd_lane_memop<kExprS128Load64Lane, 2, kS128>,

      &WasmGenerator::op_with_prefix<kExprI8x16RelaxedSwizzle, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI8x16RelaxedLaneSelect, kS128, kS128,
                                     kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8RelaxedLaneSelect, kS128, kS128,
                                     kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4RelaxedLaneSelect, kS128, kS128,
                                     kS128>,
      &WasmGenerator::op_with_prefix<kExprI64x2RelaxedLaneSelect, kS128, kS128,
                                     kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Qfma, kS128, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4Qfms, kS128, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Qfma, kS128, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2Qfms, kS128, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4RelaxedMin, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF32x4RelaxedMax, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2RelaxedMin, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprF64x2RelaxedMax, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4RelaxedTruncF32x4S, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4RelaxedTruncF32x4U, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4RelaxedTruncF64x2SZero, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4RelaxedTruncF64x2UZero, kS128>,
      &WasmGenerator::op_with_prefix<kExprI16x8DotI8x16I7x16S, kS128, kS128>,
      &WasmGenerator::op_with_prefix<kExprI32x4DotI8x16I7x16AddS, kS128, kS128,
                                     kS128>,
  };

  GenerateOneOf(alternatives, data);
}

void WasmGenerator::grow_memory(DataRange* data) {
  Generate<kI32>(data);
  builder_->EmitWithU8(kExprMemoryGrow, 0);
}

void WasmGenerator::Generate(ValueType type, DataRange* data) {
  switch (type.kind()) {
    case kVoid:
      return Generate<kVoid>(data);
    case kI32:
      return Generate<kI32>(data);
    case kI64:
      return Generate<kI64>(data);
    case kF32:
      return Generate<kF32>(data);
    case kF64:
      return Generate<kF64>(data);
    case kS128:
      return Generate<kS128>(data);
    case kRefNull:
      return GenerateRef(type.heap_type(), data, kNullable);
    case kRef:
      return GenerateRef(type.heap_type(), data, kNonNullable);
    default:
      UNREACHABLE();
  }
}

void WasmGenerator::GenerateRef(DataRange* data) {
  constexpr HeapType::Representation top_types[] = {
      HeapType::kAny,
      HeapType::kFunc,
      HeapType::kExtern,
  };
  HeapType::Representation type =
      top_types[data->get<uint8_t>() % arraysize(top_types)];
  GenerateRef(HeapType(type), data);
}

void WasmGenerator::GenerateRef(HeapType type, DataRange* data,
                                Nullability nullability) {
  base::Optional<GeneratorRecursionScope> rec_scope;
  if (nullability) {
    rec_scope.emplace(this);
  }

  if (recursion_limit_reached() || data->size() == 0) {
    if (nullability == kNullable) {
      ref_null(type, data);
      return;
    }
    // It is ok not to return here because the non-nullable types are not
    // recursive by construction, so the depth is limited already.
  }

  constexpr GenerateFnWithHeap alternatives_indexed_type[] = {
      &WasmGenerator::new_object,    &WasmGenerator::get_local_ref,
      &WasmGenerator::array_get_ref, &WasmGenerator::struct_get_ref,
      &WasmGenerator::ref_cast,      &WasmGenerator::ref_as_non_null,
      &WasmGenerator::br_on_cast};

  constexpr GenerateFnWithHeap alternatives_func_any[] = {
      &WasmGenerator::table_get,       &WasmGenerator::get_local_ref,
      &WasmGenerator::array_get_ref,   &WasmGenerator::struct_get_ref,
      &WasmGenerator::ref_cast,        &WasmGenerator::any_convert_extern,
      &WasmGenerator::ref_as_non_null, &WasmGenerator::br_on_cast};

  constexpr GenerateFnWithHeap alternatives_other[] = {
      &WasmGenerator::array_get_ref,   &WasmGenerator::get_local_ref,
      &WasmGenerator::struct_get_ref,  &WasmGenerator::ref_cast,
      &WasmGenerator::ref_as_non_null, &WasmGenerator::br_on_cast};

  switch (type.representation()) {
    // For abstract types, sometimes generate one of their subtypes.
    case HeapType::kAny: {
      // Weighed according to the types in the module:
      // If there are D data types and F function types, the relative
      // frequencies for dataref is D, for funcref F, and for i31ref and falling
      // back to anyref 2.
      const uint8_t num_data_types = num_structs_ + num_arrays_;
      const uint8_t emit_i31ref = 2;
      const uint8_t fallback_to_anyref = 2;
      uint8_t random = data->get<uint8_t>() %
                       (num_data_types + emit_i31ref + fallback_to_anyref);
      // We have to compute this first so in case GenerateOneOf fails
      // we will continue to fall back on an alternative that is guaranteed
      // to generate a value of the wanted type.
      // In order to know which alternative to fall back to in case
      // GenerateOneOf failed, the random variable is recomputed.
      if (random >= num_data_types + emit_i31ref) {
        if (GenerateOneOf(alternatives_func_any, type, data, nullability)) {
          return;
        }
        random = data->get<uint8_t>() % (num_data_types + emit_i31ref);
      }
      if (random < num_structs_) {
        GenerateRef(HeapType(HeapType::kStruct), data, nullability);
      } else if (random < num_data_types) {
        GenerateRef(HeapType(HeapType::kArray), data, nullability);
      } else {
        GenerateRef(HeapType(HeapType::kI31), data, nullability);
      }
      return;
    }
    case HeapType::kArray: {
      constexpr uint8_t fallback_to_dataref = 1;
      uint8_t random =
          data->get<uint8_t>() % (num_arrays_ + fallback_to_dataref);
      // Try generating one of the alternatives and continue to the rest of the
      // methods in case it fails.
      if (random >= num_arrays_) {
        if (GenerateOneOf(alternatives_other, type, data, nullability)) return;
        random = data->get<uint8_t>() % num_arrays_;
      }
      DCHECK(builder_->builder()->IsArrayType(random + num_structs_));
      GenerateRef(HeapType(random + num_structs_), data, nullability);
      return;
    }
    case HeapType::kStruct: {
      constexpr uint8_t fallback_to_dataref = 2;
      uint8_t random =
          data->get<uint8_t>() % (num_structs_ + fallback_to_dataref);
      // Try generating one of the alternatives
      // and continue to the rest of the methods in case it fails.
      if (random >= num_structs_) {
        if (GenerateOneOf(alternatives_other, type, data, nullability)) {
          return;
        }
        random = data->get<uint8_t>() % num_structs_;
      }
      DCHECK(builder_->builder()->IsStructType(random));
      GenerateRef(HeapType(random), data, nullability);
      return;
    }
    case HeapType::kEq: {
      const uint8_t num_types = num_arrays_ + num_structs_;
      const uint8_t emit_i31ref = 2;
      constexpr uint8_t fallback_to_eqref = 1;
      uint8_t random =
          data->get<uint8_t>() % (num_types + emit_i31ref + fallback_to_eqref);
      // Try generating one of the alternatives
      // and continue to the rest of the methods in case it fails.
      if (random >= num_types + emit_i31ref) {
        if (GenerateOneOf(alternatives_other, type, data, nullability)) {
          return;
        }
        random = data->get<uint8_t>() % (num_types + emit_i31ref);
      }
      if (random < num_types) {
        GenerateRef(HeapType(random), data, nullability);
      } else {
        GenerateRef(HeapType(HeapType::kI31), data, nullability);
      }
      return;
    }
    case HeapType::kFunc: {
      uint32_t random = data->get<uint8_t>() % (functions_.size() + 1);
      /// Try generating one of the alternatives
      // and continue to the rest of the methods in case it fails.
      if (random >= functions_.size()) {
        if (GenerateOneOf(alternatives_func_any, type, data, nullability)) {
          return;
        }
        random = data->get<uint8_t>() % functions_.size();
      }
      uint32_t signature_index = functions_[random];
      DCHECK(builder_->builder()->IsSignature(signature_index));
      GenerateRef(HeapType(signature_index), data, nullability);
      return;
    }
    case HeapType::kI31: {
      // Try generating one of the alternatives
      // and continue to the rest of the methods in case it fails.
      if (data->get<bool>() &&
          GenerateOneOf(alternatives_other, type, data, nullability)) {
        return;
      }
      Generate(kWasmI32, data);
      builder_->EmitWithPrefix(kExprRefI31);
      return;
    }
    case HeapType::kExtern:
      if (data->get<bool>()) {
        GenerateRef(HeapType(HeapType::kAny), data);
        builder_->EmitWithPrefix(kExprExternConvertAny);
        if (nullability == kNonNullable) {
          builder_->Emit(kExprRefAsNonNull);
        }
        return;
      }
      V8_FALLTHROUGH;
    case HeapType::kNoExtern:
    case HeapType::kNoFunc:
    case HeapType::kNone:
      ref_null(type, data);
      if (nullability == kNonNullable) {
        builder_->Emit(kExprRefAsNonNull);
      }
      return;
    default:
      // Indexed type.
      DCHECK(type.is_index());
      GenerateOneOf(alternatives_indexed_type, type, data, nullability);
      return;
  }
  UNREACHABLE();
}

std::vector<ValueType> WasmGenerator::GenerateTypes(DataRange* data) {
  std::vector<ValueType> types;
  int num_params = int{data->get<uint8_t>()} % (kMaxParameters + 1);
  for (int i = 0; i < num_params; ++i) {
    types.push_back(GetValueType(
        data,
        num_structs_ + num_arrays_ + static_cast<uint32_t>(functions_.size())));
  }
  return types;
}

void WasmGenerator::Generate(base::Vector<const ValueType> types,
                             DataRange* data) {
  // Maybe emit a multi-value block with the expected return type. Use a
  // non-default value to indicate block generation to avoid recursion when we
  // reach the end of the data.
  bool generate_block = data->get<uint8_t>() % 32 == 1;
  if (generate_block) {
    GeneratorRecursionScope rec_scope(this);
    if (!recursion_limit_reached()) {
      const auto param_types = GenerateTypes(data);
      Generate(base::VectorOf(param_types), data);
      any_block(base::VectorOf(param_types), types, data);
      return;
    }
  }

  if (types.size() == 0) {
    Generate(kWasmVoid, data);
    return;
  }
  if (types.size() == 1) {
    Generate(types[0], data);
    return;
  }

  // Split the types in two halves and recursively generate each half.
  // Each half is non empty to ensure termination.
  size_t split_index = data->get<uint8_t>() % (types.size() - 1) + 1;
  base::Vector<const ValueType> lower_half = types.SubVector(0, split_index);
  base::Vector<const ValueType> upper_half =
      types.SubVector(split_index, types.size());
  DataRange first_range = data->split();
  Generate(lower_half, &first_range);
  Generate(upper_half, data);
}

// Emit code to match an arbitrary signature.
// TODO(11954): Add the missing reference type conversion/upcasting.
void WasmGenerator::ConsumeAndGenerate(
    base::Vector<const ValueType> param_types,
    base::Vector<const ValueType> return_types, DataRange* data) {
  // This numeric conversion logic consists of picking exactly one
  // index in the return values and dropping all the values that come
  // before that index. Then we convert the value from that index to the
  // wanted type. If we don't find any value we generate it.
  auto primitive = [](ValueType t) -> bool {
    switch (t.kind()) {
      case kI32:
      case kI64:
      case kF32:
      case kF64:
        return true;
      default:
        return false;
    }
  };

  if (return_types.size() == 0 || param_types.size() == 0 ||
      !primitive(return_types[0])) {
    for (unsigned i = 0; i < param_types.size(); i++) {
      builder_->Emit(kExprDrop);
    }
    Generate(return_types, data);
    return;
  }

  int bottom_primitives = 0;

  while (static_cast<int>(param_types.size()) > bottom_primitives &&
         primitive(param_types[bottom_primitives])) {
    bottom_primitives++;
  }
  int return_index =
      bottom_primitives > 0 ? (data->get<uint8_t>() % bottom_primitives) : -1;
  for (int i = static_cast<int>(param_types.size() - 1); i > return_index;
       --i) {
    builder_->Emit(kExprDrop);
  }
  for (int i = return_index; i > 0; --i) {
    Convert(param_types[i], param_types[i - 1]);
    builder_->EmitI32Const(0);
    builder_->Emit(kExprSelect);
  }
  DCHECK(!return_types.empty());
  if (return_index >= 0) {
    Convert(param_types[0], return_types[0]);
    Generate(return_types + 1, data);
  } else {
    Generate(return_types, data);
  }
}

enum SigKind { kFunctionSig, kExceptionSig };

FunctionSig* GenerateSig(Zone* zone, DataRange* data, SigKind sig_kind,
                         int num_types) {
  // Generate enough parameters to spill some to the stack.
  int num_params = int{data->get<uint8_t>()} % (kMaxParameters + 1);
  int num_returns = sig_kind == kFunctionSig
                        ? int{data->get<uint8_t>()} % (kMaxReturns + 1)
                        : 0;

  FunctionSig::Builder builder(zone, num_returns, num_params);
  for (int i = 0; i < num_returns; ++i) {
    builder.AddReturn(GetValueType(data, num_types));
  }
  for (int i = 0; i < num_params; ++i) {
    builder.AddParam(GetValueType(data, num_types));
  }
  return builder.Build();
}

WasmInitExpr GenerateInitExpr(Zone* zone, DataRange& range,
                              WasmModuleBuilder* builder, ValueType type,
                              int num_structs, int num_arrays,
                              uint32_t recursion_depth);

WasmInitExpr GenerateStructNewInitExpr(Zone* zone, DataRange& range,
                                       WasmModuleBuilder* builder,
                                       uint32_t index, int num_structs,
                                       int num_arrays,
                                       uint32_t recursion_depth) {
  const StructType* struct_type = builder->GetStructType(index);
  bool use_new_default =
      std::all_of(struct_type->fields().begin(), struct_type->fields().end(),
                  [](ValueType type) { return type.is_defaultable(); }) &&
      range.get<bool>();

  if (use_new_default) {
    return WasmInitExpr::StructNewDefault(index);
  } else {
    ZoneVector<WasmInitExpr>* elements =
        zone->New<ZoneVector<WasmInitExpr>>(zone);
    int field_count = struct_type->field_count();
    for (int field_index = 0; field_index < field_count; field_index++) {
      elements->push_back(GenerateInitExpr(
          zone, range, builder, struct_type->field(field_index), num_structs,
          num_arrays, recursion_depth + 1));
    }
    return WasmInitExpr::StructNew(index, elements);
  }
}

WasmInitExpr GenerateArrayInitExpr(Zone* zone, DataRange& range,
                                   WasmModuleBuilder* builder, uint32_t index,
                                   int num_structs, int num_arrays,
                                   uint32_t recursion_depth) {
  constexpr int kMaxArrayLength = 20;
  uint8_t choice = range.get<uint8_t>() % 3;
  ValueType element_type = builder->GetArrayType(index)->element_type();
  if (choice == 0) {
    int element_count = range.get<uint8_t>() % kMaxArrayLength;
    ZoneVector<WasmInitExpr>* elements =
        zone->New<ZoneVector<WasmInitExpr>>(zone);
    for (int i = 0; i < element_count; i++) {
      elements->push_back(GenerateInitExpr(zone, range, builder, element_type,
                                           num_structs, num_arrays,
                                           recursion_depth + 1));
    }
    return WasmInitExpr::ArrayNewFixed(index, elements);
  } else if (choice == 1 || !element_type.is_defaultable()) {
    // TODO(14034): Add other int expressions to length (same below).
    WasmInitExpr length = WasmInitExpr(range.get<uint8_t>() % kMaxArrayLength);
    WasmInitExpr init =
        GenerateInitExpr(zone, range, builder, element_type, num_structs,
                         num_arrays, recursion_depth + 1);
    return WasmInitExpr::ArrayNew(zone, index, init, length);
  } else {
    WasmInitExpr length = WasmInitExpr(range.get<uint8_t>() % kMaxArrayLength);
    return WasmInitExpr::ArrayNewDefault(zone, index, length);
  }
}

// TODO(manoskouk): Add global.get.
WasmInitExpr GenerateInitExpr(Zone* zone, DataRange& range,
                              WasmModuleBuilder* builder, ValueType type,
                              int num_structs, int num_arrays,
                              uint32_t recursion_depth) {
  switch (type.kind()) {
    case kI8:
    case kI16:
    case kI32: {
      if (range.size() == 0 || recursion_depth >= kMaxRecursionDepth) {
        return WasmInitExpr(int32_t{0});
      }
      // 50% to generate a constant, 50% to generate a binary operator.
      uint8_t choice = range.get<uint8_t>() % 6;
      switch (choice) {
        case 0:
        case 1:
        case 2:
          return WasmInitExpr(range.get<int32_t>());
        default:
          WasmInitExpr::Operator op = choice == 3   ? WasmInitExpr::kI32Add
                                      : choice == 4 ? WasmInitExpr::kI32Sub
                                                    : WasmInitExpr::kI32Mul;
          return WasmInitExpr::Binop(
              zone, op,
              GenerateInitExpr(zone, range, builder, kWasmI32, num_structs,
                               num_arrays, recursion_depth + 1),
              GenerateInitExpr(zone, range, builder, kWasmI32, num_structs,
                               num_arrays, recursion_depth + 1));
      }
    }
    case kI64: {
      if (range.size() == 0 || recursion_depth >= kMaxRecursionDepth) {
        return WasmInitExpr(int64_t{0});
      }
      // 50% to generate a constant, 50% to generate a binary operator.
      uint8_t choice = range.get<uint8_t>() % 6;
      switch (choice) {
        case 0:
        case 1:
        case 2:
          return WasmInitExpr(range.get<int64_t>());
        default:
          WasmInitExpr::Operator op = choice == 3   ? WasmInitExpr::kI64Add
                                      : choice == 4 ? WasmInitExpr::kI64Sub
                                                    : WasmInitExpr::kI64Mul;
          return WasmInitExpr::Binop(
              zone, op,
              GenerateInitExpr(zone, range, builder, kWasmI64, num_structs,
                               num_arrays, recursion_depth + 1),
              GenerateInitExpr(zone, range, builder, kWasmI64, num_structs,
                               num_arrays, recursion_depth + 1));
      }
    }
    case kF32:
      return WasmInitExpr(0.0f);
    case kF64:
      return WasmInitExpr(0.0);
    case kS128: {
      uint8_t s128_const[kSimd128Size] = {0};
      return WasmInitExpr(s128_const);
    }
    case kRefNull: {
      bool null_only = false;
      switch (type.heap_representation()) {
        case HeapType::kNone:
        case HeapType::kNoFunc:
        case HeapType::kNoExtern:
          null_only = true;
          break;
        default:
          break;
      }
      if (range.size() == 0 || recursion_depth >= kMaxRecursionDepth ||
          null_only || (range.get<uint8_t>() % 4 == 0)) {
        return WasmInitExpr::RefNullConst(type.heap_type().representation());
      }
      V8_FALLTHROUGH;
    }
    case kRef: {
      switch (type.heap_representation()) {
        case HeapType::kStruct: {
          uint8_t index = range.get<uint8_t>() % num_structs;
          return GenerateStructNewInitExpr(zone, range, builder, index,
                                           num_structs, num_arrays,
                                           recursion_depth);
        }
        case HeapType::kAny: {
          // Do not use 0 as the determining value here, otherwise an exhausted
          // {range} will generate an infinite recursion with the {kExtern}
          // case.
          if (recursion_depth < kMaxRecursionDepth && range.size() > 0 &&
              range.get<uint8_t>() % 4 == 3) {
            return WasmInitExpr::AnyConvertExtern(
                zone,
                GenerateInitExpr(zone, range, builder,
                                 ValueType::RefMaybeNull(HeapType::kExtern,
                                                         type.nullability()),
                                 num_structs, num_arrays, recursion_depth + 1));
          }
          V8_FALLTHROUGH;
        }
        case HeapType::kEq: {
          uint8_t choice = range.get<uint8_t>() % 3;
          HeapType::Representation subtype = choice == 0   ? HeapType::kStruct
                                             : choice == 1 ? HeapType::kArray
                                                           : HeapType::kI31;

          return GenerateInitExpr(
              zone, range, builder,
              ValueType::RefMaybeNull(subtype, type.nullability()), num_structs,
              num_arrays, recursion_depth);
        }
        case HeapType::kFunc: {
          uint32_t index = range.get<uint32_t>() % builder->NumFunctions();
          return WasmInitExpr::RefFuncConst(index);
        }
        case HeapType::kExtern:
          return WasmInitExpr::ExternConvertAny(
              zone,
              GenerateInitExpr(
                  zone, range, builder,
                  ValueType::RefMaybeNull(HeapType::kAny, type.nullability()),
                  num_structs, num_arrays, recursion_depth + 1));
        case HeapType::kI31:
          return WasmInitExpr::RefI31(
              zone,
              GenerateInitExpr(zone, range, builder, kWasmI32, num_structs,
                               num_arrays, recursion_depth + 1));
        case HeapType::kArray: {
          uint32_t index = range.get<uint32_t>() % num_arrays + num_structs;
          return GenerateArrayInitExpr(zone, range, builder, index, num_structs,
                                       num_arrays, recursion_depth);
        }
        case HeapType::kNone:
        case HeapType::kNoFunc:
        case HeapType::kNoExtern:
          UNREACHABLE();
        default: {
          uint32_t index = type.ref_index();
          if (builder->IsStructType(index)) {
            return GenerateStructNewInitExpr(zone, range, builder, index,
                                             num_structs, num_arrays,
                                             recursion_depth);
          } else if (builder->IsArrayType(index)) {
            return GenerateArrayInitExpr(zone, range, builder, index,
                                         num_structs, num_arrays,
                                         recursion_depth);
          } else {
            DCHECK(builder->IsSignature(index));
            // Transform from signature index to function index.
            return WasmInitExpr::RefFuncConst(index -
                                              (num_structs + num_arrays));
          }
          UNREACHABLE();
        }
      }
    }
    case kVoid:
    case kRtt:
    case kBottom:
      UNREACHABLE();
  }
}
}  // namespace

class WasmCompileFuzzer : public WasmExecutionFuzzer {
  bool GenerateModule(Isolate* isolate, Zone* zone,
                      base::Vector<const uint8_t> data,
                      ZoneBuffer* buffer) override {
    TestSignatures sigs;

    WasmModuleBuilder builder(zone);

    // Split input data in two parts:
    // - One for the "module" (types, globals, ..)
    // - One for all the function bodies
    // This prevents using a too large portion on the module resulting in
    // uninteresting function bodies.
    DataRange module_range(data);
    DataRange functions_range = module_range.split();
    std::vector<uint32_t> function_signatures;

    // Add struct and array types first so that we get a chance to generate
    // these types in function signatures.
    // Currently, WasmGenerator assumes this order for struct/array/signature
    // definitions.

    static_assert(kMaxFunctions >= 1, "need min. 1 function");
    uint8_t num_functions = 1 + (module_range.get<uint8_t>() % kMaxFunctions);

    // We need at least one struct and one array in order to support
    // WasmInitExpr for abstract types.
    uint8_t num_structs = 1 + module_range.get<uint8_t>() % kMaxStructs;
    uint8_t num_arrays = 1 + module_range.get<uint8_t>() % kMaxArrays;
    uint16_t num_types = num_functions + num_structs + num_arrays;

    // (Type_index -> end of explicit rec group).
    std::map<uint8_t, uint8_t> explicit_rec_groups;
    {
      uint8_t current_type_index = 0;
      while (current_type_index < num_types) {
        // First, pick a random start for the next group. We allow it to be
        // beyond the end of types (i.e., we add no further recursive groups).
        uint8_t group_start =
            module_range.get<uint8_t>() % (num_types - current_type_index + 1) +
            current_type_index;
        DCHECK_GE(group_start, current_type_index);
        current_type_index = group_start;
        if (group_start < num_types) {
          // If we did not reach the end of the types, pick a random group size.
          uint8_t group_size =
              module_range.get<uint8_t>() % (num_types - group_start) + 1;
          DCHECK_LE(group_start + group_size, num_types);
          for (uint8_t i = group_start; i < group_start + group_size; i++) {
            explicit_rec_groups.emplace(i, group_start + group_size - 1);
          }
          builder.AddRecursiveTypeGroup(group_start, group_size);
          current_type_index += group_size;
        }
      }
    }

    uint8_t current_type_index = 0;
    for (; current_type_index < num_structs; current_type_index++) {
      auto rec_group = explicit_rec_groups.find(current_type_index);
      uint8_t current_rec_group_end = rec_group != explicit_rec_groups.end()
                                          ? rec_group->second
                                          : current_type_index;

      uint32_t supertype = kNoSuperType;
      uint8_t num_fields = module_range.get<uint8_t>() % (kMaxStructFields + 1);

      if (current_type_index > 0 && module_range.get<bool>()) {
        supertype = module_range.get<uint8_t>() % current_type_index;
        num_fields += builder.GetStructType(supertype)->field_count();
      }
      StructType::Builder struct_builder(zone, num_fields);

      // Add all fields from super type.
      uint32_t field_index = 0;
      if (supertype != kNoSuperType) {
        const StructType* parent = builder.GetStructType(supertype);
        for (; field_index < parent->field_count(); ++field_index) {
          // TODO(14034): This could also be any sub type of the supertype's
          // element type.
          struct_builder.AddField(parent->field(field_index),
                                  parent->mutability(field_index));
        }
      }
      for (; field_index < num_fields; field_index++) {
        // Notes:
        // - We allow a type to only have non-nullable fields of types that
        //   are defined earlier. This way we avoid infinite non-nullable
        //   constructions. Also relevant for arrays and functions.
        // - On the other hand, nullable fields can be picked up to the end of
        //   the current recursive group.
        // - We exclude the non-nullable generic types arrayref, anyref,
        //   structref, eqref and externref from the fields of structs and
        //   arrays. This is so that GenerateInitExpr has a way to break a
        //   recursion between a struct/array field and those types
        //   ((ref extern) gets materialized through (ref any)).
        ValueType type = GetValueTypeHelper(
            &module_range, current_rec_group_end + 1, current_type_index,
            kIncludeNumericTypes, kIncludePackedTypes,
            kExcludeSomeGenericsWhenTypeIsNonNullable);

        bool mutability = module_range.get<bool>();
        struct_builder.AddField(type, mutability);
      }
      StructType* struct_fuz = struct_builder.Build();
      builder.AddStructType(struct_fuz, false, supertype);
    }

    for (; current_type_index < num_structs + num_arrays;
         current_type_index++) {
      auto rec_group = explicit_rec_groups.find(current_type_index);
      uint8_t current_rec_group_end = rec_group != explicit_rec_groups.end()
                                          ? rec_group->second
                                          : current_type_index;
      ValueType type = GetValueTypeHelper(
          &module_range, current_rec_group_end + 1, current_type_index,
          kIncludeNumericTypes, kIncludePackedTypes,
          kExcludeSomeGenericsWhenTypeIsNonNullable);
      uint32_t supertype = kNoSuperType;
      if (current_type_index > num_structs && module_range.get<bool>()) {
        supertype =
            module_range.get<uint8_t>() % (current_type_index - num_structs) +
            num_structs;
        // TODO(14034): This could also be any sub type of the supertype's
        // element type.
        type = builder.GetArrayType(supertype)->element_type();
      }
      ArrayType* array_fuz = zone->New<ArrayType>(type, true);
      builder.AddArrayType(array_fuz, false, supertype);
    }

    // We keep the signature for the first (main) function constant.
    function_signatures.push_back(
        builder.ForceAddSignature(sigs.i_iii(), v8_flags.wasm_final_types));
    current_type_index++;

    for (; current_type_index < num_types; current_type_index++) {
      auto rec_group = explicit_rec_groups.find(current_type_index);
      uint8_t current_rec_group_end = rec_group != explicit_rec_groups.end()
                                          ? rec_group->second
                                          : current_type_index;
      FunctionSig* sig = GenerateSig(zone, &module_range, kFunctionSig,
                                     current_rec_group_end + 1);
      uint32_t signature_index =
          builder.ForceAddSignature(sig, v8_flags.wasm_final_types);
      function_signatures.push_back(signature_index);
    }

    int num_exceptions = 1 + (module_range.get<uint8_t>() % kMaxExceptions);
    for (int i = 0; i < num_exceptions; ++i) {
      FunctionSig* sig =
          GenerateSig(zone, &module_range, kExceptionSig, num_types);
      builder.AddException(sig);
    }

    // Generate function declarations before tables. This will be needed once we
    // have typed-function tables.
    std::vector<WasmFunctionBuilder*> functions;
    for (uint8_t i = 0; i < num_functions; i++) {
      // If we are using wasm-gc, we cannot allow signature normalization
      // performed by adding a function by {FunctionSig}, because we emit
      // everything in one recursive group which blocks signature
      // canonicalization.
      // TODO(14034): Relax this when we implement proper recursive-group
      // support.
      functions.push_back(builder.AddFunction(function_signatures[i]));
    }

    int num_globals = module_range.get<uint8_t>() % (kMaxGlobals + 1);
    std::vector<ValueType> globals;
    std::vector<uint8_t> mutable_globals;
    globals.reserve(num_globals);
    mutable_globals.reserve(num_globals);

    for (int i = 0; i < num_globals; ++i) {
      ValueType type = GetValueType(&module_range, num_types);
      // 1/8 of globals are immutable.
      const bool mutability = (module_range.get<uint8_t>() % 8) != 0;

      builder.AddGlobal(type, mutability,
                        GenerateInitExpr(zone, module_range, &builder, type,
                                         num_structs, num_arrays, 0));
      globals.push_back(type);
      if (mutability) mutable_globals.push_back(static_cast<uint8_t>(i));
    }

    // Generate tables before function bodies, so they are available for table
    // operations.
    // Always generate at least one table for call_indirect.
    int num_tables = module_range.get<uint8_t>() % kMaxTables + 1;
    for (int i = 0; i < num_tables; i++) {
      uint32_t min_size =
          i == 0 ? num_functions : module_range.get<uint8_t>() % kMaxTableSize;
      uint32_t max_size =
          module_range.get<uint8_t>() % (kMaxTableSize - min_size) + min_size;
      // Table 0 is always funcref. This guarantees that
      // - call_indirect has at least one funcref table to work with,
      // - we have a place to reference all functions in the program, so they
      //   count as "declared" for ref.func.
      bool force_funcref = i == 0;
      ValueType type =
          force_funcref
              ? kWasmFuncRef
              : GetValueTypeHelper(&module_range, num_types, num_types,
                                   kExcludeNumericTypes, kExcludePackedTypes,
                                   kAlwaysIncludeAllGenerics);
      bool use_initializer = !type.is_defaultable() || module_range.get<bool>();
      uint32_t table_index =
          use_initializer
              ? builder.AddTable(
                    type, min_size, max_size,
                    GenerateInitExpr(zone, module_range, &builder, type,
                                     num_structs, num_arrays, 0))
              : builder.AddTable(type, min_size, max_size);
      if (type.is_reference_to(HeapType::kFunc)) {
        // For function tables, initialize them with functions from the program.
        // Currently, the fuzzer assumes that every funcref/(ref func) table
        // contains the functions in the program in the order they are defined.
        // TODO(11954): Consider generalizing this.
        WasmModuleBuilder::WasmElemSegment segment(zone, type, table_index,
                                                   WasmInitExpr(0));
        for (int entry_index = 0; entry_index < static_cast<int>(min_size);
             entry_index++) {
          segment.entries.emplace_back(
              WasmModuleBuilder::WasmElemSegment::Entry::kRefFuncEntry,
              entry_index % num_functions);
        }
        builder.AddElementSegment(std::move(segment));
      }
    }

    int num_data_segments =
        module_range.get<uint8_t>() % kMaxPassiveDataSegments;
    for (int i = 0; i < num_data_segments; i++) {
      GeneratePassiveDataSegment(&module_range, &builder);
    }

    for (int i = 0; i < num_functions; ++i) {
      WasmFunctionBuilder* f = functions[i];
      // On the last function don't split the DataRange but just use the
      // existing DataRange.
      DataRange function_range = i != num_functions - 1
                                     ? functions_range.split()
                                     : std::move(functions_range);
      WasmGenerator gen(f, function_signatures, globals, mutable_globals,
                        num_structs, num_arrays, &function_range);
      const FunctionSig* sig = f->signature();
      base::Vector<const ValueType> return_types(sig->returns().begin(),
                                                 sig->return_count());
      gen.InitializeNonDefaultableLocals(&function_range);
      gen.Generate(return_types, &function_range);
      if (!CheckHardwareSupportsSimd() && gen.HasSimd()) return false;
      f->Emit(kExprEnd);
      if (i == 0) builder.AddExport(base::CStrVector("main"), f);
    }

    builder.SetMaxMemorySize(32);
    builder.WriteTo(buffer);
    return true;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr bool require_valid = true;
  EXPERIMENTAL_FLAG_SCOPE(relaxed_simd);
  WasmCompileFuzzer().FuzzWasmModule({data, size}, require_valid);
  return 0;
}

}  // namespace v8::internal::wasm::fuzzer
