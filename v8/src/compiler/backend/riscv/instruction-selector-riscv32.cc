// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/riscv/instruction-selector-riscv.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

template <typename Adapter>
bool RiscvOperandGeneratorT<Adapter>::IsIntegerConstant(Node* node) {
  return (node->opcode() == IrOpcode::kInt32Constant);
}

template <typename Adapter>
int64_t RiscvOperandGeneratorT<Adapter>::GetIntegerConstantValue(Node* node) {
  DCHECK_EQ(IrOpcode::kInt32Constant, node->opcode());
  return OpParameter<int32_t>(node->op());
}

template <typename Adapter>
bool RiscvOperandGeneratorT<Adapter>::CanBeImmediate(int64_t value,
                                                     InstructionCode opcode) {
  switch (ArchOpcodeField::decode(opcode)) {
    case kRiscvShl32:
    case kRiscvSar32:
    case kRiscvShr32:
      return is_uint5(value);
    case kRiscvAdd32:
    case kRiscvAnd32:
    case kRiscvAnd:
    case kRiscvOr32:
    case kRiscvOr:
    case kRiscvTst32:
    case kRiscvXor:
      return is_int12(value);
    case kRiscvLb:
    case kRiscvLbu:
    case kRiscvSb:
    case kRiscvLh:
    case kRiscvLhu:
    case kRiscvSh:
    case kRiscvLw:
    case kRiscvSw:
    case kRiscvLoadFloat:
    case kRiscvStoreFloat:
    case kRiscvLoadDouble:
    case kRiscvStoreDouble:
      return is_int32(value);
    default:
      return is_int12(value);
  }
}

template <typename Adapter>
void EmitLoad(InstructionSelectorT<Adapter>* selector, Node* node,
              InstructionCode opcode, Node* output = nullptr) {
  RiscvOperandGeneratorT<Adapter> g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  ExternalReferenceMatcher m(base);
  if (m.HasResolvedValue() && g.IsIntegerConstant(index) &&
      selector->CanAddressRelativeToRootsRegister(m.ResolvedValue())) {
    ptrdiff_t const delta =
        g.GetIntegerConstantValue(index) +
        MacroAssemblerBase::RootRegisterOffsetForExternalReference(
            selector->isolate(), m.ResolvedValue());
    // Check that the delta is a 32-bit integer due to the limitations of
    // immediate operands.
    if (is_int32(delta)) {
      opcode |= AddressingModeField::encode(kMode_Root);
      selector->Emit(opcode,
                     g.DefineAsRegister(output == nullptr ? node : output),
                     g.UseImmediate(static_cast<int32_t>(delta)));
      return;
    }
  }

  if (base != nullptr && base->opcode() == IrOpcode::kLoadRootRegister) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_Root),
                   g.DefineAsRegister(output == nullptr ? node : output),
                   g.UseImmediate(index));
    return;
  }

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output == nullptr ? node : output),
                   g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output == nullptr ? node : output),
                   addr_reg, g.TempImmediate(0));
  }
}

template <typename Adapter>
void EmitS128Load(InstructionSelectorT<Adapter>* selector, Node* node,
                  InstructionCode opcode, VSew sew, Vlmul lmul) {
  RiscvOperandGeneratorT<Adapter> g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index), g.UseImmediate(sew),
                   g.UseImmediate(lmul));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(node), addr_reg, g.TempImmediate(0),
                   g.UseImmediate(sew), g.UseImmediate(lmul));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitStoreLane(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    StoreLaneParameters params = StoreLaneParametersOf(node->op());
    LoadStoreLaneParams f(params.rep, params.laneidx);
    InstructionCode opcode = kRiscvS128StoreLane;
    opcode |= MiscField::encode(f.sz);

    RiscvOperandGeneratorT<Adapter> g(this);
    Node* base = node->InputAt(0);
    Node* index = node->InputAt(1);
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kRiscvAdd32, addr_reg, g.UseRegister(base), g.UseRegister(index));
    InstructionOperand inputs[4] = {
        g.UseRegister(node->InputAt(2)),
        g.UseImmediate(f.laneidx),
        addr_reg,
        g.TempImmediate(0),
    };
    opcode |= AddressingModeField::encode(kMode_MRI);
    Emit(opcode, 0, nullptr, 4, inputs);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitLoadLane(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    LoadLaneParameters params = LoadLaneParametersOf(node->op());
    LoadStoreLaneParams f(params.rep.representation(), params.laneidx);
    InstructionCode opcode = kRiscvS128LoadLane;
    opcode |= MiscField::encode(f.sz);

    RiscvOperandGeneratorT<Adapter> g(this);
    Node* base = node->InputAt(0);
    Node* index = node->InputAt(1);
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kRiscvAdd32, addr_reg, g.UseRegister(base), g.UseRegister(index));
    opcode |= AddressingModeField::encode(kMode_MRI);
    Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(2)),
         g.UseImmediate(params.laneidx), addr_reg, g.TempImmediate(0));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitLoad(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    LoadRepresentation load_rep = LoadRepresentationOf(node->op());

    InstructionCode opcode = kArchNop;
    switch (load_rep.representation()) {
      case MachineRepresentation::kFloat32:
        opcode = kRiscvLoadFloat;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kRiscvLoadDouble;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = load_rep.IsUnsigned() ? kRiscvLbu : kRiscvLb;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsUnsigned() ? kRiscvLhu : kRiscvLh;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:         // Fall through.
      case MachineRepresentation::kWord32:
        opcode = kRiscvLw;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kRiscvRvvLd;
        break;
      case MachineRepresentation::kCompressedPointer:
      case MachineRepresentation::kCompressed:
      case MachineRepresentation::kSandboxedPointer:
      case MachineRepresentation::kMapWord:  // Fall through.
      case MachineRepresentation::kWord64:
      case MachineRepresentation::kNone:
      case MachineRepresentation::kSimd256:  // Fall through.
      case MachineRepresentation::kIndirectPointer:
        UNREACHABLE();
    }

    EmitLoad(this, node, opcode);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitStorePair(node_t node) {
  UNREACHABLE();
}

template <>
void InstructionSelectorT<TurboshaftAdapter>::VisitStore(turboshaft::OpIndex) {
  UNREACHABLE();
}

template <>
void InstructionSelectorT<TurbofanAdapter>::VisitStore(Node* node) {
  RiscvOperandGeneratorT<TurbofanAdapter> g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  // TODO(riscv): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier &&
      V8_LIKELY(!v8_flags.disable_write_barriers)) {
    DCHECK(CanBeTaggedPointer(rep));
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    inputs[input_count++] = g.UseUniqueRegister(index);
    inputs[input_count++] = g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= RecordWriteModeField::encode(record_write_mode);
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    ArchOpcode opcode;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kRiscvStoreFloat;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kRiscvStoreDouble;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kRiscvSb;
        break;
      case MachineRepresentation::kWord16:
        opcode = kRiscvSh;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kWord32:
        opcode = kRiscvSw;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kRiscvRvvSt;
        break;
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:
        UNREACHABLE();
      case MachineRepresentation::kSandboxedPointer:
      case MachineRepresentation::kMapWord:  // Fall through.
      case MachineRepresentation::kNone:
      case MachineRepresentation::kWord64:
      case MachineRepresentation::kSimd256:  // Fall through.
      case MachineRepresentation::kIndirectPointer:
        UNREACHABLE();
    }

    if (base != nullptr && base->opcode() == IrOpcode::kLoadRootRegister) {
      Emit(opcode | AddressingModeField::encode(kMode_Root), g.NoOutput(),
           g.UseRegisterOrImmediateZero(value), g.UseImmediate(index));
      return;
    }

    if (g.CanBeImmediate(index, opcode)) {
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegisterOrImmediateZero(value), g.UseRegister(base),
           g.UseImmediate(index));
    } else {
      InstructionOperand addr_reg = g.TempRegister();
      Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None), addr_reg,
           g.UseRegister(index), g.UseRegister(base));
      // Emit desired store opcode, using temp addr_reg.
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegisterOrImmediateZero(value), addr_reg, g.TempImmediate(0));
    }
  }
}

template <>
void InstructionSelectorT<TurboshaftAdapter>::VisitWord32And(
    turboshaft::OpIndex) {
  UNIMPLEMENTED();
}

template <>
void InstructionSelectorT<TurbofanAdapter>::VisitWord32And(Node* node) {
  VisitBinop<TurbofanAdapter, Int32BinopMatcher>(this, node, kRiscvAnd, true,
                                                 kRiscvAnd);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32Or(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitBinop<Adapter, Int32BinopMatcher>(this, node, kRiscvOr, true,
                                           kRiscvOr);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32Xor(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    Int32BinopMatcher m(node);
    if (m.left().IsWord32Or() && CanCover(node, m.left().node()) &&
        m.right().Is(-1)) {
      Int32BinopMatcher mleft(m.left().node());
      if (!mleft.right().HasResolvedValue()) {
        RiscvOperandGeneratorT<Adapter> g(this);
        Emit(kRiscvNor, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseRegister(mleft.right().node()));
        return;
      }
    }
    if (m.right().Is(-1)) {
      // Use Nor for bit negation and eliminate constant loading for xori.
      RiscvOperandGeneratorT<Adapter> g(this);
      Emit(kRiscvNor, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(0));
      return;
    }
    VisitBinop<Adapter, Int32BinopMatcher>(this, node, kRiscvXor, true,
                                           kRiscvXor);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32Rol(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    UNREACHABLE();
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32Ror(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitRRO(this, kRiscvRor32, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32ReverseBits(node_t node) {
  UNREACHABLE();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64ReverseBytes(node_t node) {
  UNREACHABLE();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32ReverseBytes(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    Emit(kRiscvByteSwap32, g.DefineAsRegister(node),
         g.UseRegister(node->InputAt(0)));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitSimd128ReverseBytes(Node* node) {
  UNREACHABLE();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32Popcnt(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    Emit(kRiscvPopcnt32, g.DefineAsRegister(node),
         g.UseRegister(node->InputAt(0)));
  }
}

template <>
void InstructionSelectorT<TurboshaftAdapter>::VisitInt32Add(node_t node) {
  UNIMPLEMENTED();
}

template <>
void InstructionSelectorT<TurbofanAdapter>::VisitInt32Add(Node* node) {
  VisitBinop<TurbofanAdapter, Int32BinopMatcher>(this, node, kRiscvAdd32, true,
                                         kRiscvAdd32);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32Sub(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  VisitBinop<Adapter, Int32BinopMatcher>(this, node, kRiscvSub32);
  }
}

template <>
void InstructionSelectorT<TurboshaftAdapter>::VisitInt32Mul(node_t node) {
  UNIMPLEMENTED();
}

template <>
void InstructionSelectorT<TurbofanAdapter>::VisitInt32Mul(Node* node) {
  RiscvOperandGeneratorT<TurbofanAdapter> g(this);
  Int32BinopMatcher m(node);
  if (m.right().HasResolvedValue() && m.right().ResolvedValue() > 0) {
    uint32_t value = static_cast<uint32_t>(m.right().ResolvedValue());
    if (base::bits::IsPowerOfTwo(value)) {
      Emit(kRiscvShl32 | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(base::bits::WhichPowerOfTwo(value)));
      return;
    }
    if (base::bits::IsPowerOfTwo(value + 1)) {
      InstructionOperand temp = g.TempRegister();
      Emit(kRiscvShl32 | AddressingModeField::encode(kMode_None), temp,
           g.UseRegister(m.left().node()),
           g.TempImmediate(base::bits::WhichPowerOfTwo(value + 1)));
      Emit(kRiscvSub32 | AddressingModeField::encode(kMode_None),
           g.DefineAsRegister(node), temp, g.UseRegister(m.left().node()));
      return;
    }
  }

  VisitRRR(this, kRiscvMul32, node);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32MulHigh(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  VisitRRR(this, kRiscvMulHigh32, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint32MulHigh(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  VisitRRR(this, kRiscvMulHighU32, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32Div(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  RiscvOperandGeneratorT<Adapter> g(this);
  Int32BinopMatcher m(node);
  Emit(kRiscvDiv32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint32Div(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  RiscvOperandGeneratorT<Adapter> g(this);
  Int32BinopMatcher m(node);
  Emit(kRiscvDivU32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32Mod(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  RiscvOperandGeneratorT<Adapter> g(this);
  Int32BinopMatcher m(node);
  Emit(kRiscvMod32, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint32Mod(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  RiscvOperandGeneratorT<Adapter> g(this);
  Int32BinopMatcher m(node);
  Emit(kRiscvModU32, g.DefineAsRegister(node), g.UseRegister(m.left().node()),
       g.UseRegister(m.right().node()));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeFloat32ToFloat64(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitRR(this, kRiscvCvtDS, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitRoundInt32ToFloat32(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitRR(this, kRiscvCvtSW, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitRoundUint32ToFloat32(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitRR(this, kRiscvCvtSUw, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeInt32ToFloat64(node_t node) {
  VisitRR(this, kRiscvCvtDW, node);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeUint32ToFloat64(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitRR(this, kRiscvCvtDUw, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTruncateFloat32ToInt32(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    InstructionCode opcode = kRiscvTruncWS;
    TruncateKind kind = OpParameter<TruncateKind>(node->op());
    if (kind == TruncateKind::kSetOverflowToMin) {
      opcode |= MiscField::encode(true);
    }
    Emit(opcode, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTruncateFloat32ToUint32(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    InstructionCode opcode = kRiscvTruncUwS;
    TruncateKind kind = OpParameter<TruncateKind>(node->op());
    if (kind == TruncateKind::kSetOverflowToMin) {
      opcode |= MiscField::encode(true);
    }
    Emit(opcode, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeFloat64ToInt32(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    Node* value = node->InputAt(0);
    if (CanCover(node, value)) {
      if (value->opcode() == IrOpcode::kChangeFloat32ToFloat64) {
        // Match float32 -> float64 -> int32 representation change path.
        Emit(kRiscvTruncWS, g.DefineAsRegister(node),
             g.UseRegister(value->InputAt(0)));
        return;
      }
    }
    VisitRR(this, kRiscvTruncWD, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeFloat64ToUint32(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitRR(this, kRiscvTruncUwD, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTruncateFloat64ToUint32(node_t node) {
  VisitRR(this, kRiscvTruncUwD, node);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBitcastFloat32ToInt32(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitRR(this, kRiscvBitcastFloat32ToInt32, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBitcastInt32ToFloat32(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitRR(this, kRiscvBitcastInt32ToFloat32, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64RoundDown(node_t node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat32RoundUp(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  VisitRR(this, kRiscvFloat32RoundUp, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64RoundUp(node_t node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat32RoundTruncate(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  VisitRR(this, kRiscvFloat32RoundTruncate, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64RoundTruncate(node_t node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64RoundTiesAway(node_t node) {
  UNREACHABLE();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat32RoundTiesEven(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  VisitRR(this, kRiscvFloat32RoundTiesEven, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64RoundTiesEven(node_t node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat32Neg(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  VisitRR(this, kRiscvNegS, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Neg(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  VisitRR(this, kRiscvNegD, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Ieee754Binop(
    node_t node, InstructionCode opcode) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    Emit(opcode, g.DefineAsFixed(node, fa0), g.UseFixed(node->InputAt(0), fa0),
         g.UseFixed(node->InputAt(1), fa1))
        ->MarkAsCall();
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Ieee754Unop(
    node_t node, InstructionCode opcode) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
  RiscvOperandGeneratorT<Adapter> g(this);
  Emit(opcode, g.DefineAsFixed(node, fa0), g.UseFixed(node->InputAt(0), fa1))
      ->MarkAsCall();
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);

    // Prepare for C function call.
    if (call_descriptor->IsCFunctionCall()) {
      Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                           call_descriptor->ParameterCount())),
           0, nullptr, 0, nullptr);

      // Poke any stack arguments.
      int slot = kCArgSlotCount;
      for (PushParameter input : (*arguments)) {
        Emit(kRiscvStoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
             g.TempImmediate(slot << kSystemPointerSizeLog2));
        ++slot;
      }
    } else {
      int push_count = static_cast<int>(call_descriptor->ParameterSlotCount());
      if (push_count > 0) {
        Emit(kRiscvStackClaim, g.NoOutput(),
             g.TempImmediate(arguments->size() << kSystemPointerSizeLog2));
      }
      for (size_t n = 0; n < arguments->size(); ++n) {
        PushParameter input = (*arguments)[n];
        if (input.node) {
          Emit(kRiscvStoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
               g.TempImmediate(static_cast<int>(n << kSystemPointerSizeLog2)));
        }
      }
    }
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUnalignedLoad(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    LoadRepresentation load_rep = LoadRepresentationOf(node->op());
    RiscvOperandGeneratorT<Adapter> g(this);
    Node* base = node->InputAt(0);
    Node* index = node->InputAt(1);

    ArchOpcode opcode;
    switch (load_rep.representation()) {
      case MachineRepresentation::kFloat32:
        opcode = kRiscvULoadFloat;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kRiscvULoadDouble;
        break;
      case MachineRepresentation::kWord8:
        opcode = load_rep.IsUnsigned() ? kRiscvLbu : kRiscvLb;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsUnsigned() ? kRiscvUlhu : kRiscvUlh;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:         // Fall through.
      case MachineRepresentation::kWord32:
        opcode = kRiscvUlw;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kRiscvRvvLd;
        break;
      case MachineRepresentation::kSimd256:            // Fall through.
      case MachineRepresentation::kBit:                // Fall through.
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:         // Fall through.
      case MachineRepresentation::kSandboxedPointer:   // Fall through.
      case MachineRepresentation::kMapWord:            // Fall through.
      case MachineRepresentation::kWord64:
      case MachineRepresentation::kNone:
      case MachineRepresentation::kIndirectPointer:
        UNREACHABLE();
    }

    if (g.CanBeImmediate(index, opcode)) {
      Emit(opcode | AddressingModeField::encode(kMode_MRI),
           g.DefineAsRegister(node), g.UseRegister(base),
           g.UseImmediate(index));
    } else {
      InstructionOperand addr_reg = g.TempRegister();
      Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None), addr_reg,
           g.UseRegister(index), g.UseRegister(base));
      // Emit desired load opcode, using temp addr_reg.
      Emit(opcode | AddressingModeField::encode(kMode_MRI),
           g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
    }
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUnalignedStore(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    Node* base = node->InputAt(0);
    Node* index = node->InputAt(1);
    Node* value = node->InputAt(2);

    UnalignedStoreRepresentation rep =
        UnalignedStoreRepresentationOf(node->op());
    ArchOpcode opcode;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kRiscvUStoreFloat;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kRiscvUStoreDouble;
        break;
      case MachineRepresentation::kWord8:
        opcode = kRiscvSb;
        break;
      case MachineRepresentation::kWord16:
        opcode = kRiscvUsh;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:         // Fall through.
      case MachineRepresentation::kWord32:
        opcode = kRiscvUsw;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kRiscvRvvSt;
        break;
      case MachineRepresentation::kSimd256:            // Fall through.
      case MachineRepresentation::kBit:                // Fall through.
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:         // Fall through.
      case MachineRepresentation::kSandboxedPointer:
      case MachineRepresentation::kMapWord:  // Fall through.
      case MachineRepresentation::kNone:
      case MachineRepresentation::kWord64:
      case MachineRepresentation::kIndirectPointer:
        UNREACHABLE();
    }

    if (g.CanBeImmediate(index, opcode)) {
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegister(base), g.UseImmediate(index),
           g.UseRegisterOrImmediateZero(value));
    } else {
      InstructionOperand addr_reg = g.TempRegister();
      Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None), addr_reg,
           g.UseRegister(index), g.UseRegister(base));
      // Emit desired store opcode, using temp addr_reg.
      Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           addr_reg, g.TempImmediate(0), g.UseRegisterOrImmediateZero(value));
    }
  }
}

namespace {

template <typename Adapter>
void VisitWordCompare(InstructionSelectorT<Adapter>* selector,
                      typename Adapter::node_t node,
                      FlagsContinuationT<Adapter>* cont) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitWordCompare(selector, node, kRiscvCmp, cont, false);
  }
}

template <typename Adapter>
void VisitAtomicLoad(InstructionSelectorT<Adapter>* selector, Node* node,
                     ArchOpcode opcode, AtomicWidth width) {
  RiscvOperandGeneratorT<Adapter> g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

template <typename Adapter>
void VisitAtomicStore(InstructionSelectorT<Adapter>* selector, Node* node,
                      ArchOpcode opcode, AtomicWidth width) {
  RiscvOperandGeneratorT<Adapter> g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.NoOutput(), g.UseRegisterOrImmediateZero(value),
                   g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.NoOutput(), g.UseRegisterOrImmediateZero(value), addr_reg,
                   g.TempImmediate(0));
  }
}

template <typename Adapter>
void VisitAtomicBinop(InstructionSelectorT<Adapter>* selector, Node* node,
                      ArchOpcode opcode) {
  RiscvOperandGeneratorT<Adapter> g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  AddressingMode addressing_mode = kMode_MRI;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionOperand temps[4];
  temps[0] = g.TempRegister();
  temps[1] = g.TempRegister();
  temps[2] = g.TempRegister();
  temps[3] = g.TempRegister();
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, 1, outputs, input_count, inputs, 4, temps);
}

}  // namespace

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitStackPointerGreaterThan(
    node_t node, FlagsContinuationT<Adapter>* cont) {
  StackCheckKind kind;
  node_t value;
  if constexpr (Adapter::IsTurboshaft) {
    const auto& op =
        this->turboshaft_graph()
            ->Get(node)
            .template Cast<turboshaft::StackPointerGreaterThanOp>();
    kind = op.kind;
    value = op.stack_limit();
  } else {
    kind = StackCheckKindOf(node->op());
    value = node->InputAt(0);
  }
  InstructionCode opcode =
      kArchStackPointerGreaterThan | MiscField::encode(static_cast<int>(kind));

  RiscvOperandGeneratorT<Adapter> g(this);

  // No outputs.
  InstructionOperand* const outputs = nullptr;
  const int output_count = 0;

  // Applying an offset to this stack check requires a temp register. Offsets
  // are only applied to the first stack check. If applying an offset, we must
  // ensure the input and temp registers do not alias, thus kUniqueRegister.
  InstructionOperand temps[] = {g.TempRegister()};
  const int temp_count = (kind == StackCheckKind::kJSFunctionEntry ? 1 : 0);
  const auto register_mode = (kind == StackCheckKind::kJSFunctionEntry)
                                 ? OperandGenerator::kUniqueRegister
                                 : OperandGenerator::kRegister;

  InstructionOperand inputs[] = {g.UseRegisterWithMode(value, register_mode)};
  static constexpr int input_count = arraysize(inputs);

  EmitWithContinuation(opcode, output_count, outputs, input_count, inputs,
                       temp_count, temps, cont);
}

// Shared routine for word comparisons against zero.
template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWordCompareZero(
    node_t user, node_t value, FlagsContinuationT<Adapter>* cont) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    // Try to combine with comparisons against 0 by simply inverting the branch.
    while (CanCover(user, value)) {
      if (value->opcode() == IrOpcode::kWord32Equal) {
        Int32BinopMatcher m(value);
        if (!m.right().Is(0)) break;
        user = value;
        value = m.left().node();
      } else if (value->opcode() == IrOpcode::kWord64Equal) {
        Int64BinopMatcher m(value);
        if (!m.right().Is(0)) break;
        user = value;
        value = m.left().node();
      } else {
        break;
      }

      cont->Negate();
    }

    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kWord32Equal:
          cont->OverwriteAndNegateIfEqual(kEqual);
          return VisitWordCompare(this, value, cont);
        case IrOpcode::kInt32LessThan:
          cont->OverwriteAndNegateIfEqual(kSignedLessThan);
          return VisitWordCompare(this, value, cont);
        case IrOpcode::kInt32LessThanOrEqual:
          cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
          return VisitWordCompare(this, value, cont);
        case IrOpcode::kUint32LessThan:
          cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
          return VisitWordCompare(this, value, cont);
        case IrOpcode::kUint32LessThanOrEqual:
          cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
          return VisitWordCompare(this, value, cont);
        case IrOpcode::kFloat32Equal:
          cont->OverwriteAndNegateIfEqual(kEqual);
          return VisitFloat32Compare(this, value, cont);
        case IrOpcode::kFloat32LessThan:
          cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
          return VisitFloat32Compare(this, value, cont);
        case IrOpcode::kFloat32LessThanOrEqual:
          cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
          return VisitFloat32Compare(this, value, cont);
        case IrOpcode::kFloat64Equal:
          cont->OverwriteAndNegateIfEqual(kEqual);
          return VisitFloat64Compare(this, value, cont);
        case IrOpcode::kFloat64LessThan:
          cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
          return VisitFloat64Compare(this, value, cont);
        case IrOpcode::kFloat64LessThanOrEqual:
          cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
          return VisitFloat64Compare(this, value, cont);
        case IrOpcode::kProjection:
          // Check if this is the overflow output projection of an
          // <Operation>WithOverflow node.
          if (ProjectionIndexOf(value->op()) == 1u) {
            // We cannot combine the <Operation>WithOverflow with this branch
            // unless the 0th projection (the use of the actual value of the
            // <Operation> is either nullptr, which means there's no use of the
            // actual value, or was already defined, which means it is scheduled
            // *AFTER* this branch).
            Node* const node = value->InputAt(0);
            Node* const result = NodeProperties::FindProjection(node, 0);
            if (result == nullptr || IsDefined(result)) {
              switch (node->opcode()) {
                case IrOpcode::kInt32AddWithOverflow:
                  cont->OverwriteAndNegateIfEqual(kOverflow);
                  return VisitBinop<Adapter, Int32BinopMatcher>(
                      this, node, kRiscvAddOvf, cont);
                case IrOpcode::kInt32SubWithOverflow:
                  cont->OverwriteAndNegateIfEqual(kOverflow);
                  return VisitBinop<Adapter, Int32BinopMatcher>(
                      this, node, kRiscvSubOvf, cont);
                case IrOpcode::kInt32MulWithOverflow:
                  cont->OverwriteAndNegateIfEqual(kOverflow);
                  return VisitBinop<Adapter, Int32BinopMatcher>(
                      this, node, kRiscvMulOvf32, cont);
                case IrOpcode::kInt64AddWithOverflow:
                case IrOpcode::kInt64SubWithOverflow:
                  TRACE_UNIMPL();
                  break;
                default:
                  break;
              }
            }
          }
          break;
        case IrOpcode::kWord32And:
          return VisitWordCompare(this, value, kRiscvTst32, cont, true);
        case IrOpcode::kStackPointerGreaterThan:
          cont->OverwriteAndNegateIfEqual(kStackPointerGreaterThanCondition);
          return VisitStackPointerGreaterThan(value, cont);
        default:
          break;
      }
    }

    // Continuation could not be combined with a compare, emit compare against
    // 0.
    EmitWordCompareZero(this, value, cont);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32Equal(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
    Int32BinopMatcher m(node);
    if (m.right().Is(0)) {
      return VisitWordCompareZero(m.node(), m.left().node(), &cont);
    }

    VisitWordCompare(this, node, &cont);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32LessThan(node_t node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32LessThanOrEqual(node_t node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint32LessThan(node_t node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint32LessThanOrEqual(node_t node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32AddWithOverflow(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
      FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
      return VisitBinop<Adapter, Int32BinopMatcher>(this, node, kRiscvAddOvf,
                                                    &cont);
    }
    FlagsContinuation cont;
    VisitBinop<Adapter, Int32BinopMatcher>(this, node, kRiscvAddOvf, &cont);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32SubWithOverflow(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
      FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
      return VisitBinop<Adapter, Int32BinopMatcher>(this, node, kRiscvSubOvf,
                                                    &cont);
    }
    FlagsContinuation cont;
    VisitBinop<Adapter, Int32BinopMatcher>(this, node, kRiscvSubOvf, &cont);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32MulWithOverflow(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
      FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
      return VisitBinop<Adapter, Int32BinopMatcher>(this, node, kRiscvMulOvf32,
                                                    &cont);
    }
    FlagsContinuation cont;
    VisitBinop<Adapter, Int32BinopMatcher>(this, node, kRiscvMulOvf32, &cont);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicLoad(Node* node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    AtomicLoadParameters atomic_load_params =
        AtomicLoadParametersOf(node->op());
    LoadRepresentation load_rep = atomic_load_params.representation();
    ArchOpcode opcode;
    switch (load_rep.representation()) {
      case MachineRepresentation::kWord8:
        opcode = load_rep.IsSigned() ? kAtomicLoadInt8 : kAtomicLoadUint8;
        break;
      case MachineRepresentation::kWord16:
        opcode = load_rep.IsSigned() ? kAtomicLoadInt16 : kAtomicLoadUint16;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kWord32:
        opcode = kAtomicLoadWord32;
        break;
      default:
        UNREACHABLE();
    }
    VisitAtomicLoad(this, node, opcode, AtomicWidth::kWord32);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicStore(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    AtomicStoreParameters store_params = AtomicStoreParametersOf(node->op());
    MachineRepresentation rep = store_params.representation();
    ArchOpcode opcode;
    switch (rep) {
      case MachineRepresentation::kWord8:
        opcode = kAtomicStoreWord8;
        break;
      case MachineRepresentation::kWord16:
        opcode = kAtomicStoreWord16;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kWord32:
        opcode = kAtomicStoreWord32;
        break;
      default:
        UNREACHABLE();
    }

    VisitAtomicStore(this, node, opcode, AtomicWidth::kWord32);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicExchange(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    ArchOpcode opcode;
    MachineType type = AtomicOpType(node->op());
    if (type == MachineType::Int8()) {
      opcode = kAtomicExchangeInt8;
    } else if (type == MachineType::Uint8()) {
      opcode = kAtomicExchangeUint8;
    } else if (type == MachineType::Int16()) {
      opcode = kAtomicExchangeInt16;
    } else if (type == MachineType::Uint16()) {
      opcode = kAtomicExchangeUint16;
    } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
      opcode = kAtomicExchangeWord32;
    } else {
      UNREACHABLE();
    }

    VisitAtomicExchange(this, node, opcode, AtomicWidth::kWord32);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicCompareExchange(
    node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    ArchOpcode opcode;
    MachineType type = AtomicOpType(node->op());
    if (type == MachineType::Int8()) {
      opcode = kAtomicCompareExchangeInt8;
    } else if (type == MachineType::Uint8()) {
      opcode = kAtomicCompareExchangeUint8;
    } else if (type == MachineType::Int16()) {
      opcode = kAtomicCompareExchangeInt16;
    } else if (type == MachineType::Uint16()) {
      opcode = kAtomicCompareExchangeUint16;
    } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
      opcode = kAtomicCompareExchangeWord32;
    } else {
      UNREACHABLE();
    }

    VisitAtomicCompareExchange(this, node, opcode, AtomicWidth::kWord32);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicBinaryOperation(
    node_t node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    ArchOpcode opcode;
    MachineType type = AtomicOpType(node->op());
    if (type == MachineType::Int8()) {
      opcode = int8_op;
    } else if (type == MachineType::Uint8()) {
      opcode = uint8_op;
    } else if (type == MachineType::Int16()) {
      opcode = int16_op;
    } else if (type == MachineType::Uint16()) {
      opcode = uint16_op;
    } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
      opcode = word32_op;
    } else {
      UNREACHABLE();
    }

    VisitAtomicBinop(this, node, opcode);
  }
}

#define VISIT_ATOMIC_BINOP(op)                                             \
  template <typename Adapter>                                              \
  void InstructionSelectorT<Adapter>::VisitWord32Atomic##op(node_t node) {  \
    if constexpr (Adapter::IsTurboshaft) {                                 \
      UNIMPLEMENTED();                                                     \
    } else {                                                               \
      VisitWord32AtomicBinaryOperation(                                    \
          node, kAtomic##op##Int8, kAtomic##op##Uint8, kAtomic##op##Int16, \
          kAtomic##op##Uint16, kAtomic##op##Word32);                       \
    }                                                                      \
  }
VISIT_ATOMIC_BINOP(Add)
VISIT_ATOMIC_BINOP(Sub)
VISIT_ATOMIC_BINOP(And)
VISIT_ATOMIC_BINOP(Or)
VISIT_ATOMIC_BINOP(Xor)
#undef VISIT_ATOMIC_BINOP

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32AbsWithOverflow(node_t node) {
  UNREACHABLE();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64AbsWithOverflow(node_t node) {
  UNREACHABLE();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBitcastWord32PairToFloat64(
    node_t node) {
  UNIMPLEMENTED();
}

template <unsigned N, typename Adapter>
static void VisitInt32PairBinop(InstructionSelectorT<Adapter>* selector,
                                InstructionCode pair_opcode,
                                InstructionCode single_opcode, Node* node) {
  static_assert(N == 3 || N == 4,
                "Pair operations can only have 3 or 4 inputs");

  RiscvOperandGeneratorT<Adapter> g(selector);

  Node* projection1 = NodeProperties::FindProjection(node, 1);

  if (projection1) {
    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    if constexpr (N == 3) {
      // We use UseUniqueRegister here to avoid register sharing with the output
      // register.
      InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                     g.UseUniqueRegister(node->InputAt(1)),
                                     g.UseUniqueRegister(node->InputAt(2))};

      selector->Emit(pair_opcode, 2, outputs, N, inputs);

    } else if constexpr (N == 4) {
      // We use UseUniqueRegister here to avoid register sharing with the output
      // register.
      InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                     g.UseUniqueRegister(node->InputAt(1)),
                                     g.UseUniqueRegister(node->InputAt(2)),
                                     g.UseUniqueRegister(node->InputAt(3))};

      selector->Emit(pair_opcode, 2, outputs, N, inputs);
    }

  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    selector->Emit(single_opcode, g.DefineSameAsFirst(node),
                   g.UseRegister(node->InputAt(0)),
                   g.UseRegister(node->InputAt(2)));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32PairAdd(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitInt32PairBinop<4>(this, kRiscvAddPair, kRiscvAdd32, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32PairSub(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitInt32PairBinop<4>(this, kRiscvSubPair, kRiscvSub32, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32PairMul(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitInt32PairBinop<4>(this, kRiscvMulPair, kRiscvMul32, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitI64x2SplatI32Pair(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    InstructionOperand low = g.UseRegister(node->InputAt(0));
    InstructionOperand high = g.UseRegister(node->InputAt(1));
    Emit(kRiscvI64x2SplatI32Pair, g.DefineAsRegister(node), low, high);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitI64x2ReplaceLaneI32Pair(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    InstructionOperand operand = g.UseRegister(node->InputAt(0));
    InstructionOperand lane = g.UseImmediate(OpParameter<int32_t>(node->op()));
    InstructionOperand low = g.UseRegister(node->InputAt(1));
    InstructionOperand high = g.UseRegister(node->InputAt(2));
    Emit(kRiscvI64x2ReplaceLaneI32Pair, g.DefineSameAsFirst(node), operand,
         lane, low, high);
  }
}

// Shared routine for multiple shift operations.
template <typename Adapter>
static void VisitWord32PairShift(InstructionSelectorT<Adapter>* selector,
                                 InstructionCode opcode, Node* node) {
  RiscvOperandGeneratorT<Adapter> g(selector);
  Int32Matcher m(node->InputAt(2));
  InstructionOperand shift_operand;
  if (m.HasResolvedValue()) {
    shift_operand = g.UseImmediate(m.node());
  } else {
    shift_operand = g.UseUniqueRegister(m.node());
  }

  // We use UseUniqueRegister here to avoid register sharing with the output
  // register.
  InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                 g.UseUniqueRegister(node->InputAt(1)),
                                 shift_operand};

  Node* projection1 = NodeProperties::FindProjection(node, 1);

  InstructionOperand outputs[2];
  InstructionOperand temps[1];
  int32_t output_count = 0;
  int32_t temp_count = 0;

  outputs[output_count++] = g.DefineAsRegister(node);
  if (projection1) {
    outputs[output_count++] = g.DefineAsRegister(projection1);
  } else {
    temps[temp_count++] = g.TempRegister();
  }

  selector->Emit(opcode, output_count, outputs, 3, inputs, temp_count, temps);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32PairShl(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitWord32PairShift(this, kRiscvShlPair, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32PairShr(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitWord32PairShift(this, kRiscvShrPair, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32PairSar(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    VisitWord32PairShift(this, kRiscvSarPair, node);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairLoad(Node* node) {
  RiscvOperandGeneratorT<Adapter> g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  ArchOpcode opcode = kRiscvWord32AtomicPairLoad;
  AddressingMode addressing_mode = kMode_MRI;
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index)};
  InstructionOperand temps[3];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister(t0);
  InstructionOperand outputs[2];
  size_t output_count = 0;

  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection0) {
    outputs[output_count++] = g.DefineAsFixed(projection0, a0);
  } else {
    temps[temp_count++] = g.TempRegister(a0);
  }
  if (projection1) {
    outputs[output_count++] = g.DefineAsFixed(projection1, a1);
  } else {
    temps[temp_count++] = g.TempRegister(a1);
  }
  Emit(code, output_count, outputs, arraysize(inputs), inputs, temp_count,
       temps);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairStore(Node* node) {
  RiscvOperandGeneratorT<Adapter> g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value_low = node->InputAt(2);
  Node* value_high = node->InputAt(3);

  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseFixed(value_low, a1),
                                 g.UseFixed(value_high, a2)};
  InstructionOperand temps[] = {g.TempRegister(a0), g.TempRegister(),
                                g.TempRegister()};
  Emit(kRiscvWord32AtomicPairStore | AddressingModeField::encode(kMode_MRI), 0,
       nullptr, arraysize(inputs), inputs, arraysize(temps), temps);
}

template <typename Adapter>
static void VisitPairAtomicBinop(InstructionSelectorT<Adapter>* selector,
                                 Node* node, ArchOpcode opcode) {
  RiscvOperandGeneratorT<Adapter> g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  Node* value_high = node->InputAt(3);
  AddressingMode addressing_mode = kMode_None;
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseFixed(value, a1),
                                 g.UseFixed(value_high, a2)};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  InstructionOperand temps[3];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister(t0);

  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection0) {
    outputs[output_count++] = g.DefineAsFixed(projection0, a0);
  } else {
    temps[temp_count++] = g.TempRegister(a0);
  }
  if (projection1) {
    outputs[output_count++] = g.DefineAsFixed(projection1, a1);
  } else {
    temps[temp_count++] = g.TempRegister(a1);
  }
  selector->Emit(code, output_count, outputs, arraysize(inputs), inputs,
                 temp_count, temps);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairAdd(Node* node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairAdd);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairSub(Node* node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairSub);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairAnd(Node* node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairAnd);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairOr(Node* node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairOr);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairXor(Node* node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairXor);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairExchange(Node* node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairExchange);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairCompareExchange(
    Node* node) {
  RiscvOperandGeneratorT<Adapter> g(this);
  InstructionOperand inputs[] = {
      g.UseRegister(node->InputAt(0)),  g.UseRegister(node->InputAt(1)),
      g.UseFixed(node->InputAt(2), a1), g.UseFixed(node->InputAt(3), a2),
      g.UseFixed(node->InputAt(4), a3), g.UseFixed(node->InputAt(5), a4)};

  InstructionCode code = kRiscvWord32AtomicPairCompareExchange |
                         AddressingModeField::encode(kMode_MRI);
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  InstructionOperand outputs[2];
  size_t output_count = 0;
  InstructionOperand temps[3];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister(t0);
  if (projection0) {
    outputs[output_count++] = g.DefineAsFixed(projection0, a0);
  } else {
    temps[temp_count++] = g.TempRegister(a0);
  }
  if (projection1) {
    outputs[output_count++] = g.DefineAsFixed(projection1, a1);
  } else {
    temps[temp_count++] = g.TempRegister(a1);
  }
  Emit(code, output_count, outputs, arraysize(inputs), inputs, temp_count,
       temps);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitF64x2Min(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    InstructionOperand temp1 = g.TempFpRegister(v0);
    InstructionOperand mask_reg = g.TempFpRegister(v0);
    InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
    const int32_t kNaN = 0x7ff80000L, kNaNShift = 32;
    this->Emit(kRiscvVmfeqVv, temp1, g.UseRegister(node->InputAt(0)),
               g.UseRegister(node->InputAt(0)), g.UseImmediate(E64),
               g.UseImmediate(m1));
    this->Emit(kRiscvVmfeqVv, temp2, g.UseRegister(node->InputAt(1)),
               g.UseRegister(node->InputAt(1)), g.UseImmediate(E64),
               g.UseImmediate(m1));
    this->Emit(kRiscvVandVv, mask_reg, temp2, temp1, g.UseImmediate(E64),
               g.UseImmediate(m1));

    InstructionOperand temp3 = g.TempFpRegister(kSimd128ScratchReg);
    InstructionOperand temp4 = g.TempFpRegister(kSimd128ScratchReg);
    InstructionOperand temp5 = g.TempFpRegister(kSimd128ScratchReg);
    this->Emit(kRiscvVmv, temp3, g.UseImmediate(kNaN), g.UseImmediate(E64),
               g.UseImmediate(m1));
    this->Emit(kRiscvVsll, temp4, temp3, g.UseImmediate(kNaNShift),
               g.UseImmediate(E64), g.UseImmediate(m1));
    this->Emit(kRiscvVfminVv, temp5, g.UseRegister(node->InputAt(1)),
               g.UseRegister(node->InputAt(0)), g.UseImmediate(E64),
               g.UseImmediate(m1), g.UseImmediate(Mask));
    this->Emit(kRiscvVmv, g.DefineAsRegister(node), temp5, g.UseImmediate(E64),
               g.UseImmediate(m1));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitF64x2Max(node_t node) {
  if constexpr (Adapter::IsTurboshaft) {
    UNIMPLEMENTED();
  } else {
    RiscvOperandGeneratorT<Adapter> g(this);
    InstructionOperand temp1 = g.TempFpRegister(v0);
    InstructionOperand mask_reg = g.TempFpRegister(v0);
    InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
    const int32_t kNaN = 0x7ff80000L, kNaNShift = 32;
    this->Emit(kRiscvVmfeqVv, temp1, g.UseRegister(node->InputAt(0)),
               g.UseRegister(node->InputAt(0)), g.UseImmediate(E64),
               g.UseImmediate(m1));
    this->Emit(kRiscvVmfeqVv, temp2, g.UseRegister(node->InputAt(1)),
               g.UseRegister(node->InputAt(1)), g.UseImmediate(E64),
               g.UseImmediate(m1));
    this->Emit(kRiscvVandVv, mask_reg, temp2, temp1, g.UseImmediate(E64),
               g.UseImmediate(m1));

    InstructionOperand temp3 = g.TempFpRegister(kSimd128ScratchReg);
    InstructionOperand temp4 = g.TempFpRegister(kSimd128ScratchReg);
    InstructionOperand temp5 = g.TempFpRegister(kSimd128ScratchReg);
    this->Emit(kRiscvVmv, temp3, g.UseImmediate(kNaN), g.UseImmediate(E64),
               g.UseImmediate(m1));
    this->Emit(kRiscvVsll, temp4, temp3, g.UseImmediate(kNaNShift),
               g.UseImmediate(E64), g.UseImmediate(m1));
    this->Emit(kRiscvVfmaxVv, temp5, g.UseRegister(node->InputAt(1)),
               g.UseRegister(node->InputAt(0)), g.UseImmediate(E64),
               g.UseImmediate(m1), g.UseImmediate(Mask));
    this->Emit(kRiscvVmv, g.DefineAsRegister(node), temp5, g.UseImmediate(E64),
               g.UseImmediate(m1));
  }
}
// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags = MachineOperatorBuilder::kNoFlags;
  return flags | MachineOperatorBuilder::kWord32Ctz |
         MachineOperatorBuilder::kWord32Ctz |
         MachineOperatorBuilder::kWord32Popcnt |
         MachineOperatorBuilder::kInt32DivIsSafe |
         MachineOperatorBuilder::kUint32DivIsSafe |
         MachineOperatorBuilder::kFloat32RoundDown |
         MachineOperatorBuilder::kFloat32RoundUp |
         MachineOperatorBuilder::kFloat32RoundTruncate |
         MachineOperatorBuilder::kFloat32RoundTiesEven;
}
#undef TRACE_UNIMPL
#undef TRACE

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    InstructionSelectorT<TurbofanAdapter>;
template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    InstructionSelectorT<TurboshaftAdapter>;
}  // namespace compiler
}  // namespace internal
}  // namespace v8
