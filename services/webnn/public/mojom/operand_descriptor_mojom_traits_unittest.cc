// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/mojom/operand_descriptor_mojom_traits.h"

#include <array>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/tensor_usage_mojom_traits.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {

webnn::OperandDescriptor CreateInvalidOperandDescriptor() {
  return webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kFloat32, std::array<uint32_t, 1>{0});
}

}  // namespace

TEST(OperandDescriptorMojomTraitsTest, Basic) {
  auto input = webnn::OperandDescriptor::Create(webnn::OperandDataType::kInt32,
                                                std::array<uint32_t, 2>{2, 3});
  ASSERT_TRUE(input.has_value());

  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          *input, output));
  EXPECT_EQ(input, output);
}

TEST(OperandDescriptorMojomTraitsTest, Int4) {
  auto input = webnn::OperandDescriptor::Create(webnn::OperandDataType::kInt4,
                                                std::array<uint32_t, 2>{3, 3});
  ASSERT_TRUE(input.has_value());

  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          *input, output));
  EXPECT_EQ(*input, output);
  // 9 int4 elements are packed in 5 bytes.
  EXPECT_EQ(output.NumberOfElements(), 9u);
  EXPECT_EQ(output.PackedByteLength(), 5u);
}

TEST(OperandDescriptorMojomTraitsTest, EmptyShape) {
  // Descriptors with an empty shape are treated as scalars.
  auto input =
      webnn::OperandDescriptor::Create(webnn::OperandDataType::kInt32, {});
  ASSERT_TRUE(input.has_value());

  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          *input, output));
  EXPECT_EQ(*input, output);
  EXPECT_EQ(output.NumberOfElements(), 1u);
  EXPECT_EQ(output.PackedByteLength(), 4u);  // int32 is 4 bytes.
}

TEST(OperandDescriptorMojomTraitsTest, ZeroDimension) {
  // Descriptors with a zero-length dimension are not supported.
  const std::array<uint32_t, 3> shape{2, 0, 3};

  ASSERT_FALSE(
      webnn::OperandDescriptor::Create(webnn::OperandDataType::kInt32, shape)
          .has_value());

  auto input = webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kInt32, shape);
  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          input, output));
}

TEST(OperandDescriptorMojomTraitsTest, NumberOfElementsTooLarge) {
  // The number of elements overflows the max size_t on all platforms.
  const std::array<uint32_t, 3> shape{2, std::numeric_limits<uint32_t>::max(),
                                      std::numeric_limits<uint32_t>::max()};

  // Using int4 so that the byte length won't overflow the max size_t on 64-bit
  // platforms.
  ASSERT_FALSE(
      webnn::OperandDescriptor::Create(webnn::OperandDataType::kInt4, shape)
          .has_value());

  auto input = webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kInt4, shape);
  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          input, output));
}

TEST(OperandDescriptorMojomTraitsTest, ByteLengthTooLarge) {
  // The number of elements won't overflow the max size_t on 64-bit platforms.
  const std::array<uint32_t, 2> shape{std::numeric_limits<uint32_t>::max() / 2,
                                      std::numeric_limits<uint32_t>::max()};

  // The byte length overflows the max size_t on all platforms.
  ASSERT_FALSE(
      webnn::OperandDescriptor::Create(webnn::OperandDataType::kInt64, shape)
          .has_value());

  auto input = webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kInt64, shape);
  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          input, output));
}

}  // namespace mojo
