// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

scoped_refptr<CalculationExpressionOperationNode> BuildOperationNode(
    Vector<float> numbers,
    CalculationOperator op) {
  CalculationExpressionOperationNode::Children operands;
  for (float number : numbers) {
    scoped_refptr<CalculationExpressionNumberNode> operand =
        base::MakeRefCounted<CalculationExpressionNumberNode>(number);
    operands.push_back(operand);
  }
  scoped_refptr<CalculationExpressionOperationNode> operation =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands), op);
  return operation;
}

}  // namespace

TEST(CalculationExpressionOperationNodeTest, Comparison) {
  scoped_refptr<CalculationExpressionOperationNode> operation1 =
      BuildOperationNode({13.f, 17.f}, CalculationOperator::kMax);
  scoped_refptr<CalculationExpressionOperationNode> operation2 =
      BuildOperationNode({17.f, 13.f}, CalculationOperator::kMax);
  scoped_refptr<CalculationExpressionOperationNode> operation3 =
      BuildOperationNode({17.f, 13.f}, CalculationOperator::kMax);

  EXPECT_EQ(operation1->Evaluate(FLT_MAX, nullptr),
            operation2->Evaluate(FLT_MAX, nullptr));
  EXPECT_EQ(*operation2, *operation3);
}

TEST(CalculationExpressionOperationNodeTest, SteppedValueFunctions) {
  scoped_refptr<CalculationExpressionOperationNode> operation_nearest_1_1 =
      BuildOperationNode({1, 1}, CalculationOperator::kRoundNearest);
  scoped_refptr<CalculationExpressionOperationNode> operation_mod_1_1 =
      BuildOperationNode({1, 1}, CalculationOperator::kMod);
  scoped_refptr<CalculationExpressionOperationNode> operation_rem_1_1 =
      BuildOperationNode({1, 1}, CalculationOperator::kRem);
  scoped_refptr<CalculationExpressionOperationNode> operation_mod_18_5 =
      BuildOperationNode({18, 5}, CalculationOperator::kMod);
  scoped_refptr<CalculationExpressionOperationNode> operation_mod_17_5 =
      BuildOperationNode({17, 5}, CalculationOperator::kMod);

  CalculationExpressionOperationNode::Children operands_rem_two_mods;
  operands_rem_two_mods.push_back(operation_mod_18_5);
  operands_rem_two_mods.push_back(operation_mod_17_5);
  scoped_refptr<CalculationExpressionOperationNode> operation_rem_two_mods =
      base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(operands_rem_two_mods), CalculationOperator::kRem);

  EXPECT_EQ(operation_nearest_1_1->Evaluate(FLT_MAX, nullptr), 1.f);
  EXPECT_EQ(operation_mod_1_1->Evaluate(FLT_MAX, nullptr), 0.f);
  EXPECT_EQ(operation_rem_1_1->Evaluate(FLT_MAX, nullptr), 0.f);
  EXPECT_EQ(operation_rem_two_mods->Evaluate(FLT_MAX, nullptr), 1.f);
}

TEST(CalculationExpressionOperationNodeTest, ExponentialFunctions) {
  EXPECT_EQ(BuildOperationNode({3.f, 4.f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, nullptr),
            5.f);
  EXPECT_EQ(BuildOperationNode({3e37f, 4e37f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, nullptr),
            5e37f);
  EXPECT_EQ(BuildOperationNode({8e-46f, 15e-46f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, nullptr),
            17e-46f);
  EXPECT_EQ(
      BuildOperationNode({6e37f, 6e37f, 17e37}, CalculationOperator::kHypot)
          ->Evaluate(FLT_MAX, nullptr),
      19e37f);
  EXPECT_EQ(BuildOperationNode({-3.f, 4.f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, nullptr),
            5.f);
  EXPECT_EQ(BuildOperationNode({-3.f, -4.f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, nullptr),
            5.f);
  EXPECT_EQ(BuildOperationNode({-0.f, +0.f}, CalculationOperator::kHypot)
                ->Evaluate(FLT_MAX, nullptr),
            +0.f);
  EXPECT_EQ(
      BuildOperationNode({6e37f, -6e37f, -17e37}, CalculationOperator::kHypot)
          ->Evaluate(FLT_MAX, nullptr),
      19e37f);
}

TEST(CalculationExpressionOperationNodeTest,
     SignRelatedFunctionsPixelsAndPercent) {
  scoped_refptr<CalculationExpressionNode> pixels_and_percent_node =
      base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(-100.0f, -100.0f));
  CalculationExpressionOperationNode::Children children;
  children.push_back(pixels_and_percent_node);
  scoped_refptr<const CalculationExpressionNode>
      pixels_and_percent_operation_abs =
          CalculationExpressionOperationNode::CreateSimplified(
              std::move(children), CalculationOperator::kAbs);
  EXPECT_TRUE(pixels_and_percent_operation_abs->IsOperation());
  EXPECT_EQ(pixels_and_percent_operation_abs->Evaluate(100.0f, nullptr),
            200.0f);
}

TEST(CalculationExpressionOperationNodeTest,
     SignRelatedFunctionsPixelsAndZeroPercent) {
  scoped_refptr<CalculationExpressionNode> pixels_and_zero_percent_node =
      base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(-100.0f, 0.0f));
  CalculationExpressionOperationNode::Children children;
  children.push_back(pixels_and_zero_percent_node);
  scoped_refptr<const CalculationExpressionNode>
      pixels_and_zero_percent_operation_sign =
          CalculationExpressionOperationNode::CreateSimplified(
              std::move(children), CalculationOperator::kSign);
  EXPECT_TRUE(pixels_and_zero_percent_operation_sign->IsNumber());
  EXPECT_EQ(pixels_and_zero_percent_operation_sign->Evaluate(FLT_MAX, nullptr),
            -1.0f);
}

TEST(CalculationExpressionOperationNodeTest, SignRelatedFunctionsPixelsOnly) {
  scoped_refptr<CalculationExpressionNode> pixels_node =
      base::MakeRefCounted<CalculationExpressionNumberNode>(-0.0f);
  CalculationExpressionOperationNode::Children children;
  children.push_back(pixels_node);
  scoped_refptr<const CalculationExpressionNode> pixels_operation_sign =
      CalculationExpressionOperationNode::CreateSimplified(
          std::move(children), CalculationOperator::kSign);
  EXPECT_TRUE(pixels_operation_sign->IsOperation());
  EXPECT_TRUE(std::signbit(pixels_operation_sign->Evaluate(FLT_MAX, nullptr)));
}

TEST(CalculationExpressionOperationNodeTest, SignRelatedFunctions) {
  scoped_refptr<CalculationExpressionOperationNode> operation_abs_1 =
      BuildOperationNode({1.0f}, CalculationOperator::kAbs);
  scoped_refptr<CalculationExpressionOperationNode> operation_abs_minus_1 =
      BuildOperationNode({-1.0f}, CalculationOperator::kAbs);
  scoped_refptr<CalculationExpressionOperationNode> operation_abs_minus_0 =
      BuildOperationNode({-0.0f}, CalculationOperator::kAbs);
  scoped_refptr<CalculationExpressionOperationNode> operation_sign_1 =
      BuildOperationNode({1.0f}, CalculationOperator::kSign);
  scoped_refptr<CalculationExpressionOperationNode> operation_sign_minus_1 =
      BuildOperationNode({-1.0f}, CalculationOperator::kSign);
  scoped_refptr<CalculationExpressionOperationNode> operation_sign_0 =
      BuildOperationNode({0.0f}, CalculationOperator::kSign);
  scoped_refptr<CalculationExpressionOperationNode> operation_sign_minus_0 =
      BuildOperationNode({-0.0f}, CalculationOperator::kSign);

  EXPECT_EQ(operation_abs_1->Evaluate(FLT_MAX, nullptr), 1.0f);
  EXPECT_EQ(operation_abs_minus_1->Evaluate(FLT_MAX, nullptr), 1.0f);
  EXPECT_EQ(operation_abs_minus_0->Evaluate(FLT_MAX, nullptr), 0.0f);
  EXPECT_EQ(operation_sign_1->Evaluate(FLT_MAX, nullptr), 1.0f);
  EXPECT_EQ(operation_sign_minus_1->Evaluate(FLT_MAX, nullptr), -1.0f);
  EXPECT_EQ(operation_sign_0->Evaluate(FLT_MAX, nullptr), 0.0f);
  EXPECT_TRUE(std::signbit(operation_sign_minus_0->Evaluate(FLT_MAX, nullptr)));
}

}  // namespace blink
