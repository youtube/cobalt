// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/machine-optimization-reducer.h"

#include "src/compiler/turboshaft/operations.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

using MachineOptimizationReducerTest = ReducerTest;

TEST_F(MachineOptimizationReducerTest, ReduceToWord32RorWithXorChain) {
  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 3> reps = {rep32, rep32, rep32};
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto value = t.template GetParameter<Word32>(0);
    auto other1 = t.template GetParameter<Word32>(1);
    auto other2 = t.template GetParameter<Word32>(2);

    auto shl =
        t.Asm().Shift(value, t.Asm().Word32Constant(7),
                      ShiftOp::Kind::kShiftLeft, WordRepresentation::Word32());
    auto shr = t.Asm().Shift(value, t.Asm().Word32Constant(25),
                             ShiftOp::Kind::kShiftRightLogical,
                             WordRepresentation::Word32());

    auto xor1 = t.Asm().WordBinop(shl, other1, WordBinopOp::Kind::kBitwiseXor,
                                  WordRepresentation::Word32());
    auto xor2 = t.Asm().WordBinop(xor1, other2, WordBinopOp::Kind::kBitwiseXor,
                                  WordRepresentation::Word32());
    auto xor3 = t.Asm().WordBinop(xor2, shr, WordBinopOp::Kind::kBitwiseXor,
                                  WordRepresentation::Word32());

    t.Capture(xor3, "xor3");
    t.Asm().Return(xor3);
  });

  test.Run<MachineOptimizationReducer>();

  bool found_ror = false;
  for (OpIndex index : test.graph().AllOperationIndices()) {
    if (const ShiftOp* shift = test.graph().Get(index).TryCast<ShiftOp>()) {
      if (shift->kind == ShiftOp::Kind::kRotateRight) {
        found_ror = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_ror);
}

TEST_F(MachineOptimizationReducerTest, ReduceToWord32RorWithOrChain) {
  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 3> reps = {rep32, rep32, rep32};
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto value = t.template GetParameter<Word32>(0);
    auto other1 = t.template GetParameter<Word32>(1);
    auto other2 = t.template GetParameter<Word32>(2);

    auto shl =
        t.Asm().Shift(value, t.Asm().Word32Constant(7),
                      ShiftOp::Kind::kShiftLeft, WordRepresentation::Word32());
    auto shr = t.Asm().Shift(value, t.Asm().Word32Constant(25),
                             ShiftOp::Kind::kShiftRightLogical,
                             WordRepresentation::Word32());

    auto or1 = t.Asm().WordBinop(shl, other1, WordBinopOp::Kind::kBitwiseOr,
                                 WordRepresentation::Word32());
    auto or2 = t.Asm().WordBinop(or1, other2, WordBinopOp::Kind::kBitwiseOr,
                                 WordRepresentation::Word32());
    auto or3 = t.Asm().WordBinop(or2, shr, WordBinopOp::Kind::kBitwiseOr,
                                 WordRepresentation::Word32());

    t.Capture(or3, "or3");
    t.Asm().Return(or3);
  });

  test.Run<MachineOptimizationReducer>();

  bool found_ror = false;
  for (OpIndex index : test.graph().AllOperationIndices()) {
    if (const ShiftOp* shift = test.graph().Get(index).TryCast<ShiftOp>()) {
      if (shift->kind == ShiftOp::Kind::kRotateRight) {
        found_ror = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_ror);
}

TEST_F(MachineOptimizationReducerTest, ReduceToWord32RorWithXorTree) {
  const RegisterRepresentation rep32 = RegisterRepresentation::Word32();
  base::SmallVector<RegisterRepresentation, 3> reps = {rep32, rep32, rep32};
  auto test = CreateFromGraph(base::VectorOf(reps), [](auto& t) {
    auto value = t.template GetParameter<Word32>(0);
    auto other1 = t.template GetParameter<Word32>(1);
    auto other2 = t.template GetParameter<Word32>(2);

    auto shl =
        t.Asm().Shift(value, t.Asm().Word32Constant(7),
                      ShiftOp::Kind::kShiftLeft, WordRepresentation::Word32());
    auto shr = t.Asm().Shift(value, t.Asm().Word32Constant(25),
                             ShiftOp::Kind::kShiftRightLogical,
                             WordRepresentation::Word32());

    auto xor_left =
        t.Asm().WordBinop(shl, other1, WordBinopOp::Kind::kBitwiseXor,
                          WordRepresentation::Word32());
    auto xor_right =
        t.Asm().WordBinop(other2, shr, WordBinopOp::Kind::kBitwiseXor,
                          WordRepresentation::Word32());
    auto xor_root =
        t.Asm().WordBinop(xor_left, xor_right, WordBinopOp::Kind::kBitwiseXor,
                          WordRepresentation::Word32());

    t.Capture(xor_root, "xor_root");
    t.Asm().Return(xor_root);
  });

  test.Run<MachineOptimizationReducer>();

  bool found_ror = false;
  for (OpIndex index : test.graph().AllOperationIndices()) {
    if (const ShiftOp* shift = test.graph().Get(index).TryCast<ShiftOp>()) {
      if (shift->kind == ShiftOp::Kind::kRotateRight) {
        found_ror = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_ror);
}

}  // namespace v8::internal::compiler::turboshaft
