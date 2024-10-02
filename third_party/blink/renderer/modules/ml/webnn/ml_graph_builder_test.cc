// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test.h"

#include <algorithm>
#include <memory>

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink {

const uint32_t kSquareRootOfSizeMax =
    base::saturated_cast<uint32_t>(std::sqrt(SIZE_MAX));

class MLGraphBuilderTest : public testing::Test {
 public:
  MLGraphBuilderTest() = default;
  ~MLGraphBuilderTest() override = default;
};

TEST_F(MLGraphBuilderTest, InputTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building a 2-D input without errors.
    auto* input =
        BuildInput(builder, "input", {3, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    EXPECT_NE(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kNoError);
    EXPECT_EQ(input->Kind(), MLOperand::OperandKind::kInput);
    EXPECT_EQ(input->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(input->Dimensions(), Vector<uint32_t>({3, 4}));
    EXPECT_EQ(input->Name(), "input");
  }
  {
    // Test throwing exception if the name is empty.
    auto* input =
        BuildInput(builder, "", {3, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(), "The name is empty.");
  }
  {
    // Test throwing exception if a dimension size is 0.
    auto* input =
        BuildInput(builder, "input", {3, 0}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: All dimensions should be positive.");
  }
  {
    // Test throwing exception if the dimensions is empty.
    auto* input =
        BuildInput(builder, "input", {}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: The dimensions is empty.");
  }
  {
    // Test throwing exception if the number of elements is too large.
    // Set the dimensions that let the number of elements be 2 * SIZE_MAX.
    auto* input = BuildInput(
        builder, "input", {1, 2, kSquareRootOfSizeMax, kSquareRootOfSizeMax},
        V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: The number of elements is "
              "too large.");
  }
  {
    // Test throwing exception if the byte length is too large.
    // Set the dimensions and type that let the byte length be 4 * SIZE_MAX.
    auto* input = BuildInput(
        builder, "input", {1, 1, kSquareRootOfSizeMax, kSquareRootOfSizeMax},
        V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: The byte length is too large.");
  }
}

TEST_F(MLGraphBuilderTest, ConstantTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building a 2-D constant without errors.
    auto* constant =
        BuildConstant(builder, {2, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    EXPECT_NE(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kNoError);
    EXPECT_EQ(constant->Kind(), MLOperand::OperandKind::kConstant);
    EXPECT_EQ(constant->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(constant->Dimensions(), Vector<uint32_t>({2, 3}));
  }
  {
    // Test throwing exception if a dimension is 0.
    auto* constant =
        BuildConstant(builder, {2, 0}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: All dimensions should be positive.");
  }
  {
    // Test throwing exception if buffer view type doesn't match the operand
    // type.
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(6, V8MLOperandType::Enum::kFloat32);
    auto* constant =
        BuildConstant(builder, {2, 3}, V8MLOperandType::Enum::kInt32,
                      scope.GetExceptionState(), buffer_view);
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The buffer view type doesn't match the operand type.");
  }
  {
    // Test throwing exception if buffer view size is not expected.
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(8, V8MLOperandType::Enum::kInt32);
    auto* constant =
        BuildConstant(builder, {2, 2}, V8MLOperandType::Enum::kInt32,
                      scope.GetExceptionState(), buffer_view);
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The buffer view byte length (32) doesn't match the expected "
              "byte length (16).");
  }
  {
    // Test throwing exception if the number of elements is too large.
    // Set the dimensions that let the number of elements be 2 * SIZE_MAX.
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(1, V8MLOperandType::Enum::kFloat32);
    auto* constant = BuildConstant(
        builder, {1, 2, kSquareRootOfSizeMax, kSquareRootOfSizeMax},
        V8MLOperandType::Enum::kFloat32, scope.GetExceptionState(),
        buffer_view);
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: The number of elements is "
              "too large.");
  }
  {
    // Test throwing exception if the byte length is too large.
    // Set the dimensions and type that let the byte length be 4 * SIZE_MAX.
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(1, V8MLOperandType::Enum::kFloat32);
    auto* constant = BuildConstant(
        builder, {1, 1, kSquareRootOfSizeMax, kSquareRootOfSizeMax},
        V8MLOperandType::Enum::kFloat32, scope.GetExceptionState(),
        buffer_view);
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: The byte length is too large.");
  }
}

TEST_F(MLGraphBuilderTest, ConcatTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building Concat with one input.
    Vector<uint32_t> input_a_shape({4, 4, 3});
    Vector<uint32_t> output_shape({4, 4, 3});
    auto* input_a =
        BuildInput(builder, "input_a", input_a_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    uint32_t axis = 2;
    auto* output = builder->concat({input_a}, axis, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), output_shape);
    const MLOperator* concat = output->Operator();
    EXPECT_NE(concat, nullptr);
    EXPECT_EQ(concat->Kind(), MLOperator::OperatorKind::kConcat);
    EXPECT_EQ(concat->IsConnected(), true);
    EXPECT_EQ(concat->Options(), nullptr);
  }
  {
    // Test building Concat with two inputs.
    Vector<uint32_t> input_a_shape({3, 1, 5});
    Vector<uint32_t> input_b_shape({3, 2, 5});
    Vector<uint32_t> output_shape({3, 3, 5});
    auto* input_a =
        BuildInput(builder, "input_a", input_a_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* input_b =
        BuildInput(builder, "input_b", input_b_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    uint32_t axis = 1;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), output_shape);
    const MLOperator* concat = output->Operator();
    EXPECT_NE(concat, nullptr);
    EXPECT_EQ(concat->Kind(), MLOperator::OperatorKind::kConcat);
    EXPECT_EQ(concat->IsConnected(), true);
    EXPECT_EQ(concat->Options(), nullptr);
  }
  {
    // Test building Concat with three inputs.
    Vector<uint32_t> input_a_shape({3, 5, 1});
    Vector<uint32_t> input_b_shape({3, 5, 2});
    Vector<uint32_t> input_c_shape({3, 5, 3});
    Vector<uint32_t> output_shape({3, 5, 6});
    auto* input_a =
        BuildInput(builder, "input_a", input_a_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* input_b =
        BuildInput(builder, "input_b", input_b_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* input_c =
        BuildInput(builder, "input_c", input_c_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    uint32_t axis = 2;
    auto* output = builder->concat({input_a, input_b, input_c}, axis,
                                   scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), output_shape);
    const MLOperator* concat = output->Operator();
    EXPECT_NE(concat, nullptr);
    EXPECT_EQ(concat->Kind(), MLOperator::OperatorKind::kConcat);
    EXPECT_EQ(concat->IsConnected(), true);
    EXPECT_EQ(concat->Options(), nullptr);
  }
  {
    // Test building Concat with two 1D inputs.
    Vector<uint32_t> input_a_shape({1});
    Vector<uint32_t> input_b_shape({1});
    Vector<uint32_t> output_shape({2});
    auto* input_a =
        BuildInput(builder, "input_a", input_a_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* input_b =
        BuildInput(builder, "input_b", input_b_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    uint32_t axis = 0;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), output_shape);
    const MLOperator* concat = output->Operator();
    EXPECT_NE(concat, nullptr);
    EXPECT_EQ(concat->Kind(), MLOperator::OperatorKind::kConcat);
    EXPECT_EQ(concat->IsConnected(), true);
    EXPECT_EQ(concat->Options(), nullptr);
  }
  {
    // Test throwing exception when the inputs are empty.
    uint32_t axis = 0;
    auto* output = builder->concat({}, axis, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The inputs should not be empty.");
  }
  {
    // Test throwing exception when the argument types are inconsistent.
    Vector<uint32_t> input_a_shape({1, 1});
    Vector<uint32_t> input_b_shape({1, 1});
    auto* input_a =
        BuildInput(builder, "input_a", input_a_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* input_b =
        BuildInput(builder, "input_b", input_b_shape,
                   V8MLOperandType::Enum::kInt32, scope.GetExceptionState());
    uint32_t axis = 0;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input types don't match.");
  }
  {
    // Test throwing exception when the inputs have different dimension.
    Vector<uint32_t> input_a_shape({1, 1});
    Vector<uint32_t> input_b_shape({1, 1, 1});
    auto* input_a =
        BuildInput(builder, "input_a", input_a_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* input_b =
        BuildInput(builder, "input_b", input_b_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    uint32_t axis = 0;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All input tensors must have the same dimension.");
  }
  {
    // Test throwing exception when the axis is equal to or greater than the
    // size of dimension.
    Vector<uint32_t> input_a_shape({1, 1});
    Vector<uint32_t> input_b_shape({1, 1});
    auto* input_a =
        BuildInput(builder, "input_a", input_a_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* input_b =
        BuildInput(builder, "input_b", input_b_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    uint32_t axis = 2;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The value of axis should be in the interval [0, N-1] where N is "
              "the rank of input tensors.");
  }
  {
    // Test throwing exception when the inputs have other axes with different
    // sizes except on the axis.
    Vector<uint32_t> input_a_shape({1, 1, 1});
    Vector<uint32_t> input_b_shape({1, 2, 3});
    auto* input_a =
        BuildInput(builder, "input_a", input_a_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* input_b =
        BuildInput(builder, "input_b", input_b_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    uint32_t axis = 1;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All input tensors must have the same shape, except for the size "
              "of the dimension to concatenate on.");
  }
}

MLOperand* BuildConv2d(V8TestingScope& scope,
                       MLGraphBuilder* builder,
                       const MLOperand* input,
                       const MLOperand* filter,
                       const MLConv2dOptions* options) {
  auto* output =
      builder->conv2d(input, filter, options, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* conv2d = output->Operator();
  EXPECT_NE(conv2d, nullptr);
  EXPECT_EQ(conv2d->Kind(), MLOperator::OperatorKind::kConv2d);
  EXPECT_EQ(conv2d->IsConnected(), true);
  EXPECT_NE(conv2d->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, Conv2dTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test conv2d with default options.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    EXPECT_TRUE(options->hasAutoPad());
    EXPECT_EQ(options->autoPad(), V8MLAutoPad::Enum::kExplicit);
    EXPECT_FALSE(options->hasBias());
    EXPECT_FALSE(options->hasDilations());
    EXPECT_FALSE(options->hasActivation());
    EXPECT_TRUE(options->hasFilterLayout());
    EXPECT_EQ(options->filterLayout(),
              V8MLConv2dFilterOperandLayout::Enum::kOihw);
    EXPECT_TRUE(options->hasInputLayout());
    EXPECT_EQ(options->inputLayout(), V8MLInputOperandLayout::Enum::kNchw);
    EXPECT_TRUE(options->hasGroups());
    EXPECT_EQ(options->groups(), 1u);
    EXPECT_FALSE(options->hasPadding());
    EXPECT_FALSE(options->hasStrides());
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with padding=1.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test conv2d with autopad="same-lower".
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kSameLower);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test conv2d with autopad="same-upper".
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kSameUpper);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test conv2d with strides=2 and padding=1.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with strides=2 and asymmetric padding.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 4, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setPadding({1, 2, 0, 1});
    options->setStrides({2, 2});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test depthwise conv2d by setting groups to input channels.
    auto* input =
        BuildInput(builder, "input", {1, 4, 2, 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {4, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(4);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 4, 1, 1}));
  }
  {
    // Test depthwise conv2d with groups=4, inputLayout="nhwc" and
    // filterLayout="ihwo".
    auto* input =
        BuildInput(builder, "input", {1, 2, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 2, 4}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(4);
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 1, 4}));
  }
  {
    // Test conv2d with dilations=4, inputLayout="nhwc" and
    // filterLayout="ihwo".
    auto* input =
        BuildInput(builder, "input", {1, 65, 65, 1},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 3, 3, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    options->setDilations({4, 4});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 57, 57, 1}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="oihw".
    auto* input =
        BuildInput(builder, "input", {1, 2, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOihw);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="hwio".
    auto* input =
        BuildInput(builder, "input", {1, 2, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {3, 3, 2, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kHwio);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ohwi".
    auto* input =
        BuildInput(builder, "input", {1, 2, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 3, 3, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ihwo".
    auto* input =
        BuildInput(builder, "input", {1, 2, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {2, 3, 3, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="oihw".
    auto* input =
        BuildInput(builder, "input", {1, 5, 5, 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOihw);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="hwio".
    auto* input =
        BuildInput(builder, "input", {1, 5, 5, 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {3, 3, 2, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kHwio);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="ohwi".
    auto* input =
        BuildInput(builder, "input", {1, 5, 5, 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 3, 3, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="ihwo".
    auto* input =
        BuildInput(builder, "input", {1, 5, 5, 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {2, 3, 3, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test throwing exception if the output operand's number of elements is too
    // large.
    // Set the input and filter dimensions that let the output's number of
    // lements be 2 * SIZE_MAX.
    auto* input =
        BuildInput(builder, "input",
                   {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {8, 1, 1, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->conv2d(input, filter, MLConv2dOptions::Create(),
                                   scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid output operand: The number of elements "
              "is too large.");
  }
  {
    // Test throwing exception if the output operand's byte length is too large.
    // Set the dimensions and type of input and filter that let the output's
    // byte length be 4 * SIZE_MAX.
    auto* input =
        BuildInput(builder, "input",
                   {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {4, 1, 1, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->conv2d(input, filter, MLConv2dOptions::Create(),
                                   scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid output operand: The byte length is too large.");
  }
  {
    // Test throwing exception when the input is not a 4-D tensor.
    auto* input =
        BuildInput(builder, "input", {1, 5, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 2, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input should be a 4-D tensor.");
  }
  {
    // Test throwing exception when the filter is not a 4-D tensor.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The filter should be a 4-D tensor.");
  }
  {
    // Test throwing exception when the filter type doesn't match the input
    // type.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kInt32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The filter type doesn't match the input type.");
  }
  {
    // Test throwing exception when the length of padding is not 4.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setPadding({2, 2});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of padding should be 4.");
  }
  {
    // Test throwing exception when the length of strides is not 2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setStrides({2});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of strides should be 2.");
  }
  {
    // Test throwing exception when one stride value is smaller than 1.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setStrides({1, 0});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All strides should be greater than 0.");
  }
  {
    // Test throwing exception when the length of dilations is not 2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({1});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of dilations should be 2.");
  }
  {
    // Test throwing exception when the one dilation value is smaller than 1.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({1, 0});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All dilations should be greater than 0.");
  }
  {
    // Test throwing exception when input_channels % groups() != 0.
    auto* input =
        BuildInput(builder, "input", {1, 4, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(3);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups must evenly divide the input "
              "channels to filter input channels.");
  }
  {
    // Test throwing exception when filter_input_channels != input_channels /
    // groups().
    auto* input =
        BuildInput(builder, "input", {1, 4, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(2);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups must evenly divide the input "
              "channels to filter input channels.");
  }
  {
    // Test throwing exception when the groups is smaller than 1.
    auto* input =
        BuildInput(builder, "input", {1, 4, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(0);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups should be greater than 0.");
  }
  {
    // Test throwing exception due to overflow when calculating the padding
    // along the height dimension.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 23567, 2},
                                 V8MLOperandType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setStrides({193232, 3});
    options->setDilations({232328, 2});
    options->setAutoPad(V8MLAutoPad::Enum::kSameUpper);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Overflow occurred when calculating "
              "the padding along the height dimension.");
  }
  {
    // Test throwing exception due to overflow when calculating the padding
    // along the width dimension.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 28476},
                                 V8MLOperandType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setStrides({1, 284234});
    options->setDilations({1, 434329});
    options->setAutoPad(V8MLAutoPad::Enum::kSameLower);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Overflow occurred when calculating "
              "the padding along the width dimension.");
  }
  {
    // Test throwing exception due to overflow when calculating the effective
    // filter height.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 434983, 2},
                                 V8MLOperandType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({328442, 1});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Failed to calculate the output height: The effective filter size is "
        "too large.");
  }
  {
    // Test throwing exception due to overflow when calculating the effective
    // filter width.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 234545},
                                 V8MLOperandType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({2, 843452});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Failed to calculate the output width: The effective filter size is "
        "too large.");
  }
  {
    // Test throwing exception due to underflow when calculating the output
    // height.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 4, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({4, 1});
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output height: The input size is too "
              "small to fill the window.");
  }
  {
    // Test throwing exception due to underflow when calculating the output
    // width.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 8}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({1, 4});
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output width: The input size is too "
              "small to fill the window.");
  }
  {
    // Test throwing exception when the bias is not a 1-D tensor.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* bias = BuildConstant(builder, {1, 2}, V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
    options->setBias(bias);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the bias shape is not equal to
    // [output_channels].
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* bias = BuildConstant(builder, {2}, V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
    options->setBias(bias);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias shape should be [1].");
  }
  {
    // Test throwing exception when the bias type doesn't match input type.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* bias = BuildConstant(builder, {1}, V8MLOperandType::Enum::kInt32,
                               scope.GetExceptionState());
    options->setBias(bias);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias type doesn't match input type.");
  }
}

MLOperand* BuildConvTranspose2d(V8TestingScope& scope,
                                MLGraphBuilder* builder,
                                const MLOperand* input,
                                const MLOperand* filter,
                                const MLConvTranspose2dOptions* options) {
  auto* output = builder->convTranspose2d(input, filter, options,
                                          scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* convTranspose2d = output->Operator();
  EXPECT_NE(convTranspose2d, nullptr);
  EXPECT_EQ(convTranspose2d->Kind(),
            MLOperator::OperatorKind::kConvTranspose2d);
  EXPECT_EQ(convTranspose2d->IsConnected(), true);
  EXPECT_NE(convTranspose2d->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, ConvTranspose2dTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test convTranspose2d with default options.
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    EXPECT_TRUE(options->hasAutoPad());
    EXPECT_EQ(options->autoPad(), V8MLAutoPad::Enum::kExplicit);
    EXPECT_FALSE(options->hasBias());
    EXPECT_FALSE(options->hasDilations());
    EXPECT_FALSE(options->hasActivation());
    EXPECT_TRUE(options->hasFilterLayout());
    EXPECT_EQ(options->filterLayout(),
              V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw);
    EXPECT_TRUE(options->hasInputLayout());
    EXPECT_EQ(options->inputLayout(), V8MLInputOperandLayout::Enum::kNchw);
    EXPECT_TRUE(options->hasGroups());
    EXPECT_EQ(options->groups(), 1u);
    EXPECT_FALSE(options->hasPadding());
    EXPECT_FALSE(options->hasStrides());
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test convTranspose2d with inputLayout="nchw" and filterLayout="hwoi".
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {3, 3, 2, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kHwoi);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 5, 5}));
  }
  {
    // Test convTranspose2d with inputLayout="nchw" and filterLayout="ohwi".
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {2, 3, 3, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 5, 5}));
  }
  {
    // Test convTranspose2d with inputLayout="nhwc" and filterLayout="iohw".
    auto* input =
        BuildInput(builder, "input", {1, 3, 3, 1},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 5, 5, 2}));
  }
  {
    // Test convTranspose2d with inputLayout="nhwc" and filterLayout="hwoi".
    auto* input =
        BuildInput(builder, "input", {1, 3, 3, 1},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {3, 3, 2, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kHwoi);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 5, 5, 2}));
  }
  {
    // Test convTranspose2d with inputLayout="nhwc" and filterLayout="ohwi".
    auto* input =
        BuildInput(builder, "input", {1, 3, 3, 1},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {2, 3, 3, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 5, 5, 2}));
  }
  {
    // Test convTranspose2d with strides=[3, 2], outputSizes=[10, 8].
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputSizes({10, 8});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 10, 8}));
  }
  {
    // Test convTranspose2d with strides=[3, 2], outputPadding=[1, 1].
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputPadding({1, 1});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 10, 8}));
  }
  {
    // Test convTranspose2d with padding=1.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test convTranspose2d with autopad="explicit", strides=2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kExplicit);
    options->setStrides({2, 2});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 7, 7}));
  }
  {
    // Test convTranspose2d with autopad="same-upper".
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kSameUpper);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test convTranspose2d with autopad="same-upper", strides=2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kSameUpper);
    options->setStrides({2, 2});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 6, 6}));
  }
  {
    // Test convTranspose2d with autopad="same-lower".
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kSameLower);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test convTranspose2d with autopad="same-lower", strides=2, padding=[0, 1,
    // 0, 1].
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kSameLower);
    options->setPadding({0, 1, 0, 1});
    options->setStrides({2, 2});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 6, 6}));
  }
  {
    // Test convTranspose2d with strides=2 and padding=1.
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test convTranspose2d with outputSizes and outputPadding. When the output
    // sizes are explicitly specified, the output padding values are ignored.
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputPadding({1, 1});
    options->setOutputSizes({10, 8});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 10, 8}));
  }
  {
    // Test throwing exception if the output operand's number of elements is too
    // large.
    // Set the input and filter dimensions that let the output's number of
    // lements be 2 * SIZE_MAX.
    auto* input =
        BuildInput(builder, "input",
                   {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 8, 1, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid output operand: The number of elements "
              "is too large.");
  }
  {
    // Test throwing exception if the output operand's byte length is too large.
    // Set the dimensions and type of input and filter that let the output's
    // byte length be 4 * SIZE_MAX.
    auto* input =
        BuildInput(builder, "input",
                   {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 4, 1, 1}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid output operand: The byte length is too large.");
  }
  {
    // Test throwing exception when the input is not a 4-D tensor.
    auto* input =
        BuildInput(builder, "input", {1, 5, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input should be a 4-D tensor.");
  }
  {
    // Test throwing exception when the filter is not a 4-D tensor.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The filter should be a 4-D tensor.");
  }
  {
    // Test throwing exception when the filter type doesn't match the input
    // type.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kInt32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The filter type doesn't match the input type.");
  }
  {
    // Test throwing exception when the length of padding is not 4.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({2, 2});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of padding should be 4.");
  }
  {
    // Test throwing exception when the length of strides is not 2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({2});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of strides should be 2.");
  }
  {
    // Test throwing exception when one stride value is smaller than 1.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({1, 0});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All strides should be greater than 0.");
  }
  {
    // Test throwing exception when the length of dilations is not 2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setDilations({1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of dilations should be 2.");
  }
  {
    // Test throwing exception when the one dilation value is smaller than 1.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setDilations({1, 0});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All dilations should be greater than 0.");
  }
  {
    // Test throwing exception when input_channels % groups() != 0.
    auto* input =
        BuildInput(builder, "input", {1, 4, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setGroups(3);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups must evenly divide the input "
              "channels to filter input channels.");
  }
  {
    // Test throwing exception when filter_input_channels != input_channels /
    // groups().
    auto* input =
        BuildInput(builder, "input", {1, 4, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setGroups(2);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups must evenly divide the input "
              "channels to filter input channels.");
  }
  {
    // Test throwing exception when the groups is smaller than 1.
    auto* input =
        BuildInput(builder, "input", {1, 4, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setGroups(0);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups should be greater than 0.");
  }
  {
    // Test throwing exception due to overflow when calculating the padding
    // along the height dimension.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 23567, 2},
                                 V8MLOperandType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({193232, 3});
    options->setDilations({232328, 2});
    options->setAutoPad(V8MLAutoPad::Enum::kSameUpper);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Overflow occurred when calculating "
              "the padding along the height dimension.");
  }
  {
    // Test throwing exception due to overflow when calculating the padding
    // along the width dimension.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 28476},
                                 V8MLOperandType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({1, 284234});
    options->setDilations({1, 434329});
    options->setAutoPad(V8MLAutoPad::Enum::kSameLower);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Overflow occurred when calculating "
              "the padding along the width dimension.");
  }
  {
    // Test throwing exception due to overflow when calculating the effective
    // filter height.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 434983, 2},
                                 V8MLOperandType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setDilations({328442, 1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Failed to calculate the output height: The effective filter size is "
        "too large.");
  }
  {
    // Test throwing exception due to overflow when calculating the effective
    // filter width.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 234545},
                                 V8MLOperandType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setDilations({2, 843452});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Failed to calculate the output width: The effective filter size is "
        "too large.");
  }
  {
    // Test throwing exception when the bias is not a 1-D tensor.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* bias = BuildConstant(builder, {1, 2}, V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the bias shape is not equal to
    // [output_channels].
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* bias = BuildConstant(builder, {2}, V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias shape should be [1].");
  }
  {
    // Test throwing exception when the bias type doesn't match input type.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* bias = BuildConstant(builder, {1}, V8MLOperandType::Enum::kInt32,
                               scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias type doesn't match input type.");
  }
  {
    // Test throwing exception when the outputPadding is not a sequence of
    // length 2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputPadding({1, 1, 1, 1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of outputPadding should be 2.");
  }
  {
    // Test throwing exception when the outputPadding is greater than stride
    // along the same dimension.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({0, 0, 3, 3});
    options->setStrides({2, 2});
    options->setOutputPadding({0, 2});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The output padding must be smaller than the stride along the "
              "same dimension.");
  }
  {
    // Test throwing exception when the outputSizes is not a sequence of
    // length 2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 2, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputSizes({1, 2, 10, 8});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of outputSizes should be 2.");
  }
  {
    // Test throwing exception due to underflow when calculating the output
    // height.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({4, 4, 0, 0});
    options->setStrides({2, 2});
    options->setOutputPadding({1, 0});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output height: The stride is too large "
              "or the input size is to small for padding.");
  }
  {
    // Test throwing exception due to outputSizes values are smaller than the
    // output sizes calculated by not using outputPadding.
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    options->setOutputSizes({4, 4});
    options->setOutputPadding({1, 1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The height of output sizes is invalid.");
  }
  {
    // Test throwing exception due to outputSizes values are greater than the
    // output sizes calculated by not using outputPadding.
    auto* input =
        BuildInput(builder, "input", {1, 1, 3, 3},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    options->setOutputSizes({6, 8});
    options->setOutputPadding({1, 1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The width of output sizes is invalid.");
  }
}

MLOperand* BuildPool2d(V8TestingScope& scope,
                       MLGraphBuilder* builder,
                       Pool2dKind kind,
                       const MLOperand* input,
                       const MLPool2dOptions* options) {
  MLOperand* output = nullptr;
  switch (kind) {
    case Pool2dKind::kAverage:
      output =
          builder->averagePool2d(input, options, scope.GetExceptionState());
      break;
    case Pool2dKind::kMax:
      output = builder->maxPool2d(input, options, scope.GetExceptionState());
      break;
  }
  return output;
}

void CheckPool2dOutput(const MLOperand* input,
                       const MLOperand* output,
                       Pool2dKind kind) {
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* pool2d = output->Operator();
  EXPECT_NE(pool2d, nullptr);
  switch (kind) {
    case Pool2dKind::kAverage:
      EXPECT_EQ(pool2d->Kind(), MLOperator::OperatorKind::kAveragePool2d);
      break;
    case Pool2dKind::kMax:
      EXPECT_EQ(pool2d->Kind(), MLOperator::OperatorKind::kMaxPool2d);
      break;
  }
  EXPECT_EQ(pool2d->IsConnected(), true);
  EXPECT_NE(pool2d->Options(), nullptr);
}

TEST_F(MLGraphBuilderTest, Pool2dTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  const auto Pool2dKinds = {Pool2dKind::kAverage, Pool2dKind::kMax};
  for (const auto pool2d_kind : Pool2dKinds) {
    {
      // Test pool2d with default options.
      auto* input = BuildInput(builder, "input", {1, 3, 4, 4},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      EXPECT_TRUE(options->hasAutoPad());
      EXPECT_EQ(options->autoPad(), V8MLAutoPad::Enum::kExplicit);
      EXPECT_FALSE(options->hasWindowDimensions());
      EXPECT_FALSE(options->hasPadding());
      EXPECT_FALSE(options->hasStrides());
      EXPECT_FALSE(options->hasDilations());
      EXPECT_TRUE(options->hasLayout());
      EXPECT_EQ(options->layout(), V8MLInputOperandLayout::Enum::kNchw);
      EXPECT_TRUE(options->hasRoundingType());
      EXPECT_EQ(options->roundingType(), V8MLRoundingType::Enum::kFloor);
      EXPECT_FALSE(options->hasOutputSizes());
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 1, 1}));
    }
    {
      // Test pool2d without padding.
      auto* input = BuildInput(builder, "input", {1, 3, 4, 4},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 2, 2}));
    }
    {
      // Test pool2d with padding=2.
      auto* input = BuildInput(builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({5, 5});
      options->setPadding({2, 2, 2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 5, 5}));
    }
    {
      // Test pool2d with autoPad="same-upper".
      auto* input = BuildInput(builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({5, 5});
      options->setAutoPad(V8MLAutoPad::Enum::kSameUpper);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 5, 5}));
    }
    {
      // Test pool2d with autoPad="same-lower".
      auto* input = BuildInput(builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({5, 5});
      options->setAutoPad(V8MLAutoPad::Enum::kSameLower);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 5, 5}));
    }
    {
      // Test pool2d with strides=2.
      auto* input = BuildInput(builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({2, 2});
      options->setStrides({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 2, 2}));
    }
    {
      // Test pool2d with strides=2 and padding=1.
      auto* input = BuildInput(builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2 and asymmetric padding.
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({2, 1, 2, 1});
      options->setStrides({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 4}));
    }
    {
      // Test pool2d with strides=2, padding=1 and roundingType="floor".
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setRoundingType(V8MLRoundingType::Enum::kFloor);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2, padding=1 and roundingType="ceil".
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setRoundingType(V8MLRoundingType::Enum::kCeil);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 4}));
    }
    {
      // Test pool2d with strides=2, padding=1 and outputSizes=[3, 3].
      // When the output sizes are explicitly specified, the
      // options.roundingType is ignored.
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setRoundingType(V8MLRoundingType::Enum::kCeil);
      options->setOutputSizes({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2, padding=1 and outputSizes=[3, 3].
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setOutputSizes({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2, padding=1 and outputSizes=[4, 4].
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setOutputSizes({4, 4});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 4}));
    }
    {
      // Test pool2d with layout="nchw".
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      options->setLayout(V8MLInputOperandLayout::Enum::kNchw);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3, 3}));
    }
    {
      // Test pool2d with layout="nhwc".
      auto* input = BuildInput(builder, "input", {1, 5, 5, 2},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 2}));
    }
    {
      // Test throwing exception if the output operand's byte length is too
      // large.
      // Set the type and sizes of input, padding and window that let the output
      // operands' byte length be greater than SIZE_MAX.
      auto* input = BuildInput(
          builder, "input",
          {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
          V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({1, 1});
      options->setPadding({2, 2, 2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "Invalid output operand: The byte length is too large.");
    }
    {
      // Test throwing exception when the input is not a 4-D tensor.
      auto* input = BuildInput(builder, "input", {1, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The input should be a 4-D tensor.");
    }
    {
      // Test throwing exception when the output size is incorrect.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({2, 2});
      options->setPadding({2, 2, 2, 2});
      options->setStrides({2, 2});
      options->setOutputSizes({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The output sizes should be either [4, 4] or [5, 5].");
    }
    {
      // Test throwing exception when the length of output size is not 2.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({2, 2});
      options->setPadding({2, 2, 2, 2});
      options->setStrides({2, 2});
      options->setOutputSizes({1, 2, 4, 4});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of output sizes should be 2.");
    }
    {
      // Test throwing exception when the length of window dimensions is not 2.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({1, 1, 1, 1});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of window dimensions should be 2.");
    }
    {
      // Test throwing exception when not all window dimensions is greater than
      // or equal to 1.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({0, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "All window dimensions should be greater than 0.");
    }
    {
      // Test throwing exception when the input height is too small to fill the
      // pool window height.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({8, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "Failed to calculate the output height: The input size is too "
                "small to fill the window.");
    }
    {
      // Test throwing exception when the input width is too small to fill the
      // pool window width.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({2, 8});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "Failed to calculate the output width: The input size is too "
                "small to fill the window.");
    }
    {
      // Test throwing exception when the calculated output height is equal to
      // 0.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({6, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "Invalid output operand: All dimensions should be positive.");
    }
    {
      // Test throwing exception when the length of padding is not 4.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setPadding({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of padding should be 4.");
    }
    {
      // Test throwing exception when the length of strides is not 2.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setStrides({2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of strides should be 2.");
    }
    {
      // Test throwing exception when one stride value is smaller than 1.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setStrides({0, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "All strides should be greater than 0.");
    }
    {
      // Test throwing exception when the length of dilations is not 2.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setDilations({1, 1, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of dilations should be 2.");
    }
    {
      // Test throwing exception when one dilation value is smaller than 1.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setDilations({1, 0});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "All dilations should be greater than 0.");
    }
  }
}

TEST_F(MLGraphBuilderTest, PReluTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building prelu when slope_shape is the same as the input_shape.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input =
        BuildInput(builder, "input", input_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* slope =
        BuildConstant(builder, {3, 2, 5}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* p_relu = output->Operator();
    EXPECT_NE(p_relu, nullptr);
    EXPECT_EQ(p_relu->Kind(), MLOperator::OperatorKind::kPRelu);
    EXPECT_EQ(p_relu->IsConnected(), true);
    EXPECT_EQ(p_relu->Options(), nullptr);
  }
  {
    // Test building prelu with input_shape = {3, 2, 5} and slope_shape = {5}.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input =
        BuildInput(builder, "input", input_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* slope = BuildConstant(builder, {5}, V8MLOperandType::Enum::kFloat32,
                                scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* p_relu = output->Operator();
    EXPECT_NE(p_relu, nullptr);
    EXPECT_EQ(p_relu->Kind(), MLOperator::OperatorKind::kPRelu);
    EXPECT_EQ(p_relu->IsConnected(), true);
    EXPECT_EQ(p_relu->Options(), nullptr);
  }
  {
    // Test building prelu with input_shape = {3, 2, 5} and slope_shape = {2,
    // 5}.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input =
        BuildInput(builder, "input", input_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* slope =
        BuildConstant(builder, {2, 5}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* p_relu = output->Operator();
    EXPECT_NE(p_relu, nullptr);
    EXPECT_EQ(p_relu->Kind(), MLOperator::OperatorKind::kPRelu);
    EXPECT_EQ(p_relu->IsConnected(), true);
    EXPECT_EQ(p_relu->Options(), nullptr);
  }
  {
    // Test building prelu with input_shape = {3, 2, 5} and slope_shape = {2}.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input =
        BuildInput(builder, "input", input_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* slope = BuildConstant(builder, {2}, V8MLOperandType::Enum::kFloat32,
                                scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ("The shape of slope is not broadcastable to the shape of input.",
              scope.GetExceptionState().Message());
  }
  {
    // Test building prelu with input_shape = {5, 1, 2} and slope_shape = {2,
    // 2}.
    Vector<uint32_t> input_shape({5, 1, 2});
    auto* input =
        BuildInput(builder, "input", input_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* slope =
        BuildConstant(builder, {2, 2}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ("The shape of slope is not broadcastable to the shape of input.",
              scope.GetExceptionState().Message());
  }
  {
    // Test building prelu with input_type = float and slope_type = int32.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input =
        BuildInput(builder, "input", input_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* slope = BuildConstant(builder, {5}, V8MLOperandType::Enum::kInt32,
                                scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ("The type of slope doesn't match the type of input.",
              scope.GetExceptionState().Message());
  }
  {
    // Test building prelu with input_type = int32.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input =
        BuildInput(builder, "input", input_shape, V8MLOperandType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* slope = BuildConstant(builder, {5}, V8MLOperandType::Enum::kInt32,
                                scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        "The type of input and slope must be one of the floating point types.",
        scope.GetExceptionState().Message());
  }
}

TEST_F(MLGraphBuilderTest, ReluTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building relu with float32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input =
        BuildInput(builder, "input", input_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* output = builder->relu(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* relu = output->Operator();
    EXPECT_NE(relu, nullptr);
    EXPECT_EQ(relu->Kind(), MLOperator::OperatorKind::kRelu);
    EXPECT_EQ(relu->IsConnected(), true);
    EXPECT_EQ(relu->Options(), nullptr);
  }
  {
    // Test building relu with int32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input =
        BuildInput(builder, "input", input_shape, V8MLOperandType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* output = builder->relu(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kInt32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* relu = output->Operator();
    EXPECT_NE(relu, nullptr);
    EXPECT_EQ(relu->Kind(), MLOperator::OperatorKind::kRelu);
    EXPECT_EQ(relu->IsConnected(), true);
    EXPECT_EQ(relu->Options(), nullptr);
  }
  {
    // Test building relu operator.
    auto* relu = builder->relu(scope.GetExceptionState());
    EXPECT_NE(relu, nullptr);
    EXPECT_NE(relu->Operator(), nullptr);
    EXPECT_EQ(relu->Operator()->Kind(), MLOperator::OperatorKind::kRelu);
    EXPECT_EQ(relu->Operator()->IsConnected(), false);
    EXPECT_EQ(relu->Operator()->Options(), nullptr);
  }
}

TEST_F(MLGraphBuilderTest, HardSwishTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  ASSERT_NE(nullptr, builder);
  {
    // Test building hard-swish with float32 input.
    auto* input =
        BuildInput(builder, "input", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->hardSwish(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({3, 4, 5}));
    auto* hard_swish = output->Operator();
    EXPECT_NE(hard_swish, nullptr);
    EXPECT_EQ(hard_swish->Kind(), MLOperator::OperatorKind::kHardSwish);
    EXPECT_EQ(hard_swish->IsConnected(), true);
    EXPECT_EQ(hard_swish->Options(), nullptr);
  }
  {
    // Test throwing exception when building hard-swish with int32 input.
    auto* input =
        BuildInput(builder, "input", {3, 4, 5}, V8MLOperandType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* output = builder->hardSwish(input, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ("The input type must be one of the floating point types.",
              scope.GetExceptionState().Message());
  }
  {
    // Test building hard-swish as a standalone operator.
    auto* hard_swish = builder->hardSwish(scope.GetExceptionState());
    EXPECT_NE(hard_swish, nullptr);
    EXPECT_NE(hard_swish->Operator(), nullptr);
    EXPECT_EQ(hard_swish->Operator()->Kind(),
              MLOperator::OperatorKind::kHardSwish);
    EXPECT_EQ(hard_swish->Operator()->IsConnected(), false);
    EXPECT_EQ(hard_swish->Operator()->Options(), nullptr);
  }
}

MLOperand* BuildGemm(V8TestingScope& scope,
                     MLGraphBuilder* builder,
                     const MLOperand* a,
                     const MLOperand* b,
                     const MLGemmOptions* options) {
  auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), a->Type());
  auto* gemm = output->Operator();
  EXPECT_NE(gemm, nullptr);
  EXPECT_EQ(gemm->Kind(), MLOperator::OperatorKind::kGemm);
  EXPECT_EQ(gemm->IsConnected(), true);
  EXPECT_NE(gemm->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, GemmTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  ASSERT_NE(nullptr, builder);
  {
    // Test building gemm with default option.
    auto* a = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {3, 4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    EXPECT_FALSE(options->hasC());
    EXPECT_TRUE(options->hasAlpha());
    EXPECT_EQ(options->alpha(), 1);
    EXPECT_TRUE(options->hasBeta());
    EXPECT_EQ(options->beta(), 1);
    EXPECT_TRUE(options->hasATranspose());
    EXPECT_EQ(options->aTranspose(), false);
    EXPECT_TRUE(options->hasBTranspose());
    EXPECT_EQ(options->bTranspose(), false);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with two matrices - {2, 3} and {2, 4} that can't be
    // multiplied together due to incompatible dimensions.
    auto* a = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {2, 4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of columns (3) in the first matrix isn't equal to the "
        "number of rows (2) in the second matrix.");
  }
  {
    // Test building gemm with aTranspose = true.
    // Transposed a_dimensions would be {3, 2} and it's compatible with
    // b_dimensions {2, 4}.
    auto* a = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {2, 4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    options->setATranspose(true);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({3, 4}));
  }
  {
    // Test building gemm with aTranspose = true.
    // Transposed a_dimensions would be {3, 2} and it can't be multiplied with
    // b_dimensions {3, 4}.
    auto* a = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {3, 4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    options->setATranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of columns (2) in the transposed first matrix isn't equal "
        "to the number of rows (3) in the second matrix.");
  }
  {
    // Test building gemm with bTranspose = true.
    // Transposed b_dimensions would be {3, 4} and it's compatible with
    // a_dimensions {2, 3}.
    auto* a = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {4, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    options->setBTranspose(true);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with bTranspose = true.
    // Transposed b_dimensions would be {4, 3} and it's incompatible with
    // a_dimensions {2, 3}.
    auto* a = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {3, 4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    options->setBTranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of columns (3) in the first matrix isn't equal to the "
        "number of rows (4) in the transposed second matrix.");
  }
  {
    // Test building gemm with a_dimensions = {2, 3, 1}.
    // Test throwing an error due to input_a is not a 2-D tensor.
    auto* a =
        BuildInput(builder, "a", {2, 3, 1}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {2, 4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The first input must be a 2-D tensor.");
  }
  {
    // Test building gemm with two mismatching input types.
    auto* a = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {3, 4}, V8MLOperandType::Enum::kInt32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The types of first two inputs don't match.");
  }
  {
    // Test building gemm with setting optional input C.
    // The output dimensions of a * b would be {2, 4} and
    // c_dimensions {4} is able to broadcast to {2, 4}.
    auto* a = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {3, 4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* c = BuildInput(builder, "c", {4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    options->setC(c);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // The output dimensions of a * b would be {2, 4} and
    // c_dimension {2, 3} is incompatible with {2, 4}.
    auto* a = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {3, 4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* c = BuildInput(builder, "a", {2, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    options->setC(c);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The third input tensor isn't unidirectionally broadcastable to "
              "the output tensor.");
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // Set optional input C with type = int32 and it mismatches with input
    // type float32.
    auto* a = BuildInput(builder, "a", {3, 2}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {4, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* c = BuildInput(builder, "c", {2, 4}, V8MLOperandType::Enum::kInt32,
                         scope.GetExceptionState());
    options->setC(c);
    options->setATranspose(true);
    options->setBTranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The third input type doesn't match other inputs' type.");
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // Set optional input C with dimensions = {2, 3, 4} and an error should be
    // thrown since c_dimensions is not a 2-D tensor.
    auto* a = BuildInput(builder, "a", {3, 2}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {4, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* c =
        BuildInput(builder, "c", {2, 3, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    options->setC(c);
    options->setATranspose(true);
    options->setBTranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The third input tensor should be either a scalar or a 2-D tensor.");
  }
  {
    // Test throwing exception if the output operand's byte length is too large.
    // Set the type and dimensions of inputs that let the output operand's byte
    // length be 4 * SIZE_MAX.
    auto* a =
        BuildInput(builder, "a", {kSquareRootOfSizeMax, 2},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {2, kSquareRootOfSizeMax},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* output =
        builder->gemm(a, b, MLGemmOptions::Create(), scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid output operand: The byte length is too large.");
  }
}

MLOperand* BuildElementWiseBinary(V8TestingScope& scope,
                                  MLGraphBuilder* builder,
                                  ElementWiseBinaryKind kind,
                                  const MLOperand* a,
                                  const MLOperand* b) {
  MLOperand* output = nullptr;
  switch (kind) {
    case ElementWiseBinaryKind::kAdd:
      output = builder->add(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kSub:
      output = builder->sub(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kMul:
      output = builder->mul(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kDiv:
      output = builder->div(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kMin:
      output = builder->min(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kMax:
      output = builder->max(a, b, scope.GetExceptionState());
      break;
  }
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), a->Type());
  auto* op = output->Operator();
  EXPECT_NE(op, nullptr);
  switch (kind) {
    case ElementWiseBinaryKind::kAdd:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kAdd);
      break;
    case ElementWiseBinaryKind::kSub:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kSub);
      break;
    case ElementWiseBinaryKind::kMul:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kMul);
      break;
    case ElementWiseBinaryKind::kDiv:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kDiv);
      break;
    case ElementWiseBinaryKind::kMin:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kMin);
      break;
    case ElementWiseBinaryKind::kMax:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kMax);
      break;
  }
  EXPECT_EQ(op->IsConnected(), true);
  return output;
}

TEST_F(MLGraphBuilderTest, ElementWiseBinaryTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Testing building add with two input dimensions - {8, 1, 6, 1} and {7, 1,
    // 5}. Both the a and b dimensions have axes with length one that are
    // expanded to a larger size during the broadcast operation.
    // a_dimensions     (4d) 8 * 1 * 6 * 1
    // b_dimensions     (3d)     7 * 1 * 5
    // output_dimenions (4d) 8 * 7 * 6 * 5
    auto* a =
        BuildInput(builder, "a", {8, 1, 6, 1}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {7, 1, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = BuildElementWiseBinary(scope, builder,
                                          ElementWiseBinaryKind::kAdd, a, b);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({8, 7, 6, 5}));
  }
  {
    // Testing building add with two input dimensions - {4, 2, 1} and {4}.
    // a_dimensions     (3d) 4 * 2 * 1
    // b_dimensions     (1d)         4
    // output_dimenions (3d) 4 * 2 * 4
    auto* a =
        BuildInput(builder, "a", {4, 2, 1}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* output = BuildElementWiseBinary(scope, builder,
                                          ElementWiseBinaryKind::kAdd, a, b);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({4, 2, 4}));
  }
  {
    // Test throwing exception when the input shapes are not broadcastable.
    auto* a = BuildInput(builder, "a", {4, 2}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* output = builder->sub(a, b, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input shapes are not broadcastable.");
  }
  {
    // Test throwing exception when the input types don't match.
    auto* a = BuildInput(builder, "a", {4, 2}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {1}, V8MLOperandType::Enum::kInt32,
                         scope.GetExceptionState());
    auto* output = builder->max(a, b, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input types don't match.");
  }
}

TEST_F(MLGraphBuilderTest, ReshapeTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building reshape with new shape = {3, null}.
    auto* input =
        BuildInput(builder, "input", {2, 3, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output =
        builder->reshape(input, {3, absl::nullopt}, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({3, 8}));
    auto* reshape = output->Operator();
    EXPECT_NE(reshape, nullptr);
    EXPECT_EQ(reshape->Kind(), MLOperator::OperatorKind::kReshape);
    EXPECT_EQ(reshape->IsConnected(), true);
  }
  {
    // Test building reshape with new shape = {null}, src shape = {2, 3, 4}.
    auto* input =
        BuildInput(builder, "input", {2, 3, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output =
        builder->reshape(input, {absl::nullopt}, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({24}));
    auto* reshape = output->Operator();
    EXPECT_NE(reshape, nullptr);
    EXPECT_EQ(reshape->Kind(), MLOperator::OperatorKind::kReshape);
    EXPECT_EQ(reshape->IsConnected(), true);
  }
  {
    // Test building reshape with new shape = {null}, src shape = {1}.
    auto* input =
        BuildInput(builder, "input", {1}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output =
        builder->reshape(input, {absl::nullopt}, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1}));
    auto* reshape = output->Operator();
    EXPECT_NE(reshape, nullptr);
    EXPECT_EQ(reshape->Kind(), MLOperator::OperatorKind::kReshape);
    EXPECT_EQ(reshape->IsConnected(), true);
  }
  {
    // Test throwing error when one value of new shape is 0.
    auto* input =
        BuildInput(builder, "input", {2, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->reshape(input, {2, absl::nullopt, 0},
                                    scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The value of new shape should not be 0.");
  }
  {
    // Setting new shape = {}.
    // Test throwing error since the number of elements implied by new shape is
    // not equal to the number of elements in the input tensor.
    auto* input =
        BuildInput(builder, "input", {2, 3, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->reshape(input, {}, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The number of elements (1) implied by new shape doesn't match "
              "the number of elements (24) in the input tensor.");
  }
  {
    // Test throwing error when more than one components of new_shape are null.
    auto* input =
        BuildInput(builder, "input", {2, 3, 1}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->reshape(input, {6, absl::nullopt, absl::nullopt},
                                    scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Only one component of new shape can be null.");
  }
  {
    // Test throwing error since the number of elements (9) of the input tensor
    // can't be divided evenly by the number of elements (2) implied by the new
    // shape.
    auto* input =
        BuildInput(builder, "input", {3, 3}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output =
        builder->reshape(input, {2, absl::nullopt}, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The number of elements (9) in the input tensor can't be "
              "divided evenly by the number of elements (2) implied by new "
              "shape.");
  }
}

MLOperand* BuildResample2d(V8TestingScope& scope,
                           MLGraphBuilder* builder,
                           const MLOperand* input,
                           const MLResample2dOptions* options) {
  auto* output = builder->resample2d(input, options, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* resample2d = output->Operator();
  EXPECT_NE(resample2d, nullptr);
  EXPECT_EQ(resample2d->Kind(), MLOperator::OperatorKind::kResample2d);
  EXPECT_EQ(resample2d->IsConnected(), true);
  EXPECT_NE(resample2d->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, Resample2dTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building resample2d with default options.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    EXPECT_TRUE(options->hasMode());
    EXPECT_EQ(options->mode(), V8MLInterpolationMode::Enum::kNearestNeighbor);
    EXPECT_FALSE(options->hasScales());
    EXPECT_FALSE(options->hasSizes());
    EXPECT_FALSE(options->hasAxes());
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 2, 4}));
  }
  {
    // Test building resample2d with scales = {2.0, 2.0}.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({2.0, 2.0});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 4, 8}));
  }
  {
    // Test building resample2d with scales = {0.5, 0.5}.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({0.5, 0.5});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 2, 2}));
  }
  {
    // Test building resample2d with sizes = {3, 6}.
    // When the target sizes are specified, scales argument is
    // ignored.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({2.0, 2.0});
    options->setSizes({3, 6});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 6}));
  }
  {
    // Test building resample2d with scales = {1.0, 2.0} and axes = {0, 1}.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({1.0, 2.0});
    options->setAxes({0, 1});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 2, 4}));
  }
  {
    // Test building resample2d with scales = {2.0, 2.0} and axes = {1, 2}.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({2.0, 2.0});
    options->setAxes({1, 2});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 4, 4}));
  }
  {
    // Test throwing error when the input is not a 4-D tensor.
    auto* input =
        BuildInput(builder, "input", {2, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input must be a 4-D tensor.");
  }
  {
    // Test throwing error when the length of scales is not 2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({1.0, 1.0, 2.0, 2.0});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of scales should be 2.");
  }
  {
    // Test throwing error when the scale is negative.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({1.0, -2.0});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All scales should be greater than 0.");
  }
  {
    // Test throwing error when the length of sizes is not 2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setSizes({1, 1, 4, 6});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of sizes should be 2.");
  }
  {
    // Test throwing error when the scale height is too large.
    auto* input =
        BuildInput(builder, "input", {1, 1, 34902, 23243},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({232433, 4});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output height: The scale is too large.");
  }
  {
    // Test throwing error when the scale height is too small.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({0.02, 0.8});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output height: The scale is too small.");
  }
  {
    // Test throwing error when the scale width is too large.
    auto* input =
        BuildInput(builder, "input", {1, 1, 34902, 23243},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({20, 434324});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output width: The scale is too large.");
  }
  {
    // Test throwing error when the scale width is too small.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({0.7, 0.1});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output width: The scale is too small.");
  }
  {
    // Test throwing error when the length of axes is not 2.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setAxes({0, 1, 2});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of axes should be 2.");
  }
  {
    // Test throwing error when the values of axes are inconsecutive.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setAxes({0, 2});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The values of axes are invalid.");
  }
}

MLOperand* BuildTranspose(V8TestingScope& scope,
                          MLGraphBuilder* builder,
                          const MLOperand* input,
                          const MLTransposeOptions* options) {
  auto* output = builder->transpose(input, options, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* transpose = output->Operator();
  EXPECT_NE(transpose, nullptr);
  EXPECT_EQ(transpose->Kind(), MLOperator::OperatorKind::kTranspose);
  EXPECT_EQ(transpose->IsConnected(), true);
  EXPECT_NE(transpose->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, TransposeTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building transpose with default options.
    auto* input =
        BuildInput(builder, "input", {1, 2, 3, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* output = BuildTranspose(scope, builder, input);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({4, 3, 2, 1}));
  }
  {
    // Test building transpose with permutation = {0, 2, 3, 1}.
    auto* input =
        BuildInput(builder, "input", {1, 2, 3, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLTransposeOptions::Create();
    options->setPermutation({0, 2, 3, 1});
    auto* output = BuildTranspose(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 2}));
  }
  {
    // Test throwing error when the number of values in permutation is not the
    // same as the rank of the input tensor.
    auto* input =
        BuildInput(builder, "input", {1, 2, 4}, V8MLOperandType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* options = MLTransposeOptions::Create();
    options->setPermutation({0, 2, 3, 1});
    auto* output =
        builder->transpose(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of values in permutation must be the same as the rank "
        "of the input tensor.");
  }
  {
    // Test throwing error when two values in permutation are same.
    auto* input =
        BuildInput(builder, "input", {1, 2, 3, 4},
                   V8MLOperandType::Enum::kInt32, scope.GetExceptionState());
    auto* options = MLTransposeOptions::Create();
    options->setPermutation({0, 2, 3, 2});
    auto* output =
        builder->transpose(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Two or more values are same in the permutation sequence.");
  }
  {
    // Test throwing error when one value in permutation is greater than
    // input_rank-1.
    auto* input =
        BuildInput(builder, "input", {1, 2, 3, 4},
                   V8MLOperandType::Enum::kInt32, scope.GetExceptionState());
    auto* options = MLTransposeOptions::Create();
    options->setPermutation({0, 1, 2, 4});
    auto* output =
        builder->transpose(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The values in permutation must be within the range from 0 to (3).");
  }
}

MLOperand* BuildClamp(V8TestingScope& scope,
                      MLGraphBuilder* builder,
                      const MLOperand* input,
                      const MLClampOptions* options) {
  auto* output = builder->clamp(input, options, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* clamp = output->Operator();
  EXPECT_NE(clamp, nullptr);
  EXPECT_EQ(clamp->Kind(), MLOperator::OperatorKind::kClamp);
  EXPECT_EQ(clamp->IsConnected(), true);
  EXPECT_NE(clamp->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, ClampTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building clamp with default options.
    auto* input =
        BuildInput(builder, "input", {2, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLClampOptions::Create();
    EXPECT_FALSE(options->hasMaxValue());
    EXPECT_FALSE(options->hasMinValue());
    auto* output = BuildClamp(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building clamp with max value = 0 and min value = 0.
    auto* input =
        BuildInput(builder, "input", {1, 2, 2, 7},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* options = MLClampOptions::Create();
    options->setMaxValue(0);
    options->setMinValue(0);
    auto* output = BuildClamp(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 2, 7}));
  }
  {
    // Test throwing error when the max value is less than the min value.
    auto* input =
        BuildInput(builder, "input", {2, 4}, V8MLOperandType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* options = MLClampOptions::Create();
    options->setMaxValue(-3.243432);
    options->setMinValue(4.432232);
    auto* output = builder->clamp(input, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The min value (4.432232) should be less than or equal to "
              "the max value (-3.243432).");
  }
  {
    // Test building clamp as a standalone operator.
    auto* clamp =
        builder->clamp(MLClampOptions::Create(), scope.GetExceptionState());
    EXPECT_NE(clamp, nullptr);
    EXPECT_NE(clamp->Operator(), nullptr);
    EXPECT_EQ(clamp->Operator()->Kind(), MLOperator::OperatorKind::kClamp);
    EXPECT_EQ(clamp->Operator()->IsConnected(), false);
    EXPECT_NE(clamp->Operator()->Options(), nullptr);
  }
}

void TestBuildElu(V8TestingScope& scope,
                  MLGraphBuilder* builder,
                  const MLOperand* input,
                  const Vector<uint32_t>& output_shape,
                  const MLEluOptions* options) {
  auto* output = builder->elu(input, options, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  EXPECT_EQ(output->Dimensions(), output_shape);
  auto* elu = output->Operator();
  EXPECT_NE(elu, nullptr);
  EXPECT_EQ(elu->Kind(), MLOperator::OperatorKind::kElu);
  EXPECT_EQ(elu->IsConnected(), true);
  EXPECT_NE(elu->Options(), nullptr);
}

TEST_F(MLGraphBuilderTest, EluTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building elu with float32 input and default options.
    auto* input =
        BuildInput(builder, "input", {1, 2, 3}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLEluOptions::Create();
    EXPECT_TRUE(options->hasAlpha());
    EXPECT_EQ(options->alpha(), 1.0f);
    TestBuildElu(scope, builder, input, {1, 2, 3}, options);
  }
  {
    // Test building elu with int32 input and alpha = 0.1.
    auto* input =
        BuildInput(builder, "input", {2, 2, 3}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLEluOptions::Create();
    options->setAlpha(0.1);
    TestBuildElu(scope, builder, input, {2, 2, 3}, options);
  }
  {
    // Test building elu with int32 input.
    auto* input =
        BuildInput(builder, "input", {2, 2, 3}, V8MLOperandType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* output =
        builder->elu(input, MLEluOptions::Create(), scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The type of input must be one of the floating point types.");
  }
  {
    // Test building elu as a standalone operator.
    auto* elu = builder->elu(MLEluOptions::Create(), scope.GetExceptionState());
    EXPECT_NE(elu, nullptr);
    EXPECT_NE(elu->Operator(), nullptr);
    EXPECT_EQ(elu->Operator()->Kind(), MLOperator::OperatorKind::kElu);
    EXPECT_EQ(elu->Operator()->IsConnected(), false);
    EXPECT_NE(elu->Operator()->Options(), nullptr);
  }
}

MLOperand* BuildLeakyRelu(V8TestingScope& scope,
                          MLGraphBuilder* builder,
                          const MLOperand* input,
                          const MLLeakyReluOptions* options) {
  auto* output = builder->leakyRelu(input, options, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* leaky_relu = output->Operator();
  EXPECT_NE(leaky_relu, nullptr);
  EXPECT_EQ(leaky_relu->Kind(), MLOperator::OperatorKind::kLeakyRelu);
  EXPECT_EQ(leaky_relu->IsConnected(), true);
  EXPECT_NE(leaky_relu->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, LeakyReluTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building leaky_relu with float32 input.
    auto* input =
        BuildInput(builder, "input", {1, 2, 3}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLLeakyReluOptions::Create();
    auto* output = BuildLeakyRelu(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3}));
  }
  {
    // Test building leaky_relu with int32 input.
    auto* input =
        BuildInput(builder, "input", {2, 2, 3}, V8MLOperandType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* options = MLLeakyReluOptions::Create();
    auto* output = BuildLeakyRelu(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 2, 3}));
  }
  {
    // Test building leaky_relu as a standalone operator.
    auto* leaky_relu = builder->leakyRelu(MLLeakyReluOptions::Create(),
                                          scope.GetExceptionState());
    EXPECT_NE(leaky_relu, nullptr);
    EXPECT_NE(leaky_relu->Operator(), nullptr);
    EXPECT_EQ(leaky_relu->Operator()->Kind(),
              MLOperator::OperatorKind::kLeakyRelu);
    EXPECT_EQ(leaky_relu->Operator()->IsConnected(), false);
    EXPECT_NE(leaky_relu->Operator()->Options(), nullptr);
  }
}

MLOperand* BuildPad(V8TestingScope& scope,
                    MLGraphBuilder* builder,
                    const MLOperand* input,
                    const Vector<uint32_t>& beginningPadding,
                    const Vector<uint32_t>& endingPadding,
                    const MLPadOptions* options) {
  auto* output = builder->pad(input, beginningPadding, endingPadding, options,
                              scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* pad = output->Operator();
  EXPECT_NE(pad, nullptr);
  EXPECT_EQ(pad->Kind(), MLOperator::OperatorKind::kPad);
  EXPECT_EQ(pad->IsConnected(), true);
  EXPECT_NE(pad->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, PadTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building pad with default options, beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    auto* input =
        BuildInput(builder, "input", {2, 3}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLPadOptions::Create();
    EXPECT_TRUE(options->hasMode());
    EXPECT_EQ(options->mode(), V8MLPaddingMode::Enum::kConstant);
    EXPECT_TRUE(options->hasValue());
    EXPECT_EQ(options->value(), 0);
    auto* output = BuildPad(scope, builder, input, {1, 2}, {1, 2}, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({4, 7}));
  }
  {
    // Test throwing error when the length of beginningPadding is not equal to
    // the input rank.
    auto* input =
        BuildInput(builder, "input", {2, 3}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLPadOptions::Create();
    options->setMode(V8MLPaddingMode::Enum::kEdge);
    auto* output =
        builder->pad(input, {1}, {1, 2}, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of beginningPadding must be equal to the rank of the "
              "input tensor.");
  }
  {
    // Test throwing error when the length of endingPadding is not equal to the
    // input rank.
    auto* input =
        BuildInput(builder, "input", {2, 3}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLPadOptions::Create();
    options->setMode(V8MLPaddingMode::Enum::kReflection);
    auto* output = builder->pad(input, {1, 0}, {1, 2, 0}, options,
                                scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of endingPadding must be equal to the rank of the "
              "input tensor.");
  }
  {
    // Test throwing error when the padding of one dimension is too large.
    auto* input =
        BuildInput(builder, "input", {2, 3}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLPadOptions::Create();
    options->setMode(V8MLPaddingMode::Enum::kReflection);
    auto* output = builder->pad(input, {2294967295, 0}, {3294967295, 2},
                                options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The padding of dimension (0) is too large.");
  }
}

TEST_F(MLGraphBuilderTest, Softmax) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building softmax with float32 input.
    auto* input =
        BuildInput(builder, "input", {2, 4}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->softmax(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
    auto* softmax = output->Operator();
    EXPECT_NE(softmax, nullptr);
    EXPECT_EQ(softmax->Kind(), MLOperator::OperatorKind::kSoftmax);
    EXPECT_EQ(softmax->IsConnected(), true);
    EXPECT_EQ(softmax->Options(), nullptr);
  }
  {
    // Test throwing exception when building softmax with 4-D input.
    auto* input =
        BuildInput(builder, "input", {1, 1, 2, 4},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* output = builder->softmax(input, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input must be a 2-D tensor.");
  }
  {
    // Test throwing exception when building softmax with int32 input.
    auto* input =
        BuildInput(builder, "input", {3, 4}, V8MLOperandType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* output = builder->softmax(input, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input type must be one of the floating point types.");
  }
}

TEST_F(MLGraphBuilderTest, SigmoidTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test building sigmoid with float32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input =
        BuildInput(builder, "input", input_shape,
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* output = builder->sigmoid(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* sigmoid = output->Operator();
    EXPECT_NE(sigmoid, nullptr);
    EXPECT_EQ(sigmoid->Kind(), MLOperator::OperatorKind::kSigmoid);
    EXPECT_EQ(sigmoid->IsConnected(), true);
    EXPECT_EQ(sigmoid->Options(), nullptr);
  }
  {
    // Test throwing exception when building sigmoid with int32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input =
        BuildInput(builder, "input", input_shape, V8MLOperandType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* output = builder->sigmoid(input, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input type must be one of the floating point types.");
  }
  {
    // Test building sigmoid operator.
    auto* sigmoid = builder->sigmoid(scope.GetExceptionState());
    EXPECT_NE(sigmoid, nullptr);
    EXPECT_NE(sigmoid->Operator(), nullptr);
    EXPECT_EQ(sigmoid->Operator()->Kind(), MLOperator::OperatorKind::kSigmoid);
    EXPECT_EQ(sigmoid->Operator()->IsConnected(), false);
    EXPECT_EQ(sigmoid->Operator()->Options(), nullptr);
  }
}

class FakeMLGraphBackend final : public MLGraph {
 public:
  // Create and build a FakeMLGraphBackend object. Resolve the promise with
  // this concrete object if no errors.
  static void ValidateAndBuildAsync(MLContext* context,
                                    const MLNamedOperands& named_outputs,
                                    ScriptPromiseResolver* resolver) {
    auto* graph = MakeGarbageCollected<FakeMLGraphBackend>(context);
    graph->BuildAsync(named_outputs, resolver);
  }

  // Create and build a FakeMLGraphBackend object synchronously.
  static MLGraph* ValidateAndBuildSync(MLContext* context,
                                       const MLNamedOperands& named_outputs,
                                       ExceptionState& exception_state) {
    return MakeGarbageCollected<FakeMLGraphBackend>(context)->BuildSync(
        named_outputs, exception_state);
  }

  // The constructor shouldn't be called directly. The callers should use
  // ValidateAndBuildAsync() method instead.
  explicit FakeMLGraphBackend(MLContext* context) : MLGraph(context) {}

  ~FakeMLGraphBackend() override = default;

 private:
  // Resolve the promise with this FakeMLGraphBackend object for testing the
  // input and output resources info.
  void BuildAsyncImpl(const MLNamedOperands& named_outputs,
                      ScriptPromiseResolver* resolver) override {
    resolver->Resolve(this);
  }

  // Return this FakeMLGraphBackend object for testing the input and output
  // resources info.
  MLGraph* BuildSyncImpl(const MLNamedOperands& named_outputs,
                         ExceptionState& exception_state) override {
    return this;
  }

  // Resolve the promise for testing the validation of inputs and outputs in
  // MLGraph::ComputeAsync().
  void ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                        const MLNamedArrayBufferViews& outputs,
                        ScriptPromiseResolver* resolver,
                        ExceptionState& exception_state) override {
    resolver->Resolve();
  }

  // Just return for testing the validation of inputs and outputs in
  // MLGraph::ComputeSync().
  void ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                       const MLNamedArrayBufferViews& outputs,
                       ExceptionState& exception_state) override {
    return;
  }
};

FakeMLGraphBackend* ToFakeMLGraphBackend(V8TestingScope* scope,
                                         ScriptValue value) {
  return NativeValueTraits<FakeMLGraphBackend>::NativeValue(
      scope->GetIsolate(), value.V8Value(), scope->GetExceptionState());
}

namespace {

// Helper class to create the FakeMLGraphBackend that is intended to test
// the GraphBuilder validation steps.
class FakeMLGraphBuilderBackend : public MLGraphBuilder::BackendForTesting {
 public:
  void BuildGraphAsyncImpl(MLContext* context,
                           const MLNamedOperands& named_outputs,
                           ScriptPromiseResolver* resolver) override {
    FakeMLGraphBackend::ValidateAndBuildAsync(context, named_outputs, resolver);
  }

  MLGraph* BuildGraphSyncImpl(MLContext* context,
                              const MLNamedOperands& named_outputs,
                              ExceptionState& exception_state) override {
    return FakeMLGraphBackend::ValidateAndBuildSync(context, named_outputs,
                                                    exception_state);
  }
};

}  // namespace

// Helper class to test FakeMLGraphBackend.
class FakeMLGraphTest : public MLGraphTestBase {
 public:
  void SetUp() override {
    // Ensure MLGraphBuilder builds a FakeMLGraphBackend.
    MLGraphBuilder::SetBackendForTesting(&backend_for_testing);
  }

  void TearDown() override { MLGraphBuilder::SetBackendForTesting(nullptr); }

 private:
  FakeMLGraphBuilderBackend backend_for_testing;
};

TEST_P(FakeMLGraphTest, BuildTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test throwing exception if the named outputs is empty.
    MLNamedOperands named_outputs;
    auto [graph, exception] = BuildGraph(scope, builder, named_outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "At least one output needs to be provided.");
  }
  {
    // Test throwing exception if the named output is an input operand.
    auto* input =
        BuildInput(builder, "input", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto [graph, exception] = BuildGraph(scope, builder, {{"output", input}});
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "The operand with name \"output\" is not an output operand.");
  }
  {
    // Test throwing exception if the named output is a constant operand.
    auto* constant =
        BuildConstant(builder, {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto [graph, exception] =
        BuildGraph(scope, builder, {{"output", constant}});
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "The operand with name \"output\" is not an output operand.");
  }
  {
    // Test throwing exception if the named outputs is a mix of input and
    // constant operands.
    auto* input =
        BuildInput(builder, "input", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* constant =
        BuildConstant(builder, {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto [graph, exception] =
        BuildGraph(scope, builder, {{"output1", input}, {"output2", constant}});
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "The operand with name \"output1\" is not an output operand.");
  }
  {
    // Test throwing exception if two inputs have the same name.
    auto* a =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* c = builder->add(a, b, scope.GetExceptionState());
    ASSERT_NE(c, nullptr);

    auto [graph, exception] = BuildGraph(scope, builder, {{"c", c}});
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(), "The input name \"a\" is duplicated.");
  }
  {
    // Test building a graph with an elementwise add operator that uses the same
    // input for both lhs and rhs:
    //   [a]
    //   / \
    //   \ /
    //   add
    //    |
    //   [b]
    auto* a =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->add(a, a, scope.GetExceptionState());
    ASSERT_NE(output, nullptr);
    auto [graph, exception] = BuildGraph(scope, builder, {{"b", output}});
    EXPECT_NE(graph, nullptr);
    const auto& inputs = graph->GetInputResourcesInfo();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(inputs.at("a").type, a->Type());
    EXPECT_EQ(inputs.at("a").byte_length, a->ByteLength());
    const auto& outputs = graph->GetOutputResourcesInfo();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(outputs.at("b").type, output->Type());
    EXPECT_EQ(outputs.at("b").byte_length, output->ByteLength());
  }
  {
    // Test building a graph with two operators sharing a same input:
    //      [a]
    //     /   \
    //  relu   sigmoid
    //    |      |
    //   [b]    [c]
    auto* a =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b = builder->relu(a, scope.GetExceptionState());
    ASSERT_NE(b, nullptr);
    auto* c = builder->sigmoid(a, scope.GetExceptionState());
    ASSERT_NE(c, nullptr);
    auto [graph, exception] = BuildGraph(scope, builder, {{"b", b}, {"c", c}});
    EXPECT_NE(graph, nullptr);
    const auto& inputs = graph->GetInputResourcesInfo();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(inputs.at("a").type, a->Type());
    EXPECT_EQ(inputs.at("a").byte_length, a->ByteLength());
    const auto& outputs = graph->GetOutputResourcesInfo();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(outputs.at("b").type, b->Type());
    EXPECT_EQ(outputs.at("b").byte_length, b->ByteLength());
    EXPECT_EQ(outputs.at("c").type, b->Type());
    EXPECT_EQ(outputs.at("c").byte_length, b->ByteLength());
  }
  {
    // Test building a fake graph with two inputs, one gemm operation and one
    // output.
    auto* a = BuildInput(builder, "a", {3, 4}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {4, 3}, V8MLOperandType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* c = BuildGemm(scope, builder, a, b);

    auto [graph, exception] = BuildGraph(scope, builder, {{"c", c}});
    EXPECT_NE(graph, nullptr);
    const auto& inputs = graph->GetInputResourcesInfo();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(inputs.at("a").type, a->Type());
    EXPECT_EQ(inputs.at("a").byte_length, a->ByteLength());
    EXPECT_EQ(inputs.at("b").type, b->Type());
    EXPECT_EQ(inputs.at("b").byte_length, b->ByteLength());
    const auto& outputs = graph->GetOutputResourcesInfo();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(outputs.at("c").type, c->Type());
    EXPECT_EQ(outputs.at("c").byte_length, c->ByteLength());
  }
  {
    // Test building a fake graph with conv2d, add and relu operations.
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* conv2d = BuildConv2d(scope, builder, input, filter);
    auto* bias = BuildConstant(builder, {1}, V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* add = builder->add(conv2d, bias, scope.GetExceptionState());
    ASSERT_NE(add, nullptr);
    auto* output = builder->relu(add, scope.GetExceptionState());
    ASSERT_NE(output, nullptr);

    auto [graph, exception] = BuildGraph(scope, builder, {{"output", output}});
    EXPECT_NE(graph, nullptr);
    const auto& inputs = graph->GetInputResourcesInfo();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(inputs.at("input").type, input->Type());
    EXPECT_EQ(inputs.at("input").byte_length, input->ByteLength());
    const auto& outputs = graph->GetOutputResourcesInfo();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(outputs.at("output").type, output->Type());
    EXPECT_EQ(outputs.at("output").byte_length, output->ByteLength());
  }
}

// Helper struct to create an ArrayBufferView for MLNamedArrayBufferViews test.
struct ArrayBufferViewInfo {
  size_t number_of_elements;
  V8MLOperandType::Enum type;

  NotShared<DOMArrayBufferView> ToArrayBufferView() {
    return CreateDOMArrayBufferView(number_of_elements, type);
  }
};

// Helper function to create an ArrayBufferView given an operand.
NotShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand) {
  return CreateDOMArrayBufferView(operand->NumberOfElements(), operand->Type());
}

TEST_P(FakeMLGraphTest, ComputeTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  // Build a fake graph represents computation 'c = a * b';
  auto* a = BuildInput(builder, "a", {3, 4}, V8MLOperandType::Enum::kFloat32,
                       scope.GetExceptionState());
  auto* b = BuildInput(builder, "b", {4, 3}, V8MLOperandType::Enum::kFloat32,
                       scope.GetExceptionState());
  auto* c = BuildGemm(scope, builder, a, b);
  auto [graph, build_exception] = BuildGraph(scope, builder, {{"c", c}});
  DCHECK_NE(graph, nullptr);
  DCHECK_EQ(build_exception, nullptr);
  {
    // Test throwing exception if the inputs is empty.
    MLNamedArrayBufferViews inputs;
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid inputs: The number (0) of the array buffer views "
              "doesn't match the expectation (2).");
  }
  {
    // Test throwing exception if the number of inputs doesn't match.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid inputs: The number (1) of the array buffer views "
              "doesn't match the expectation (2).");
  }
  {
    // Test throwing exception if the outputs is empty.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The number (0) of the array buffer views "
              "doesn't match the expectation (1).");
  }
  {
    // Test throwing exception if the number of outputs doesn't match.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    outputs.emplace_back("d", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The number (2) of the array buffer views "
              "doesn't match the expectation (1).");
  }
  {
    // Test throwing exception if the input name is unknown.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("invalid-input-name",
                        CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid inputs: The name \"invalid-input-name\" isn't part of "
              "the graph.");
  }
  {
    // Test throwing exception if the output name is unknown.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("invalid-output-name",
                         CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The name \"invalid-output-name\" isn't part of "
              "the graph.");
  }
  {
    // Test throwing exception if the input array buffer view type is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back(
        "a", ArrayBufferViewInfo{.number_of_elements = 12,
                                 .type = V8MLOperandType::Enum::kInt32}
                 .ToArrayBufferView());
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid inputs: The type (Int32) of the array buffer view with "
              "name \"a\" doesn't match the expected operand type (float32).");
  }
  {
    // Test throwing exception if the input array buffer view size is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back(
        "a", ArrayBufferViewInfo{.number_of_elements = 10,
                                 .type = V8MLOperandType::Enum::kFloat32}
                 .ToArrayBufferView());
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid inputs: The byte length (40) of the array buffer view "
              "with name \"a\" doesn't match the expected byte length (48).");
  }
  {
    // Test throwing exception if the output array buffer view type is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back(
        "c", ArrayBufferViewInfo{.number_of_elements = 9,
                                 .type = V8MLOperandType::Enum::kInt32}
                 .ToArrayBufferView());
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The type (Int32) of the array buffer view with "
              "name \"c\" doesn't match the expected operand type (float32).");
  }
  {
    // Test throwing exception if the output array buffer view size is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back(
        "c", ArrayBufferViewInfo{.number_of_elements = 8,
                                 .type = V8MLOperandType::Enum::kFloat32}
                 .ToArrayBufferView());
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The byte length (32) of the array buffer view "
              "with name \"c\" doesn't match the expected byte length (36).");
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FakeMLGraphTest,
    testing::Combine(::testing::Values(BackendType::kFake),
                     ::testing::Values(ExecutionMode::kAsync,
                                       ExecutionMode::kSync)),
    TestVarietyToString);

}  // namespace blink
