// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/tensor_desc.h"

#include "base/check.h"
#include "base/check_op.h"
#include "services/webnn/dml/utils.h"

namespace webnn::dml {

TensorDesc::TensorDesc(DML_TENSOR_DATA_TYPE data_type,
                       std::vector<uint32_t> dimensions)
    : TensorDesc(data_type, DML_TENSOR_FLAG_NONE, std::move(dimensions)) {}

TensorDesc::TensorDesc(DML_TENSOR_DATA_TYPE data_type,
                       DML_TENSOR_FLAGS flags,
                       std::vector<uint32_t> dimensions,
                       std::vector<uint32_t> strides) {
  // DML (as of at least 1.11) requires dimension count to be at least 1 because
  // otherwise validation during operator creation will complain with
  // E_INVALIDARG. So scalars must be conveyed with dimensions = [1].
  dimensions_ =
      dimensions.empty() ? std::vector<uint32_t>{1} : std::move(dimensions);

  if (!strides.empty()) {
    CHECK_EQ(dimensions_.size(), strides.size());
  }

  // If no strides are given, set strides as default value calculated from
  // dimensions, e.g., a tensor with dimensions [1, 2, 3, 4] should have default
  // strides [24, 12, 4, 1], referring to
  // https://docs.microsoft.com/en-us/windows/win32/direct3d12/dml-helper-functions#calculatestrides.
  strides_ =
      strides.empty() ? CalculateStrides(dimensions_) : std::move(strides);

  // Round up to the nearest 4 bytes. The buffer allocation already aligned
  // chunks up to DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT.
  uint64_t minimum_implied_size_in_bytes =
      CalculateDMLBufferTensorSize(data_type, dimensions_, strides_);

  buffer_desc_.DimensionCount = dimensions_.size();
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  buffer_desc_.TotalTensorSizeInBytes = minimum_implied_size_in_bytes;
  buffer_desc_.GuaranteedBaseOffsetAlignment = 0;
  buffer_desc_.DataType = data_type;
  buffer_desc_.Flags = flags;

  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};
}

TensorDesc::TensorDesc(TensorDesc const& other)
    : dimensions_(other.dimensions_),
      strides_(other.strides_),
      buffer_desc_(other.buffer_desc_) {
  // Update the internal pointers to dimensions and strides.
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};
}

TensorDesc::TensorDesc(TensorDesc&& other)
    : dimensions_(std::move(other.dimensions_)),
      strides_(std::move(other.strides_)),
      buffer_desc_(std::move(other.buffer_desc_)) {
  // Update the internal pointers to dimensions and strides.
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};
}

TensorDesc& TensorDesc::operator=(const TensorDesc& other) {
  dimensions_ = other.dimensions_;
  strides_ = other.strides_;
  buffer_desc_ = other.buffer_desc_;

  // Update the internal pointers to dimensions and strides.
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};

  return *this;
}

TensorDesc& TensorDesc::operator=(TensorDesc&& other) {
  dimensions_ = std::move(other.dimensions_);
  strides_ = std::move(other.strides_);
  buffer_desc_ = std::move(other.buffer_desc_);

  // Update the internal pointers to dimensions and strides.
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};

  return *this;
}

TensorDesc::~TensorDesc() = default;

bool TensorDesc::operator==(const TensorDesc& other) const {
  return dimensions_ == other.dimensions_ && strides_ == other.strides_ &&
         buffer_desc_.DataType == other.buffer_desc_.DataType &&
         buffer_desc_.TotalTensorSizeInBytes ==
             other.buffer_desc_.TotalTensorSizeInBytes &&
         buffer_desc_.GuaranteedBaseOffsetAlignment ==
             other.buffer_desc_.GuaranteedBaseOffsetAlignment;
}

void TensorDesc::Transpose(base::span<const uint32_t> permutation) {
  dimensions_ = PermuteArray(dimensions_, permutation);
  strides_ = PermuteArray(strides_, permutation);

  // Round up to the nearest 4 bytes. The buffer allocation already aligned
  // chunks up to DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT.
  uint64_t minimum_implied_size_in_bytes = CalculateDMLBufferTensorSize(
      buffer_desc_.DataType, dimensions_, strides_);
  CHECK_EQ(buffer_desc_.TotalTensorSizeInBytes, minimum_implied_size_in_bytes);

  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
}

// Change the dimensions and strides of the TensorDesc by setting the stride of
// broadcasting dimension to 0 and reuse the stride value for other dimensions,
// its behavior follows the helper function:
// https://learn.microsoft.com/en-us/windows/ai/directml/dml-helper-functions#calculatestrides
void TensorDesc::BroadcastTo(base::span<const uint32_t> broadcasted_dims) {
  // When broadcasting to a 0D scalar (broadcasted_dims = {}), only the
  // dimension of original tensor equals to {1} is legal, and the dimensions_
  // and strides_ of TensorDesc do not need to be changed at this time.
  if (broadcasted_dims.empty()) {
    CHECK_EQ(dimensions_.size(), 1u);
    CHECK_EQ(dimensions_[0], 1u);
    return;
  }

  auto original_rank = dimensions_.size(),
       broadcasted_rank = broadcasted_dims.size();
  CHECK_LE(original_rank, broadcasted_rank);

  std::vector<uint32_t> broadcasted_strides(broadcasted_rank, 0u);
  for (uint32_t i = 0; i < original_rank; ++i) {
    if (dimensions_[original_rank - i - 1] ==
        broadcasted_dims[broadcasted_rank - i - 1]) {
      broadcasted_strides[broadcasted_rank - i - 1] =
          strides_[original_rank - i - 1];
    } else {
      CHECK_EQ(dimensions_[original_rank - i - 1], 1u);
    }
  }

  // Update the internal pointers to dimensions and strides.
  dimensions_.assign(broadcasted_dims.begin(), broadcasted_dims.end());
  strides_ = std::move(broadcasted_strides);
  buffer_desc_.DimensionCount = dimensions_.size();
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
}

}  // namespace webnn::dml
