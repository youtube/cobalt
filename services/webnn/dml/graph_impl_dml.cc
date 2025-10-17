// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/graph_impl_dml.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/349653202): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <winerror.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <numeric>
#include <variant>

#include "base/bits.h"
#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_ref.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/context_impl_dml.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/dml/graph_builder_dml.h"
#include "services/webnn/dml/tensor_desc.h"
#include "services/webnn/dml/tensor_impl_dml.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/fp16/src/include/fp16.h"

namespace webnn::dml {
namespace {

// The feature flag allows us to disable the graph fusion if it causes
// something wrong.
BASE_FEATURE(kApplyGraphFusion,
             "ApplyGraphFusion",
             base::FEATURE_ENABLED_BY_DEFAULT);

using Microsoft::WRL::ComPtr;
using mojom::Operand;
using mojom::OperandPtr;
using mojom::Operation;

// A map of all node outputs in `dml::GraphBuilderDml` using the mojom operand
// id as key.
using IdToNodeOutputMap = absl::flat_hash_map<OperandId, const NodeOutput*>;

static constexpr auto kDmlFloatDataTypes =
    base::MakeFixedFlatSet<DML_TENSOR_DATA_TYPE>(
        {DML_TENSOR_DATA_TYPE_FLOAT32, DML_TENSOR_DATA_TYPE_FLOAT16});

// TODO(crbug.com/335909582): Take an MLNumber rather than a float, and replace
// all these saturated_casts with conversions handled by MLNumber.
DML_SCALAR_UNION ToScalarUnion(float value, DML_TENSOR_DATA_TYPE type) {
  switch (type) {
    case DML_TENSOR_DATA_TYPE_FLOAT32:
      return DML_SCALAR_UNION{.Float32 = value};
    case DML_TENSOR_DATA_TYPE_FLOAT16:
      // Use UInt16 since DML_SCALAR_UNION does not have a float16 variant. The
      // bits in this value will correctly be interpreted as float16 by
      // functions which allow passing a DML_SCALAR_UNION paired with a
      // corresponding DML_TENSOR_DATA_TYPE of DML_TENSOR_DATA_TYPE_FLOAT16.
      return DML_SCALAR_UNION{.UInt16 = fp16_ieee_from_fp32_value(value)};
    case DML_TENSOR_DATA_TYPE_INT8:
      return DML_SCALAR_UNION{.Int8 = base::saturated_cast<int8_t>(value)};
    case DML_TENSOR_DATA_TYPE_UINT8:
      return DML_SCALAR_UNION{.UInt8 = base::saturated_cast<uint8_t>(value)};
    case DML_TENSOR_DATA_TYPE_INT64:
      return DML_SCALAR_UNION{.Int64 = base::saturated_cast<int64_t>(value)};
    case DML_TENSOR_DATA_TYPE_UINT64:
      return DML_SCALAR_UNION{.UInt64 = base::saturated_cast<uint64_t>(value)};
    case DML_TENSOR_DATA_TYPE_INT32:
      return DML_SCALAR_UNION{.Int32 = base::saturated_cast<int32_t>(value)};
    case DML_TENSOR_DATA_TYPE_UINT32:
      return DML_SCALAR_UNION{.UInt32 = base::saturated_cast<uint32_t>(value)};
    default:
      NOTREACHED() << "[WebNN] This data type is not supported.";
  }
}

DML_TENSOR_DATA_TYPE GetTensorDataType(OperandDataType type) {
  switch (type) {
    case OperandDataType::kFloat32:
      return DML_TENSOR_DATA_TYPE_FLOAT32;
    case OperandDataType::kFloat16:
      return DML_TENSOR_DATA_TYPE_FLOAT16;
    case OperandDataType::kInt8:
      return DML_TENSOR_DATA_TYPE_INT8;
    case OperandDataType::kUint8:
      return DML_TENSOR_DATA_TYPE_UINT8;
    case OperandDataType::kInt64:
      return DML_TENSOR_DATA_TYPE_INT64;
    case OperandDataType::kUint64:
      return DML_TENSOR_DATA_TYPE_UINT64;
    case OperandDataType::kInt32:
      return DML_TENSOR_DATA_TYPE_INT32;
    case OperandDataType::kUint32:
      return DML_TENSOR_DATA_TYPE_UINT32;
    case OperandDataType::kInt4:
      return DML_TENSOR_DATA_TYPE_INT4;
    case OperandDataType::kUint4:
      return DML_TENSOR_DATA_TYPE_UINT4;
  }
}

OperandDataType DmlDataTypeToOperand(DML_TENSOR_DATA_TYPE type) {
  switch (type) {
    case DML_TENSOR_DATA_TYPE_FLOAT32:
      return OperandDataType::kFloat32;
    case DML_TENSOR_DATA_TYPE_FLOAT16:
      return OperandDataType::kFloat16;
    case DML_TENSOR_DATA_TYPE_INT8:
      return OperandDataType::kInt8;
    case DML_TENSOR_DATA_TYPE_UINT8:
      return OperandDataType::kUint8;
    case DML_TENSOR_DATA_TYPE_INT64:
      return OperandDataType::kInt64;
    case DML_TENSOR_DATA_TYPE_UINT64:
      return OperandDataType::kUint64;
    case DML_TENSOR_DATA_TYPE_INT32:
      return OperandDataType::kInt32;
    case DML_TENSOR_DATA_TYPE_UINT32:
      return OperandDataType::kUint32;
    case DML_TENSOR_DATA_TYPE_INT4:
      return OperandDataType::kInt4;
    case DML_TENSOR_DATA_TYPE_UINT4:
      return OperandDataType::kUint4;
    default:
      NOTREACHED() << "[WebNN] This data type is not supported.";
  }
}

DML_REDUCE_FUNCTION MapReduceKindToReduceFuntion(mojom::Reduce::Kind kind) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      return DML_REDUCE_FUNCTION_L1;
    case mojom::Reduce::Kind::kL2:
      return DML_REDUCE_FUNCTION_L2;
    case mojom::Reduce::Kind::kLogSum:
      return DML_REDUCE_FUNCTION_LOG_SUM;
    case mojom::Reduce::Kind::kLogSumExp:
      return DML_REDUCE_FUNCTION_LOG_SUM_EXP;
    case mojom::Reduce::Kind::kMax:
      return DML_REDUCE_FUNCTION_MAX;
    case mojom::Reduce::Kind::kMean:
      return DML_REDUCE_FUNCTION_AVERAGE;
    case mojom::Reduce::Kind::kMin:
      return DML_REDUCE_FUNCTION_MIN;
    case mojom::Reduce::Kind::kProduct:
      return DML_REDUCE_FUNCTION_MULTIPLY;
    case mojom::Reduce::Kind::kSum:
      return DML_REDUCE_FUNCTION_SUM;
    case mojom::Reduce::Kind::kSumSquare:
      return DML_REDUCE_FUNCTION_SUM_SQUARE;
  }
}

void CheckInputDataTypeForReduce(const DataTypeLimits& data_type_limits,
                                 mojom::Reduce::Kind kind,
                                 OperandDataType data_type) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      CHECK(data_type_limits.reduce_l1_input.data_types.Has(data_type));
      break;
    case mojom::Reduce::Kind::kL2:
      CHECK(data_type_limits.reduce_l2_input.data_types.Has(data_type));
      break;
    case mojom::Reduce::Kind::kLogSum:
      CHECK(data_type_limits.reduce_log_sum_input.data_types.Has(data_type));
      break;
    case mojom::Reduce::Kind::kLogSumExp:
      CHECK(
          data_type_limits.reduce_log_sum_exp_input.data_types.Has(data_type));
      break;
    case mojom::Reduce::Kind::kMax:
      CHECK(data_type_limits.reduce_max_input.data_types.Has(data_type));
      break;
    case mojom::Reduce::Kind::kMean:
      CHECK(data_type_limits.reduce_mean_input.data_types.Has(data_type));
      break;
    case mojom::Reduce::Kind::kMin:
      CHECK(data_type_limits.reduce_min_input.data_types.Has(data_type));
      break;
    case mojom::Reduce::Kind::kProduct:
      CHECK(data_type_limits.reduce_product_input.data_types.Has(data_type));
      break;
    case mojom::Reduce::Kind::kSum:
      CHECK(data_type_limits.reduce_sum_input.data_types.Has(data_type));
      break;
    case mojom::Reduce::Kind::kSumSquare:
      CHECK(data_type_limits.reduce_sum_square_input.data_types.Has(data_type));
      break;
  }
}

DML_RECURRENT_NETWORK_DIRECTION MojoRecurrentNetworkDirectionToDml(
    mojom::RecurrentNetworkDirection direction) {
  switch (direction) {
    case mojom::RecurrentNetworkDirection::kForward:
      return DML_RECURRENT_NETWORK_DIRECTION_FORWARD;
    case mojom::RecurrentNetworkDirection::kBackward:
      return DML_RECURRENT_NETWORK_DIRECTION_BACKWARD;
    case mojom::RecurrentNetworkDirection::kBoth:
      return DML_RECURRENT_NETWORK_DIRECTION_BIDIRECTIONAL;
  }
}

// TODO(crbug.com/354543926): All calls to CreateError can be replaced by
// CreateUnexpectedError.
base::expected<void, mojom::ErrorPtr> CreateUnexpectedError(
    mojom::Error::Code error_code,
    const std::string& error_message,
    std::string_view label) {
  return base::unexpected(CreateError(error_code, error_message, label));
}

// Calculate the total byte length of buffers and the D3D12_RANGE for each
// buffer, all with the required alignment.
std::optional<AlignedByteLength<OperandId>> CalculateAlignedByteLength(
    const base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands) {
  base::CheckedNumeric<size_t> total_byte_length(0);
  absl::flat_hash_map<OperandId, D3D12_RANGE> key_to_d3d12_range_map;

  for (const auto& [operand_id, constant_operand] : constant_operands) {
    auto& d3d12_range = key_to_d3d12_range_map[operand_id];
    d3d12_range.Begin = total_byte_length.ValueOrDie();

    // The buffer has a minimum base address alignment requirement of 16 bytes
    // in the macro `DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT`:
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-directml-constants
    total_byte_length +=
        base::bits::AlignUp<size_t>(constant_operand->ByteSpan().size(),
                                    DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT);
    if (!total_byte_length.IsValid()) {
      LOG(ERROR) << "[WebNN] Failed to calculate the total byte length.";
      return std::nullopt;
    }

    // The aligned byte length calculated with `End` sub `Begin` attribute is
    // used to set the `SizeInBytes` field of `DML_BUFFER_BINDING`.
    d3d12_range.End = total_byte_length.ValueOrDie();
  }

  return AlignedByteLength<OperandId>{
      .total_byte_length = total_byte_length.ValueOrDie(),
      .key_to_d3d12_range_map = std::move(key_to_d3d12_range_map)};
}

// Same as above, but given a map of names to descriptors.
std::optional<AlignedByteLength<std::string>>
CalculateAlignedByteLengthFromDescriptors(
    const base::flat_map<std::string, OperandDescriptor>&
        names_to_descriptors) {
  base::CheckedNumeric<size_t> total_byte_length(0);
  absl::flat_hash_map<std::string, D3D12_RANGE> key_to_d3d12_range_map;

  for (auto& [name, descriptor] : names_to_descriptors) {
    auto& d3d12_range = key_to_d3d12_range_map[name];
    d3d12_range.Begin = total_byte_length.ValueOrDie();

    // The buffer has a minimum base address alignment requirement of 16 bytes
    // in the macro `DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT`:
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-directml-constants
    total_byte_length += base::bits::AlignUp<size_t>(
        descriptor.PackedByteLength(), DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT);
    if (!total_byte_length.IsValid()) {
      LOG(ERROR) << "[WebNN] Failed to calculate the total byte length.";
      return std::nullopt;
    }

    // The aligned byte length calculated with `End` sub `Begin` attribute is
    // used to set the `SizeInBytes` field of `DML_BUFFER_BINDING`.
    d3d12_range.End = total_byte_length.ValueOrDie();
  }

  return AlignedByteLength<std::string>{
      .total_byte_length = total_byte_length.ValueOrDie(),
      .key_to_d3d12_range_map = std::move(key_to_d3d12_range_map)};
}

struct UploadAndDefaultBuffers {
  ComPtr<ID3D12Resource> upload_buffer;
  ComPtr<ID3D12Resource> default_buffer;
};

// Upload constants buffers in one Direct3D 12 committed resource, the
// DML_BUFFER_BINDING specifies a resource binding described by a range of bytes
// in the single buffer. For GPU supports UMA, pass a custom upload buffer via
// `buffer_variant` for both constants uploading and binding. For GPU doesn't
// support UMA, pass a upload buffer and a default buffer via `buffer_variant`
// for uploading and binding separately.
base::expected<absl::flat_hash_map<OperandId, DML_BUFFER_BINDING>, HRESULT>
UploadAndCreateConstantBufferBinding(
    CommandRecorder* command_recorder,
    const base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    const AlignedByteLength<OperandId>& aligned_byte_length,
    std::variant<UploadAndDefaultBuffers, ComPtr<ID3D12Resource>>
        buffer_variant) {
  // Map entire resource to copy the array buffer of constant/input one by one
  // with byte offset.
  void* mapped_buffer = nullptr;
  ID3D12Resource* buffer_to_map = nullptr;
  ID3D12Resource* buffer_to_bind = nullptr;
  ComPtr<ID3D12Resource> cpu_buffer;
  ComPtr<ID3D12Resource> upload_buffer;
  ComPtr<ID3D12Resource> default_buffer;
  if (std::holds_alternative<ComPtr<ID3D12Resource>>(buffer_variant)) {
    cpu_buffer = std::move(std::get<ComPtr<ID3D12Resource>>(buffer_variant));
    buffer_to_map = cpu_buffer.Get();
    buffer_to_bind = buffer_to_map;
  } else {
    upload_buffer = std::move(
        std::get<UploadAndDefaultBuffers>(buffer_variant).upload_buffer);
    default_buffer = std::move(
        std::get<UploadAndDefaultBuffers>(buffer_variant).default_buffer);
    buffer_to_map = upload_buffer.Get();
    buffer_to_bind = default_buffer.Get();
  }
  CHECK(buffer_to_map);
  CHECK(buffer_to_bind);

  RETURN_UNEXPECTED_IF_FAILED(buffer_to_map->Map(0, nullptr, &mapped_buffer));

  absl::flat_hash_map<OperandId, DML_BUFFER_BINDING> key_to_buffer_binding_map;
  for (auto& [operand_id, constant_operand] : constant_operands) {
    // Copy the input data to the upload heap with byte offset
    const auto& d3d12_range =
        aligned_byte_length.key_to_d3d12_range_map.at(operand_id);
    auto mapped_buffer_span =
        base::span(static_cast<uint8_t*>(mapped_buffer) + d3d12_range.Begin,
                   constant_operand->descriptor().PackedByteLength());
    mapped_buffer_span.copy_from(constant_operand->ByteSpan());
    // Create the buffer binding for each constant/input and push back into the
    // DML_BUFFER_BINDING array.
    auto size_in_bytes = d3d12_range.End - d3d12_range.Begin;
    key_to_buffer_binding_map[operand_id] =
        DML_BUFFER_BINDING{.Buffer = buffer_to_bind,
                           .Offset = d3d12_range.Begin,
                           .SizeInBytes = size_in_bytes};
  }
  buffer_to_map->Unmap(0, nullptr);

  if (std::holds_alternative<ComPtr<ID3D12Resource>>(buffer_variant)) {
    CHECK(cpu_buffer);
    command_recorder->ReferenceCommandResources(std::move(cpu_buffer));
  } else {
    CHECK(default_buffer);
    CHECK(upload_buffer);
    UploadBufferWithBarrier(command_recorder, std::move(default_buffer),
                            std::move(upload_buffer),
                            aligned_byte_length.total_byte_length);
  }

  return key_to_buffer_binding_map;
}

HRESULT MapAndCopyInputDataToBuffer(
    const base::flat_map<std::string, mojo_base::BigBuffer>& named_inputs,
    const absl::flat_hash_map<std::string, D3D12_RANGE>&
        input_name_to_d3d12_range_map,
    ID3D12Resource* buffer) {
  // Map entire resource to copy the array buffer of input one by one
  // with byte offset.
  void* mapped_buffer = nullptr;
  CHECK(buffer);
  RETURN_IF_FAILED(buffer->Map(0, nullptr, &mapped_buffer));

  for (auto& [name, input] : named_inputs) {
    // Copy the input data to the upload heap with byte offset
    const auto& d3d12_range = input_name_to_d3d12_range_map.at(name);
    memcpy(static_cast<uint8_t*>(mapped_buffer) + d3d12_range.Begin,
           input.data(), input.size());
  }
  buffer->Unmap(0, nullptr);

  return S_OK;
}

// Define some methods like CreateInputNode and CreateOperatorNodeForRelu here
// to focus on converting the mojo graph struct to corresponding DML graph node
// by using dml::GraphBuilderDml as a helper. dml::GraphBuilderDml should be
// decoupled from mojo graph structs and focus on manipulating DML graph
// structs.
//
// The return value is the GraphInputIndex assigned by graph builder.
uint32_t CreateInputNode(const std::vector<OperandPtr>& operands,
                         OperandId input_id,
                         GraphBuilderDml& graph_builder,
                         IdToNodeOutputMap& id_to_node_output_map) {
  // TODO(crbug.com/413722115): Move this to GetOperand helper function.
  const OperandPtr& operand = operands.at(input_id.value());
  CHECK_EQ(operand->kind, Operand::Kind::kInput);

  TensorDesc input_tensor_desc(
      GetTensorDataType(operand->descriptor.data_type()), DML_TENSOR_FLAG_NONE,
      operand->descriptor.shape());
  const InputNode* input_node = graph_builder.CreateInputNode();
  CHECK(input_node);
  const NodeOutput* node_output =
      graph_builder.CreateNodeOutput(input_node, std::move(input_tensor_desc));
  CHECK(node_output);
  id_to_node_output_map[input_id] = std::move(node_output);
  return input_node->GetGraphInputIndex();
}

// Create a graph node for constant operand.
// When DML_FEATURE_LEVEL >= 6.2 and the operand tensor only has one element,
// create a constant node and transfer the ownership of the operand to
// `graph_builder` which will be consumed directly during graph compilation, the
// corresponding element will be erased from `constant_operands`. Otherwise,
// create an input node, the constant buffer should be later uploaded during
// graph initialization.
void CreateConstantNode(
    Adapter* adapter,
    OperandId operand_id,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map,
    absl::flat_hash_map<OperandId, uint32_t>& constant_id_to_input_index_map) {
  const OperandDescriptor operand_descriptor =
      constant_operands.at(operand_id)->descriptor();

  // DML_GRAPH_NODE_TYPE_CONSTANT is introduced in DML_FEATURE_LEVEL_6_2.
  bool should_create_dml_constant_node =
      adapter->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_6_2) &&
      CalculatePhysicalElementCount(operand_descriptor.shape()) == 1;

  const Node* node = nullptr;
  if (should_create_dml_constant_node) {
    node = graph_builder.CreateConstantNode(
        std::move(constant_operands[operand_id]));
    constant_operands.erase(operand_id);
  } else {
    node = graph_builder.CreateInputNode();
    constant_id_to_input_index_map[operand_id] =
        node->AsInputNode()->GetGraphInputIndex();
  }

  // Only when the tensor needs to be bound during graph initialization should
  // it be identified by DML_TENSOR_FLAG_OWNED_BY_DML.
  TensorDesc tensor_desc(GetTensorDataType(operand_descriptor.data_type()),
                         should_create_dml_constant_node
                             ? DML_TENSOR_FLAG_NONE
                             : DML_TENSOR_FLAG_OWNED_BY_DML,
                         operand_descriptor.shape());

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(node, std::move(tensor_desc));
  CHECK(id_to_node_output_map.try_emplace(operand_id, output).second);
}

const NodeOutput* GetNodeOutputForOperand(
    const IdToNodeOutputMap& id_to_node_output_map,
    OperandId operand_id) {
  const auto input_iterator = id_to_node_output_map.find(operand_id);
  CHECK(input_iterator != id_to_node_output_map.end());
  CHECK(input_iterator->second);
  return input_iterator->second;
}

const NodeOutput* GetOptionalNodeOutputForOperand(
    const IdToNodeOutputMap& id_to_node_output_map,
    std::optional<OperandId> operand_id) {
  return operand_id.has_value() ? GetNodeOutputForOperand(id_to_node_output_map,
                                                          operand_id.value())
                                : nullptr;
}

const DML_TENSOR_DESC* GetOptionalDmlTensorDescPtr(
    base::optional_ref<const TensorDesc> tensor_desc) {
  return tensor_desc.has_value() ? &tensor_desc->GetDMLTensorDesc() : nullptr;
}

// Build a one-element constant operand with specified rank for float value and
// add it into the graph info. For example, if the rank is 3, the operand
// dimensions would be {1, 1, 1}.
OperandId BuildConstantOperandForFloatValue(
    const ContextProperties& context_properties,
    mojom::GraphInfoPtr& graph_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    OperandDataType data_type,
    size_t rank,
    float value) {
  auto descriptor =
      *OperandDescriptor::Create(context_properties, data_type,
                                 std::vector<uint32_t>(rank, 1), "constant");

  auto constant_operand =
      Operand::New(Operand::Kind::kConstant, descriptor, /*name=*/std::nullopt);

  OperandId constant_operand_id(graph_info->operands.size());
  graph_info->operands.push_back(std::move(constant_operand));

  base::HeapArray<uint8_t> buffer;
  switch (data_type) {
    case OperandDataType::kFloat32:
      buffer = base::HeapArray<uint8_t>::CopiedFrom(
          base::byte_span_from_ref(base::allow_nonunique_obj, value));
      break;
    case OperandDataType::kFloat16: {
      uint16_t fp16_value = fp16_ieee_from_fp32_value(value);
      buffer = base::HeapArray<uint8_t>::CopiedFrom(
          base::byte_span_from_ref(fp16_value));
      break;
    }
    default:
      LOG(ERROR) << "[WebNN] The data type must be one of the floating point "
                    "data types.";
      NOTREACHED();
  }

  CHECK(constant_operands
            .try_emplace(constant_operand_id,
                         std::make_unique<WebNNConstantOperand>(
                             std::move(descriptor), std::move(buffer)))
            .second);

  return constant_operand_id;
}

const TensorDesc CreateOutputTensorDesc(const std::vector<OperandPtr>& operands,
                                        OperandId output_id) {
  const OperandPtr& output_operand = operands.at(output_id.value());
  return TensorDesc(GetTensorDataType(output_operand->descriptor.data_type()),
                    output_operand->descriptor.shape());
}

void CreateOperatorNodeForArgMinMax(const std::vector<OperandPtr>& operands,
                                    const mojom::ArgMinMaxPtr& arg_min_max,
                                    GraphBuilderDml& graph_builder,
                                    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, arg_min_max->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  const OperandId output_id = arg_min_max->output_operand_id;
  const auto& output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  const uint32_t axis = arg_min_max->axis;
  // Determine output sizes. Ignore output_desc->dimensions for the dimensions,
  // since DirectML expects the output dimensions to have the same rank as the
  // input, and output_desc->dimensions may have removed dimensions if
  // keepDimensions was false.
  std::vector<uint32_t> output_dimensions = input_tensor_desc.GetDimensions();
  CHECK_LT(axis, output_dimensions.size());
  output_dimensions[axis] = 1u;

  TensorDesc new_output_tensor_desc(output_tensor_desc.GetDataType(),
                                    std::move(output_dimensions));

  DML_OPERATOR_TYPE operator_type;
  switch (arg_min_max->kind) {
    case mojom::ArgMinMax_Kind::kMin: {
      operator_type = DML_OPERATOR_ARGMIN;
      break;
    }
    case mojom::ArgMinMax_Kind::kMax: {
      operator_type = DML_OPERATOR_ARGMAX;
      break;
    }
  }

  const std::array<const uint32_t, 1> axes = {axis};
  DML_ARGMAX_OPERATOR_DESC operator_desc = {};
  operator_desc.InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
  operator_desc.OutputTensor = &new_output_tensor_desc.GetDMLTensorDesc(),
  operator_desc.AxisCount = axes.size();
  operator_desc.Axes = axes.data();
  operator_desc.AxisDirection =
      DML_AXIS_DIRECTION::DML_AXIS_DIRECTION_INCREASING;

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* arg_min_max_node = graph_builder.CreateOperatorNode(
      operator_type, &operator_desc, inputs, arg_min_max->label);

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(arg_min_max_node, output_tensor_desc);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

struct ActivationOperatorDesc {
  std::variant<DML_ACTIVATION_ELU_OPERATOR_DESC,
               DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC,
               DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC,
               DML_ACTIVATION_LINEAR_OPERATOR_DESC,
               DML_ACTIVATION_RELU_OPERATOR_DESC,
               DML_ACTIVATION_SIGMOID_OPERATOR_DESC,
               DML_ACTIVATION_SOFTMAX1_OPERATOR_DESC,
               DML_ACTIVATION_SOFTPLUS_OPERATOR_DESC,
               DML_ACTIVATION_SOFTSIGN_OPERATOR_DESC,
               DML_ACTIVATION_TANH_OPERATOR_DESC>
      desc;

  DML_OPERATOR_DESC GetActivationDmlDesc() const {
    if (std::holds_alternative<DML_ACTIVATION_ELU_OPERATOR_DESC>(desc)) {
      return {DML_OPERATOR_ACTIVATION_ELU,
              &std::get<DML_ACTIVATION_ELU_OPERATOR_DESC>(desc)};
    } else if (std::holds_alternative<
                   DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC>(desc)) {
      return {DML_OPERATOR_ACTIVATION_HARD_SIGMOID,
              &std::get<DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC>(desc)};
    } else if (std::holds_alternative<DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_LEAKY_RELU,
              &std::get<DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC>(desc)};
    } else if (std::holds_alternative<DML_ACTIVATION_LINEAR_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_LINEAR,
              &std::get<DML_ACTIVATION_LINEAR_OPERATOR_DESC>(desc)};
    } else if (std::holds_alternative<DML_ACTIVATION_RELU_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_RELU,
              &std::get<DML_ACTIVATION_RELU_OPERATOR_DESC>(desc)};
    } else if (std::holds_alternative<DML_ACTIVATION_SIGMOID_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_SIGMOID,
              &std::get<DML_ACTIVATION_SIGMOID_OPERATOR_DESC>(desc)};
    } else if (std::holds_alternative<DML_ACTIVATION_SOFTMAX1_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_SOFTMAX1,
              &std::get<DML_ACTIVATION_SOFTMAX1_OPERATOR_DESC>(desc)};
    } else if (std::holds_alternative<DML_ACTIVATION_SOFTPLUS_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_SOFTPLUS,
              &std::get<DML_ACTIVATION_SOFTPLUS_OPERATOR_DESC>(desc)};
    } else if (std::holds_alternative<DML_ACTIVATION_SOFTSIGN_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_SOFTSIGN,
              &std::get<DML_ACTIVATION_SOFTSIGN_OPERATOR_DESC>(desc)};
    } else if (std::holds_alternative<DML_ACTIVATION_TANH_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_TANH,
              &std::get<DML_ACTIVATION_TANH_OPERATOR_DESC>(desc)};
    } else {
      NOTREACHED() << "The activation type is not supported.";
    }
  }
};

ActivationOperatorDesc CreateOperatorDescForActivation(
    mojom::RecurrentNetworkActivation activation) {
  switch (activation) {
    case mojom::RecurrentNetworkActivation::kRelu:
      return ActivationOperatorDesc{.desc =
                                        DML_ACTIVATION_RELU_OPERATOR_DESC{}};
    case mojom::RecurrentNetworkActivation::kSigmoid:
      return ActivationOperatorDesc{.desc =
                                        DML_ACTIVATION_SIGMOID_OPERATOR_DESC{}};
    case mojom::RecurrentNetworkActivation::kTanh:
      return ActivationOperatorDesc{.desc =
                                        DML_ACTIVATION_TANH_OPERATOR_DESC{}};
  }
}

std::optional<const Operation*> GetFusibleActivationFromOperation(
    const absl::flat_hash_map<const Operation*,
                              raw_ptr<const Operation, CtnExperimental>>&
        operation_to_fusible_standalone_activation_map,
    const Operation* operation) {
  const auto activation_iterator =
      operation_to_fusible_standalone_activation_map.find(operation);
  if (activation_iterator !=
      operation_to_fusible_standalone_activation_map.end()) {
    return activation_iterator->second;
  }
  return std::optional<const Operation*>();
}

std::optional<OperandId> GetFusibleTransposeInputId(
    const absl::flat_hash_map<OperandId,
                              raw_ptr<const Operation, CtnExperimental>>&
        output_id_to_fusible_transpose_map,
    OperandId input_id) {
  const auto transpose_iterator =
      output_id_to_fusible_transpose_map.find(input_id);
  if (transpose_iterator != output_id_to_fusible_transpose_map.end()) {
    return transpose_iterator->second->get_transpose()->input_operand_id;
  }
  return std::optional<OperandId>();
}

// According to the DirectML documentations:
// https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_add1_operator_desc,
// and
// https://learn.microsoft.com/en-us/windows/ai/directml/dml-fused-activations,
// for the element wise binary operation, only `DML_OPERATOR_ELEMENT_WISE_ADD1`
// supports fused activation when the output data type is FLOAT16 or FLOAT32.
bool CanElementWiseBinarySupportFusion(
    const mojom::ElementWiseBinaryPtr& binary,
    const std::vector<OperandPtr>& operands) {
  const OperandPtr& output_operand =
      operands.at(binary->output_operand_id.value());
  OperandDataType output_data_type = output_operand->descriptor.data_type();
  return binary->kind == mojom::ElementWiseBinary::Kind::kAdd &&
         (output_data_type == OperandDataType::kFloat32 ||
          output_data_type == OperandDataType::kFloat16);
}

// Return true if the operation can be fused with any of the following
// standalone activations operators according to
// https://learn.microsoft.com/en-us/windows/ai/directml/dml-fused-activations:
// DML_OPERATOR_BATCH_NORMALIZATION
// DML_OPERATOR_BATCH_NORMALIZATION_TRAINING
// DML_OPERATOR_CONVOLUTION
// DML_OPERATOR_ELEMENT_WISE_ADD1
// DML_OPERATOR_GEMM
// DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION
// DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION1
bool CanFuseStandaloneActivation(const Operation* operation,
                                 const std::vector<OperandPtr>& operands) {
  switch (operation->which()) {
    case Operation::Tag::kElementWiseBinary:
      return CanElementWiseBinarySupportFusion(
          operation->get_element_wise_binary(), operands);
    case Operation::Tag::kConv2d:
    case Operation::Tag::kBatchNormalization:
    case Operation::Tag::kGemm:
    case Operation::Tag::kInstanceNormalization:
    case Operation::Tag::kLayerNormalization:
    case Operation::Tag::kMatmul:
      return true;
    default:
      return false;
  }
}

// Return a valid output id if the operation is a fusible activation according
// to
// https://learn.microsoft.com/en-us/windows/ai/directml/dml-fused-activations.
// DML_OPERATOR_ELEMENT_WISE_CLIP will be supported after the DirectML version
// upper than DML_FEATURE_LEVEL_6_0 according to
// https://learn.microsoft.com/en-us/windows/ai/directml/dml-feature-level-history#dml_feature_level_6_0.
//
// TODO(crbug.com/345640552): Fuse clip and other operators when possible.
std::optional<OperandId> GetFusibleActivationOutputId(
    const mojom::Operation& operation) {
  switch (operation.which()) {
    case mojom::Operation::Tag::kElu:
      return operation.get_elu()->output_operand_id;
    case mojom::Operation::Tag::kHardSigmoid:
      return operation.get_hard_sigmoid()->output_operand_id;
    case mojom::Operation::Tag::kLeakyRelu:
      return operation.get_leaky_relu()->output_operand_id;
    case mojom::Operation::Tag::kLinear:
      return operation.get_linear()->output_operand_id;
    case mojom::Operation::Tag::kRelu:
      return operation.get_relu()->output_operand_id;
    case mojom::Operation::Tag::kSigmoid:
      return operation.get_sigmoid()->output_operand_id;
    case mojom::Operation::Tag::kSoftplus:
      return operation.get_softplus()->output_operand_id;
    case mojom::Operation::Tag::kSoftsign:
      return operation.get_softsign()->output_operand_id;
    case mojom::Operation::Tag::kTanh:
      return operation.get_tanh()->output_operand_id;
    default:
      return std::nullopt;
  }
}

std::string_view GetOperatorLabel(std::string_view original_label,
                                  std::string_view default_label) {
  return original_label.empty() ? default_label : original_label;
}

std::string_view GetFusibleActivationLabel(const mojom::Operation& operation) {
  switch (operation.which()) {
    case mojom::Operation::Tag::kElu:
      return GetOperatorLabel(operation.get_elu()->label, "elu");
    case mojom::Operation::Tag::kHardSigmoid:
      return GetOperatorLabel(operation.get_hard_sigmoid()->label,
                              "hard_sigmoid");
    case mojom::Operation::Tag::kLeakyRelu:
      return GetOperatorLabel(operation.get_leaky_relu()->label, "leaky_relu");
    case mojom::Operation::Tag::kLinear:
      return GetOperatorLabel(operation.get_linear()->label, "linear");
    case mojom::Operation::Tag::kRelu:
      return GetOperatorLabel(operation.get_relu()->label, "relu");
    case mojom::Operation::Tag::kSigmoid:
      return GetOperatorLabel(operation.get_sigmoid()->label, "sigmoid");
    case mojom::Operation::Tag::kSoftplus:
      return GetOperatorLabel(operation.get_softplus()->label, "softplus");
    case mojom::Operation::Tag::kSoftsign:
      return GetOperatorLabel(operation.get_softsign()->label, "softsign");
    case mojom::Operation::Tag::kTanh:
      return GetOperatorLabel(operation.get_tanh()->label, "tanh");
    default:
      NOTREACHED() << "The operation is not a fusible activation.";
  }
}

std::string GetFusedOperatorLabel(std::string_view original_label,
                                  std::string_view default_label,
                                  const mojom::Operation& fusible_activation) {
  return base::JoinString(
      {original_label.empty() ? default_label : original_label,
       GetFusibleActivationLabel(fusible_activation)},
      "+");
}

ActivationOperatorDesc CreateOperatorDescForFusibleActivation(
    const mojom::Operation& activation) {
  CHECK(GetFusibleActivationOutputId(activation));
  switch (activation.which()) {
    case mojom::Operation::Tag::kElu:
      return ActivationOperatorDesc{.desc = DML_ACTIVATION_ELU_OPERATOR_DESC{
                                        .Alpha = activation.get_elu()->alpha}};
    case mojom::Operation::Tag::kHardSigmoid:
      return ActivationOperatorDesc{
          .desc = DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC{
              .Alpha = activation.get_hard_sigmoid()->alpha,
              .Beta = activation.get_hard_sigmoid()->beta}};
    case mojom::Operation::Tag::kLeakyRelu:
      return ActivationOperatorDesc{
          .desc = DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC{
              .Alpha = activation.get_leaky_relu()->alpha}};
    case mojom::Operation::Tag::kLinear:
      return ActivationOperatorDesc{.desc = DML_ACTIVATION_LINEAR_OPERATOR_DESC{
                                        .Alpha = activation.get_linear()->alpha,
                                        .Beta = activation.get_linear()->beta}};
    case mojom::Operation::Tag::kRelu:
      return ActivationOperatorDesc{.desc =
                                        DML_ACTIVATION_RELU_OPERATOR_DESC{}};
    case mojom::Operation::Tag::kSigmoid:
      return ActivationOperatorDesc{.desc =
                                        DML_ACTIVATION_SIGMOID_OPERATOR_DESC{}};
    case mojom::Operation::Tag::kSoftplus:
      return ActivationOperatorDesc{
          .desc = DML_ACTIVATION_SOFTPLUS_OPERATOR_DESC{.Steepness = 1.0}};
    case mojom::Operation::Tag::kSoftsign:
      return ActivationOperatorDesc{
          .desc = DML_ACTIVATION_SOFTSIGN_OPERATOR_DESC{}};
    case mojom::Operation::Tag::kTanh:
      return ActivationOperatorDesc{.desc =
                                        DML_ACTIVATION_TANH_OPERATOR_DESC{}};
    default:
      NOTREACHED() << "The operation is not a fusible activation.";
  }
}

// The struct contains the connectivity information of an operation in
// `mojom::GraphInfo::operations`. It helps to generate and represent the
// topological information about how all operations are connected.
struct OperationConnectivity {
  // The operation's input ids which are used to identity the input operands in
  // `mojom::GraphInfo::operands`.
  std::vector<OperandId> input_ids;
  // The operation's output ids which are used to identity the output operands
  // in `mojom::GraphInfo::operands`.
  std::vector<OperandId> output_ids;
};

void RetrieveOperationConnectivity(
    const Operation* operation,
    OperationConnectivity& out_operation_connectivity) {
  std::vector<OperandId>& input_ids = out_operation_connectivity.input_ids;
  std::vector<OperandId>& output_ids = out_operation_connectivity.output_ids;
  input_ids.clear();
  output_ids.clear();
  switch (operation->which()) {
    case Operation::Tag::kArgMinMax: {
      const auto& arg_min_max = operation->get_arg_min_max();
      input_ids = {arg_min_max->input_operand_id};
      output_ids = {arg_min_max->output_operand_id};
      break;
    }
    case Operation::Tag::kBatchNormalization: {
      const auto& batch_norm = operation->get_batch_normalization();
      input_ids = {batch_norm->input_operand_id, batch_norm->mean_operand_id,
                   batch_norm->variance_operand_id};
      auto& scale_operand_id = batch_norm->scale_operand_id;
      if (scale_operand_id) {
        input_ids.push_back(scale_operand_id.value());
      }
      auto& bias_operand_id = batch_norm->bias_operand_id;
      if (bias_operand_id) {
        input_ids.push_back(bias_operand_id.value());
      }
      output_ids = {batch_norm->output_operand_id};
      break;
    }
    case Operation::Tag::kClamp: {
      const auto& clamp = operation->get_clamp();
      input_ids = {clamp->input_operand_id};
      output_ids = {clamp->output_operand_id};
      break;
    }
    case Operation::Tag::kConcat: {
      const auto& concat = operation->get_concat();
      input_ids = {concat->input_operand_ids};
      output_ids = {concat->output_operand_id};
      break;
    }
    case Operation::Tag::kConv2d: {
      const auto& conv2d = operation->get_conv2d();
      input_ids = {conv2d->input_operand_id, conv2d->filter_operand_id};
      auto& bias_operand_id = conv2d->bias_operand_id;
      if (bias_operand_id) {
        input_ids.push_back(bias_operand_id.value());
      }
      output_ids = {conv2d->output_operand_id};
      break;
    }
    case Operation::Tag::kCumulativeSum: {
      const auto& cumulative_sum = operation->get_cumulative_sum();
      input_ids = {cumulative_sum->input_operand_id};
      output_ids = {cumulative_sum->output_operand_id};
      break;
    }
    case Operation::Tag::kDequantizeLinear: {
      const auto& dequantize_linear = operation->get_dequantize_linear();
      input_ids = {dequantize_linear->input_operand_id,
                   dequantize_linear->scale_operand_id,
                   dequantize_linear->zero_point_operand_id};
      output_ids = {dequantize_linear->output_operand_id};
      break;
    }
    case Operation::Tag::kElementWiseBinary: {
      const auto& binary = operation->get_element_wise_binary();
      input_ids = {binary->lhs_operand_id, binary->rhs_operand_id};
      output_ids = {binary->output_operand_id};
      break;
    }
    case Operation::Tag::kElu: {
      const auto& elu = operation->get_elu();
      input_ids = {elu->input_operand_id};
      output_ids = {elu->output_operand_id};
      break;
    }
    case Operation::Tag::kElementWiseUnary: {
      const auto& unary = operation->get_element_wise_unary();
      input_ids = {unary->input_operand_id};
      output_ids = {unary->output_operand_id};
      break;
    }
    case Operation::Tag::kExpand: {
      const auto& expand = operation->get_expand();
      input_ids = {expand->input_operand_id};
      output_ids = {expand->output_operand_id};
      break;
    }
    case Operation::Tag::kGather: {
      const auto& gather = operation->get_gather();
      input_ids = {gather->input_operand_id, gather->indices_operand_id};
      output_ids = {gather->output_operand_id};
      break;
    }
    case Operation::Tag::kGatherElements: {
      const auto& gather_elements = operation->get_gather_elements();
      input_ids = {gather_elements->input_operand_id,
                   gather_elements->indices_operand_id};
      output_ids = {gather_elements->output_operand_id};
      break;
    }
    case Operation::Tag::kGatherNd: {
      const auto& gather_nd = operation->get_gather_nd();
      input_ids = {gather_nd->input_operand_id, gather_nd->indices_operand_id};
      output_ids = {gather_nd->output_operand_id};
      break;
    }
    case Operation::Tag::kGelu: {
      const auto& gelu = operation->get_gelu();
      input_ids = {gelu->input_operand_id};
      output_ids = {gelu->output_operand_id};
      break;
    }
    case Operation::Tag::kGemm: {
      const auto& gemm = operation->get_gemm();
      input_ids = {gemm->a_operand_id, gemm->b_operand_id};
      auto& c_operand_id = gemm->c_operand_id;
      if (c_operand_id) {
        input_ids.push_back(c_operand_id.value());
      }
      output_ids = {gemm->output_operand_id};
      break;
    }
    case Operation::Tag::kGru: {
      const auto& gru = operation->get_gru();
      input_ids = {gru->input_operand_id, gru->weight_operand_id,
                   gru->recurrent_weight_operand_id};
      auto& bias_operand_id = gru->bias_operand_id;
      if (bias_operand_id) {
        input_ids.push_back(bias_operand_id.value());
      }
      auto& recurrent_bias_operand_id = gru->recurrent_bias_operand_id;
      if (recurrent_bias_operand_id) {
        input_ids.push_back(recurrent_bias_operand_id.value());
      }
      auto& initial_hidden_state_operand_id =
          gru->initial_hidden_state_operand_id;
      if (initial_hidden_state_operand_id) {
        input_ids.push_back(initial_hidden_state_operand_id.value());
      }
      output_ids = {gru->output_operand_ids};
      break;
    }
    case Operation::Tag::kGruCell: {
      const auto& gru_cell = operation->get_gru_cell();
      input_ids = {gru_cell->input_operand_id, gru_cell->weight_operand_id,
                   gru_cell->recurrent_weight_operand_id,
                   gru_cell->hidden_state_operand_id};
      auto& bias_operand_id = gru_cell->bias_operand_id;
      if (bias_operand_id) {
        input_ids.push_back(bias_operand_id.value());
      }
      auto& recurrent_bias_operand_id = gru_cell->recurrent_bias_operand_id;
      if (recurrent_bias_operand_id) {
        input_ids.push_back(recurrent_bias_operand_id.value());
      }
      output_ids = {gru_cell->output_operand_id};
      break;
    }
    case Operation::Tag::kHardSigmoid: {
      const auto& hard_sgmoid = operation->get_hard_sigmoid();
      input_ids = {hard_sgmoid->input_operand_id};
      output_ids = {hard_sgmoid->output_operand_id};
      break;
    }
    case Operation::Tag::kHardSwish: {
      const auto& hard_swish = operation->get_hard_swish();
      input_ids = {hard_swish->input_operand_id};
      output_ids = {hard_swish->output_operand_id};
      break;
    }
    case Operation::Tag::kInstanceNormalization: {
      const auto& instance_norm = operation->get_instance_normalization();
      input_ids = {instance_norm->input_operand_id};
      auto& scale_operand_id = instance_norm->scale_operand_id;
      if (scale_operand_id) {
        input_ids.push_back(scale_operand_id.value());
      }
      auto& bias_operand_id = instance_norm->bias_operand_id;
      if (bias_operand_id) {
        input_ids.push_back(bias_operand_id.value());
      }
      output_ids = {instance_norm->output_operand_id};
      break;
    }
    case Operation::Tag::kLayerNormalization: {
      const auto& layer_norm = operation->get_layer_normalization();
      input_ids = {layer_norm->input_operand_id};
      auto& scale_operand_id = layer_norm->scale_operand_id;
      if (scale_operand_id) {
        input_ids.push_back(scale_operand_id.value());
      }
      auto& bias_operand_id = layer_norm->bias_operand_id;
      if (bias_operand_id) {
        input_ids.push_back(bias_operand_id.value());
      }
      output_ids = {layer_norm->output_operand_id};
      break;
    }
    case Operation::Tag::kLeakyRelu: {
      const auto& leaky_relu = operation->get_leaky_relu();
      input_ids = {leaky_relu->input_operand_id};
      output_ids = {leaky_relu->output_operand_id};
      break;
    }
    case Operation::Tag::kLinear: {
      const auto& linear = operation->get_linear();
      input_ids = {linear->input_operand_id};
      output_ids = {linear->output_operand_id};
      break;
    }
    case Operation::Tag::kLstm: {
      const auto& lstm = operation->get_lstm();
      input_ids = {lstm->input_operand_id, lstm->weight_operand_id,
                   lstm->recurrent_weight_operand_id};
      auto& bias_operand_id = lstm->bias_operand_id;
      if (bias_operand_id) {
        input_ids.push_back(bias_operand_id.value());
      }
      auto& recurrent_bias_operand_id = lstm->recurrent_bias_operand_id;
      if (recurrent_bias_operand_id) {
        input_ids.push_back(recurrent_bias_operand_id.value());
      }
      auto& peephole_weight_operand_id = lstm->peephole_weight_operand_id;
      if (peephole_weight_operand_id) {
        input_ids.push_back(peephole_weight_operand_id.value());
      }
      auto& initial_hidden_state_operand_id =
          lstm->initial_hidden_state_operand_id;
      if (initial_hidden_state_operand_id) {
        input_ids.push_back(initial_hidden_state_operand_id.value());
      }
      auto& initial_cell_state_operand_id = lstm->initial_cell_state_operand_id;
      if (initial_cell_state_operand_id) {
        input_ids.push_back(initial_cell_state_operand_id.value());
      }
      output_ids = {lstm->output_operand_ids};
      break;
    }
    case Operation::Tag::kLstmCell: {
      const auto& lstm_cell = operation->get_lstm_cell();
      input_ids = {lstm_cell->input_operand_id, lstm_cell->weight_operand_id,
                   lstm_cell->recurrent_weight_operand_id,
                   lstm_cell->hidden_state_operand_id,
                   lstm_cell->cell_state_operand_id};
      auto& bias_operand_id = lstm_cell->bias_operand_id;
      if (bias_operand_id) {
        input_ids.push_back(bias_operand_id.value());
      }
      auto& recurrent_bias_operand_id = lstm_cell->recurrent_bias_operand_id;
      if (recurrent_bias_operand_id) {
        input_ids.push_back(recurrent_bias_operand_id.value());
      }
      auto& peephole_weight_operand_id = lstm_cell->peephole_weight_operand_id;
      if (peephole_weight_operand_id) {
        input_ids.push_back(peephole_weight_operand_id.value());
      }
      output_ids = {lstm_cell->output_operand_ids};
      break;
    }
    case Operation::Tag::kMatmul: {
      const auto& matmul = operation->get_matmul();
      input_ids = {matmul->a_operand_id, matmul->b_operand_id};
      output_ids = {matmul->output_operand_id};
      break;
    }
    case Operation::Tag::kPad: {
      const auto& pad = operation->get_pad();
      input_ids = {pad->input_operand_id};
      output_ids = {pad->output_operand_id};
      break;
    }
    case Operation::Tag::kPool2d: {
      const auto& pool2d = operation->get_pool2d();
      input_ids = {pool2d->input_operand_id};
      output_ids = {pool2d->output_operand_id};
      break;
    }
    case Operation::Tag::kPrelu: {
      const auto& prelu = operation->get_prelu();
      input_ids = {prelu->input_operand_id, prelu->slope_operand_id};
      output_ids = {prelu->output_operand_id};
      break;
    }
    case Operation::Tag::kQuantizeLinear: {
      const auto& quantize_linear = operation->get_quantize_linear();
      input_ids = {quantize_linear->input_operand_id,
                   quantize_linear->scale_operand_id,
                   quantize_linear->zero_point_operand_id};
      output_ids = {quantize_linear->output_operand_id};
      break;
    }
    case Operation::Tag::kReduce: {
      const auto& reduce = operation->get_reduce();
      input_ids = {reduce->input_operand_id};
      output_ids = {reduce->output_operand_id};
      break;
    }
    case Operation::Tag::kRelu: {
      const auto& relu = operation->get_relu();
      input_ids = {relu->input_operand_id};
      output_ids = {relu->output_operand_id};
      break;
    }
    case Operation::Tag::kResample2d: {
      const auto& resample2d = operation->get_resample2d();
      input_ids = {resample2d->input_operand_id};
      output_ids = {resample2d->output_operand_id};
      break;
    }
    case Operation::Tag::kReshape: {
      const auto& reshape = operation->get_reshape();
      input_ids = {reshape->input_operand_id};
      output_ids = {reshape->output_operand_id};
      break;
    }
    case Operation::Tag::kReverse: {
      const auto& reverse = operation->get_reverse();
      input_ids = {reverse->input_operand_id};
      output_ids = {reverse->output_operand_id};
      break;
    }
    case Operation::Tag::kScatterElements: {
      const auto& scatter_elements = operation->get_scatter_elements();
      input_ids = {scatter_elements->input_operand_id,
                   scatter_elements->indices_operand_id,
                   scatter_elements->updates_operand_id};
      output_ids = {scatter_elements->output_operand_id};
      break;
    }
    case Operation::Tag::kScatterNd: {
      const auto& scatter_nd = operation->get_scatter_nd();
      input_ids = {scatter_nd->input_operand_id, scatter_nd->indices_operand_id,
                   scatter_nd->updates_operand_id};
      output_ids = {scatter_nd->output_operand_id};
      break;
    }
    case Operation::Tag::kSigmoid: {
      const auto& sigmoid = operation->get_sigmoid();
      input_ids = {sigmoid->input_operand_id};
      output_ids = {sigmoid->output_operand_id};
      break;
    }
    case Operation::Tag::kSlice: {
      const auto& slice = operation->get_slice();
      input_ids = {slice->input_operand_id};
      output_ids = {slice->output_operand_id};
      break;
    }
    case Operation::Tag::kSoftmax: {
      const auto& softmax = operation->get_softmax();
      input_ids = {softmax->input_operand_id};
      output_ids = {softmax->output_operand_id};
      break;
    }
    case Operation::Tag::kSoftplus: {
      const auto& softplus = operation->get_softplus();
      input_ids = {softplus->input_operand_id};
      output_ids = {softplus->output_operand_id};
      break;
    }
    case Operation::Tag::kSoftsign: {
      const auto& softsign = operation->get_softsign();
      input_ids = {softsign->input_operand_id};
      output_ids = {softsign->output_operand_id};
      break;
    }
    case Operation::Tag::kSplit: {
      const auto& split = operation->get_split();
      input_ids = {split->input_operand_id};
      output_ids = {split->output_operand_ids};
      break;
    }
    case Operation::Tag::kTanh: {
      const auto& tanh = operation->get_tanh();
      input_ids = {tanh->input_operand_id};
      output_ids = {tanh->output_operand_id};
      break;
    }
    case Operation::Tag::kTile: {
      const auto& tile = operation->get_tile();
      input_ids = {tile->input_operand_id};
      output_ids = {tile->output_operand_id};
      break;
    }
    case Operation::Tag::kTranspose: {
      const auto& transpose = operation->get_transpose();
      input_ids = {transpose->input_operand_id};
      output_ids = {transpose->output_operand_id};
      break;
    }
    case Operation::Tag::kTriangular: {
      const auto& triangular = operation->get_triangular();
      input_ids = {triangular->input_operand_id};
      output_ids = {triangular->output_operand_id};
      break;
    }
    case Operation::Tag::kWhere: {
      const auto& where = operation->get_where();
      input_ids = {where->condition_operand_id, where->true_value_operand_id,
                   where->false_value_operand_id};
      output_ids = {where->output_operand_id};
      break;
    }
  }
}

// The struct contains the information of graph fusion. In `CreateAndBuild`
// method, when going through all operations to add each operation into the
// final graph, this struct will be used for graph fusion.
struct GraphFusionInfo {
  // A map of all standalone activations in `mojom::GraphInfo` which can be
  // fused into preceding operations.
  // The key is the preceding operation which can support fusion. The value is
  // the standalone activation which can be fused into the preceding operation.
  absl::flat_hash_map<const Operation*,
                      raw_ptr<const Operation, CtnExperimental>>
      operation_to_fusible_standalone_activation_map;

  // A map of all transposes that can be fused into the following matmul using
  // transpose's output operand id as the key.
  absl::flat_hash_map<OperandId, raw_ptr<const Operation, CtnExperimental>>
      output_id_to_fusible_transpose_map;

  // A set of all operations in `mojom::GraphInfo` which can be fused into
  // another operation. No DirectML operator node will be created for operations
  // in this set.
  absl::flat_hash_set<const Operation*> fusible_operations_set;
};

// The method gets the graph fusion information from `mojom::GraphInfo`, based
// on that the `operations` in `mojom::GraphInfo` have been in topological
// order which means if operation 'j' depends on 'i', 'i' must appear before
// 'j'.
// TODO(issues.chromium.org/41494177): Validate the topological order of
// operations in `mojom::GraphInfo` on services side.
GraphFusionInfo GetGraphFusionInfo(const mojom::GraphInfoPtr& graph_info) {
  // If it's disabled, just return empty 'GraphFusionInfo' object which means no
  // graph fusion will be applied.
  if (!base::FeatureList::IsEnabled(kApplyGraphFusion)) {
    return GraphFusionInfo();
  }

  // A map of all fusible activations in `mojom::GraphInfo` using activation's
  // input operand id as the key.
  absl::flat_hash_map<OperandId, raw_ptr<const Operation, CtnExperimental>>
      input_id_to_activation_map;

  // The case we're interested in includes a fusible base operation with exactly
  // one output edge, followed by a fusible activation operation:
  //
  //     [input]
  //        |
  //      conv2d (fusible base operation)
  //        |
  //       relu  (fusible activation operation)
  //        |
  //    [output]
  //
  // If the base operation has more than one output edge, because the outputs go
  // to any other operation or a graph output, then no fusion occurs. For
  // example, if `relu` was fused into `conv2d`, `elu` would lose the input, so
  // conv2d should be skipped, and similarly for graph `output2`:
  //
  //     [input]
  //        |
  //      conv2d (unfusible base operation)
  //      /    \
  //   relu    elu
  //     |      |
  // [output1][output2]
  //
  //     [input]
  //        |
  //      conv2d (unfusible base operation)
  //      /    \
  //   relu     \
  //     |       \
  // [output1] [output2]
  //
  // If the base operation is not followed by a fusible activation, skip
  // it:
  //
  //     [input]
  //        |
  //      conv2d (unfusible base operation)
  //        |
  //      pool2d
  //        |
  //     [output]
  //

  // A map of all matmul operations in `mojom::GraphInfo` using matmul's input
  // operand id as the key.
  absl::flat_hash_map<OperandId, const Operation*> input_id_to_matmul_map;

  // This is a scenario where transpose can be fused into the following matmul.
  // The transpose output solely feeds matmul. The transposed input can be
  // either on the input a, input b or both. The transpose should only swap
  // the last two axes (the row and column of the inner matrix), so it can be
  // fused into `TransA()` or `TransB()` of the following calculation that
  // DirectML `GEMM` operator performs:
  //
  // Output = FusedActivation(Alpha * TransA(A) x TransB(B) + Beta * C)
  //
  // See more details at:
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gemm_operator_desc
  //
  //     [input a]    [input b]
  //         |          /
  //     transpose     /
  //          \       /
  //           \     /
  //            matmul
  //
  // TODO(crbug.com/340729469): Remove the complex operator fusions when the
  // underlying DirectML runtime can handle.

  GraphFusionInfo graph_fusion_info;
  // Record how many times each operand id is used as one
  // operation's input edge or the graph's output edge.
  base::FixedArray<uint32_t> operand_id_to_use_count_map(
      graph_info->operands.size(), 0);

  for (OperandId graph_output_id : graph_info->output_operands) {
    ++operand_id_to_use_count_map[graph_output_id.value()];
  }

  // Iterate from the end of operations instead from the beginning, so we
  // can easily get the total use count of a fusible base operation's output
  // edge before visiting it.
  OperationConnectivity operation_connectivity;
  for (size_t operation_index = graph_info->operations.size();
       operation_index-- > 0;) {
    const auto& operation = graph_info->operations[operation_index];
    RetrieveOperationConnectivity(
        operation.get(),
        /*out_operation_connectivity*/ operation_connectivity);

    for (OperandId input_id : operation_connectivity.input_ids) {
      ++operand_id_to_use_count_map[input_id.value()];
    }

    // Try to find standalone activations that can be fused into preceding
    // operations.
    if (GetFusibleActivationOutputId(*operation)) {
      // We found a standalone activation operation that may need to be fused
      // with a predecessor. So record its input edge to later check
      // against any fusible base operation's corresponding output edge.
      CHECK_EQ(operation_connectivity.input_ids.size(), 1U);
      // We needn't check the result of `try_emplace` here, because if the key
      // `output_id` is already in container, there must be more than 1 output
      // edges from a predecessor in which case the fusion must be skipped.
      input_id_to_activation_map.try_emplace(
          operation_connectivity.input_ids[0], operation.get());
    } else if (CanFuseStandaloneActivation(operation.get(),
                                           graph_info->operands)) {
      CHECK_EQ(operation_connectivity.output_ids.size(), 1U);
      OperandId output_id = operation_connectivity.output_ids[0];
      // Add this operation to the fusion info if there's exactly one output
      // edge to a fusible standalone activation.
      const auto activation_iterator =
          input_id_to_activation_map.find(output_id);
      if (operand_id_to_use_count_map[output_id.value()] == 1 &&
          activation_iterator != input_id_to_activation_map.end()) {
        const auto* activation = activation_iterator->second.get();
        graph_fusion_info.fusible_operations_set.insert(activation);
        graph_fusion_info
            .operation_to_fusible_standalone_activation_map[operation.get()] =
            activation;
      }
    }

    // Try to find transposes that can be fused into following matmul
    // operations.
    switch (operation->which()) {
      case Operation::Tag::kMatmul: {
        // Map matmul's inputs to operation, so the following algorithm can find
        // a transpose whose output is consumed by a matmul.
        CHECK_EQ(operation_connectivity.input_ids.size(), 2U);
        // We needn't check the result of `try_emplace` here, because if the key
        // `input_id` is already in container, there must be more than 1 output
        // edges from a predecessor in which case the transpose fusion won't
        // happen.
        input_id_to_matmul_map.try_emplace(operation_connectivity.input_ids[0],
                                           operation.get());
        input_id_to_matmul_map.try_emplace(operation_connectivity.input_ids[1],
                                           operation.get());
        break;
      }
      case Operation::Tag::kTranspose: {
        // If a transpose's output is solely used by a matmul and it only swaps
        // the last two axes, it can be fused into DirectML GEMM operator by
        // setting corresponding input tensor transformation attribute.
        CHECK_EQ(operation_connectivity.output_ids.size(), 1U);
        OperandId output_id = operation_connectivity.output_ids[0];
        if (!input_id_to_matmul_map.contains(output_id) ||
            operand_id_to_use_count_map[output_id.value()] != 1) {
          break;
        }
        const mojom::TransposePtr& transpose = operation->get_transpose();
        const mojom::OperandPtr& input_operand =
            graph_info->operands.at(transpose->input_operand_id.value());
        uint32_t input_rank = input_operand->descriptor.shape().size();
        if (input_rank < 2) {
          break;
        }
        std::vector<uint32_t> swap_last_two_axes(input_rank);
        std::iota(swap_last_two_axes.begin(), swap_last_two_axes.end(), 0);
        std::swap(swap_last_two_axes[input_rank - 2],
                  swap_last_two_axes[input_rank - 1]);
        if (swap_last_two_axes == transpose->permutation) {
          graph_fusion_info.fusible_operations_set.insert(operation.get());
          graph_fusion_info.output_id_to_fusible_transpose_map[output_id] =
              operation.get();
        }
        break;
      }
      default: {
        // Skip other operations.
        break;
      }
    }
  }

  CHECK_EQ(
      graph_fusion_info.operation_to_fusible_standalone_activation_map.size() +
          graph_fusion_info.output_id_to_fusible_transpose_map.size(),
      graph_fusion_info.fusible_operations_set.size());

  return graph_fusion_info;
}

void CreateOperatorNodeForBatchNormalization(
    Adapter* adapter,
    const ContextProperties& context_properties,
    const Operation* operation,
    const absl::flat_hash_map<const Operation*,
                              raw_ptr<const Operation, CtnExperimental>>&
        operation_to_fusible_standalone_activation_map,
    mojom::GraphInfoPtr& graph_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map,
    absl::flat_hash_map<OperandId, uint32_t>& constant_id_to_input_index_map) {
  const auto& batch_normalization = operation->get_batch_normalization();
  const auto& operands = graph_info->operands;

  OperandId input_id = batch_normalization->input_operand_id;
  const OperandPtr& input_operand = operands.at(input_id.value());
  CHECK(context_properties.data_type_limits.batch_normalization_input.Supports(
      input_operand->descriptor));

  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, input_id);
  const TensorDesc& input_tensor_desc = input->GetTensorDesc();
  const auto input_rank = input_tensor_desc.GetDimensions().size();

  OperandId output_id = batch_normalization->output_operand_id;
  const OperandPtr& output_operand = operands.at(output_id.value());
  OperandDataType data_type = output_operand->descriptor.data_type();
  CHECK(context_properties.data_type_limits.batch_normalization_input.data_types
            .Has(data_type));

  const TensorDesc output_tensor_desc(GetTensorDataType(data_type),
                                      output_operand->descriptor.shape());

  const NodeOutput* mean = GetNodeOutputForOperand(
      id_to_node_output_map, batch_normalization->mean_operand_id);
  auto mean_tensor_desc = mean->GetTensorDesc();
  auto mean_rank = mean_tensor_desc.GetDimensions().size();
  CHECK_EQ(mean_rank, 1U);

  auto axis = batch_normalization->axis;
  uint32_t axes[1] = {axis};

  // In WebNN spec, mean operand is specified as a 1-D tensor and its size equal
  // to the size of the input dimension denoted by axis. But for DML,
  // InputTensor and MeanTensor must have the same DimensionCount -
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_batch_normalization_operator_desc.
  mean_tensor_desc.MakeBroadcastCompatible(input_rank, axes);

  const NodeOutput* variance = GetNodeOutputForOperand(
      id_to_node_output_map, batch_normalization->variance_operand_id);
  auto variance_tensor_desc = variance->GetTensorDesc();
  auto variance_rank = variance_tensor_desc.GetDimensions().size();
  CHECK_EQ(variance_rank, 1U);

  // In WebNN spec, variance operand is specified as a 1-D tensor and its size
  // equal to the size of the input dimension denoted by axis. But for DML,
  // InputTensor and VarianceTensor must have the same DimensionCount -
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_batch_normalization_operator_desc.
  variance_tensor_desc.MakeBroadcastCompatible(input_rank, axes);

  OperandId scale_operand_id;
  if (batch_normalization->scale_operand_id.has_value()) {
    scale_operand_id = batch_normalization->scale_operand_id.value();
  } else {
    // If the scale is not present, create a constant operand for scale and
    // insert the operand into the graph.
    scale_operand_id = BuildConstantOperandForFloatValue(
        context_properties, graph_info, constant_operands, data_type,
        /*rank*/ 1, /*default scale*/ 1.0);
    CreateConstantNode(adapter, scale_operand_id, constant_operands,
                       graph_builder, id_to_node_output_map,
                       constant_id_to_input_index_map);
  }

  const NodeOutput* scale =
      GetNodeOutputForOperand(id_to_node_output_map, scale_operand_id);
  auto scale_tensor_desc = scale->GetTensorDesc();
  auto scale_rank = scale_tensor_desc.GetDimensions().size();
  CHECK_EQ(scale_rank, 1U);

  // In WebNN spec, scale operand is specified as a 1-D tensor and its size
  // equal to the size of the input dimension denoted by axis. But for DML,
  // InputTensor and ScaleTensor must have the same DimensionCount -
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_batch_normalization_operator_desc.
  scale_tensor_desc.MakeBroadcastCompatible(input_rank, axes);

  OperandId bias_operand_id;
  if (batch_normalization->bias_operand_id.has_value()) {
    bias_operand_id = batch_normalization->bias_operand_id.value();
  } else {
    // If the bias is not present, create a constant operand for bias and insert
    // the operand into the graph.
    bias_operand_id = BuildConstantOperandForFloatValue(
        context_properties, graph_info, constant_operands, data_type,
        /*rank*/ 1, /*default bias*/ 0);
    CreateConstantNode(adapter, bias_operand_id, constant_operands,
                       graph_builder, id_to_node_output_map,
                       constant_id_to_input_index_map);
  }

  const NodeOutput* bias =
      GetNodeOutputForOperand(id_to_node_output_map, bias_operand_id);
  auto bias_tensor_desc = bias->GetTensorDesc();
  auto bias_rank = bias_tensor_desc.GetDimensions().size();
  CHECK_EQ(bias_rank, 1U);

  // In WebNN spec, bias operand is specified as a 1-D tensor and its size
  // equal to the size of the input dimension denoted by axis. But for DML,
  // InputTensor and BiasTensor must have the same DimensionCount -
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_batch_normalization_operator_desc.
  bias_tensor_desc.MakeBroadcastCompatible(input_rank, axes);

  std::array<const NodeOutput*, 5> inputs = {input, mean, variance, scale,
                                             bias};

  std::optional<const Operation*> fusible_activation =
      GetFusibleActivationFromOperation(
          operation_to_fusible_standalone_activation_map, operation);
  std::optional<ActivationOperatorDesc> activation_operator_desc;
  std::optional<DML_OPERATOR_DESC> activation_dml_desc;
  std::string label = batch_normalization->label;
  if (fusible_activation) {
    activation_operator_desc =
        CreateOperatorDescForFusibleActivation(*fusible_activation.value());
    output_id =
        GetFusibleActivationOutputId(*fusible_activation.value()).value();
    activation_dml_desc = activation_operator_desc->GetActivationDmlDesc();
    label = GetFusedOperatorLabel(label, "batch_normalization",
                                  *fusible_activation.value());
  }

  DML_BATCH_NORMALIZATION_OPERATOR_DESC batch_normalization_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .MeanTensor = &mean_tensor_desc.GetDMLTensorDesc(),
      .VarianceTensor = &variance_tensor_desc.GetDMLTensorDesc(),
      .ScaleTensor = &scale_tensor_desc.GetDMLTensorDesc(),
      .BiasTensor = &bias_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      // Spatial is used to specify whether locations are spatial.
      // This parameter was deprecated in DML_FEATURE_LEVEL_4_0, and has no
      // effect.
      .Spatial = true,
      .Epsilon = batch_normalization->epsilon,
      .FusedActivation =
          activation_dml_desc ? &activation_dml_desc.value() : nullptr,
  };

  const GraphNode* batch_normalization_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_BATCH_NORMALIZATION, &batch_normalization_operator_desc,
      inputs, label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      batch_normalization_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForClamp(Adapter* adapter,
                                const ContextProperties& context_properties,
                                const std::vector<OperandPtr>& operands,
                                const mojom::ClampPtr& clamp,
                                GraphBuilderDml& graph_builder,
                                IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, clamp->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  CHECK(context_properties.data_type_limits.clamp_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  OperandId output_id = clamp->output_operand_id;
  auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  const GraphNode* clamp_node = nullptr;
  std::array<const NodeOutput*, 1> inputs = {input};

  if (adapter->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_5_0)) {
    DML_ELEMENT_WISE_CLIP1_OPERATOR_DESC clamp_operator_desc{
        .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
        .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
        // No scale or bias applies to the input.
        .ScaleBias = nullptr,
        .MinMaxDataType = output_tensor_desc.GetDataType(),
        .Min =
            ToScalarUnion(clamp->min_value, output_tensor_desc.GetDataType()),
        .Max =
            ToScalarUnion(clamp->max_value, output_tensor_desc.GetDataType())};
    clamp_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_ELEMENT_WISE_CLIP1, &clamp_operator_desc, inputs,
        clamp->label);
  } else {
    DML_ELEMENT_WISE_CLIP_OPERATOR_DESC clamp_operator_desc{
        .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
        .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
        // No scale or bias applies to the input.
        .ScaleBias = nullptr,
        .Min = clamp->min_value,
        .Max = clamp->max_value};
    clamp_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_ELEMENT_WISE_CLIP, &clamp_operator_desc, inputs,
        clamp->label);
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      clamp_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForConcat(const ContextProperties& context_properties,
                                 const std::vector<OperandPtr>& operands,
                                 const mojom::ConcatPtr& concat,
                                 GraphBuilderDml& graph_builder,
                                 IdToNodeOutputMap& id_to_node_output_map) {
  const auto& input_operand_ids = concat->input_operand_ids;
  CHECK(std::ranges::all_of(input_operand_ids, [&](OperandId input_operand_id) {
    return context_properties.data_type_limits.concat_inputs.Supports(
        operands.at(input_operand_id.value())->descriptor);
  }));

  size_t input_num = input_operand_ids.size();
  base::FixedArray<const NodeOutput*> inputs(input_num);
  base::FixedArray<DML_TENSOR_DESC> input_dml_tensor_descs(input_num);
  for (size_t i = 0; i < input_num; ++i) {
    const NodeOutput* input =
        GetNodeOutputForOperand(id_to_node_output_map, input_operand_ids[i]);
    inputs[i] = input;
    input_dml_tensor_descs[i] = input->GetTensorDesc().GetDMLTensorDesc();
  }

  OperandId output_id = concat->output_operand_id;
  auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  DML_JOIN_OPERATOR_DESC concat_operator_desc{
      .InputCount = base::checked_cast<uint32_t>(input_dml_tensor_descs.size()),
      .InputTensors = input_dml_tensor_descs.data(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Axis = concat->axis};

  const GraphNode* concat_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_JOIN, &concat_operator_desc, inputs, concat->label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      concat_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForConv2d(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const Operation* operation,
    const absl::flat_hash_map<const Operation*,
                              raw_ptr<const Operation, CtnExperimental>>&
        operation_to_fusible_standalone_activation_map,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& conv2d = operation->get_conv2d();

  const OperandPtr& input_operand =
      operands.at(conv2d->input_operand_id.value());
  const OperandPtr& filter_operand =
      operands.at(conv2d->filter_operand_id.value());
  DML_CONVOLUTION_DIRECTION conv2d_direction;
  switch (conv2d->kind) {
    case mojom::Conv2d::Kind::kDirect: {
      CHECK(context_properties.data_type_limits.conv2d_input.SupportsAll(
          {input_operand->descriptor, filter_operand->descriptor}));
      conv2d_direction =
          DML_CONVOLUTION_DIRECTION::DML_CONVOLUTION_DIRECTION_FORWARD;
      break;
    }
    case mojom::Conv2d::Kind::kTransposed: {
      CHECK(context_properties.data_type_limits.conv_transpose2d_input
                .SupportsAll(
                    {input_operand->descriptor, filter_operand->descriptor}));
      conv2d_direction =
          DML_CONVOLUTION_DIRECTION::DML_CONVOLUTION_DIRECTION_BACKWARD;
      break;
    }
  }

  OperandId output_id = conv2d->output_operand_id;
  // The output tensor description may be transposed.
  auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  CHECK_EQ(output_tensor_desc.GetDimensions().size(), 4u);

  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, conv2d->input_operand_id);
  // The input tensor description may be transposed.
  auto input_tensor_desc = input->GetTensorDesc();
  const NodeOutput* filter =
      GetNodeOutputForOperand(id_to_node_output_map, conv2d->filter_operand_id);
  auto filter_tensor_desc = filter->GetTensorDesc();
  std::vector<const NodeOutput*> inputs = {input, filter};
  std::optional<TensorDesc> reshaped_bias_tensor_desc;
  auto& bias_operand_id = conv2d->bias_operand_id;
  if (bias_operand_id) {
    const OperandPtr& bias_operand = operands.at(*bias_operand_id.value());
    if (conv2d->kind == mojom::Conv2d::Kind::kDirect) {
      CHECK(context_properties.data_type_limits.conv2d_bias.Supports(
          bias_operand->descriptor));
    } else {
      CHECK(context_properties.data_type_limits.conv_transpose2d_bias.Supports(
          bias_operand->descriptor));
    }

    const NodeOutput* bias_node_output =
        GetNodeOutputForOperand(id_to_node_output_map, bias_operand_id.value());
    const auto& bias_tensor_desc = bias_node_output->GetTensorDesc();
    const auto& bias_dims = bias_tensor_desc.GetDimensions();

    // In WebNN spec bias specifies the additional 1-D tensor with the shape of
    // {outputChannels}. But for DML the expected dimensions of the BiasTensor
    // are { 1, OutputChannelCount, 1, 1 } for 4D. So reshape the bias:
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_convolution_operator_desc
    std::vector<uint32_t> reshaped_bias_dims = {1, bias_dims[0], 1, 1};
    reshaped_bias_tensor_desc =
        TensorDesc(bias_tensor_desc.GetDataType(), bias_tensor_desc.GetFlags(),
                   std::move(reshaped_bias_dims));

    const NodeOutput* reshaped_bias_node_output =
        graph_builder.CreateNodeOutput(&bias_node_output->GetNode(),
                                       reshaped_bias_tensor_desc.value());
    inputs.push_back(reshaped_bias_node_output);
  }

  std::array<uint32_t, 2> strides = {conv2d->strides->height,
                                     conv2d->strides->width};
  std::array<uint32_t, 2> dilations = {conv2d->dilations->height,
                                       conv2d->dilations->width};
  std::array<uint32_t, 2> start_padding = {conv2d->padding->beginning->height,
                                           conv2d->padding->beginning->width};
  std::array<uint32_t, 2> end_padding = {conv2d->padding->ending->height,
                                         conv2d->padding->ending->width};

  // The outputSizes of WebNN convTranspose2d specifies the sizes of the last
  // two dimensions of the output tensor but the outputPadding of DirectML
  // convolution applies a zero padding to the result of the operator. Since
  // graph builder will explicitly pass in the output tensor shape anyway. So,
  // there is no ambiguity of the output shape and we set the output_padding to
  // {0, 0}:
  // https://www.w3.org/TR/webnn/#dom-mlconvtranspose2doptions-outputpadding
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_convolution_operator_desc
  std::array<uint32_t, 2> default_out_padding = {0, 0};

  std::optional<const Operation*> fusible_activation =
      GetFusibleActivationFromOperation(
          operation_to_fusible_standalone_activation_map, operation);
  std::optional<ActivationOperatorDesc> activation_operator_desc;
  std::optional<DML_OPERATOR_DESC> activation_dml_desc;
  std::string label = conv2d->label;
  if (fusible_activation) {
    activation_operator_desc =
        CreateOperatorDescForFusibleActivation(*fusible_activation.value());
    output_id =
        GetFusibleActivationOutputId(*fusible_activation.value()).value();
    activation_dml_desc = activation_operator_desc->GetActivationDmlDesc();
    label = GetFusedOperatorLabel(label, "conv2d", *fusible_activation.value());
  }

  DML_CONVOLUTION_OPERATOR_DESC conv2d_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .FilterTensor = &filter_tensor_desc.GetDMLTensorDesc(),
      .BiasTensor = GetOptionalDmlTensorDescPtr(reshaped_bias_tensor_desc),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION,
      .Direction = conv2d_direction,
      .DimensionCount =
          2u, /*Determines the size of the Strides, Dilations, StartPadding,
                 EndPadding, and OutputPadding arrays.*/
      .Strides = strides.data(),
      .Dilations = dilations.data(),
      .StartPadding = start_padding.data(),
      .EndPadding = end_padding.data(),
      .OutputPadding = default_out_padding.data(),
      .GroupCount = conv2d->groups,
      .FusedActivation =
          activation_dml_desc ? &activation_dml_desc.value() : nullptr,
  };

  const GraphNode* conv2d_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_CONVOLUTION, &conv2d_operator_desc, inputs, label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      conv2d_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForCumulativeSum(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const mojom::CumulativeSumPtr& cumulative_sum,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, cumulative_sum->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  CHECK(context_properties.data_type_limits.cumulative_sum_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  OperandId output_id = cumulative_sum->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  const uint32_t axis = cumulative_sum->axis;
  DML_AXIS_DIRECTION axis_direction =
      cumulative_sum->reversed
          ? DML_AXIS_DIRECTION::DML_AXIS_DIRECTION_DECREASING
          : DML_AXIS_DIRECTION::DML_AXIS_DIRECTION_INCREASING;
  DML_CUMULATIVE_SUMMATION_OPERATOR_DESC cumulative_sum_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Axis = axis,
      .AxisDirection = axis_direction,
      .HasExclusiveSum = cumulative_sum->exclusive};

  std::array<const NodeOutput*, 1> inputs = {input};
  const std::string& label = cumulative_sum->label;
  const GraphNode* cumulative_sum_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_CUMULATIVE_SUMMATION, &cumulative_sum_operator_desc, inputs,
      label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      cumulative_sum_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

template <typename DML_OPERATOR_DESC, DML_OPERATOR_TYPE operator_type>
const GraphNode* CreateUnaryOperator(const TensorDesc& input_tensor,
                                     const TensorDesc& output_tensor,
                                     const NodeOutput* input,
                                     GraphBuilderDml& graph_builder,
                                     std::string_view label = "") {
  DML_OPERATOR_DESC unary_operator_desc{
      .InputTensor = &input_tensor.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor.GetDMLTensorDesc()};
  std::array<const NodeOutput*, 1> inputs = {input};
  return graph_builder.CreateOperatorNode(operator_type, &unary_operator_desc,
                                          inputs, label);
}

template <typename DML_OPERATOR_DESC>
const GraphNode* CreateBinaryOperator(const TensorDesc& a_tensor,
                                      const TensorDesc& b_tensor,
                                      const TensorDesc& output_tensor,
                                      GraphBuilderDml& graph_builder,
                                      DML_OPERATOR_TYPE operator_type,
                                      base::span<const NodeOutput*> inputs,
                                      std::string_view label) {
  DML_OPERATOR_DESC binary_operator_desc{
      .ATensor = &a_tensor.GetDMLTensorDesc(),
      .BTensor = &b_tensor.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor.GetDMLTensorDesc()};
  return graph_builder.CreateOperatorNode(operator_type, &binary_operator_desc,
                                          inputs, label);
}

// Append an identity node to the input node output. Return the node output of
// the identity operator if it's successfully created, otherwise return a
// nullptr.
const NodeOutput* AppendIdentityNode(
    GraphBuilderDml& graph_builder,
    const NodeOutput* input,
    const TensorDesc* input_tensor_desc = nullptr) {
  CHECK(input);
  if (!input_tensor_desc) {
    input_tensor_desc = &input->GetTensorDesc();
  }
  TensorDesc identity_tensor_desc(input_tensor_desc->GetDataType(),
                                  DML_TENSOR_FLAG_NONE,
                                  input_tensor_desc->GetDimensions());
  const GraphNode* identity =
      CreateUnaryOperator<DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC,
                          DML_OPERATOR_ELEMENT_WISE_IDENTITY>(
          *input_tensor_desc, identity_tensor_desc, input, graph_builder);

  return graph_builder.CreateNodeOutput(identity,
                                        std::move(identity_tensor_desc));
}

// Create a reshape node with the given new shape.
const NodeOutput* CreateReshapeNode(GraphBuilderDml& graph_builder,
                                    const NodeOutput* input,
                                    base::span<const uint32_t> new_shape) {
  CHECK(input);
  const auto& input_tensor_desc = input->GetTensorDesc();
  const TensorDesc reshaped_input_tensor_desc(
      input_tensor_desc.GetDataType(), input_tensor_desc.GetFlags(),
      std::vector<uint32_t>(new_shape.begin(), new_shape.end()));
  const NodeOutput* reshape_node =
      AppendIdentityNode(graph_builder, input, &reshaped_input_tensor_desc);

  return reshape_node;
}

// Create a expand node with the given new shape.
const NodeOutput* CreateExpandNode(GraphBuilderDml& graph_builder,
                                   const NodeOutput* input,
                                   base::span<const uint32_t> new_shape,
                                   std::string_view label) {
  CHECK(input);
  auto input_tensor_desc = input->GetTensorDesc();
  // Use identity to implement the expand operation with broadcasting strides
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-strides#broadcasting-with-strides.
  if (input_tensor_desc.GetDimensions() != new_shape) {
    input_tensor_desc.BroadcastTo(new_shape);
  }

  const auto expand_tensor_desc =
      TensorDesc(input_tensor_desc.GetDataType(),
                 std::vector<uint32_t>(new_shape.begin(), new_shape.end()));
  const GraphNode* identity_node =
      CreateUnaryOperator<DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC,
                          DML_OPERATOR_ELEMENT_WISE_IDENTITY>(
          input_tensor_desc, expand_tensor_desc, input, graph_builder, label);

  const NodeOutput* expand_node = graph_builder.CreateNodeOutput(
      identity_node, std::move(expand_tensor_desc));
  return expand_node;
}

// Block-wise expand the dimension of the node_tensor_desc along the given axis.
// Firstly, reshape the input to 4D by flattening consecutive dimensions before
// and after the axis when the dimension count >= 4. otherwise, we
// need to insert value 1 before and after the axis. Then broadcast axis by
// block_size. Finally, reshape it back to output dimensions. For example,
// given an 4-D input dimensions = {2, 3, 4, 5} and axis = 2, block_size = 3,
// Firstly, we reshape the input dimensions to {6, 4, 1, 5}. Then broadcast {6,
// 4, 1, 5} to {6, 4, 3, 5}. Finally, reshape {6, 4, 3, 5} to {2, 3, 12, 5}.
base::expected<const NodeOutput*, mojom::ErrorPtr> BlockwiseExpandAlongAxis(
    const NodeOutput* node,
    GraphBuilderDml& graph_builder,
    uint32_t axis,
    uint32_t block_size,
    std::string_view label) {
  auto node_tensor_desc = node->GetTensorDesc();
  auto input_dimensions = node_tensor_desc.GetDimensions();
  std::array<uint32_t, 4> reshaped_input_dimensions;

  // TODO: Validation work will be completed at blink-side, and DirectML backend
  // should just check it.
  base::CheckedNumeric<uint32_t> checked_pre_values =
      std::accumulate(input_dimensions.begin(), input_dimensions.begin() + axis,
                      base::CheckedNumeric<uint32_t>(1), std::multiplies());
  if (!checked_pre_values.IsValid()) {
    return base::unexpected(CreateError(
        mojom::Error::Code::kUnknownError,
        "The shape values are too large for block-wise quantization emulation.",
        label));
  }
  reshaped_input_dimensions[0] = checked_pre_values.ValueOrDie();
  reshaped_input_dimensions[1] = input_dimensions[axis];
  reshaped_input_dimensions[2] = 1;

  // TODO: Validation work will be completed at blink-side, and DirectML backend
  // should just check it.
  base::CheckedNumeric<uint32_t> checked_after_values = std::accumulate(
      input_dimensions.begin() + axis + 1, input_dimensions.end(),
      base::CheckedNumeric<uint32_t>(1), std::multiplies());
  if (!checked_after_values.IsValid()) {
    return base::unexpected(CreateError(
        mojom::Error::Code::kUnknownError,
        "The shape values are too large for block-wise quantization emulation.",
        label));
  }
  reshaped_input_dimensions[3] = checked_after_values.ValueOrDie();

  const NodeOutput* reshape_node =
      CreateReshapeNode(graph_builder, node, reshaped_input_dimensions);

  auto expanded_new_operand_dimensions = reshaped_input_dimensions;
  expanded_new_operand_dimensions[2] = block_size;
  const NodeOutput* expand_reshaped_node = CreateExpandNode(
      graph_builder, reshape_node, expanded_new_operand_dimensions, label);

  auto output_dimensions = input_dimensions;
  output_dimensions[axis] = block_size * input_dimensions[axis];

  return CreateReshapeNode(graph_builder, expand_reshaped_node,
                           output_dimensions);
}

template <typename DML_OPERATOR_DESC, typename DequantizeOrQuantizeLinearPtr>
  requires((std::is_same_v<DequantizeOrQuantizeLinearPtr,
                           mojom::DequantizeLinearPtr> ||
            std::is_same_v<DequantizeOrQuantizeLinearPtr,
                           mojom::QuantizeLinearPtr>) &&
           (std::is_same_v<DML_OPERATOR_DESC, DML_QUANTIZE_OPERATOR_DESC> ||
            std::is_same_v<DML_OPERATOR_DESC, DML_DEQUANTIZE_OPERATOR_DESC> ||
            std::is_same_v<DML_OPERATOR_DESC,
                           DML_ELEMENT_WISE_DEQUANTIZE_LINEAR_OPERATOR_DESC> ||
            std::is_same_v<DML_OPERATOR_DESC,
                           DML_ELEMENT_WISE_QUANTIZE_LINEAR_OPERATOR_DESC>))
base::expected<void, mojom::ErrorPtr>
CreateOperatorNodeForDequantizeOrQuantizeLinear(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const DequantizeOrQuantizeLinearPtr& operation_ptr,
    GraphBuilderDml& graph_builder,
    DML_OPERATOR_TYPE operator_type,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, operation_ptr->input_operand_id);
  auto input_tensor_desc = input->GetTensorDesc();

  const NodeOutput* scale = GetNodeOutputForOperand(
      id_to_node_output_map, operation_ptr->scale_operand_id);
  auto scale_tensor_desc = scale->GetTensorDesc();

  const NodeOutput* zero_point = GetNodeOutputForOperand(
      id_to_node_output_map, operation_ptr->zero_point_operand_id);
  auto zero_point_tensor_desc = zero_point->GetTensorDesc();

  OperandId output_id = operation_ptr->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  auto adjusted_output_tensor_desc = output_tensor_desc;
  const auto& output_dimensions = output_tensor_desc.GetDimensions();
  const std::string& label = operation_ptr->label;

  if constexpr (std::is_same_v<DML_OPERATOR_DESC, DML_QUANTIZE_OPERATOR_DESC> ||
                std::is_same_v<DML_OPERATOR_DESC,
                               DML_DEQUANTIZE_OPERATOR_DESC>) {
    const auto input_rank = input_tensor_desc.GetDimensions().size();
    // DML_QUANTIZE_OPERATOR_DESC and DML_DEQUANTIZE_OPERATOR_DESC constraint
    // scale and zeroPoint must have the same dimension rank with input.
    if (scale_tensor_desc.GetDimensions().size() < input_rank) {
      scale_tensor_desc.EnsureMinimumRank(input_rank,
                                          TensorDesc::Alignment::kTrailing);
      zero_point_tensor_desc.EnsureMinimumRank(
          input_rank, TensorDesc::Alignment::kTrailing);
    }

    // A invalid parameter error will be reported from DirectML when a
    // dequantize is followed by a matmul and the input rank of the dequantize
    // is smaller than 4 which found in phi-3 mini model. This is a workaround
    // for reshape the input rank to 4D and onnxruntime DML EP also did this -
    // https://github.com/microsoft/onnxruntime/blob/main/onnxruntime/core/providers/dml/DmlExecutionProvider/src/Operators/DmlOperatorElementWise.cpp#L625.
    // TODO(crbug.com/376777334): Remove these workaround code after DirectML
    // fixes this bug.
    if (input_rank < 4) {
      input_tensor_desc.EnsureMinimumRank(4, TensorDesc::Alignment::kTrailing);
      scale_tensor_desc.EnsureMinimumRank(4, TensorDesc::Alignment::kTrailing);
      zero_point_tensor_desc.EnsureMinimumRank(
          4, TensorDesc::Alignment::kTrailing);
      adjusted_output_tensor_desc.EnsureMinimumRank(
          4, TensorDesc::Alignment::kTrailing);
    }
  } else {
    const auto input_dimensions = input_tensor_desc.GetDimensions();
    auto scale_dimensions = scale_tensor_desc.GetDimensions();
    // When FL < 6.3, DML_ELEMENT_WISE_DEQUANTIZE_LINEAR and
    // DML_ELEMENT_WISE_QUANTIZE_LINEAR can't support block-wise.
    // For each dimension where we need to do expansion of block_size which is
    // calculated by input_dimensions[i] / scale_dimensions[i], we use reshape
    // and broadcast to emulate.
    for (size_t index = 0; index < scale_dimensions.size(); index++) {
      if (input_dimensions[input_dimensions.size() - index - 1] !=
              scale_dimensions[scale_dimensions.size() - index - 1] &&
          input_dimensions[input_dimensions.size() - index - 1] != 1 &&
          scale_dimensions[scale_dimensions.size() - index - 1] != 1) {
        uint32_t block_size =
            input_dimensions[input_dimensions.size() - index - 1] /
            scale_dimensions[scale_dimensions.size() - index - 1];
        uint32_t axis = scale_dimensions.size() - index - 1;

        ASSIGN_OR_RETURN(scale,
                         BlockwiseExpandAlongAxis(scale, graph_builder, axis,
                                                  block_size, label));
        scale_tensor_desc = scale->GetTensorDesc();
        scale_dimensions = scale_tensor_desc.GetDimensions();

        ASSIGN_OR_RETURN(zero_point,
                         BlockwiseExpandAlongAxis(zero_point, graph_builder,
                                                  axis, block_size, label));

        zero_point_tensor_desc = zero_point->GetTensorDesc();
      }
    }

    if (scale_tensor_desc.GetDimensions() != output_dimensions) {
      scale_tensor_desc.BroadcastTo(output_dimensions);
      zero_point_tensor_desc.BroadcastTo(output_dimensions);
    }
  }

  if constexpr (std::is_same_v<DequantizeOrQuantizeLinearPtr,
                               mojom::DequantizeLinearPtr>) {
    CHECK(context_properties.data_type_limits.dequantize_linear_input.data_types
              .Has(DmlDataTypeToOperand(input_tensor_desc.GetDataType())));
    CHECK(context_properties.data_type_limits.dequantize_linear_scale.data_types
              .Has(DmlDataTypeToOperand(scale_tensor_desc.GetDataType())));
    CHECK(context_properties.data_type_limits.dequantize_linear_zero_point
              .data_types.Has(
                  DmlDataTypeToOperand(zero_point_tensor_desc.GetDataType())));
  } else /* `DequantizeOrQuantizeLinearPtr` is `mojom::QuantizeLinearPtr` */ {
    CHECK(context_properties.data_type_limits.quantize_linear_input.data_types
              .Has(DmlDataTypeToOperand(input_tensor_desc.GetDataType())));
    CHECK(context_properties.data_type_limits.quantize_linear_input.data_types
              .Has(DmlDataTypeToOperand(scale_tensor_desc.GetDataType())));
    CHECK(context_properties.data_type_limits.quantize_linear_zero_point
              .data_types.Has(
                  DmlDataTypeToOperand(zero_point_tensor_desc.GetDataType())));
  }

  DML_OPERATOR_DESC operator_desc;
  std::array<DML_TENSOR_DESC, 2> quantization_tensors = {
      scale_tensor_desc.GetDMLTensorDesc(),
      zero_point_tensor_desc.GetDMLTensorDesc()};
  if constexpr (std::is_same_v<DML_OPERATOR_DESC, DML_QUANTIZE_OPERATOR_DESC> ||
                std::is_same_v<DML_OPERATOR_DESC,
                               DML_DEQUANTIZE_OPERATOR_DESC>) {
    operator_desc = {
        .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
        .QuantizationType = DML_QUANTIZATION_TYPE_SCALE_ZERO_POINT,
        .QuantizationTensorCount = quantization_tensors.size(),
        .QuantizationTensors = quantization_tensors.data(),
        .OutputTensor = &adjusted_output_tensor_desc.GetDMLTensorDesc()};
  } else {
    operator_desc = {
        .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
        .ScaleTensor = &scale_tensor_desc.GetDMLTensorDesc(),
        .ZeroPointTensor = &zero_point_tensor_desc.GetDMLTensorDesc(),
        .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};
  }
  std::array<const NodeOutput*, 3> inputs = {input, scale, zero_point};
  const GraphNode* operator_node = graph_builder.CreateOperatorNode(
      operator_type, &operator_desc, inputs, label);
  const NodeOutput* node_output = graph_builder.CreateNodeOutput(
      operator_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);

  return base::ok();
}

template <typename OperatorDesc,
          DML_OPERATOR_TYPE operator_type,
          typename Operation>
void CreateOperatorNodeForUnary(const std::vector<OperandPtr>& operands,
                                const Operation& operation,
                                GraphBuilderDml& graph_builder,
                                IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, operation->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  OperandId output_id = operation->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  const GraphNode* unary_node =
      CreateUnaryOperator<OperatorDesc, operator_type>(
          input_tensor_desc, output_tensor_desc, input, graph_builder,
          operation->label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      unary_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForBinary(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const Operation* operation,
    const absl::flat_hash_map<const Operation*,
                              raw_ptr<const Operation, CtnExperimental>>&
        operation_to_fusible_standalone_activation_map,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& binary = operation->get_element_wise_binary();
  // The input a and b tensor descriptions may be broadcasted.
  const NodeOutput* input_a =
      GetNodeOutputForOperand(id_to_node_output_map, binary->lhs_operand_id);
  auto input_a_tensor_desc = input_a->GetTensorDesc();
  const NodeOutput* input_b =
      GetNodeOutputForOperand(id_to_node_output_map, binary->rhs_operand_id);
  auto input_b_tensor_desc = input_b->GetTensorDesc();

  OperandId output_id = binary->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  auto output_dimensions = output_tensor_desc.GetDimensions();
  if (input_a_tensor_desc.GetDimensions() != output_dimensions) {
    input_a_tensor_desc.BroadcastTo(output_dimensions);
  }
  if (input_b_tensor_desc.GetDimensions() != output_dimensions) {
    input_b_tensor_desc.BroadcastTo(output_dimensions);
  }

  CHECK_EQ(input_a_tensor_desc.GetDataType(),
           input_b_tensor_desc.GetDataType());

  const OperandDataType input_data_type =
      DmlDataTypeToOperand(input_a_tensor_desc.GetDataType());
  std::string label = binary->label;
  const GraphNode* binary_node = nullptr;
  std::array<const NodeOutput*, 2> inputs = {input_a, input_b};
  switch (binary->kind) {
    case mojom::ElementWiseBinary::Kind::kAdd: {
      CHECK(context_properties.data_type_limits.add_input.data_types.Has(
          input_data_type));
      std::optional<const Operation*> fusible_activation =
          GetFusibleActivationFromOperation(
              operation_to_fusible_standalone_activation_map, operation);
      if (fusible_activation) {
        ActivationOperatorDesc activation_operator_desc =
            CreateOperatorDescForFusibleActivation(*fusible_activation.value());
        DML_OPERATOR_DESC activation_dml_desc =
            activation_operator_desc.GetActivationDmlDesc();

        DML_ELEMENT_WISE_ADD1_OPERATOR_DESC add1_operator_desc{
            .ATensor = &input_a_tensor_desc.GetDMLTensorDesc(),
            .BTensor = &input_b_tensor_desc.GetDMLTensorDesc(),
            .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
            .FusedActivation = &activation_dml_desc,
        };

        label =
            GetFusedOperatorLabel(label, "add", *fusible_activation.value());

        binary_node = graph_builder.CreateOperatorNode(
            DML_OPERATOR_ELEMENT_WISE_ADD1, &add1_operator_desc, inputs, label);
        output_id =
            GetFusibleActivationOutputId(*fusible_activation.value()).value();
      }
      // If no standalone activation need to be fused, prefer
      // `DML_OPERATOR_ELEMENT_WISE_ADD` which supports more data types than
      // `DML_OPERATOR_ELEMENT_WISE_ADD1`.
      else {
        binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_ADD_OPERATOR_DESC>(
            input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
            graph_builder, DML_OPERATOR_ELEMENT_WISE_ADD, inputs, label);
      }
      break;
    }
    case mojom::ElementWiseBinary::Kind::kDiv: {
      CHECK(context_properties.data_type_limits.div_input.data_types.Has(
          input_data_type));
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_DIVIDE_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_DIVIDE, inputs, label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMax: {
      CHECK(context_properties.data_type_limits.max_input.data_types.Has(
          input_data_type));
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_MAX_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_MAX, inputs, label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMin: {
      CHECK(context_properties.data_type_limits.min_input.data_types.Has(
          input_data_type));
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_MIN_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_MIN, inputs, label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMul: {
      CHECK(context_properties.data_type_limits.mul_input.data_types.Has(
          input_data_type));
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_MULTIPLY, inputs, label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kSub: {
      CHECK(context_properties.data_type_limits.sub_input.data_types.Has(
          input_data_type));
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_SUBTRACT_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_SUBTRACT, inputs, label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kPow: {
      CHECK(context_properties.data_type_limits.pow_input.data_types.Has(
          input_data_type));
      DML_ELEMENT_WISE_POW_OPERATOR_DESC element_wise_operator_desc{
          .InputTensor = &input_a_tensor_desc.GetDMLTensorDesc(),
          .ExponentTensor = &input_b_tensor_desc.GetDMLTensorDesc(),
          .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};
      binary_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_ELEMENT_WISE_POW, &element_wise_operator_desc, inputs,
          label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kEqual: {
      CHECK(context_properties.data_type_limits.equal_input.data_types.Has(
          input_data_type));
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_LOGICAL_EQUALS_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_EQUALS, inputs,
              label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreater: {
      CHECK(context_properties.data_type_limits.greater_input.data_types.Has(
          input_data_type));
      binary_node = CreateBinaryOperator<
          DML_ELEMENT_WISE_LOGICAL_GREATER_THAN_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_GREATER_THAN, inputs,
          label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual: {
      CHECK(context_properties.data_type_limits.greater_or_equal_input
                .data_types.Has(input_data_type));
      binary_node = CreateBinaryOperator<
          DML_ELEMENT_WISE_LOGICAL_GREATER_THAN_OR_EQUAL_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder,
          DML_OPERATOR_ELEMENT_WISE_LOGICAL_GREATER_THAN_OR_EQUAL, inputs,
          label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesser: {
      CHECK(context_properties.data_type_limits.lesser_input.data_types.Has(
          input_data_type));
      binary_node = CreateBinaryOperator<
          DML_ELEMENT_WISE_LOGICAL_LESS_THAN_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_LESS_THAN, inputs,
          label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual: {
      CHECK(context_properties.data_type_limits.lesser_or_equal_input.data_types
                .Has(input_data_type));
      binary_node = CreateBinaryOperator<
          DML_ELEMENT_WISE_LOGICAL_LESS_THAN_OR_EQUAL_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_LESS_THAN_OR_EQUAL,
          inputs, label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kNotEqual: {
      CHECK(context_properties.data_type_limits.not_equal_input.data_types.Has(
          input_data_type));
      // DirectML doesn't support `notEqual`, emulate it by `logicalNot(equal(a,
      // b))`. Step 1: calculate `equal(a, b)`.
      const TensorDesc equal_output_tensor_desc =
          TensorDesc(output_tensor_desc.GetDataType(), output_dimensions);
      const GraphNode* equal_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_LOGICAL_EQUALS_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc,
              equal_output_tensor_desc, graph_builder,
              DML_OPERATOR_ELEMENT_WISE_LOGICAL_EQUALS, inputs, label);
      const NodeOutput* equal_output =
          graph_builder.CreateNodeOutput(equal_node, equal_output_tensor_desc);
      // Step 2: calculate `logicalNot(equal_output)`
      binary_node =
          CreateUnaryOperator<DML_ELEMENT_WISE_LOGICAL_NOT_OPERATOR_DESC,
                              DML_OPERATOR_ELEMENT_WISE_LOGICAL_NOT>(
              equal_output_tensor_desc, output_tensor_desc, equal_output,
              graph_builder, label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalAnd: {
      CHECK(
          context_properties.data_type_limits.logical_and_input.data_types.Has(
              input_data_type));
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_LOGICAL_AND_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_AND, inputs,
              label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalOr: {
      CHECK(context_properties.data_type_limits.logical_or_input.data_types.Has(
          input_data_type));
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_LOGICAL_OR_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_OR, inputs,
              label);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalXor: {
      CHECK(
          context_properties.data_type_limits.logical_xor_input.data_types.Has(
              input_data_type));
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_LOGICAL_XOR_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_XOR, inputs,
              label);
      break;
    }
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      binary_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForPad(const ContextProperties& context_properties,
                              const std::vector<OperandPtr>& operands,
                              const mojom::PadPtr& pad,
                              GraphBuilderDml& graph_builder,
                              IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, pad->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  CHECK(context_properties.data_type_limits.pad_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  OperandId output_id = pad->output_operand_id;
  const auto& output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  DML_PADDING_MODE padding_mode;
  // This value is ignored for other padding modes.
  float padding_value = 0;
  switch (pad->mode->which()) {
    case mojom::PaddingMode::Tag::kConstant:
      padding_mode = DML_PADDING_MODE::DML_PADDING_MODE_CONSTANT;
      padding_value = pad->mode->get_constant()->value;
      break;
    case mojom::PaddingMode::Tag::kEdge:
      padding_mode = DML_PADDING_MODE::DML_PADDING_MODE_EDGE;
      break;
    case mojom::PaddingMode::Tag::kReflection:
      padding_mode = DML_PADDING_MODE::DML_PADDING_MODE_REFLECTION;
      break;
  }

  const auto& beginning_padding = pad->beginning_padding;
  const auto& ending_padding = pad->ending_padding;
  CHECK_EQ(beginning_padding.size(), ending_padding.size());

  DML_PADDING_OPERATOR_DESC pad_operator_desc = {
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .PaddingMode = padding_mode,
      .PaddingValue = padding_value,
      .DimensionCount = static_cast<uint32_t>(beginning_padding.size()),
      .StartPadding = beginning_padding.data(),
      .EndPadding = ending_padding.data()};

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* pad_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_PADDING, &pad_operator_desc, {inputs}, pad->label);

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(pad_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForPool2d(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const mojom::Pool2dPtr& pool2d,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, pool2d->input_operand_id);
  // The input tensor description may be transposed.
  auto input_tensor_desc = input->GetTensorDesc();

  OperandId output_id = pool2d->output_operand_id;
  // The output tensor description may be transposed.
  auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  std::array<uint32_t, 2> strides = {pool2d->strides->height,
                                     pool2d->strides->width};
  std::array<uint32_t, 2> dilations = {pool2d->dilations->height,
                                       pool2d->dilations->width};
  std::array<uint32_t, 2> window_dimensions = {
      pool2d->window_dimensions->height, pool2d->window_dimensions->width};
  std::array<uint32_t, 2> start_padding = {pool2d->padding->beginning->height,
                                           pool2d->padding->beginning->width};
  std::array<uint32_t, 2> end_padding = {pool2d->padding->ending->height,
                                         pool2d->padding->ending->width};
  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* pool2d_node = nullptr;
  const std::string& label = pool2d->label;
  switch (pool2d->kind) {
    case mojom::Pool2d::Kind::kAveragePool2d: {
      CHECK(context_properties.data_type_limits.average_pool2d_input.data_types
                .Has(DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

      // TODO(crbug.com/40206287): Work around dilation support for L2 and
      // average pooling. According to WebNN spec:
      // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pool2d, dilations are
      // supported by pooling operations, while for DirectML AVERAGE_POOLING
      // and LP_POOLING don't support dilations. Spec issue tracked on
      // https://github.com/webmachinelearning/webnn/issues/180.
      if (dilations[0] != 1 || dilations[1] != 1) {
        return base::unexpected(CreateError(
            mojom::Error::Code::kNotSupportedError,
            "Dilations are not supported for average pooling operator.",
            label));
      }
      DML_AVERAGE_POOLING_OPERATOR_DESC average_pooling_desc = {
          .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
          .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
          .DimensionCount =
              base::checked_cast<uint32_t>(window_dimensions.size()),
          .Strides = strides.data(),
          .WindowSize = window_dimensions.data(),
          .StartPadding = start_padding.data(),
          .EndPadding = end_padding.data(),
          // The padding elements are not counted as part of the averaging
          // calculation.
          .IncludePadding = false};
      pool2d_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_AVERAGE_POOLING, &average_pooling_desc, inputs, label);
      break;
    }
    case mojom::Pool2d::Kind::kL2Pool2d: {
      CHECK(context_properties.data_type_limits.l2_pool2d_input.data_types.Has(
          DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

      DML_LP_POOLING_OPERATOR_DESC l2_pooling_desc = {
          .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
          .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
          .DimensionCount =
              base::checked_cast<uint32_t>(window_dimensions.size()),
          .Strides = strides.data(),
          .WindowSize = window_dimensions.data(),
          .StartPadding = start_padding.data(),
          .EndPadding = end_padding.data(),
          .P = 2};
      pool2d_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_LP_POOLING, &l2_pooling_desc, inputs, label);
      break;
    }
    case mojom::Pool2d::Kind::kMaxPool2d: {
      CHECK(context_properties.data_type_limits.max_pool2d_input.data_types.Has(
          DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

      // If the dilations are { 1, 1 } by default, prefer using
      // `DML_MAX_POOLING_OPERATOR_DESC` without dilations supported for best
      // compatibility.
      // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_max_pooling_operator_desc.
      // TODO(issues.chromium.org/327244278): Remove the workaround of using
      // `DML_MAX_POOLING_OPERATOR_DESC` without dilations.
      if (dilations[0] == 1 && dilations[1] == 1) {
        DML_MAX_POOLING_OPERATOR_DESC max_pooling_desc = {
            .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
            .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
            .DimensionCount =
                base::checked_cast<uint32_t>(window_dimensions.size()),
            .Strides = strides.data(),
            .WindowSize = window_dimensions.data(),
            .StartPadding = start_padding.data(),
            .EndPadding = end_padding.data()};
        pool2d_node = graph_builder.CreateOperatorNode(
            DML_OPERATOR_MAX_POOLING, &max_pooling_desc, inputs, label);
      } else {
        DML_MAX_POOLING2_OPERATOR_DESC max_pooling2_desc = {
            .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
            .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
            .OutputIndicesTensor = nullptr,
            .DimensionCount =
                base::checked_cast<uint32_t>(window_dimensions.size()),
            .Strides = strides.data(),
            .WindowSize = window_dimensions.data(),
            .StartPadding = start_padding.data(),
            .EndPadding = end_padding.data(),
            .Dilations = dilations.data()};
        pool2d_node = graph_builder.CreateOperatorNode(
            DML_OPERATOR_MAX_POOLING2, &max_pooling2_desc, inputs, label);
      }
      break;
    }
    default:
      LOG(ERROR) << "[WebNN] Invalid Pool2d operator type";
      NOTREACHED();
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      pool2d_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

void CreateOperatorNodeForPrelu(const ContextProperties context_properties,
                                const std::vector<OperandPtr>& operands,
                                const mojom::PreluPtr& prelu,
                                GraphBuilderDml& graph_builder,
                                IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, prelu->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  CHECK(context_properties.data_type_limits.prelu_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  const NodeOutput* slope =
      GetNodeOutputForOperand(id_to_node_output_map, prelu->slope_operand_id);
  auto slope_tensor_desc = slope->GetTensorDesc();
  CHECK_EQ(input_tensor_desc.GetDataType(), slope_tensor_desc.GetDataType());

  OperandId output_id = prelu->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  const auto& output_dimensions = output_tensor_desc.GetDimensions();
  if (slope_tensor_desc.GetDimensions() != output_dimensions) {
    slope_tensor_desc.BroadcastTo(output_dimensions);
  }

  DML_ACTIVATION_PARAMETERIZED_RELU_OPERATOR_DESC prelu_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .SlopeTensor = &slope_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};

  const std::string& label = prelu->label;
  std::array<const NodeOutput*, 2> inputs = {input, slope};
  const GraphNode* prelu_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_PARAMETERIZED_RELU, &prelu_desc, inputs, label);

  const NodeOutput* node_output =
      graph_builder.CreateNodeOutput(prelu_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
}

void CreateOperatorNodeForScatterElements(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const mojom::ScatterElementsPtr& scatter_elements,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, scatter_elements->input_operand_id);
  TensorDesc input_tensor_desc = input->GetTensorDesc();
  CHECK(
      context_properties.data_type_limits.scatter_elements_input.data_types.Has(
          DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  const NodeOutput* indices = GetNodeOutputForOperand(
      id_to_node_output_map, scatter_elements->indices_operand_id);
  TensorDesc indices_tensor_desc = indices->GetTensorDesc();
  CHECK(context_properties.data_type_limits.scatter_elements_indices.data_types
            .Has(DmlDataTypeToOperand(indices_tensor_desc.GetDataType())));

  const NodeOutput* updates = GetNodeOutputForOperand(
      id_to_node_output_map, scatter_elements->updates_operand_id);
  TensorDesc updates_tensor_desc = updates->GetTensorDesc();
  CHECK(
      context_properties.data_type_limits.scatter_elements_input.data_types.Has(
          DmlDataTypeToOperand(updates_tensor_desc.GetDataType())));

  OperandId output_id = scatter_elements->output_operand_id;
  const TensorDesc output_tensor_desc =
      CreateOutputTensorDesc(operands, output_id);

  DML_SCATTER_OPERATOR_DESC scatter_elements_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .IndicesTensor = &indices_tensor_desc.GetDMLTensorDesc(),
      .UpdatesTensor = &updates_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Axis = scatter_elements->axis};

  std::array<const NodeOutput*, 3> inputs = {input, indices, updates};
  const GraphNode* node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_SCATTER, &scatter_elements_desc, inputs,
      scatter_elements->label);

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForScatterND(const ContextProperties& context_properties,
                                    const std::vector<OperandPtr>& operands,
                                    const mojom::ScatterNDPtr& scatter_nd,
                                    GraphBuilderDml& graph_builder,
                                    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, scatter_nd->input_operand_id);
  TensorDesc input_tensor_desc = input->GetTensorDesc();
  CHECK(context_properties.data_type_limits.scatter_nd_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  const NodeOutput* indices = GetNodeOutputForOperand(
      id_to_node_output_map, scatter_nd->indices_operand_id);
  TensorDesc indices_tensor_desc = indices->GetTensorDesc();
  CHECK(context_properties.data_type_limits.scatter_nd_indices.data_types.Has(
      DmlDataTypeToOperand(indices_tensor_desc.GetDataType())));

  const NodeOutput* updates = GetNodeOutputForOperand(
      id_to_node_output_map, scatter_nd->updates_operand_id);
  TensorDesc updates_tensor_desc = updates->GetTensorDesc();
  CHECK(context_properties.data_type_limits.scatter_nd_updates.data_types.Has(
      DmlDataTypeToOperand(updates_tensor_desc.GetDataType())));

  OperandId output_id = scatter_nd->output_operand_id;
  const TensorDesc original_output_tensor_desc =
      CreateOutputTensorDesc(operands, output_id);

  size_t input_rank = input_tensor_desc.GetDimensions().size();
  size_t indices_rank = indices_tensor_desc.GetDimensions().size();
  size_t updates_rank = updates_tensor_desc.GetDimensions().size();
  size_t output_rank = original_output_tensor_desc.GetDimensions().size();
  size_t maximum_rank =
      std::max({input_rank, indices_rank, updates_rank, output_rank});

  // DML_SCATTER_ND_OPERATOR_DESC requires IndicesTensor, InputTensor,
  // OutputTensor, and UpdatesTensor must have the same DimensionCount.
  input_tensor_desc.EnsureMinimumRank(maximum_rank,
                                      TensorDesc::Alignment::kTrailing);
  indices_tensor_desc.EnsureMinimumRank(maximum_rank,
                                        TensorDesc::Alignment::kTrailing);
  updates_tensor_desc.EnsureMinimumRank(maximum_rank,
                                        TensorDesc::Alignment::kTrailing);

  TensorDesc output_tensor_desc = original_output_tensor_desc;
  output_tensor_desc.EnsureMinimumRank(maximum_rank,
                                       TensorDesc::Alignment::kTrailing);

  DML_SCATTER_ND_OPERATOR_DESC scatter_nd_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .IndicesTensor = &indices_tensor_desc.GetDMLTensorDesc(),
      .UpdatesTensor = &updates_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .InputDimensionCount = base::checked_cast<uint32_t>(input_rank),
      .IndicesDimensionCount = base::checked_cast<uint32_t>(indices_rank)};

  std::array<const NodeOutput*, 3> inputs = {input, indices, updates};
  const GraphNode* node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_SCATTER_ND, &scatter_nd_desc, inputs, scatter_nd->label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      node, std::move(original_output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForSlice(const std::vector<OperandPtr>& operands,
                                const mojom::SlicePtr& slice,
                                GraphBuilderDml& graph_builder,
                                IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, slice->input_operand_id);
  const TensorDesc& input_tensor_desc = input->GetTensorDesc();
  const std::vector<uint32_t>& input_dimensions =
      input_tensor_desc.GetDimensions();
  const size_t input_rank = input_dimensions.size();

  const TensorDesc& output_tensor_desc =
      CreateOutputTensorDesc(operands, slice->output_operand_id);
  const std::vector<uint32_t>& output_dimensions =
      output_tensor_desc.GetDimensions();

  CHECK_EQ(input_rank, output_dimensions.size());
  CHECK_EQ(input_rank, slice->ranges.size());

  // Start, size and stride attributes must be unpacked from the mojo interface.
  base::FixedArray<uint32_t> starts(input_rank);
  base::FixedArray<uint32_t> sizes(input_rank);
  base::FixedArray<uint32_t> strides(input_rank);
  for (size_t i = 0; i < input_rank; ++i) {
    starts[i] = slice->ranges[i].start;
    // `sizes` should be the number of elements to copy.
    sizes[i] = output_dimensions[i];
    strides[i] = slice->ranges[i].stride;
  }

  DML_SLICE_OPERATOR_DESC slice_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .DimensionCount = static_cast<UINT>(input_dimensions.size()),
      .Offsets = starts.data(),
      .Sizes = sizes.data(),
      .Strides = strides.data(),
  };

  std::array<const NodeOutput*, 1> input_node_output = {input};
  const GraphNode* slice_node =
      graph_builder.CreateOperatorNode(DML_OPERATOR_SLICE, &slice_operator_desc,
                                       input_node_output, slice->label);

  const auto* slice_output =
      graph_builder.CreateNodeOutput(slice_node, std::move(output_tensor_desc));
  id_to_node_output_map[slice->output_operand_id] = std::move(slice_output);
}

void CreateOperatorNodeForSplit(const std::vector<OperandPtr>& operands,
                                const mojom::SplitPtr& split,
                                GraphBuilderDml& graph_builder,
                                IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, split->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  // Since TensorDesc stores dimensions and strides vectors, we need to keep
  // TensorDescs until create CreateOperatorNode is called.
  base::FixedArray<TensorDesc> output_tensor_desc(
      split->output_operand_ids.size());
  base::FixedArray<DML_TENSOR_DESC> output_tensor_desc_dml(
      split->output_operand_ids.size());
  for (size_t i = 0; i < split->output_operand_ids.size(); ++i) {
    output_tensor_desc[i] =
        CreateOutputTensorDesc(operands, split->output_operand_ids[i]);
    output_tensor_desc_dml[i] = output_tensor_desc[i].GetDMLTensorDesc();
  }

  auto output_count =
      base::checked_cast<uint32_t>(output_tensor_desc_dml.size());
  DML_SPLIT_OPERATOR_DESC split_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputCount = output_count,
      .OutputTensors = output_tensor_desc_dml.data(),
      .Axis = split->axis};

  const std::string& label = split->label;
  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* split_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_SPLIT, &split_desc, inputs, label);

  for (uint32_t i = 0; i < output_count; ++i) {
    OperandId output_id = split->output_operand_ids[i];
    const auto* output = graph_builder.CreateNodeOutput(
        split_node, std::move(output_tensor_desc[i]), i);
    CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
  }
}

void CreateOperatorNodeForNeg(const std::vector<OperandPtr>& operands,
                              const mojom::ElementWiseUnaryPtr& operation,
                              GraphBuilderDml& graph_builder,
                              IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, operation->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  const OperandId output_id = operation->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  // Set the values of scale and bias terms supplied to identity operator.
  // Scale and bias have the effect of applying the function g(x) = x * Scale
  // + Bias. When we set Scale to -1 and Bias to 0, we can simulate identity
  // as negate operator.
  DML_SCALE_BIAS scale_bias{.Scale = -1.f, .Bias = 0.f};
  DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC identity_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .ScaleBias = &scale_bias};

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* identity_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_IDENTITY, &identity_operator_desc, inputs,
      operation->label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      identity_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForElementWiseUnary(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const mojom::ElementWiseUnaryPtr& operation,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const OperandDataType input_data_type =
      DmlDataTypeToOperand(GetNodeOutputForOperand(id_to_node_output_map,
                                                   operation->input_operand_id)
                               ->GetTensorDesc()
                               .GetDataType());
  switch (operation->kind) {
    case mojom::ElementWiseUnary::Kind::kAbs: {
      CHECK(context_properties.data_type_limits.abs_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_ABS_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_ABS>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kCast: {
      CHECK(context_properties.data_type_limits.cast_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_CAST_OPERATOR_DESC,
                                        DML_OPERATOR_CAST>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kCeil: {
      CHECK(context_properties.data_type_limits.ceil_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_CEIL_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_CEIL>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kCos: {
      CHECK(context_properties.data_type_limits.cos_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_COS_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_COS>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kErf: {
      CHECK(context_properties.data_type_limits.erf_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_ERF_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_ERF>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kExp: {
      CHECK(context_properties.data_type_limits.exp_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_EXP_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_EXP>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kFloor: {
      CHECK(context_properties.data_type_limits.floor_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_FLOOR_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_FLOOR>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kIdentity: {
      CHECK(context_properties.data_type_limits.identity_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_IDENTITY>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kLog: {
      CHECK(context_properties.data_type_limits.log_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_LOG_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_LOG>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kLogicalNot: {
      CHECK(
          context_properties.data_type_limits.logical_not_input.data_types.Has(
              input_data_type));
      return CreateOperatorNodeForUnary<
          DML_ELEMENT_WISE_LOGICAL_NOT_OPERATOR_DESC,
          DML_OPERATOR_ELEMENT_WISE_LOGICAL_NOT>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    // TODO(crbug.com/40943114): Implement the negate operator directly by
    // DML_ELEMENT_WISE_NEGATE_OPERATOR_DESC which is available in
    // DML_FEATURE_LEVEL_5_0.
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_negate_operator_desc#availability
    case mojom::ElementWiseUnary::Kind::kNeg: {
      CHECK(context_properties.data_type_limits.neg_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForNeg(operands, operation, graph_builder,
                                      id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kReciprocal: {
      CHECK(context_properties.data_type_limits.reciprocal_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_RECIP_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_RECIP>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kSign: {
      CHECK(context_properties.data_type_limits.sign_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_SIGN_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_SIGN>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kSin: {
      CHECK(context_properties.data_type_limits.sin_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_SIN_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_SIN>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kSqrt: {
      CHECK(context_properties.data_type_limits.sqrt_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_SQRT_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_SQRT>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kTan: {
      CHECK(context_properties.data_type_limits.tan_input.data_types.Has(
          input_data_type));
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_TAN_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_TAN>(
          operands, operation, graph_builder, id_to_node_output_map);
    }
  }
}

void CreateOperatorNodeForResample2d(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const mojom::Resample2dPtr& resample2d,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, resample2d->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  CHECK(context_properties.data_type_limits.resample2d_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  OperandId output_id = resample2d->output_operand_id;
  const auto& output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  const auto& input_dimensions = input_tensor_desc.GetDimensions();
  const auto& output_dimensions = output_tensor_desc.GetDimensions();
  size_t input_rank = input_dimensions.size();
  CHECK_EQ(input_rank, output_dimensions.size());

  // Use explicit scales if given, otherwise, compute scales from output
  // dimensions / input dimensions. Then expand scales to full scales (same
  // size as input rank using axes).
  base::FixedArray<float> full_scales(input_rank, 1);
  const auto& scales = resample2d->scales;
  const auto& axes = resample2d->axes;
  if (scales) {
    for (size_t i = 0; i < axes.size(); ++i) {
      auto axis = axes[i];
      CHECK_LT(axis, full_scales.size());
      full_scales[axis] = scales.value()[i];
    }
  } else {
    for (size_t i = 0; i < input_rank; ++i) {
      full_scales[i] =
          base::checked_cast<float>(output_dimensions[i]) / input_dimensions[i];
    }
  }

  DML_INTERPOLATION_MODE mode;
  switch (resample2d->mode) {
    case mojom::Resample2d::InterpolationMode::kNearestNeighbor:
      mode = DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
      break;
    case mojom::Resample2d::InterpolationMode::kLinear:
      mode = DML_INTERPOLATION_MODE_LINEAR;
      break;
  }

  DML_RESAMPLE_OPERATOR_DESC resample2d_operator_desc = {
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .InterpolationMode = mode,
      .ScaleCount = static_cast<uint32_t>(full_scales.size()),
      .Scales = full_scales.data()};

  const std::string& label = resample2d->label;
  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* resample2d_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_RESAMPLE, &resample2d_operator_desc, inputs, label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      resample2d_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForReduce(const ContextProperties& context_properties,
                                 const std::vector<OperandPtr>& operands,
                                 const mojom::ReducePtr& reduce,
                                 GraphBuilderDml& graph_builder,
                                 IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, reduce->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  CheckInputDataTypeForReduce(
      context_properties.data_type_limits, reduce->kind,
      DmlDataTypeToOperand(input_tensor_desc.GetDataType()));

  OperandId output_id = reduce->output_operand_id;
  const auto& output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  const auto& axes = reduce->axes;
  // Determine output sizes. Ignore output_desc->dimensions for the
  // dimensions, since DirectML expects the output dimensions to have the same
  // rank as the input, and output_desc->dimensions may have removed
  // dimensions if keepDimensions was false.
  std::vector<uint32_t> output_dimensions = input_tensor_desc.GetDimensions();
  for (uint32_t axis : axes) {
    CHECK_LT(axis, output_dimensions.size());
    output_dimensions[axis] = 1u;
  }
  TensorDesc new_output_tensor_desc(output_tensor_desc.GetDataType(),
                                    output_dimensions);

  std::array<const NodeOutput*, 1> inputs = {input};
  DML_REDUCE_OPERATOR_DESC operator_desc = {};
  operator_desc.Function = MapReduceKindToReduceFuntion(reduce->kind);
  operator_desc.InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
  operator_desc.OutputTensor = &new_output_tensor_desc.GetDMLTensorDesc(),
  operator_desc.AxisCount = static_cast<uint32_t>(axes.size());
  operator_desc.Axes = axes.data();
  const GraphNode* reduce_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_REDUCE, &operator_desc, inputs, reduce->label);

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(reduce_node, output_tensor_desc);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

// DirectML API does not have a real Reshape operator. The WebNN Reshape is
// implemented by a DirectML Identity operator. DirectML runtime is able to
// optimize the unnecessary IDENTITY operators when compiling the graph.
void CreateOperatorNodeForReshape(const ContextProperties& context_properties,
                                  const std::vector<OperandPtr>& operands,
                                  const mojom::ReshapePtr& reshape,
                                  GraphBuilderDml& graph_builder,
                                  IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, reshape->input_operand_id);
  CHECK(context_properties.data_type_limits.reshape_input.data_types.Has(
      DmlDataTypeToOperand(input->GetTensorDesc().GetDataType())));

  OperandId output_id = reshape->output_operand_id;
  const OperandPtr& output_operand = operands.at(output_id.value());
  base::span<const uint32_t> new_shape = output_operand->descriptor.shape();

  const NodeOutput* output = CreateReshapeNode(graph_builder, input, new_shape);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

// Emulated by slice operator.
void CreateOperatorNodeForReverse(const ContextProperties& context_properties,
                                  const std::vector<OperandPtr>& operands,
                                  const mojom::Reverse& reverse,
                                  GraphBuilderDml& graph_builder,
                                  IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, reverse.input_operand_id);
  const TensorDesc& input_tensor_desc = input->GetTensorDesc();
  const size_t input_rank = input_tensor_desc.GetDimensions().size();

  const OperandId output_id = reverse.output_operand_id;
  const TensorDesc output_tensor_desc =
      CreateOutputTensorDesc(operands, output_id);

  // Slice copies the tensor as-is when setting starting offset=0, window
  // size=dimension size and stride=1, then by setting stride=-1 for specified
  // axes the copying order can be reversed on these dimensions.
  base::FixedArray<uint32_t> starts(input_rank, 0);
  base::FixedArray<int32_t> strides(input_rank, 1);
  for (uint32_t axis : reverse.axes) {
    CHECK_LT(axis, input_rank);
    strides[axis] = -1;
  }

  DML_SLICE1_OPERATOR_DESC reverse_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .DimensionCount = base::checked_cast<uint32_t>(input_rank),
      .InputWindowOffsets = starts.data(),
      .InputWindowSizes = input_tensor_desc.GetDimensions().data(),
      .InputWindowStrides = strides.data()};

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_SLICE1, &reverse_desc, inputs, reverse.label);

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForElu(const std::vector<OperandPtr>& operands,
                              const mojom::EluPtr& elu,
                              GraphBuilderDml& graph_builder,
                              IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, elu->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  OperandId output_id = elu->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  DML_ACTIVATION_ELU_OPERATOR_DESC elu_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Alpha = elu->alpha};

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* elu_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_ELU, &elu_desc, inputs, elu->label);

  const NodeOutput* node_output =
      graph_builder.CreateNodeOutput(elu_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
}

void CreateOperatorNodeForExpand(const ContextProperties& context_properties,
                                 const std::vector<OperandPtr>& operands,
                                 const mojom::ExpandPtr& expand,
                                 GraphBuilderDml& graph_builder,
                                 IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, expand->input_operand_id);
  auto input_tensor_desc = input->GetTensorDesc();

  CHECK(context_properties.data_type_limits.expand_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  const OperandId output_id = expand->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  const auto& output_dimensions = output_tensor_desc.GetDimensions();

  const NodeOutput* node_output =
      CreateExpandNode(graph_builder, input, output_dimensions, expand->label);

  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForGather(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const mojom::GatherPtr& gather,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  OperandId input_id = gather->input_operand_id;
  CHECK(context_properties.data_type_limits.gather_input.Supports(
      operands.at(input_id.value())->descriptor));
  OperandId indices_id = gather->indices_operand_id;
  CHECK(context_properties.data_type_limits.gather_indices.Supports(
      operands.at(indices_id.value())->descriptor));

  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, input_id);
  auto input_tensor_desc = input->GetTensorDesc();
  const NodeOutput* indices =
      GetNodeOutputForOperand(id_to_node_output_map, indices_id);
  auto indices_tensor_desc = indices->GetTensorDesc();
  OperandId output_id = gather->output_operand_id;
  const auto original_output_tensor_desc =
      CreateOutputTensorDesc(operands, output_id);
  auto output_tensor_desc = original_output_tensor_desc;

  size_t input_rank = input_tensor_desc.GetDimensions().size();
  size_t output_rank = output_tensor_desc.GetDimensions().size();
  size_t expanded_rank = std::max(input_rank, output_rank);
  size_t indices_rank = indices_tensor_desc.GetDimensions().size();

  // According to the DirectML documentation
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gather_operator_desc,
  // the parameters `InputTensor`, `OutputTensor` and `IndicesTensor` must
  // have the same dimension count.
  input_tensor_desc.EnsureMinimumRank(expanded_rank,
                                      TensorDesc::Alignment::kTrailing);
  indices_tensor_desc.EnsureMinimumRank(expanded_rank,
                                        TensorDesc::Alignment::kTrailing);

  uint32_t axis = gather->axis;
  if (output_rank < input_rank) {
    // There is only one case in which `output_rank` is less than
    // `input_rank`, that is when indices is scalar. In this case, a one value
    // should be inserted at the `axis` position of the output dimensions,
    // because the indices dimensions is set to {1} since DirectML requires
    // the tensor dimension count to be at least 1.
    CHECK_EQ(indices_rank, 1u);
    CHECK_EQ(output_rank, input_rank - 1);

    auto output_dimensions = input_tensor_desc.GetDimensions();
    CHECK_LT(axis, output_dimensions.size());
    output_dimensions[axis] = 1;
    output_tensor_desc = TensorDesc(output_tensor_desc.GetDataType(),
                                    std::move(output_dimensions));
  }

  auto expanded_axis = base::MakeCheckedNum(expanded_rank) - input_rank +
                       base::checked_cast<size_t>(axis);
  const std::string& label = gather->label;
  if (!expanded_axis.AssignIfValid<uint32_t>(&axis)) {
    return base::unexpected(
        CreateError(mojom::Error::Code::kUnknownError,
                    "The axis of gather operator is too large.", label));
  }

  // TODO(crbug.com/40206287): Include a DirectML documentation link and a
  // Chromium test that validates the out-of-bounds indices handling.
  //
  // DirectML implementation for gather operator has already handled the
  // indices tensor by clamping it in the shader to prevent out-of-bounds
  // access.
  DML_GATHER_OPERATOR_DESC gather_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .IndicesTensor = &indices_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      // The axis dimension of InputTensor to gather on.
      .Axis = axis,
      // The number of actual index dimensions within the IndicesTensor.
      .IndexDimensions = base::checked_cast<uint32_t>(indices_rank)};

  std::array<const NodeOutput*, 2> inputs = {input, indices};
  const GraphNode* gather_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GATHER, &gather_operator_desc, inputs, label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      gather_node, std::move(original_output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

void CreateOperatorNodeForGatherElements(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const mojom::GatherElementsPtr& gather_elements,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  OperandId input_id = gather_elements->input_operand_id;
  CHECK(context_properties.data_type_limits.gather_elements_input.Supports(
      operands.at(input_id.value())->descriptor));
  OperandId indices_id = gather_elements->indices_operand_id;
  CHECK(context_properties.data_type_limits.gather_elements_indices.Supports(
      operands.at(indices_id.value())->descriptor));

  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, input_id);
  const TensorDesc& input_tensor_desc = input->GetTensorDesc();
  const NodeOutput* indices =
      GetNodeOutputForOperand(id_to_node_output_map, indices_id);
  const TensorDesc& indices_tensor_desc = indices->GetTensorDesc();
  OperandId output_id = gather_elements->output_operand_id;
  const TensorDesc output_tensor_desc =
      CreateOutputTensorDesc(operands, output_id);

  // DirectML implementation for gatherElements operator has already handled
  // the indices tensor by clamping it in the shader to prevent out-of-bounds
  // access.
  DML_GATHER_ELEMENTS_OPERATOR_DESC gather_elements_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .IndicesTensor = &indices_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      // The dimension of InputTensor to gather along.
      .Axis = gather_elements->axis};

  std::array<const NodeOutput*, 2> inputs = {input, indices};
  const GraphNode* node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GATHER_ELEMENTS, &gather_elements_desc, inputs,
      gather_elements->label);

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForGatherND(const ContextProperties& context_properties,
                                   const std::vector<OperandPtr>& operands,
                                   const mojom::GatherNDPtr& gather_nd,
                                   GraphBuilderDml& graph_builder,
                                   IdToNodeOutputMap& id_to_node_output_map) {
  OperandId input_id = gather_nd->input_operand_id;
  CHECK(context_properties.data_type_limits.gather_nd_input.Supports(
      operands.at(input_id.value())->descriptor));
  OperandId indices_id = gather_nd->indices_operand_id;
  CHECK(context_properties.data_type_limits.gather_nd_indices.Supports(
      operands.at(indices_id.value())->descriptor));

  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, input_id);
  TensorDesc input_tensor_desc = input->GetTensorDesc();
  const NodeOutput* indices =
      GetNodeOutputForOperand(id_to_node_output_map, indices_id);
  TensorDesc indices_tensor_desc = indices->GetTensorDesc();
  OperandId output_id = gather_nd->output_operand_id;
  const TensorDesc original_output_tensor_desc =
      CreateOutputTensorDesc(operands, output_id);

  size_t input_rank = input_tensor_desc.GetDimensions().size();
  size_t indices_rank = indices_tensor_desc.GetDimensions().size();
  size_t output_rank = original_output_tensor_desc.GetDimensions().size();
  size_t maximum_rank = std::max({input_rank, indices_rank, output_rank});

  // Add leading ones to the dimensions to ensure these tensors have the same
  // rank as required.
  input_tensor_desc.EnsureMinimumRank(maximum_rank,
                                      TensorDesc::Alignment::kTrailing);
  indices_tensor_desc.EnsureMinimumRank(maximum_rank,
                                        TensorDesc::Alignment::kTrailing);

  TensorDesc output_tensor_desc = original_output_tensor_desc;
  output_tensor_desc.EnsureMinimumRank(maximum_rank,
                                       TensorDesc::Alignment::kTrailing);

  // DirectML handles out-of-bounds indices internally and ensures no invalid
  // reads outside of input tensor.
  DML_GATHER_ND_OPERATOR_DESC gather_nd_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .IndicesTensor = &indices_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .InputDimensionCount = base::checked_cast<uint32_t>(input_rank),
      .IndicesDimensionCount = base::checked_cast<uint32_t>(indices_rank)};

  std::array<const NodeOutput*, 2> inputs = {input, indices};
  const GraphNode* node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GATHER_ND, &gather_nd_desc, inputs, gather_nd->label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      node, std::move(original_output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

void CreateOperatorNodeForGelu(
    const ContextProperties& context_properties,
    Adapter* adapter,
    const mojom::GeluPtr& gelu,
    mojom::GraphInfoPtr& graph_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map,
    absl::flat_hash_map<OperandId, uint32_t>& constant_id_to_input_index_map) {
  const auto& operands = graph_info->operands;
  // Check feature level by referring to MSDN doc:
  // https://learn.microsoft.com/en-us/windows/ai/directml/api/ns-directml-dml_activation_gelu_operator_desc
  if (adapter->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_5_1)) {
    return CreateOperatorNodeForUnary<DML_ACTIVATION_GELU_OPERATOR_DESC,
                                      DML_OPERATOR_ACTIVATION_GELU>(
        operands, gelu, graph_builder, id_to_node_output_map);
  }

  // Emulate gelu (0.5 * x * (1 + erf(x / sqrt(2)))) with decomposed
  // operations on platforms with low feature level according to
  // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-gelu-method
  //
  // Build constant operand (2.0)
  const OperandPtr& input_operand = operands.at(gelu->input_operand_id.value());
  const OperandDataType data_type = input_operand->descriptor.data_type();
  OperandId constant_for_sqrt_operand_id = BuildConstantOperandForFloatValue(
      context_properties, graph_info, constant_operands, data_type, /*rank*/ 1,
      /*default value*/ 2.0);
  CreateConstantNode(adapter, constant_for_sqrt_operand_id, constant_operands,
                     graph_builder, id_to_node_output_map,
                     constant_id_to_input_index_map);
  const NodeOutput* constant_for_sqrt_output = GetNodeOutputForOperand(
      id_to_node_output_map, constant_for_sqrt_operand_id);

  // Formula: sqrt(2)
  const TensorDesc sqrt_output_tensor_desc =
      TensorDesc(GetTensorDataType(data_type), /*dimensions*/ {1});
  DML_ELEMENT_WISE_SQRT_OPERATOR_DESC sqrt_operator_desc{
      .InputTensor =
          &constant_for_sqrt_output->GetTensorDesc().GetDMLTensorDesc(),
      .OutputTensor = &sqrt_output_tensor_desc.GetDMLTensorDesc(),
  };

  const std::string& label = gelu->label;
  std::array<const NodeOutput*, 1> sqrt_inputs = {constant_for_sqrt_output};
  const GraphNode* sqrt_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_SQRT, &sqrt_operator_desc, sqrt_inputs, label);

  const NodeOutput* sqrt_output =
      graph_builder.CreateNodeOutput(sqrt_node, sqrt_output_tensor_desc);

  // Formula: x / sqrt(2)
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, gelu->input_operand_id);
  const TensorDesc& input_tensor_desc = input->GetTensorDesc();
  const std::vector<uint32_t>& input_dimensions =
      input_tensor_desc.GetDimensions();
  TensorDesc div_divisor_tensor_desc = sqrt_output->GetTensorDesc();
  div_divisor_tensor_desc.BroadcastTo(input_dimensions);
  OperandId output_id = gelu->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  const TensorDesc& div_output_tensor_desc = output_tensor_desc;
  std::array<const NodeOutput*, 2> div_inputs = {input, sqrt_output};
  const GraphNode* div_node =
      CreateBinaryOperator<DML_ELEMENT_WISE_DIVIDE_OPERATOR_DESC>(
          input_tensor_desc, div_divisor_tensor_desc, div_output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_DIVIDE, div_inputs, label);

  const NodeOutput* div_output =
      graph_builder.CreateNodeOutput(div_node, div_output_tensor_desc);

  // Formula: erf(x / sqrt(2))
  const TensorDesc& erf_output_tensor_desc = output_tensor_desc;
  DML_ELEMENT_WISE_ERF_OPERATOR_DESC erf_operator_desc{
      .InputTensor = &div_output->GetTensorDesc().GetDMLTensorDesc(),
      .OutputTensor = &erf_output_tensor_desc.GetDMLTensorDesc(),
  };
  std::array<const NodeOutput*, 1> erf_inputs = {div_output};
  const GraphNode* erf_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_ERF, &erf_operator_desc, erf_inputs, label);

  const NodeOutput* erf_output =
      graph_builder.CreateNodeOutput(erf_node, erf_output_tensor_desc);

  // Build constant operand (1.0)
  OperandId constant_for_add_operand_id = BuildConstantOperandForFloatValue(
      context_properties, graph_info, constant_operands, data_type, /*rank*/ 1,
      /*default value*/ 1.0);
  CreateConstantNode(adapter, constant_for_add_operand_id, constant_operands,
                     graph_builder, id_to_node_output_map,
                     constant_id_to_input_index_map);
  const NodeOutput* constant_for_add_output = GetNodeOutputForOperand(
      id_to_node_output_map, constant_for_add_operand_id);

  // Formula: 1 + erf(x / sqrt(2))
  const TensorDesc& add_output_tensor_desc = output_tensor_desc;
  TensorDesc constant_for_add_tensor_desc =
      constant_for_add_output->GetTensorDesc();
  constant_for_add_tensor_desc.BroadcastTo(input_dimensions);
  std::array<const NodeOutput*, 2> add_inputs = {erf_output,
                                                 constant_for_add_output};
  const GraphNode* add_node =
      CreateBinaryOperator<DML_ELEMENT_WISE_ADD_OPERATOR_DESC>(
          erf_output_tensor_desc, constant_for_add_tensor_desc,
          add_output_tensor_desc, graph_builder, DML_OPERATOR_ELEMENT_WISE_ADD,
          add_inputs, label);

  const NodeOutput* add_output =
      graph_builder.CreateNodeOutput(add_node, add_output_tensor_desc);

  // Formula: x * (1 + erf(x / sqrt(2)))
  const TensorDesc& second_mul_output_tensor_desc = output_tensor_desc;
  std::array<const NodeOutput*, 2> second_mul_inputs = {input, add_output};
  const GraphNode* second_mul_node =
      CreateBinaryOperator<DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC>(
          input_tensor_desc, add_output_tensor_desc,
          second_mul_output_tensor_desc, graph_builder,
          DML_OPERATOR_ELEMENT_WISE_MULTIPLY, second_mul_inputs, label);

  const NodeOutput* second_mul_output = graph_builder.CreateNodeOutput(
      second_mul_node, second_mul_output_tensor_desc);

  // Build constant operand (0.5)
  OperandId constant_for_mul_operand_id = BuildConstantOperandForFloatValue(
      context_properties, graph_info, constant_operands, data_type, /*rank*/ 1,
      /*default value*/ 0.5);
  CreateConstantNode(adapter, constant_for_mul_operand_id, constant_operands,
                     graph_builder, id_to_node_output_map,
                     constant_id_to_input_index_map);
  const NodeOutput* constant_for_mul_output = GetNodeOutputForOperand(
      id_to_node_output_map, constant_for_mul_operand_id);

  // Formula: 0.5 * x * (1 + erf(x / sqrt(2)))
  TensorDesc constant_for_mul_tensor_desc =
      constant_for_mul_output->GetTensorDesc();
  constant_for_mul_tensor_desc.BroadcastTo(input_dimensions);
  std::array<const NodeOutput*, 2> mul_constant_inputs = {
      second_mul_output, constant_for_mul_output};
  const GraphNode* mul_constant_node =
      CreateBinaryOperator<DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC>(
          second_mul_output_tensor_desc, constant_for_mul_tensor_desc,
          output_tensor_desc, graph_builder, DML_OPERATOR_ELEMENT_WISE_MULTIPLY,
          mul_constant_inputs, label);

  const NodeOutput* node_output = graph_builder.CreateNodeOutput(
      mul_constant_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
}

// Creates a DirectML operator for the WebNN general matrix multiplication
// (GEMM) of the expression alpha * A * B + beta * C.
void CreateOperatorNodeForGemm(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const Operation* operation,
    const absl::flat_hash_map<const Operation*,
                              raw_ptr<const Operation, CtnExperimental>>&
        operation_to_fusible_standalone_activation_map,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& gemm = operation->get_gemm();
  OperandId input_a_id = gemm->a_operand_id;
  OperandId input_b_id = gemm->b_operand_id;
  CHECK(context_properties.data_type_limits.gemm_a.SupportsAll(
      {operands.at(input_a_id.value())->descriptor,
       operands.at(input_b_id.value())->descriptor}));

  const NodeOutput* input_a_node_output =
      GetNodeOutputForOperand(id_to_node_output_map, input_a_id);
  auto input_a_tensor_desc = input_a_node_output->GetTensorDesc();
  const NodeOutput* input_b_node_output =
      GetNodeOutputForOperand(id_to_node_output_map, input_b_id);
  auto input_b_tensor_desc = input_b_node_output->GetTensorDesc();

  std::vector<const NodeOutput*> inputs{input_a_node_output,
                                        input_b_node_output};

  OperandId output_id = gemm->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  // The input c tensor description may be broadcasted.
  std::optional<TensorDesc> input_c_tensor_desc;
  auto& c_operand_id = gemm->c_operand_id;
  if (c_operand_id) {
    OperandId input_c_id = c_operand_id.value();
    CHECK(context_properties.data_type_limits.gemm_c.Supports(
        operands.at(input_c_id.value())->descriptor));

    const NodeOutput* input_c_node_output =
        GetNodeOutputForOperand(id_to_node_output_map, input_c_id);
    input_c_tensor_desc = input_c_node_output->GetTensorDesc();

    // Ensure the graph edge for c operand will be created.
    inputs.push_back(input_c_node_output);

    auto output_dimensions = output_tensor_desc.GetDimensions();
    if (input_c_tensor_desc->GetDimensions() != output_dimensions) {
      input_c_tensor_desc->BroadcastTo(output_dimensions);
    }
  }

  // Use 4D GEMM which is available since feature level 1.0 for best
  // compatibility. There is no performance difference in the shader between
  // 2D/3D/4D, as 2D is just a variant of 4D with a batch/channel size of 1.
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gemm_operator_desc.
  // TODO(issues.chromium.org/327244277): Remove the workaround of coercing
  // GEMM's tensors to 4D.
  input_a_tensor_desc.EnsureMinimumRank(4, TensorDesc::Alignment::kTrailing);
  input_b_tensor_desc.EnsureMinimumRank(4, TensorDesc::Alignment::kTrailing);
  if (input_c_tensor_desc) {
    input_c_tensor_desc->EnsureMinimumRank(4, TensorDesc::Alignment::kTrailing);
  }
  auto expanded_output_tensor_desc = output_tensor_desc;
  expanded_output_tensor_desc.EnsureMinimumRank(
      4, TensorDesc::Alignment::kTrailing);

  std::optional<const Operation*> fusible_activation =
      GetFusibleActivationFromOperation(
          operation_to_fusible_standalone_activation_map, operation);
  std::optional<ActivationOperatorDesc> activation_operator_desc;
  std::optional<DML_OPERATOR_DESC> activation_dml_desc;
  std::string label = gemm->label;
  if (fusible_activation) {
    activation_operator_desc =
        CreateOperatorDescForFusibleActivation(*fusible_activation.value());
    activation_dml_desc = activation_operator_desc->GetActivationDmlDesc();

    output_id =
        GetFusibleActivationOutputId(*fusible_activation.value()).value();
    label = GetFusedOperatorLabel(label, "gemm", *fusible_activation.value());
  }

  DML_GEMM_OPERATOR_DESC gemm_operator_desc{
      .ATensor = &input_a_tensor_desc.GetDMLTensorDesc(),
      .BTensor = &input_b_tensor_desc.GetDMLTensorDesc(),
      .CTensor = GetOptionalDmlTensorDescPtr(input_c_tensor_desc),
      .OutputTensor = &expanded_output_tensor_desc.GetDMLTensorDesc(),
      .TransA = (gemm->a_transpose) ? DML_MATRIX_TRANSFORM_TRANSPOSE
                                    : DML_MATRIX_TRANSFORM_NONE,
      .TransB = (gemm->b_transpose) ? DML_MATRIX_TRANSFORM_TRANSPOSE
                                    : DML_MATRIX_TRANSFORM_NONE,
      .Alpha = gemm->alpha,
      .Beta = gemm->beta,
      .FusedActivation =
          activation_dml_desc ? &activation_dml_desc.value() : nullptr,
  };

  const GraphNode* gemm_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GEMM, &gemm_operator_desc, inputs, label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      gemm_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

// This helper checks if the input node output is a constant operand, if so,
// append an identity node to the input node output by calling
// `AppendIdentityNode`, otherwise do nothing and return `input` directly.
const NodeOutput* AppendIdentityToConstantOperand(
    GraphBuilderDml& graph_builder,
    const NodeOutput* input) {
  CHECK(input);
  // Do nothing if the input is without the DML_TENSOR_FLAG_OWNED_BY_DML flag.
  if (!(input->GetTensorDesc().GetFlags() & DML_TENSOR_FLAG_OWNED_BY_DML)) {
    return input;
  }
  // Append an identity node if the input is with the
  // DML_TENSOR_FLAG_OWNED_BY_DML flag. For certain operators like lstm and
  // gru, their input tensors don't support this flag and an identity is
  // needed to remove it.
  return AppendIdentityNode(graph_builder, input);
}

// `GruType` must be `mojom::GruPtr` or `mojom::GruCellPtr`.
template <typename GruType>
  requires(std::is_same_v<GruType, mojom::GruPtr> ||
           std::is_same_v<GruType, mojom::GruCellPtr>)
base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForGru(
    Adapter* adapter,
    const ContextProperties& context_properties,
    const GruType& gru,
    mojom::GraphInfoPtr& graph_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map,
    absl::flat_hash_map<OperandId, uint32_t>& constant_id_to_input_index_map) {
  const auto& operands = graph_info->operands;
  mojom::Operation::Tag op_tag;
  std::optional<OperandId> initial_hidden_state_operand_id;
  bool return_sequence;
  mojom::RecurrentNetworkDirection direction;
  if constexpr (std::is_same_v<GruType, mojom::GruPtr>) {
    CHECK(context_properties.data_type_limits.gru_input.SupportsAll(
        {operands.at(gru->input_operand_id.value())->descriptor,
         operands.at(gru->weight_operand_id.value())->descriptor,
         operands.at(gru->recurrent_weight_operand_id.value())->descriptor}));
    op_tag = mojom::Operation::Tag::kGru;
    initial_hidden_state_operand_id = gru->initial_hidden_state_operand_id;
    return_sequence = gru->return_sequence;
    direction = gru->direction;
  } else /* GruType is mojom::GruCellPtr */ {
    CHECK(context_properties.data_type_limits.gru_cell_input.SupportsAll(
        {operands.at(gru->input_operand_id.value())->descriptor,
         operands.at(gru->weight_operand_id.value())->descriptor,
         operands.at(gru->recurrent_weight_operand_id.value())->descriptor}));
    op_tag = mojom::Operation::Tag::kGruCell;
    initial_hidden_state_operand_id = gru->hidden_state_operand_id;
    return_sequence = false;
    direction = mojom::RecurrentNetworkDirection::kForward;
  }

  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, gru->input_operand_id);
  // Since the InputTensor doesn't support the DML_TENSOR_FLAG_OWNED_BY_DML
  // flag, add an identity operator to change the input type:
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gru_operator_desc
  input = AppendIdentityToConstantOperand(graph_builder, input);
  TensorDesc input_tensor_desc = input->GetTensorDesc();
  // The input tensor is 3-D for gru and 2-D for gruCell, while DirectML
  // expects a 4-D tensor.
  input_tensor_desc.EnsureMinimumRank(/*rank=*/4,
                                      TensorDesc::Alignment::kTrailing);

  const NodeOutput* weight =
      GetNodeOutputForOperand(id_to_node_output_map, gru->weight_operand_id);
  // Since the WeightTensor doesn't support the DML_TENSOR_FLAG_OWNED_BY_DML
  // flag, add an identity operator to change the input type:
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gru_operator_desc
  weight = AppendIdentityToConstantOperand(graph_builder, weight);
  TensorDesc weight_tensor_desc = weight->GetTensorDesc();
  // The weight tensor is 3-D for gru and 2-D for gruCell, while DirectML
  // expects a 4-D tensor.
  weight_tensor_desc.EnsureMinimumRank(/*rank*/ 4,
                                       TensorDesc::Alignment::kTrailing);

  const NodeOutput* recurrent_weight = GetNodeOutputForOperand(
      id_to_node_output_map, gru->recurrent_weight_operand_id);
  // Since the RecurrenceTensor doesn't support the
  // DML_TENSOR_FLAG_OWNED_BY_DML flag, add an identity operator to change the
  // input type:
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gru_operator_desc
  recurrent_weight =
      AppendIdentityToConstantOperand(graph_builder, recurrent_weight);
  TensorDesc recurrent_weight_tensor_desc = recurrent_weight->GetTensorDesc();
  // The recurrent weight tensor is 3-D for gru and 2-D for gruCell, while
  // DirectML expects a 4-D tensor.
  recurrent_weight_tensor_desc.EnsureMinimumRank(
      /*rank=*/4, TensorDesc::Alignment::kTrailing);

  std::vector<const NodeOutput*> inputs{input, weight, recurrent_weight};

  const OperandPtr& input_operand = operands.at(gru->input_operand_id.value());
  const OperandDataType data_type = input_operand->descriptor.data_type();

  const std::string& label = gru->label;
  std::optional<TensorDesc> concatenated_bias_tensor_desc;
  if (!gru->bias_operand_id.has_value() &&
      !gru->recurrent_bias_operand_id.has_value()) {
    // Use a nullptr to indicate there is no input edge for BiasTensor.
    inputs.push_back(nullptr);
  } else {
    // The DirectML bias tensor is the concatenation of bias and recurrent
    // bias (if bidirectional). Get or create the node output of bias and
    // recurrent bias for the following concat operation.
    std::optional<const NodeOutput*> zero_bias;
    if (!gru->bias_operand_id.has_value() ||
        !gru->recurrent_bias_operand_id.has_value()) {
      OperandId zero_bias_operand_id = BuildConstantOperandForFloatValue(
          context_properties, graph_info, constant_operands, data_type,
          /*rank*/ 1,
          /*default bias*/ 0);
      CreateConstantNode(adapter, zero_bias_operand_id, constant_operands,
                         graph_builder, id_to_node_output_map,
                         constant_id_to_input_index_map);
      zero_bias =
          GetNodeOutputForOperand(id_to_node_output_map, zero_bias_operand_id);
    }

    const NodeOutput* bias =
        gru->bias_operand_id.has_value()
            ? GetOptionalNodeOutputForOperand(id_to_node_output_map,
                                              gru->bias_operand_id)
            : zero_bias.value();
    const NodeOutput* recurrent_bias =
        gru->recurrent_bias_operand_id.has_value()
            ? GetOptionalNodeOutputForOperand(id_to_node_output_map,
                                              gru->recurrent_bias_operand_id)
            : zero_bias.value();

    const uint32_t num_directions =
        direction == mojom::RecurrentNetworkDirection::kBoth ? 2 : 1;
    uint32_t hidden_size = gru->hidden_size;
    // 3 * hidden_size has been verified.
    auto checked_three_times_hidden_size =
        base::MakeCheckedNum(hidden_size) * 3;
    CHECK(checked_three_times_hidden_size.IsValid());
    // The half bias dimensions is [1, 1, num_directions, 3 * hidden_size] for
    // gru and [1, 1, 1, 3 * hidden_size] for gruCell.
    const std::array<uint32_t, 4> half_bias_dimensions = {
        1, 1, num_directions, checked_three_times_hidden_size.ValueOrDie()};
    TensorDesc bias_tensor_desc = bias->GetTensorDesc();
    // The bias tensor shape is either [1] or [direction_count, 3 *
    // hidden_size], which can be broadcasted to [1, 1, direction_count, 3 *
    // hidden_size] as DirectML requires.
    bias_tensor_desc.BroadcastTo(half_bias_dimensions);
    TensorDesc recurrent_bias_tensor_desc = recurrent_bias->GetTensorDesc();
    recurrent_bias_tensor_desc.BroadcastTo(half_bias_dimensions);
    std::array<DML_TENSOR_DESC, 2> concat_input_tensor_descs = {
        bias_tensor_desc.GetDMLTensorDesc(),
        recurrent_bias_tensor_desc.GetDMLTensorDesc()};

    // The DirectML bias dimensions is [1, 1, num_directions, 6 *
    // hidden_size]. Ideally, 6 * hidden_size validation should be part of the
    // spec and validated for all backends. Spec issue tracked on
    // https://github.com/webmachinelearning/webnn/issues/625.
    auto checked_six_times_hidden_size = base::MakeCheckedNum(hidden_size) * 6;
    if (!checked_six_times_hidden_size.IsValid()) {
      return CreateUnexpectedError(
          mojom::Error::Code::kUnknownError,
          base::StringPrintf("The hidden size is too large for %s operator.",
                             OpTagToString(op_tag).c_str()),
          label);
    }
    std::vector<uint32_t> concatenated_bias_dimensions = {
        1, 1, num_directions, checked_six_times_hidden_size.ValueOrDie()};
    concatenated_bias_tensor_desc = TensorDesc(
        GetTensorDataType(data_type), std::move(concatenated_bias_dimensions));

    DML_JOIN_OPERATOR_DESC concat_operator_desc{
        .InputCount = concat_input_tensor_descs.size(),
        .InputTensors = concat_input_tensor_descs.data(),
        .OutputTensor = &concatenated_bias_tensor_desc->GetDMLTensorDesc(),
        .Axis = 3};
    std::array<const NodeOutput*, 2> bias_outputs = {bias, recurrent_bias};
    const GraphNode* concat_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_JOIN, &concat_operator_desc, bias_outputs, label);

    const NodeOutput* concatenated_bias = graph_builder.CreateNodeOutput(
        concat_node, concatenated_bias_tensor_desc.value(), 0);
    inputs.push_back(concatenated_bias);
  }

  std::optional<TensorDesc> initial_hidden_state_tensor_desc;
  if (initial_hidden_state_operand_id.has_value()) {
    if constexpr (std::is_same_v<GruType, mojom::GruPtr>) {
      CHECK(context_properties.data_type_limits.gru_input.Supports(
          operands.at(*initial_hidden_state_operand_id.value())->descriptor));
    } else /* GruType is mojom::GruCellPtr */ {
      CHECK(context_properties.data_type_limits.gru_cell_input.Supports(
          operands.at(*initial_hidden_state_operand_id.value())->descriptor));
    }
    const NodeOutput* initial_hidden_state = GetNodeOutputForOperand(
        id_to_node_output_map, initial_hidden_state_operand_id.value());
    // Since the HiddenInitTensor doesn't support the
    // DML_TENSOR_FLAG_OWNED_BY_DML flag, add an identity operator to change
    // the input type:
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gru_operator_desc
    initial_hidden_state =
        AppendIdentityToConstantOperand(graph_builder, initial_hidden_state);
    initial_hidden_state_tensor_desc = initial_hidden_state->GetTensorDesc();
    // The initial hidden state tensor shape is `[num_directions, batch_size,
    // hidden_size]`, while DirectML expects the shape to be `[1,
    // num_directions, batch_size, hidden_size]`.
    initial_hidden_state_tensor_desc->EnsureMinimumRank(
        /*rank*/ 4, TensorDesc::Alignment::kTrailing);
    inputs.push_back(initial_hidden_state);
  } else {
    // Use a nullptr to indicate there is no input edge for HiddenInitTensor.
    inputs.push_back(nullptr);
  }

  // Use a nullptr to indicate all sequences in the batch have length
  // seq_length:
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gru_operator_desc
  inputs.push_back(nullptr);

  std::vector<OperandId> output_ids;
  OperandId output_hidden_state_id;
  if constexpr (std::is_same<GruType, mojom::GruPtr>::value) {
    output_ids = gru->output_operand_ids;
    output_hidden_state_id = output_ids[0];
  } else {
    output_hidden_state_id = gru->output_operand_id;
  }
  TensorDesc output_hidden_state_tensor_desc =
      CreateOutputTensorDesc(operands, output_hidden_state_id);
  // The output hidden state tensor is 3-D for gru and 2-D for gruCell, while
  // DirectML expects a 4-D tensor.
  output_hidden_state_tensor_desc.EnsureMinimumRank(
      /*rank*/ 4, TensorDesc::Alignment::kTrailing);

  std::optional<OperandId> output_sequence_id;
  std::optional<TensorDesc> output_sequence_tensor_desc;
  if (return_sequence) {
    CHECK_EQ(output_ids.size(), 2u);
    output_sequence_id = output_ids[1];
    output_sequence_tensor_desc =
        CreateOutputTensorDesc(operands, output_sequence_id.value());
  }

  if (gru->layout != mojom::GruWeightLayout::kZrn) {
    return CreateUnexpectedError(
        mojom::Error::Code::kNotSupportedError,
        "The gru weight layout (rzn) is not supported.", label);
  }

  // When the recurrent network is bidirectional, dual activations must be
  // provided for the forward and backward directions.
  const size_t number_of_activations =
      direction == mojom::RecurrentNetworkDirection::kBoth
          ? gru->activations.size() * 2
          : gru->activations.size();

  base::FixedArray<ActivationOperatorDesc> activation_operator_descs(
      number_of_activations);
  for (size_t i = 0; i < gru->activations.size(); ++i) {
    activation_operator_descs[i] =
        CreateOperatorDescForActivation(gru->activations[i]);
    // For bidirectional, activations must be provided f() and g() for forward
    // followed by f() and g() for backwards.
    if (direction == mojom::RecurrentNetworkDirection::kBoth) {
      activation_operator_descs[gru->activations.size() + i] =
          activation_operator_descs[i];
    }
  }

  base::FixedArray<DML_OPERATOR_DESC> activation_dml_descs(
      activation_operator_descs.size());
  std::ranges::transform(
      activation_operator_descs, std::begin(activation_dml_descs),
      [](const auto& activation_operator_desc) {
        return activation_operator_desc.GetActivationDmlDesc();
      });

  DML_GRU_OPERATOR_DESC gru_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .WeightTensor = &weight_tensor_desc.GetDMLTensorDesc(),
      .RecurrenceTensor = &recurrent_weight_tensor_desc.GetDMLTensorDesc(),
      .BiasTensor = GetOptionalDmlTensorDescPtr(concatenated_bias_tensor_desc),
      .HiddenInitTensor =
          GetOptionalDmlTensorDescPtr(initial_hidden_state_tensor_desc),
      .SequenceLengthsTensor = nullptr,
      .OutputSequenceTensor =
          GetOptionalDmlTensorDescPtr(output_sequence_tensor_desc),
      .OutputSingleTensor = &output_hidden_state_tensor_desc.GetDMLTensorDesc(),
      .ActivationDescCount = static_cast<uint32_t>(activation_dml_descs.size()),
      .ActivationDescs = activation_dml_descs.data(),
      .Direction = MojoRecurrentNetworkDirectionToDml(direction),
      .LinearBeforeReset = gru->reset_after};

  const GraphNode* gru_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GRU, &gru_desc, inputs, label);

  const NodeOutput* output_hidden_state = graph_builder.CreateNodeOutput(
      gru_node, output_hidden_state_tensor_desc, /*output_index*/ 1);
  CHECK(id_to_node_output_map
            .try_emplace(output_hidden_state_id, output_hidden_state)
            .second);

  if (return_sequence) {
    const NodeOutput* output_sequence = graph_builder.CreateNodeOutput(
        gru_node, output_sequence_tensor_desc.value(), /*output_index*/ 0);
    CHECK(id_to_node_output_map
              .try_emplace(output_sequence_id.value(), output_sequence)
              .second);
  }

  return base::ok();
}

void CreateOperatorNodeForHardSigmoid(
    const std::vector<OperandPtr>& operands,
    const mojom::HardSigmoidPtr& hard_sigmoid,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, hard_sigmoid->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  const OperandId output_id = hard_sigmoid->output_operand_id;
  auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC hard_sigmoid_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Alpha = hard_sigmoid->alpha,
      .Beta = hard_sigmoid->beta};

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* hard_sigmoid_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_HARD_SIGMOID, &hard_sigmoid_desc, inputs,
      hard_sigmoid->label);

  const NodeOutput* node_output = graph_builder.CreateNodeOutput(
      hard_sigmoid_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
}

void CreateOperatorNodeForHardSwish(Adapter* adapter,
                                    const std::vector<OperandPtr>& operands,
                                    const mojom::HardSwishPtr& hard_swish,
                                    GraphBuilderDml& graph_builder,
                                    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, hard_swish->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  const OperandId output_id = hard_swish->output_operand_id;
  auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  const float scale = 1.0 / 6.0;
  const float bias = 0.5;
  const std::string& label = hard_swish->label;
  if (adapter->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_6_2)) {
    std::array<const NodeOutput*, 1> inputs = {input};
    DML_ACTIVATION_HARD_SWISH_OPERATOR_DESC hard_swish_desc{
        .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
        .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
        .Alpha = scale,
        .Beta = bias};
    const GraphNode* hard_swish_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_ACTIVATION_HARD_SWISH, &hard_swish_desc, inputs, label);

    const NodeOutput* output =
        graph_builder.CreateNodeOutput(hard_swish_node, output_tensor_desc);
    // The output id must be unique in the map.
    CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
    return;
  }
  // If DirectML's feature level is before 6.2, we need to implement hardSwish
  // by composing from smaller operators:
  // Output = input * clamp((input / 6) + 0.5, 0, 1).
  // First step: build `clamp((x / 6) + 0.5, 0, 1)`.
  DML_SCALE_BIAS scale_bias = {.Scale = scale, .Bias = bias};
  DML_ELEMENT_WISE_CLIP_OPERATOR_DESC clamp_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      // Applying the function `g(x) = x / 6 + 0.5` to each input element
      // prior to clamp.
      .ScaleBias = &scale_bias,
      .Min = 0,
      .Max = 1};
  std::array<const NodeOutput*, 1> clamp_inputs = {input};
  const GraphNode* clamp_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_CLIP, &clamp_operator_desc, clamp_inputs,
      label);

  const NodeOutput* clamp_output =
      graph_builder.CreateNodeOutput(clamp_node, output_tensor_desc, 0);
  const auto& clamp_output_tensor_desc = clamp_output->GetTensorDesc();

  // Second step: build `x * first_step`.
  std::array<const NodeOutput*, 2> mul_inputs = {input, clamp_output};
  DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC binary_mul_desc{
      .ATensor = &input_tensor_desc.GetDMLTensorDesc(),
      .BTensor = &clamp_output_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};
  const GraphNode* binary_mul_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_MULTIPLY, &binary_mul_desc, mul_inputs, label);

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(binary_mul_node, output_tensor_desc);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

template <typename NormalizationPtr>
  requires(std::is_same_v<NormalizationPtr, mojom::InstanceNormalizationPtr> ||
           std::is_same_v<NormalizationPtr, mojom::LayerNormalizationPtr>)
base::expected<void, mojom::ErrorPtr>
CreateOperatorNodeForMeanVarianceNormalization(
    Adapter* adapter,
    const ContextProperties& context_properties,
    const NormalizationPtr& normalization,
    const Operation* operation,
    const absl::flat_hash_map<const Operation*,
                              raw_ptr<const Operation, CtnExperimental>>&
        operation_to_fusible_standalone_activation_map,
    mojom::GraphInfoPtr& graph_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map,
    absl::flat_hash_map<OperandId, uint32_t>& constant_id_to_input_index_map,
    base::span<const uint32_t> mean_variance_axes,
    base::span<const uint32_t> scale_bias_broadcast_axes,
    mojom::Operation::Tag op) {
  const auto& operands = graph_info->operands;

  OperandId input_id = normalization->input_operand_id;
  const OperandPtr& input_operand = operands.at(input_id.value());
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, input_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  size_t input_rank = input_tensor_desc.GetDimensions().size();

  OperandId output_id = normalization->output_operand_id;
  const OperandPtr& output_operand = operands.at(output_id.value());
  OperandDataType output_data_type = output_operand->descriptor.data_type();

  if constexpr (std::is_same_v<NormalizationPtr,
                               mojom::InstanceNormalizationPtr>) {
    CHECK(context_properties.data_type_limits.instance_normalization_input
              .Supports(input_operand->descriptor));
    CHECK(context_properties.data_type_limits.instance_normalization_input
              .data_types.Has(output_data_type));
  } else /* `NormalizationPtr` is `mojom::LayerNormalizationPtr` */ {
    CHECK(
        context_properties.data_type_limits.layer_normalization_input.Supports(
            input_operand->descriptor));
    CHECK(context_properties.data_type_limits.layer_normalization_input
              .data_types.Has(output_data_type));
  }

  const TensorDesc output_tensor_desc(GetTensorDataType(output_data_type),
                                      output_operand->descriptor.shape());

  const NodeOutput* scale = GetOptionalNodeOutputForOperand(
      id_to_node_output_map, normalization->scale_operand_id);
  const NodeOutput* bias = GetOptionalNodeOutputForOperand(
      id_to_node_output_map, normalization->bias_operand_id);

  // DML_MEAN_VARIANCE_NORMALIZATION1_OPERATOR_DESC requires `ScaleTensor` and
  // `BiasTensor` to be both present or not present when DML_FEATURE_LEVEL is
  // less than DML_FEATURE_LEVEL_5_2.
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_mean_variance_normalization1_operator_desc.
  //
  // If one of scale/bias is not present, create a constant operand for it and
  // insert the operand into the graph.
  if ((scale && !bias) || (!scale && bias)) {
    if (!scale) {
      OperandId scale_operand_id = BuildConstantOperandForFloatValue(
          context_properties, graph_info, constant_operands, output_data_type,
          scale_bias_broadcast_axes.size(),
          /*default scale*/ 1.0);
      CreateConstantNode(adapter, scale_operand_id, constant_operands,
                         graph_builder, id_to_node_output_map,
                         constant_id_to_input_index_map);

      scale = GetNodeOutputForOperand(id_to_node_output_map, scale_operand_id);
    }
    if (!bias) {
      OperandId bias_operand_id = BuildConstantOperandForFloatValue(
          context_properties, graph_info, constant_operands, output_data_type,
          scale_bias_broadcast_axes.size(),
          /*default bias*/ 0);
      CreateConstantNode(adapter, bias_operand_id, constant_operands,
                         graph_builder, id_to_node_output_map,
                         constant_id_to_input_index_map);

      bias = GetNodeOutputForOperand(id_to_node_output_map, bias_operand_id);
    }
  }

  std::string label = normalization->label;
  if (!base::MakeCheckedNum(mean_variance_axes.size()).IsValid<uint32_t>()) {
    return base::unexpected(CreateError(
        mojom::Error::Code::kUnknownError,
        OpTagToString(op) + ": The axes rank is too large.", label));
  }

  std::vector<const NodeOutput*> inputs = {input};
  std::optional<TensorDesc> scale_tensor_desc;
  std::optional<TensorDesc> bias_tensor_desc;

  if (scale) {
    inputs.push_back(scale);
    scale_tensor_desc = scale->GetTensorDesc();
    // The scale tensor should have the same rank as the input tensor required
    // by DML_MEAN_VARIANCE_NORMALIZATION1_OPERATOR_DESC.
    scale_tensor_desc->MakeBroadcastCompatible(input_rank,
                                               scale_bias_broadcast_axes);
  }
  if (bias) {
    inputs.push_back(bias);
    bias_tensor_desc = bias->GetTensorDesc();
    // The bias tensor should have the same rank as the input tensor required
    // by DML_MEAN_VARIANCE_NORMALIZATION1_OPERATOR_DESC.
    bias_tensor_desc->MakeBroadcastCompatible(input_rank,
                                              scale_bias_broadcast_axes);
  }

  std::optional<const Operation*> fusible_activation =
      GetFusibleActivationFromOperation(
          operation_to_fusible_standalone_activation_map, operation);
  std::optional<ActivationOperatorDesc> activation_operator_desc;
  std::optional<DML_OPERATOR_DESC> activation_dml_desc;
  if (fusible_activation) {
    activation_operator_desc =
        CreateOperatorDescForFusibleActivation(*fusible_activation.value());
    activation_dml_desc = activation_operator_desc->GetActivationDmlDesc();

    output_id =
        GetFusibleActivationOutputId(*fusible_activation.value()).value();

    std::string_view default_label;
    if (label.empty()) {
      if constexpr (std::is_same_v<NormalizationPtr,
                                   mojom::InstanceNormalizationPtr>) {
        default_label = "instance_normalization";
      } else /* `NormalizationPtr` is `mojom::LayerNormalizationPtr` */ {
        default_label = "layer_normalization";
      }
    }
    label = GetFusedOperatorLabel(label, default_label,
                                  *fusible_activation.value());
  }

  DML_MEAN_VARIANCE_NORMALIZATION1_OPERATOR_DESC
  normalization_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .ScaleTensor = GetOptionalDmlTensorDescPtr(scale_tensor_desc),
      .BiasTensor = GetOptionalDmlTensorDescPtr(bias_tensor_desc),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .AxisCount = base::checked_cast<uint32_t>(mean_variance_axes.size()),
      .Axes = mean_variance_axes.data(),
      // The layer normalization and instance normalization includes variance.
      .NormalizeVariance = true,
      .Epsilon = normalization->epsilon,
      .FusedActivation =
          activation_dml_desc ? &activation_dml_desc.value() : nullptr,
  };

  const GraphNode* normalization_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION1, &normalization_operator_desc,
      inputs, label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      normalization_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

void CreateOperatorNodeForLeakyRelu(const std::vector<OperandPtr>& operands,
                                    const mojom::LeakyReluPtr& leaky_relu,
                                    GraphBuilderDml& graph_builder,
                                    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, leaky_relu->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  OperandId output_id = leaky_relu->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC leaky_relu_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Alpha = leaky_relu->alpha};

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* leaky_relu_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_LEAKY_RELU, &leaky_relu_desc, inputs,
      leaky_relu->label);

  const NodeOutput* node_output = graph_builder.CreateNodeOutput(
      leaky_relu_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
}

void CreateOperatorNodeForLinear(const ContextProperties& context_properties,
                                 const std::vector<OperandPtr>& operands,
                                 const mojom::LinearPtr& linear,
                                 GraphBuilderDml& graph_builder,
                                 IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, linear->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  CHECK(context_properties.data_type_limits.linear_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  OperandId output_id = linear->output_operand_id;
  auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  DML_ACTIVATION_LINEAR_OPERATOR_DESC linear_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Alpha = linear->alpha,
      .Beta = linear->beta};

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* linear_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_LINEAR, &linear_desc, inputs, linear->label);

  const NodeOutput* node_output = graph_builder.CreateNodeOutput(
      linear_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
}

// `LstmType` must be `mojom::Lstm` or `mojom::LstmCell`.
template <typename LstmType>
  requires(std::is_same_v<LstmType, mojom::Lstm> ||
           std::is_same_v<LstmType, mojom::LstmCell>)
base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForLstm(
    Adapter* adapter,
    const ContextProperties& context_properties,
    const LstmType& lstm,
    mojom::GraphInfoPtr& graph_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map,
    absl::flat_hash_map<OperandId, uint32_t>& constant_id_to_input_index_map) {
  const std::string& label = lstm.label;
  const auto& operands = graph_info->operands;

  mojom::Operation::Tag op_tag;
  std::optional<OperandId> initial_hidden_state_operand_id;
  std::optional<OperandId> initial_cell_state_operand_id;
  bool return_sequence;
  mojom::RecurrentNetworkDirection direction;
  if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
    CHECK(context_properties.data_type_limits.lstm_input.SupportsAll(
        {operands.at(lstm.input_operand_id.value())->descriptor,
         operands.at(lstm.weight_operand_id.value())->descriptor,
         operands.at(lstm.recurrent_weight_operand_id.value())->descriptor}));
    op_tag = mojom::Operation::Tag::kLstm;
    initial_hidden_state_operand_id = lstm.initial_hidden_state_operand_id;
    initial_cell_state_operand_id = lstm.initial_cell_state_operand_id;
    return_sequence = lstm.return_sequence;
    direction = lstm.direction;
  } else /* `LstmType` is `mojom::LstmCell` */ {
    CHECK(context_properties.data_type_limits.lstm_cell_input.SupportsAll(
        {operands.at(lstm.input_operand_id.value())->descriptor,
         operands.at(lstm.weight_operand_id.value())->descriptor,
         operands.at(lstm.recurrent_weight_operand_id.value())->descriptor}));
    op_tag = mojom::Operation::Tag::kLstmCell;
    initial_hidden_state_operand_id = lstm.hidden_state_operand_id;
    initial_cell_state_operand_id = lstm.cell_state_operand_id;
    return_sequence = false;
    direction = mojom::RecurrentNetworkDirection::kForward;
  }

  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, lstm.input_operand_id);
  // Append an identity node if the input is a constant operand since
  // InputTensor doesn't support the DML_TENSOR_FLAG_OWNED_BY_DML flag.
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_lstm_operator_desc
  input = AppendIdentityToConstantOperand(graph_builder, input);
  TensorDesc input_tensor_desc = input->GetTensorDesc();
  const DML_TENSOR_DATA_TYPE input_dml_data_type =
      input_tensor_desc.GetDataType();
  const OperandDataType input_data_type =
      DmlDataTypeToOperand(input_dml_data_type);
  // The input tensor is 2-D for lstmCell and 3-D for lstm, while DirectML
  // expects a 4-D tensor.
  input_tensor_desc.EnsureMinimumRank(/*rank=*/4,
                                      TensorDesc::Alignment::kTrailing);

  const NodeOutput* weight =
      GetNodeOutputForOperand(id_to_node_output_map, lstm.weight_operand_id);
  // Append an identity node if the weight is a constant operand since
  // WeightTensor doesn't support the DML_TENSOR_FLAG_OWNED_BY_DML flag.
  weight = AppendIdentityToConstantOperand(graph_builder, weight);
  TensorDesc weight_tensor_desc = weight->GetTensorDesc();
  // The weight tensor is 2-D for lstmCell and 3-D for lstm, while DirectML
  // expects a 4-D tensor.
  weight_tensor_desc.EnsureMinimumRank(/*rank=*/4,
                                       TensorDesc::Alignment::kTrailing);

  const NodeOutput* recurrent_weight = GetNodeOutputForOperand(
      id_to_node_output_map, lstm.recurrent_weight_operand_id);
  // Append an identity node if the recurrent weight is a constant operand
  // since RecurrenceTensor doesn't support the DML_TENSOR_FLAG_OWNED_BY_DML
  // flag.
  recurrent_weight =
      AppendIdentityToConstantOperand(graph_builder, recurrent_weight);
  TensorDesc recurrent_weight_tensor_desc = recurrent_weight->GetTensorDesc();
  // The recurrent weight tensor is 2-D for lstmCell and 3-D for lstm, while
  // DirectML expects a 4-D tensor.
  recurrent_weight_tensor_desc.EnsureMinimumRank(
      /*rank=*/4, TensorDesc::Alignment::kTrailing);

  const uint32_t direction_count =
      direction == mojom::RecurrentNetworkDirection::kBoth ? 2 : 1;

  const NodeOutput* weight_iofg = weight;
  const NodeOutput* recurrent_weight_iofg = recurrent_weight;
  if (lstm.layout == mojom::LstmWeightLayout::kIfgo) {
    // Rearrange the layout of weights from ifgo to iofg by splitting ifgo to
    // i,f,g,o and concatenating them to iofg.

    const uint32_t input_size = input_tensor_desc.GetDimensions().at(3);
    std::vector<uint32_t> split_weight_output_dims = {
        1, direction_count, lstm.hidden_size, input_size};
    TensorDesc split_weight_output_tensor_desc(
        input_dml_data_type, std::move(split_weight_output_dims));
    std::array<DML_TENSOR_DESC, 4> split_weight_tensor_descs_dml;
    split_weight_tensor_descs_dml.fill(
        split_weight_output_tensor_desc.GetDMLTensorDesc());

    DML_SPLIT_OPERATOR_DESC split_desc{
        .InputTensor = &weight_tensor_desc.GetDMLTensorDesc(),
        .OutputCount = 4,
        .OutputTensors = split_weight_tensor_descs_dml.data(),
        .Axis = 2};

    std::array<const NodeOutput*, 1> split_weight_inputs = {weight};
    const GraphNode* split_weight_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_SPLIT, &split_desc, split_weight_inputs,
        label + "_split_weight_ifgo");
    const NodeOutput* split_weight_output_i = graph_builder.CreateNodeOutput(
        split_weight_node, split_weight_output_tensor_desc, /*output_index=*/0);
    const NodeOutput* split_weight_output_f = graph_builder.CreateNodeOutput(
        split_weight_node, split_weight_output_tensor_desc, /*output_index=*/1);
    const NodeOutput* split_weight_output_g = graph_builder.CreateNodeOutput(
        split_weight_node, split_weight_output_tensor_desc, /*output_index=*/2);
    const NodeOutput* split_weight_output_o = graph_builder.CreateNodeOutput(
        split_weight_node, split_weight_output_tensor_desc, /*output_index=*/3);

    DML_JOIN_OPERATOR_DESC concat_desc{
        .InputCount = 4,
        .InputTensors = split_weight_tensor_descs_dml.data(),
        .OutputTensor = &weight_tensor_desc.GetDMLTensorDesc(),
        .Axis = 2};

    std::array<const NodeOutput*, 4> concat_weight_inputs = {
        split_weight_output_i, split_weight_output_o, split_weight_output_f,
        split_weight_output_g};
    const GraphNode* concat_weight_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_JOIN, &concat_desc, concat_weight_inputs,
        label + "_concat_weight_iofg");
    weight_iofg =
        graph_builder.CreateNodeOutput(concat_weight_node, weight_tensor_desc);

    std::vector<uint32_t> split_recurrent_weight_output_dims = {
        1, direction_count, lstm.hidden_size, lstm.hidden_size};
    TensorDesc split_recurrent_weight_output_tensor_desc(
        input_dml_data_type, std::move(split_recurrent_weight_output_dims));
    std::array<DML_TENSOR_DESC, 4> split_recurrent_weight_tensor_descs_dml;
    split_recurrent_weight_tensor_descs_dml.fill(
        split_recurrent_weight_output_tensor_desc.GetDMLTensorDesc());

    split_desc.InputTensor = &recurrent_weight_tensor_desc.GetDMLTensorDesc();
    split_desc.OutputTensors = split_recurrent_weight_tensor_descs_dml.data();

    std::array<const NodeOutput*, 1> split_recurrent_weight_inputs = {
        recurrent_weight};
    const GraphNode* split_recurrent_weight_node =
        graph_builder.CreateOperatorNode(
            DML_OPERATOR_SPLIT, &split_desc, split_recurrent_weight_inputs,
            label + "_split_recurrent_weight_ifgo");
    const NodeOutput* split_recurrent_weight_output_i =
        graph_builder.CreateNodeOutput(
            split_recurrent_weight_node,
            split_recurrent_weight_output_tensor_desc, /*output_index=*/0);
    const NodeOutput* split_recurrent_weight_output_f =
        graph_builder.CreateNodeOutput(
            split_recurrent_weight_node,
            split_recurrent_weight_output_tensor_desc, /*output_index=*/1);
    const NodeOutput* split_recurrent_weight_output_g =
        graph_builder.CreateNodeOutput(
            split_recurrent_weight_node,
            split_recurrent_weight_output_tensor_desc, /*output_index=*/2);
    const NodeOutput* split_recurrent_weight_output_o =
        graph_builder.CreateNodeOutput(
            split_recurrent_weight_node,
            split_recurrent_weight_output_tensor_desc, /*output_index=*/3);

    concat_desc.InputTensors = split_recurrent_weight_tensor_descs_dml.data();
    concat_desc.OutputTensor = &recurrent_weight_tensor_desc.GetDMLTensorDesc();

    std::array<const NodeOutput*, 4> concat_recurrent_weight_inputs = {
        split_recurrent_weight_output_i, split_recurrent_weight_output_o,
        split_recurrent_weight_output_f, split_recurrent_weight_output_g};
    const GraphNode* concat_recurrent_weight_node =
        graph_builder.CreateOperatorNode(
            DML_OPERATOR_JOIN, &concat_desc, concat_recurrent_weight_inputs,
            label + "_concat_recurrent_weight_iofg");
    recurrent_weight_iofg = graph_builder.CreateNodeOutput(
        concat_recurrent_weight_node, recurrent_weight_tensor_desc);
  }

  const std::vector<OperandId>& output_ids = lstm.output_operand_ids;
  const size_t output_count = output_ids.size();
  CHECK_GE(output_count, 2u);

  const OperandId output_hidden_state_id = output_ids[0];
  const OperandPtr& output_hidden_state_operand =
      operands.at(output_hidden_state_id.value());
  TensorDesc output_hidden_state_tensor_desc(
      input_dml_data_type, output_hidden_state_operand->descriptor.shape());
  // The output hidden state tensor is 2-D for lstmCell and 3-D for lstm,
  // while DirectML expects a 4-D tensor.
  output_hidden_state_tensor_desc.EnsureMinimumRank(
      /*rank=*/4, TensorDesc::Alignment::kTrailing);

  const OperandId output_cell_state_id = output_ids[1];
  TensorDesc output_cell_state_tensor_desc =
      CreateOutputTensorDesc(operands, output_cell_state_id);
  // The output cell state tensor is 2-D for lstmCell and 3-D for lstm, while
  // DirectML expects a 4-D tensor.
  output_cell_state_tensor_desc.EnsureMinimumRank(
      /*rank=*/4, TensorDesc::Alignment::kTrailing);

  std::optional<OperandId> output_sequence_id;
  std::optional<TensorDesc> output_sequence_tensor_desc;
  if (return_sequence) {
    CHECK_EQ(output_count, 3u);
    output_sequence_id = output_ids[2];
    output_sequence_tensor_desc =
        CreateOutputTensorDesc(operands, output_sequence_id.value());
  }

  const NodeOutput* bias = GetOptionalNodeOutputForOperand(
      id_to_node_output_map, lstm.bias_operand_id);
  const NodeOutput* recurrent_bias = GetOptionalNodeOutputForOperand(
      id_to_node_output_map, lstm.recurrent_bias_operand_id);

  // DML_LSTM_OPERATOR_DESC only takes a concatenation of {bias,
  // recurrent_bias} or none, so create a constant bias operand if one of the
  // biases is not given.
  if ((bias && !recurrent_bias) || (!bias && recurrent_bias)) {
    OperandId bias_operand_id = BuildConstantOperandForFloatValue(
        context_properties, graph_info, constant_operands, input_data_type,
        /*rank=*/1, /*default bias=*/0);
    CreateConstantNode(adapter, bias_operand_id, constant_operands,
                       graph_builder, id_to_node_output_map,
                       constant_id_to_input_index_map);

    if (!bias) {
      bias = GetNodeOutputForOperand(id_to_node_output_map, bias_operand_id);
    }
    if (!recurrent_bias) {
      recurrent_bias =
          GetNodeOutputForOperand(id_to_node_output_map, bias_operand_id);
    }
  }

  // Bias operands should be both present or not present.
  CHECK((bias && recurrent_bias) || (!bias && !recurrent_bias));

  std::vector<const NodeOutput*> inputs{input, weight_iofg,
                                        recurrent_weight_iofg};

  // Concatenate the bias operands if they are both present.
  std::optional<TensorDesc> concatenated_bias_tensor_desc;
  if (bias && recurrent_bias) {
    auto checked_four_times_hidden_size =
        base::MakeCheckedNum(lstm.hidden_size) * 4;
    // Four times hidden size should have already been validated.
    CHECK(checked_four_times_hidden_size.IsValid());
    const std::array<uint32_t, 4> bias_dimensions = {
        1, 1, direction_count, checked_four_times_hidden_size.ValueOrDie()};

    // The bias tensor shape is [1] or `[4 * hidden_size]` or
    // [direction_count, 4 * hidden_size], which can be broadcasted to [1, 1,
    // direction_count, 4 * hidden_size] as DirectML requires.
    TensorDesc bias_tensor_desc = bias->GetTensorDesc();
    bias_tensor_desc.BroadcastTo(bias_dimensions);

    TensorDesc recurrent_bias_tensor_desc = recurrent_bias->GetTensorDesc();
    recurrent_bias_tensor_desc.BroadcastTo(bias_dimensions);

    std::array<DML_TENSOR_DESC, 2> bias_dml_tensor_descs = {
        bias_tensor_desc.GetDMLTensorDesc(),
        recurrent_bias_tensor_desc.GetDMLTensorDesc()};

    auto checked_eight_times_hidden_size = checked_four_times_hidden_size * 2;
    if (!checked_eight_times_hidden_size.IsValid()) {
      return CreateUnexpectedError(
          mojom::Error::Code::kUnknownError,
          base::StringPrintf("The hidden size is too large for %s operator.",
                             OpTagToString(op_tag).c_str()),
          label);
    }
    // The concatenated bias dimensions is [1, 1, direction_count, 8 *
    // hidden_size].
    std::vector<uint32_t> concatenated_dimensions = {
        1, 1, direction_count, checked_eight_times_hidden_size.ValueOrDie()};
    concatenated_bias_tensor_desc =
        TensorDesc(input_dml_data_type, std::move(concatenated_dimensions));

    DML_JOIN_OPERATOR_DESC concat_operator_desc{
        .InputCount = static_cast<uint32_t>(bias_dml_tensor_descs.size()),
        .InputTensors = bias_dml_tensor_descs.data(),
        .OutputTensor = &concatenated_bias_tensor_desc->GetDMLTensorDesc(),
        .Axis = 3};

    std::array<const NodeOutput*, 2> biases = {bias, recurrent_bias};
    const GraphNode* concat_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_JOIN, &concat_operator_desc, biases,
        label + "_concat_bias_and_recurrent");

    const NodeOutput* concatenated_bias = graph_builder.CreateNodeOutput(
        concat_node, concatenated_bias_tensor_desc.value(), 0);

    const NodeOutput* concatenated_bias_iofg = concatenated_bias;
    if (lstm.layout == mojom::LstmWeightLayout::kIfgo) {
      // Rearrange the layout of biases from ifgo to iofg by splitting ifgo to
      // i,f,g,o and concatenating them to iofg.

      std::vector<uint32_t> split_bias_output_dims = {1, 1, direction_count,
                                                      lstm.hidden_size};
      TensorDesc split_bias_output_tensor_desc(
          input_dml_data_type, std::move(split_bias_output_dims));
      std::array<DML_TENSOR_DESC, 8> split_bias_tensor_descs_dml;
      split_bias_tensor_descs_dml.fill(
          split_bias_output_tensor_desc.GetDMLTensorDesc());

      DML_SPLIT_OPERATOR_DESC split_desc{
          .InputTensor = &concatenated_bias_tensor_desc->GetDMLTensorDesc(),
          .OutputCount = 8,
          .OutputTensors = split_bias_tensor_descs_dml.data(),
          .Axis = 3};

      std::array<const NodeOutput*, 1> split_bias_inputs = {concatenated_bias};
      const GraphNode* split_bias_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_SPLIT, &split_desc, split_bias_inputs,
          label + "_split_bias_ifgo");
      const NodeOutput* split_bias_output_i = graph_builder.CreateNodeOutput(
          split_bias_node, split_bias_output_tensor_desc, /*output_index=*/0);
      const NodeOutput* split_bias_output_f = graph_builder.CreateNodeOutput(
          split_bias_node, split_bias_output_tensor_desc, /*output_index=*/1);
      const NodeOutput* split_bias_output_g = graph_builder.CreateNodeOutput(
          split_bias_node, split_bias_output_tensor_desc, /*output_index=*/2);
      const NodeOutput* split_bias_output_o = graph_builder.CreateNodeOutput(
          split_bias_node, split_bias_output_tensor_desc, /*output_index=*/3);
      const NodeOutput* split_recurrent_bias_output_i =
          graph_builder.CreateNodeOutput(split_bias_node,
                                         split_bias_output_tensor_desc,
                                         /*output_index=*/4);
      const NodeOutput* split_recurrent_bias_output_f =
          graph_builder.CreateNodeOutput(split_bias_node,
                                         split_bias_output_tensor_desc,
                                         /*output_index=*/5);
      const NodeOutput* split_recurrent_bias_output_g =
          graph_builder.CreateNodeOutput(split_bias_node,
                                         split_bias_output_tensor_desc,
                                         /*output_index=*/6);
      const NodeOutput* split_recurrent_bias_output_o =
          graph_builder.CreateNodeOutput(split_bias_node,
                                         split_bias_output_tensor_desc,
                                         /*output_index=*/7);

      DML_JOIN_OPERATOR_DESC concat_bias_desc{
          .InputCount = 8,
          .InputTensors = split_bias_tensor_descs_dml.data(),
          .OutputTensor = &concatenated_bias_tensor_desc->GetDMLTensorDesc(),
          .Axis = 3};

      std::array<const NodeOutput*, 8> concat_bias_inputs = {
          split_bias_output_i,           split_bias_output_o,
          split_bias_output_f,           split_bias_output_g,
          split_recurrent_bias_output_i, split_recurrent_bias_output_o,
          split_recurrent_bias_output_f, split_recurrent_bias_output_g};
      const GraphNode* concat_bias_iofg_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_JOIN, &concat_bias_desc, concat_bias_inputs,
          label + "_concat_bias_iofg");
      concatenated_bias_iofg = graph_builder.CreateNodeOutput(
          concat_bias_iofg_node, concatenated_bias_tensor_desc.value());
    }
    inputs.push_back(concatenated_bias_iofg);
  } else {
    // Use a nullptr to indicate there is no input edge for BiasTensor.
    inputs.push_back(nullptr);
  }

  std::optional<TensorDesc> initial_hidden_state_tensor_desc;
  if (initial_hidden_state_operand_id.has_value()) {
    if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
      CHECK(context_properties.data_type_limits.lstm_input.Supports(
          operands.at(*initial_hidden_state_operand_id.value())->descriptor));
    } else /* `LstmType` is `mojom::LstmCell` */ {
      CHECK(context_properties.data_type_limits.lstm_cell_input.Supports(
          operands.at(*initial_hidden_state_operand_id.value())->descriptor));
    }
    const NodeOutput* initial_hidden_state = GetNodeOutputForOperand(
        id_to_node_output_map, initial_hidden_state_operand_id.value());
    // Append an identity node if the initial hidden state is a constant
    // operand since HiddenInitTensor doesn't support the
    // DML_TENSOR_FLAG_OWNED_BY_DML flag.
    initial_hidden_state =
        AppendIdentityToConstantOperand(graph_builder, initial_hidden_state);
    inputs.push_back(initial_hidden_state);
    initial_hidden_state_tensor_desc = initial_hidden_state->GetTensorDesc();
    // The initial hidden state tensor is 2-D for lstmCell and 3-D for lstm,
    // while DirectML expects a 4-D tensor.
    initial_hidden_state_tensor_desc->EnsureMinimumRank(
        /*rank=*/4, TensorDesc::Alignment::kTrailing);
  } else {
    // Use a nullptr to indicate there is no input edge for HiddenInitTensor.
    inputs.push_back(nullptr);
  }

  std::optional<TensorDesc> initial_cell_state_tensor_desc;
  if (initial_cell_state_operand_id.has_value()) {
    if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
      CHECK(context_properties.data_type_limits.lstm_input.Supports(
          operands.at(*initial_cell_state_operand_id.value())->descriptor));
    } else /* `LstmType` is `mojom::LstmCell` */ {
      CHECK(context_properties.data_type_limits.lstm_cell_input.Supports(
          operands.at(*initial_cell_state_operand_id.value())->descriptor));
    }
    const NodeOutput* initial_cell_state = GetNodeOutputForOperand(
        id_to_node_output_map, initial_cell_state_operand_id.value());
    // Append an identity node if the initial cell state is a constant operand
    // since CellMemInitTensor doesn't support the
    // DML_TENSOR_FLAG_OWNED_BY_DML flag.
    initial_cell_state =
        AppendIdentityToConstantOperand(graph_builder, initial_cell_state);
    inputs.push_back(initial_cell_state);
    initial_cell_state_tensor_desc = initial_cell_state->GetTensorDesc();
    // The initial cell state tensor is 2-D for lstmCell and 3-D for lstm,
    // while DirectML expects a 4-D tensor.
    initial_cell_state_tensor_desc->EnsureMinimumRank(
        /*rank=*/4, TensorDesc::Alignment::kTrailing);
  } else {
    // Use a nullptr to indicate there is no input edge for CellMemInitTensor.
    inputs.push_back(nullptr);
  }

  // Use a nullptr to indicate there is no input edge for
  // SequenceLengthsTensor.
  inputs.push_back(nullptr);

  std::optional<TensorDesc> peephole_weight_tensor_desc;
  if (lstm.peephole_weight_operand_id.has_value()) {
    if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
      CHECK(context_properties.data_type_limits.lstm_bias.Supports(
          operands.at(*lstm.peephole_weight_operand_id.value())->descriptor));
    } else /* `LstmType` is `mojom::LstmCell` */ {
      CHECK(context_properties.data_type_limits.lstm_cell_bias.Supports(
          operands.at(*lstm.peephole_weight_operand_id.value())->descriptor));
    }
    const NodeOutput* peephole_weight = GetNodeOutputForOperand(
        id_to_node_output_map, lstm.peephole_weight_operand_id.value());
    // Append an identity node if the peephole weight is a constant operand
    // since PeepholeTensor doesn't support the DML_TENSOR_FLAG_OWNED_BY_DML
    // flag.
    peephole_weight =
        AppendIdentityToConstantOperand(graph_builder, peephole_weight);

    inputs.push_back(peephole_weight);
    peephole_weight_tensor_desc = peephole_weight->GetTensorDesc();
    // The peephole weight tensor is 1-D for lstmCell and 2-D for lstm, while
    // DirectML expects a 4-D tensor.
    peephole_weight_tensor_desc->EnsureMinimumRank(
        /*rank=*/4, TensorDesc::Alignment::kTrailing);
  }

  // When the recurrent network is bidirectional, dual activations must be
  // provided for the forward and backward directions.
  const size_t number_of_activations =
      direction == mojom::RecurrentNetworkDirection::kBoth
          ? lstm.activations.size() * 2
          : lstm.activations.size();

  base::FixedArray<ActivationOperatorDesc> activation_operator_descs(
      number_of_activations);
  for (size_t i = 0; i < lstm.activations.size(); ++i) {
    activation_operator_descs[i] =
        CreateOperatorDescForActivation(lstm.activations[i]);

    // For bidirectional, activations must be provided f() and g() for forward
    // followed by f() and g() for backwards.
    if (direction == mojom::RecurrentNetworkDirection::kBoth) {
      activation_operator_descs[lstm.activations.size() + i] =
          activation_operator_descs[i];
    }
  }

  base::FixedArray<DML_OPERATOR_DESC> activation_dml_descs(
      activation_operator_descs.size());
  std::ranges::transform(
      activation_operator_descs, activation_dml_descs.begin(),
      [](const auto& activation_operator_desc) {
        return activation_operator_desc.GetActivationDmlDesc();
      });

  DML_LSTM_OPERATOR_DESC lstm_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .WeightTensor = &weight_tensor_desc.GetDMLTensorDesc(),
      .RecurrenceTensor = &recurrent_weight_tensor_desc.GetDMLTensorDesc(),
      .BiasTensor = GetOptionalDmlTensorDescPtr(concatenated_bias_tensor_desc),
      .HiddenInitTensor =
          GetOptionalDmlTensorDescPtr(initial_hidden_state_tensor_desc),
      .CellMemInitTensor =
          GetOptionalDmlTensorDescPtr(initial_cell_state_tensor_desc),
      // All sequences in the batch have the same length.
      .SequenceLengthsTensor = nullptr,
      .PeepholeTensor =
          GetOptionalDmlTensorDescPtr(peephole_weight_tensor_desc),
      .OutputSequenceTensor =
          GetOptionalDmlTensorDescPtr(output_sequence_tensor_desc),
      .OutputSingleTensor = &output_hidden_state_tensor_desc.GetDMLTensorDesc(),
      .OutputCellSingleTensor =
          &output_cell_state_tensor_desc.GetDMLTensorDesc(),
      .ActivationDescCount = static_cast<uint32_t>(activation_dml_descs.size()),
      .ActivationDescs = activation_dml_descs.data(),
      .Direction = MojoRecurrentNetworkDirectionToDml(direction),
      // The cell clip threshold for the input of activations is not used.
      .ClipThreshold = 0,
      // The clip threshold is not used.
      .UseClipThreshold = FALSE,
      // The input and forget gates are not coupled.
      .CoupleInputForget = FALSE};

  const GraphNode* lstm_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_LSTM, &lstm_desc, inputs, label);

  if (return_sequence) {
    const NodeOutput* output_sequence = graph_builder.CreateNodeOutput(
        lstm_node, output_sequence_tensor_desc.value(), 0);
    CHECK(id_to_node_output_map
              .try_emplace(output_sequence_id.value(), output_sequence)
              .second);
  }

  const NodeOutput* output_hidden_state = graph_builder.CreateNodeOutput(
      lstm_node, output_hidden_state_tensor_desc, 1);
  CHECK(id_to_node_output_map
            .try_emplace(output_hidden_state_id, output_hidden_state)
            .second);

  const NodeOutput* output_cell_state = graph_builder.CreateNodeOutput(
      lstm_node, output_cell_state_tensor_desc, 2);
  CHECK(
      id_to_node_output_map.try_emplace(output_cell_state_id, output_cell_state)
          .second);

  return base::ok();
}

// Using DML_GEMM_OPERATOR_DESC to implement WebNN matmul.
base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForMatmul(
    const ContextProperties& context_properties,
    const std::vector<OperandPtr>& operands,
    const Operation* operation,
    const absl::flat_hash_map<const Operation*,
                              raw_ptr<const Operation, CtnExperimental>>&
        operation_to_fusible_standalone_activation_map,
    const absl::flat_hash_map<OperandId,
                              raw_ptr<const Operation, CtnExperimental>>&
        output_id_to_fusible_transpose_map,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& matmul = operation->get_matmul();

  // If the transpose operation that produces input a (or b) is fusible, use
  // the the input operand of that transpose operation instead and set the
  // `TransA` (or `TransB`) of DirectML GEMM operator to
  // `DML_MATRIX_TRANSFORM_TRANSPOSE`.
  bool transpose_a = false;
  OperandId a_operand_id = matmul->a_operand_id;
  std::optional<OperandId> fusible_transpose_input_id =
      GetFusibleTransposeInputId(output_id_to_fusible_transpose_map,
                                 a_operand_id);

  std::string label;
  if (fusible_transpose_input_id) {
    std::string_view transpose_a_label =
        output_id_to_fusible_transpose_map.at(a_operand_id)
            ->get_transpose()
            ->label;
    base::StrAppend(
        &label,
        {transpose_a_label.empty() ? "transpose_a" : transpose_a_label, "+"});

    a_operand_id = fusible_transpose_input_id.value();
    transpose_a = true;
  }
  const NodeOutput* input_a_node_output =
      GetNodeOutputForOperand(id_to_node_output_map, a_operand_id);
  auto input_a_tensor_desc = input_a_node_output->GetTensorDesc();
  CHECK(kDmlFloatDataTypes.contains(input_a_tensor_desc.GetDataType()));

  bool transpose_b = false;
  OperandId b_operand_id = matmul->b_operand_id;
  fusible_transpose_input_id = GetFusibleTransposeInputId(
      output_id_to_fusible_transpose_map, b_operand_id);
  if (fusible_transpose_input_id) {
    std::string_view transpose_b_label =
        output_id_to_fusible_transpose_map.at(b_operand_id)
            ->get_transpose()
            ->label;
    base::StrAppend(
        &label,
        {transpose_b_label.empty() ? "transpose_b" : transpose_b_label, "+"});

    b_operand_id = fusible_transpose_input_id.value();
    transpose_b = true;
  }
  const NodeOutput* input_b_node_output =
      GetNodeOutputForOperand(id_to_node_output_map, b_operand_id);
  auto input_b_tensor_desc = input_b_node_output->GetTensorDesc();

  OperandId output_id = matmul->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  const auto output_tensor_dims = output_tensor_desc.GetDimensions();
  // Because DML_GEMM_OPERATOR_DESC restricts input_a_tensor and
  // input_b_tensor, output_tensor must have the same DimensionCount and can't
  // support broadcasting, input_a_tensor and input_b_tensor may need to be
  // broadcasted.
  if (output_tensor_dims.size() > 2) {
    input_a_tensor_desc.BroadcastTo(output_tensor_dims, 2);
    input_b_tensor_desc.BroadcastTo(output_tensor_dims, 2);
  }

  CHECK(context_properties.data_type_limits.matmul_input.data_types.Has(
      DmlDataTypeToOperand(input_a_tensor_desc.GetDataType())));
  CHECK_EQ(input_a_tensor_desc.GetDimensions().size(),
           input_b_tensor_desc.GetDimensions().size());
  CHECK_EQ(input_a_tensor_desc.GetDimensions().size(),
           output_tensor_dims.size());

  if (!label.empty()) {
    base::StrAppend(&label, {matmul->label.empty() ? "matmul" : matmul->label});
  } else {
    label = matmul->label;
  }
  // Flatten adjacent dimensions for GEMM > 4D because DML_GEMM_OPERATOR_DESC
  // restricts tensor's rank <= 4.
  auto adjusted_output_tensor_desc = output_tensor_desc;
  if (output_tensor_dims.size() > 4) {
    // If flattening fails due to the non-default strides caused by
    // broadcasting, append an identity node after the input to consume the
    // non-default strides, ensuring successful flattening of the input.
    if (!input_a_tensor_desc.RightAlignedFlattenTo(4)) {
      input_a_node_output = AppendIdentityNode(
          graph_builder, input_a_node_output, &input_a_tensor_desc);
      input_a_tensor_desc = input_a_node_output->GetTensorDesc();

      CHECK(input_a_tensor_desc.RightAlignedFlattenTo(4));
    }

    if (!input_b_tensor_desc.RightAlignedFlattenTo(4)) {
      input_b_node_output = AppendIdentityNode(
          graph_builder, input_b_node_output, &input_b_tensor_desc);
      input_b_tensor_desc = input_b_node_output->GetTensorDesc();

      CHECK(input_b_tensor_desc.RightAlignedFlattenTo(4));
    }

    CHECK(adjusted_output_tensor_desc.RightAlignedFlattenTo(4));
  }

  // Use 4D GEMM which is available since feature level 1.0 for best
  // compatibility. There is no performance difference in the shader between
  // 2D/3D/4D, as 2D is just a variant of 4D with a batch/channel size of 1.
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gemm_operator_desc.
  // TODO(issues.chromium.org/327244277): Remove the workaround of coercing
  // GEMM's tensors to 4D.
  else if (output_tensor_dims.size() < 4) {
    input_a_tensor_desc.EnsureMinimumRank(4, TensorDesc::Alignment::kTrailing);
    input_b_tensor_desc.EnsureMinimumRank(4, TensorDesc::Alignment::kTrailing);
    adjusted_output_tensor_desc.EnsureMinimumRank(
        4, TensorDesc::Alignment::kTrailing);
  }

  std::optional<const Operation*> fusible_activation =
      GetFusibleActivationFromOperation(
          operation_to_fusible_standalone_activation_map, operation);
  std::optional<ActivationOperatorDesc> activation_operator_desc;
  std::optional<DML_OPERATOR_DESC> activation_dml_desc;
  if (fusible_activation) {
    activation_operator_desc =
        CreateOperatorDescForFusibleActivation(*fusible_activation.value());
    activation_dml_desc = activation_operator_desc->GetActivationDmlDesc();

    output_id =
        GetFusibleActivationOutputId(*fusible_activation.value()).value();
    label = GetFusedOperatorLabel(label, "matmul", *fusible_activation.value());
  }

  DML_GEMM_OPERATOR_DESC matmul_operator_desc{
      .ATensor = &input_a_tensor_desc.GetDMLTensorDesc(),
      .BTensor = &input_b_tensor_desc.GetDMLTensorDesc(),
      .CTensor = nullptr,
      .OutputTensor = &adjusted_output_tensor_desc.GetDMLTensorDesc(),
      .TransA = transpose_a ? DML_MATRIX_TRANSFORM_TRANSPOSE
                            : DML_MATRIX_TRANSFORM_NONE,
      .TransB = transpose_b ? DML_MATRIX_TRANSFORM_TRANSPOSE
                            : DML_MATRIX_TRANSFORM_NONE,
      .Alpha = 1.0f,
      .Beta = 0.0f,
      .FusedActivation =
          activation_dml_desc ? &activation_dml_desc.value() : nullptr,
  };

  std::array<const NodeOutput*, 2> inputs{input_a_node_output,
                                          input_b_node_output};
  const GraphNode* matmul_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GEMM, &matmul_operator_desc, inputs, label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      matmul_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

// Create a transpose node with the given permutation.
const NodeOutput* CreateTransposeNode(GraphBuilderDml& graph_builder,
                                      const NodeOutput* input,
                                      base::span<const uint32_t> permutation) {
  CHECK(input);
  const TensorDesc& input_tensor_desc = input->GetTensorDesc();
  TensorDesc transposed_input_tensor_desc = input_tensor_desc;
  transposed_input_tensor_desc.Transpose(permutation);

  // Append an identity node to consume the strides.
  const NodeOutput* transpose_node =
      AppendIdentityNode(graph_builder, input, &transposed_input_tensor_desc);

  return transpose_node;
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForSoftmax(
    Adapter* adapter,
    const std::vector<OperandPtr>& operands,
    const mojom::SoftmaxPtr& softmax,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, softmax->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  OperandId output_id = softmax->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  std::array<const NodeOutput*, 1> inputs = {input};
  const uint32_t axis = softmax->axis;
  const std::string& label = softmax->label;

  if (adapter->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_5_1)) {
    std::array<uint32_t, 1> axes = {axis};
    DML_ACTIVATION_SOFTMAX1_OPERATOR_DESC softmax1_operator_desc{
        .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
        .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
        .AxisCount = base::checked_cast<uint32_t>(axes.size()),
        .Axes = axes.data()};

    const GraphNode* softmax_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_ACTIVATION_SOFTMAX1, &softmax1_operator_desc, inputs,
        label);

    const NodeOutput* output = graph_builder.CreateNodeOutput(
        softmax_node, std::move(output_tensor_desc));

    // The output id must be unique in the map.
    CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
  } else {
    // Emulate softmax with N-D input and axis parameter supported when
    // feature level less than DML_FEATURE_LEVEL_5_1:
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_activation_softmax_operator_desc.
    //
    // Transpose the input tensor to make the axis to be the last dimension if
    // needed.
    const NodeOutput* axis_transposed_to_last_output = nullptr;
    const uint32_t input_rank = input_tensor_desc.GetDimensions().size();
    std::vector<uint32_t> permutation(input_rank);
    std::iota(permutation.begin(), permutation.end(), 0);
    if (axis == (input_rank - 1)) {
      axis_transposed_to_last_output = input;
    } else {
      std::vector<uint32_t> transpose_axis_to_last(permutation);
      std::swap(transpose_axis_to_last[axis],
                transpose_axis_to_last[input_rank - 1]);
      axis_transposed_to_last_output =
          CreateTransposeNode(graph_builder, input, transpose_axis_to_last);
    }

    // Reshape the input tensor to 2D if needed.
    const NodeOutput* reshaped_2d_output = nullptr;
    if (axis_transposed_to_last_output->GetTensorDesc()
            .GetDimensions()
            .size() <= 2) {
      reshaped_2d_output = axis_transposed_to_last_output;
    } else {
      const std::vector<uint32_t>& axis_transposed_to_last_output_dims =
          axis_transposed_to_last_output->GetTensorDesc().GetDimensions();
      auto reshaped_2d_dim_0 = base::MakeCheckedNum<uint32_t>(1);
      for (uint32_t i = 0; i < axis_transposed_to_last_output_dims.size() - 1;
           i++) {
        reshaped_2d_dim_0 *= axis_transposed_to_last_output_dims[i];
        if (!reshaped_2d_dim_0.IsValid<uint32_t>()) {
          return CreateUnexpectedError(
              mojom::Error::Code::kNotSupportedError,
              "For softmax impl: failed to reshape the input to 2-D tensor.",
              label);
        }
      }
      std::vector<uint32_t> reshaped_2d_dims = {
          reshaped_2d_dim_0.ValueOrDie(),
          axis_transposed_to_last_output_dims.back()};

      reshaped_2d_output = CreateReshapeNode(
          graph_builder, axis_transposed_to_last_output, reshaped_2d_dims);
    }

    // Perform 2-D softmax.
    const TensorDesc softmax_2d_output_tensor_desc =
        TensorDesc(reshaped_2d_output->GetTensorDesc().GetDataType(),
                   reshaped_2d_output->GetTensorDesc().GetDimensions());
    DML_ACTIVATION_SOFTMAX_OPERATOR_DESC softmax_2d_operator_desc{
        .InputTensor = &reshaped_2d_output->GetTensorDesc().GetDMLTensorDesc(),
        .OutputTensor = &softmax_2d_output_tensor_desc.GetDMLTensorDesc()};

    std::array<const NodeOutput*, 1> softmax_2d_inputs = {reshaped_2d_output};
    const GraphNode* softmax_2d_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_ACTIVATION_SOFTMAX, &softmax_2d_operator_desc,
        softmax_2d_inputs, label);

    const NodeOutput* softmax_2d_output = graph_builder.CreateNodeOutput(
        softmax_2d_node, softmax_2d_output_tensor_desc);

    // Reshape the 2-D tensor back to N-D.
    const NodeOutput* reshaped_nd_output = nullptr;
    if (axis_transposed_to_last_output->GetTensorDesc()
            .GetDimensions()
            .size() <= 2) {
      reshaped_nd_output = softmax_2d_output;
    } else {
      reshaped_nd_output = CreateReshapeNode(
          graph_builder, softmax_2d_output,
          axis_transposed_to_last_output->GetTensorDesc().GetDimensions());
    }

    // Transpose the output tensor back to the original shape.
    const NodeOutput* last_transposed_to_axis_output = nullptr;
    if (axis == (input_rank - 1)) {
      last_transposed_to_axis_output = reshaped_nd_output;
    } else {
      std::vector<uint32_t> transpose_axis_back(permutation);
      std::swap(transpose_axis_back[axis], transpose_axis_back[input_rank - 1]);
      last_transposed_to_axis_output = CreateTransposeNode(
          graph_builder, reshaped_nd_output, transpose_axis_back);
    }

    // The output id must be unique in the map.
    CHECK(id_to_node_output_map
              .try_emplace(output_id, last_transposed_to_axis_output)
              .second);
  }

  return base::ok();
}

void CreateOperatorNodeForSoftplus(const std::vector<OperandPtr>& operands,
                                   const mojom::SoftplusPtr& softplus,
                                   GraphBuilderDml& graph_builder,
                                   IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(id_to_node_output_map,
                                                    softplus->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  const OperandId output_id = softplus->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  DML_ACTIVATION_SOFTPLUS_OPERATOR_DESC softplus_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Steepness = 1.0};

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* softplus_node =
      graph_builder.CreateOperatorNode(DML_OPERATOR_ACTIVATION_SOFTPLUS,
                                       &softplus_desc, inputs, softplus->label);

  const NodeOutput* node_output = graph_builder.CreateNodeOutput(
      softplus_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
}

void CreateOperatorNodeForTile(const ContextProperties& context_properties,
                               const std::vector<OperandPtr>& operands,
                               const mojom::TilePtr& tile,
                               GraphBuilderDml& graph_builder,
                               IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, tile->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  CHECK(context_properties.data_type_limits.tile_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  const OperandId output_id = tile->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);

  base::span<const uint32_t> repetitions = tile->repetitions;
  DML_TILE_OPERATOR_DESC tile_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .RepeatsCount = base::checked_cast<uint32_t>(repetitions.size()),
      .Repeats = repetitions.data()};

  std::array<const NodeOutput*, 1> inputs = {input};
  const GraphNode* tile_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_TILE, &tile_desc, inputs, tile->label);

  const NodeOutput* node_output =
      graph_builder.CreateNodeOutput(tile_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
}

// Transpose is not a real DirectML operator. As for implementation, the input
// tensor is remapped for reading elements following the strides after the
// permutation, and an identity operator is appended to consume the remapped
// strides.
void CreateOperatorNodeForTranspose(const ContextProperties& context_properties,
                                    const std::vector<OperandPtr>& operands,
                                    const mojom::TransposePtr& transpose,
                                    GraphBuilderDml& graph_builder,
                                    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, transpose->input_operand_id);
  CHECK(context_properties.data_type_limits.transpose_input.data_types.Has(
      DmlDataTypeToOperand(input->GetTensorDesc().GetDataType())));

  const OperandPtr& operand = operands.at(transpose->input_operand_id.value());

  // When input is scalar, permutation should be empty. In this case pass
  // permutation=[0] to `CreateTransposeNode`.
  if (operand->descriptor.shape().empty()) {
    CHECK_EQ(input->GetTensorDesc().GetDimensions().size(), 1u);
    CHECK(transpose->permutation.empty());
  }

  OperandId output_id = transpose->output_operand_id;

  const NodeOutput* output = CreateTransposeNode(
      graph_builder, input,
      operand->descriptor.shape().empty() ? std::vector<uint32_t>{0}
                                          : transpose->permutation);

  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

// For DirectML feature levels before 5.1, we need to compose triangular
// from smaller operators: identity, slice, bitwise and.
//
//  1. expand the basic mask into an expanded mask big enough for the input
//  2. shear the expanded mask
//  3. slice the sheared mask
//  4. mask the input via bitwise and
//
// A simple constant mask is created with two values, one to
// fully preserve input values and one to fully zero them. Then, expand the
// mask from [1, 2, 1] to [mask_height, 2, mask_width]. Note the mask_width is
// calculated according to the input width and the diagonal. Next, shear the
// mask to achieve a diagonal shape by reshaping the dimensions from
// [mask_height, 2, mask_width] to [mask_height, 2 * mask_width] and set
// strides = {2 * mask_width - 1, 1}. By changing the default strides, the
// shape of the mask looks like a rhomboid. Then, we can get a mask with bit
// values filled with 0 or 0xFFFF using DML_SLICE_OPERATOR_DESC.
//                                              ----------------
// [ 0xFFFF, 0xFFFF, 0, 0     [0xFFFF, 0xFFFF, | 0,      0      |
//   0xFFFF, 0xFFFF, 0, 0  =>          0xFFFF, | 0xFFFF, 0,     | 0
//   0xFFFF, 0xFFFF, 0, 0]                     | 0xFFFF, 0xFFFF,| 0, 0]
//                                              -----------------
// Finally, the mask is a matrix shown above which
// has the same shape and the same data type with the input and consists of 0
// or 1 value in each bit. So the mask can be used to get either the upper or
// lower triangular part of the input tensor by doing bitwise and computation
// between the mask and the input. For example: [ 2, 3              [0, 0,]
// [0, 0,
//   4, 5,   bit_and   [0xFFFF, 0,]      =>    4, 0,
//   6, 7]             [0xFFFF, 0xFFFF]        6, 7]
base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForTriangular(
    const ContextProperties& context_properties,
    Adapter* adapter,
    const mojom::TriangularPtr& triangular,
    mojom::GraphInfoPtr& graph_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    GraphBuilderDml& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map,
    absl::flat_hash_map<OperandId, uint32_t>& constant_id_to_input_index_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, triangular->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  CHECK(context_properties.data_type_limits.triangular_input.data_types.Has(
      DmlDataTypeToOperand(input_tensor_desc.GetDataType())));

  const auto& operands = graph_info->operands;
  OperandId output_id = triangular->output_operand_id;
  auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  CHECK_EQ(input_tensor_desc.GetDimensions().size(),
           output_tensor_desc.GetDimensions().size());

  const auto& input_dimensions = input_tensor_desc.GetDimensions();
  const auto input_rank = input_dimensions.size();
  CHECK_GE(input_rank, 2U);
  bool upper = triangular->upper;
  int32_t diagonal = triangular->diagonal;
  const std::string& label = triangular->label;
  // Initialize scale union with a zero value.
  DML_SCALAR_UNION scalar_union = {};
  // DML_DIAGONAL_MATRIX1_OPERATOR_DESC was introduced in
  // DML_FEATURE_LEVEL_5_1 and supported input dimension count is from 2 to 4.
  if (adapter->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_5_1) &&
      input_rank <= 4) {
    // DML_DIAGONAL_MATRIX1_OPERATOR_DESC will generate an identity-like
    // matrix with zero between the given diagonal span, with other elements
    // being filled with the input values. The diagonal values may be shifted
    // anywhere between DiagonalFillBegin and DiagonalFillEnd, where a value
    // greater than zero shifts all values to the right, and less than zero
    // shifts them to the left.
    DML_DIAGONAL_MATRIX1_OPERATOR_DESC diagonal_matrix1_desc{
        .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
        .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
        .ValueDataType = output_tensor_desc.GetDataType(),
        .Value = scalar_union,
        .DiagonalFillBegin =
            upper ? std::numeric_limits<int32_t>::min() : diagonal + 1,
        .DiagonalFillEnd =
            upper ? diagonal : std::numeric_limits<int32_t>::max()};

    std::array<const NodeOutput*, 1> inputs = {input};
    const GraphNode* diagonal_matrix1_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_DIAGONAL_MATRIX1, &diagonal_matrix1_desc, inputs, label);

    const NodeOutput* node_output = graph_builder.CreateNodeOutput(
        diagonal_matrix1_node, std::move(output_tensor_desc));
    // The output id must be unique in the map.
    CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);
    return base::ok();
  }

  // For DirectML feature levels before 5.1, we need to compose triangular
  // from smaller operators: identity, slice, bitwise and.
  const OperandPtr& output_operand = operands.at(output_id.value());
  OperandDataType data_type = output_operand->descriptor.data_type();
  const uint32_t height = input_dimensions[input_rank - 2];
  const uint32_t width = input_dimensions[input_rank - 1];
  uint32_t longest_dimension_length = std::max(height, width);
  // Check the case where the diagonal shift value shifts all the values
  // too far above when keeping the top triangle or too far below when keeping
  // the bottom triangle, yielding all zeros.
  // 1. Upper = true
  // [ 1, 2, 3 \
  //   4, 5, 6, \
  //   7, 8, 9]  \
  // 2. Upper = false
  //  \ [ 1, 2, 3,
  //   \  4, 5, 6,
  //    \ 7, 8, 9]
  if ((diagonal > 0 &&
       (base::checked_cast<uint32_t>(diagonal) >= longest_dimension_length) &&
       upper) ||
      (diagonal < 0 &&
       (base::checked_cast<uint32_t>(-diagonal) >= longest_dimension_length) &&
       !upper)) {
    DML_FILL_VALUE_CONSTANT_OPERATOR_DESC fill_constant_operator_desc{
        .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
        .ValueDataType = output_tensor_desc.GetDataType(),
        .Value = scalar_union,
    };
    const GraphNode* fill_constant_node = graph_builder.CreateOperatorNode(
        DML_OPERATOR_FILL_VALUE_CONSTANT, &fill_constant_operator_desc, {},
        label);

    const NodeOutput* constant = graph_builder.CreateNodeOutput(
        fill_constant_node, std::move(output_tensor_desc), 0);
    auto constant_tensor_desc = constant->GetTensorDesc();

    std::array<const NodeOutput*, 2> inputs = {input, constant};
    const GraphNode* mul_node =
        CreateBinaryOperator<DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC>(
            input_tensor_desc, constant_tensor_desc, output_tensor_desc,
            graph_builder, DML_OPERATOR_ELEMENT_WISE_MULTIPLY, inputs, label);

    const NodeOutput* output =
        graph_builder.CreateNodeOutput(mul_node, output_tensor_desc);
    // The output id must be unique in the map.
    CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
    return base::ok();
  }
  // Check the case where the diagonal shift value shifts all the values
  // too far above when keeping the bottom triangle or too far below when
  // keeping the top triangle, returning the input tensor.
  // 1. Upper = false
  // [ 1, 2, 3 \
  //   4, 5, 6, \
  //   7, 8, 9]  \
  // 2. Upper = true
  //  \ [ 1, 2, 3,
  //   \  4, 5, 6,
  //    \ 7, 8, 9]
  if ((diagonal > 0 &&
       (base::checked_cast<uint32_t>(diagonal) >= longest_dimension_length) &&
       !upper) ||
      (diagonal < 0 &&
       (base::checked_cast<uint32_t>(-diagonal) >= longest_dimension_length) &&
       upper)) {
    // Return input matrix.
    const Node& input_node = input->GetNode();
    // The output_index of this NodeOutput should be the same as the input
    // NodeOutput for creating correct intermediate edges of the graph.
    const NodeOutput* output = graph_builder.CreateNodeOutput(
        &input_node, std::move(output_tensor_desc), input->GetOutputIndex());
    // The output id must be unique in the map.
    CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
    return base::ok();
  }

  // First step: create a simple constant mask with two values, one to
  // fully preserve input values and one to fully zero them.
  uint64_t lower_mask = 0;
  uint64_t upper_mask = std::numeric_limits<uint64_t>::max();
  if (!upper) {
    std::swap(lower_mask, upper_mask);
  }

  OperandDataType webnn_mask_data_type;
  DML_TENSOR_DATA_TYPE dml_mask_data_type;
  base::HeapArray<uint8_t> buffer;
  switch (data_type) {
    case OperandDataType::kInt8:
    case OperandDataType::kUint8: {
      webnn_mask_data_type = OperandDataType::kUint8;
      dml_mask_data_type = DML_TENSOR_DATA_TYPE_UINT8;
      std::array<uint8_t, 2> values = {static_cast<uint8_t>(lower_mask),
                                       static_cast<uint8_t>(upper_mask)};
      buffer = base::HeapArray<uint8_t>::CopiedFrom(base::as_byte_span(values));
      break;
    }
    case OperandDataType::kFloat16: {
      // Here we create a mask with float16 data type since WebNN doesn't
      // define uint16.
      webnn_mask_data_type = OperandDataType::kFloat16;
      dml_mask_data_type = DML_TENSOR_DATA_TYPE_UINT16;
      std::array<uint16_t, 2> values = {static_cast<uint16_t>(lower_mask),
                                        static_cast<uint16_t>(upper_mask)};
      buffer = base::HeapArray<uint8_t>::CopiedFrom(base::as_byte_span(values));
      break;
    }
    case OperandDataType::kFloat32:
    case OperandDataType::kInt32:
    case OperandDataType::kUint32: {
      webnn_mask_data_type = OperandDataType::kUint32;
      dml_mask_data_type = DML_TENSOR_DATA_TYPE_UINT32;
      std::array<uint32_t, 2> values = {static_cast<uint32_t>(lower_mask),
                                        static_cast<uint32_t>(upper_mask)};
      buffer = base::HeapArray<uint8_t>::CopiedFrom(base::as_byte_span(values));
      break;
    }
    case OperandDataType::kInt64:
    case OperandDataType::kUint64: {
      webnn_mask_data_type = OperandDataType::kUint64;
      dml_mask_data_type = DML_TENSOR_DATA_TYPE_UINT64;
      std::array<uint64_t, 2> values = {static_cast<uint64_t>(lower_mask),
                                        static_cast<uint64_t>(upper_mask)};
      buffer = base::HeapArray<uint8_t>::CopiedFrom(base::as_byte_span(values));
      break;
    }
    default:
      NOTREACHED() << "Unsupported data type.";
  }

  auto descriptor = *OperandDescriptor::Create(
      context_properties, webnn_mask_data_type,
      std::array<uint32_t, 3>{1, 2, 1}, "triangular");

  auto constant_operand = Operand::New(Operand::Kind::kConstant, descriptor,
                                       /*name=*/std::nullopt);

  OperandId constant_operand_id(graph_info->operands.size());
  graph_info->operands.push_back(std::move(constant_operand));
  CHECK(constant_operands
            .try_emplace(constant_operand_id,
                         std::make_unique<WebNNConstantOperand>(
                             descriptor, std::move(buffer)))
            .second);

  CreateConstantNode(adapter, constant_operand_id, constant_operands,
                     graph_builder, id_to_node_output_map,
                     constant_id_to_input_index_map);

  const NodeOutput* constant =
      GetNodeOutputForOperand(id_to_node_output_map, constant_operand_id);
  auto constant_tensor_desc = constant->GetTensorDesc();

  const auto mask_height = height;
  const auto checked_mask_width =
      (base::MakeCheckedNum<uint32_t>(longest_dimension_length) +
       std::min(base::checked_cast<uint32_t>(std::abs(diagonal)),
                longest_dimension_length)) *
      2;
  // TODO(issues.chromium.org/335524385): All error handlings of checked_math
  // values inside the implementation of triangular here should be removed and
  // performing proper validation at graph creation time.
  if (!checked_mask_width.IsValid<uint32_t>()) {
    return base::unexpected(CreateError(
        mojom::Error::Code::kUnknownError,
        "For triangular impl: the mask width is too large.", label));
  }
  const uint32_t mask_width = checked_mask_width.ValueOrDie();

  // Second step: expand the mask from [1, 2, 1] to [mask_height, 2,
  // mask_width].
  std::vector<uint32_t> expand_constant_dims = {mask_height, 2, mask_width};
  if (constant_tensor_desc.GetDimensions() != expand_constant_dims) {
    constant_tensor_desc.BroadcastTo(expand_constant_dims);
  }

  const auto expand_constant_tensor_desc = TensorDesc(
      constant_tensor_desc.GetDataType(), std::move(expand_constant_dims));
  const GraphNode* expand_constant_node =
      CreateUnaryOperator<DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC,
                          DML_OPERATOR_ELEMENT_WISE_IDENTITY>(
          constant_tensor_desc, expand_constant_tensor_desc, constant,
          graph_builder, label);

  const auto* expand_constant_output = graph_builder.CreateNodeOutput(
      expand_constant_node, std::move(expand_constant_tensor_desc));
  auto expand_constant_output_tensor_desc =
      expand_constant_output->GetTensorDesc();

  // Third step: shear the mask to achieve a diagonal shape by reshaping
  // the dimensions from [mask_height, 2, mask_width] to [mask_height,
  // 2 * mask_width] and set strides = {2 * mask_width - 1, 1}. By changing
  // the default strides, we can get the rhomboid to slice.
  // For example:
  // [ 1, 1, 0, 0     [1, 1, 0, 0
  //   1, 1, 0, 0  =>     1, 1, 0, 0
  //   1, 1, 0, 0]          1, 1, 0, 0]
  const auto checked_slice_input_width =
      base::MakeCheckedNum<uint32_t>(mask_width) * 2;
  if (!checked_slice_input_width.IsValid<uint32_t>()) {
    return base::unexpected(CreateError(
        mojom::Error::Code::kUnknownError,
        "For triangular impl: the input width for slice is too large.", label));
  }
  const uint32_t slice_input_width = checked_slice_input_width.ValueOrDie();
  std::vector<uint32_t> slice_input_dims = {mask_height, slice_input_width};

  const auto checked_slice_input_stride = checked_slice_input_width - 1;
  if (!checked_slice_input_stride.IsValid<uint32_t>()) {
    return base::unexpected(CreateError(
        mojom::Error::Code::kUnknownError,
        "For triangular impl: the input stride for slice is invalid.", label));
  }
  const uint32_t slice_input_stride = checked_slice_input_stride.ValueOrDie();
  std::vector<uint32_t> slice_input_strides = {slice_input_stride, 1};
  auto slice_input_tensor_desc =
      TensorDesc(expand_constant_output_tensor_desc.GetDataType(),
                 expand_constant_output_tensor_desc.GetFlags(),
                 std::move(slice_input_dims), std::move(slice_input_strides));
  // Since we change both the output dims and strides of
  // expand_constant_output to get the slice_input_tensor_desc, the
  // total_tensor_size_in_bytes of expand_constant_tensor_desc and
  // slice_input_tensor_desc are not the same.
  slice_input_tensor_desc.SetTotalTensorSizeInBytes(
      expand_constant_output_tensor_desc.GetTotalTensorSizeInBytes());
  std::vector<uint32_t> slice_output_dims = {height, width};
  auto slice_output_tensor_desc = TensorDesc(
      expand_constant_tensor_desc.GetDataType(), std::move(slice_output_dims));

  std::array<uint32_t, 2> sizes = {height, width};
  std::array<uint32_t, 2> offset =
      upper ? std::array<uint32_t, 2>{0, mask_width - diagonal}
            : std::array<uint32_t, 2>{0, mask_width - diagonal - 1};
  std::array<uint32_t, 2> strides = {1, 1};
  // Fourth step: get the sliced mask with bit values filled with 0 or
  // 0xFFFF...
  DML_SLICE_OPERATOR_DESC slice_operator_desc{
      .InputTensor = &slice_input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &slice_output_tensor_desc.GetDMLTensorDesc(),
      .DimensionCount = 2,
      .Offsets = offset.data(),
      .Sizes = sizes.data(),
      .Strides = strides.data(),
  };
  std::array<const NodeOutput*, 1> input_for_slice = {expand_constant_output};
  const GraphNode* slice_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_SLICE, &slice_operator_desc, input_for_slice, label);

  const auto* slice_output = graph_builder.CreateNodeOutput(
      slice_node, std::move(slice_output_tensor_desc));
  slice_output_tensor_desc = slice_output->GetTensorDesc();

  if (slice_output_tensor_desc.GetDimensions() != input_dimensions) {
    slice_output_tensor_desc.BroadcastTo(input_dimensions);
  }

  // Fifth step: using bit_and_operator to do the bit computation between
  // input and mask.
  // Here we need to cast the input and mask tensor data type to the data type
  // that DML elementwise-bit-and operator supports and has the same bit
  // width. For example casting float16 to uint16, float32 to uint32.
  TensorDesc bit_and_operator_input_tensor_desc =
      TensorDesc(dml_mask_data_type, input_tensor_desc.GetFlags(),
                 input_tensor_desc.GetDimensions());
  TensorDesc bit_and_operator_mask_tensor_desc =
      TensorDesc(dml_mask_data_type, slice_output_tensor_desc.GetFlags(),
                 slice_output_tensor_desc.GetDimensions(),
                 slice_output_tensor_desc.GetStrides());
  TensorDesc bit_and_operator_output_tensor_desc =
      TensorDesc(dml_mask_data_type, output_tensor_desc.GetFlags(),
                 output_tensor_desc.GetDimensions());
  DML_ELEMENT_WISE_BIT_AND_OPERATOR_DESC bit_and_operator_desc{
      .ATensor = &bit_and_operator_input_tensor_desc.GetDMLTensorDesc(),
      .BTensor = &bit_and_operator_mask_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &bit_and_operator_output_tensor_desc.GetDMLTensorDesc()};

  std::array<const NodeOutput*, 2> inputs{input, slice_output};
  const GraphNode* bit_and_operator_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_BIT_AND, &bit_and_operator_desc, inputs, label);

  const NodeOutput* bit_and_operator_output =
      graph_builder.CreateNodeOutput(bit_and_operator_node, output_tensor_desc);

  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, bit_and_operator_output)
            .second);
  return base::ok();
}

void CreateOperatorNodeForWhere(const std::vector<OperandPtr>& operands,
                                const mojom::WherePtr& where,
                                GraphBuilderDml& graph_builder,
                                IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* condition = GetNodeOutputForOperand(
      id_to_node_output_map, where->condition_operand_id);
  auto condition_tensor_desc = condition->GetTensorDesc();

  const NodeOutput* true_value = GetNodeOutputForOperand(
      id_to_node_output_map, where->true_value_operand_id);
  auto true_value_tensor_desc = true_value->GetTensorDesc();

  const NodeOutput* false_value = GetNodeOutputForOperand(
      id_to_node_output_map, where->false_value_operand_id);
  auto false_value_tensor_desc = false_value->GetTensorDesc();

  OperandId output_id = where->output_operand_id;
  const auto output_tensor_desc = CreateOutputTensorDesc(operands, output_id);
  const auto output_tensor_dims = output_tensor_desc.GetDimensions();

  // Broadcast each of the inputs to the output.
  if (condition_tensor_desc.GetDimensions() != output_tensor_dims) {
    condition_tensor_desc.BroadcastTo(output_tensor_dims);
  }
  if (true_value_tensor_desc.GetDimensions() != output_tensor_dims) {
    true_value_tensor_desc.BroadcastTo(output_tensor_dims);
  }
  if (false_value_tensor_desc.GetDimensions() != output_tensor_dims) {
    false_value_tensor_desc.BroadcastTo(output_tensor_dims);
  }

  DML_ELEMENT_WISE_IF_OPERATOR_DESC where_operator_desc{
      .ConditionTensor = &condition_tensor_desc.GetDMLTensorDesc(),
      .ATensor = &true_value_tensor_desc.GetDMLTensorDesc(),
      .BTensor = &false_value_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};

  std::array<const NodeOutput*, 3> inputs{condition, true_value, false_value};
  const GraphNode* where_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_IF, &where_operator_desc, inputs, where->label);

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      where_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

// If graph creation fails, report the error message via `callback` and let
// `context` handle the error.
void HandleGraphCreationFailure(
    const std::string& error_message,
    WebNNContextImpl::CreateGraphImplCallback callback,
    ContextImplDml* context,
    HRESULT hr) {
  std::move(callback).Run(base::unexpected(
      CreateError(mojom::Error::Code::kUnknownError, error_message)));
  context->HandleContextLostOrCrash(error_message, hr);
}

bool IsDispatchBindingValid(
    const base::flat_map<std::string, WebNNTensorImpl*>& named_tensors,
    const base::flat_map<std::string, base::WeakPtr<const WebNNTensorImpl>>&
        prev_named_tensors) {
  return std::ranges::equal(
      named_tensors, prev_named_tensors,
      [](const auto& pair, const auto& previous_pair) {
        const auto& [name, tensor] = pair;
        const auto& [prev_name, prev_tensor] = previous_pair;
        return name == prev_name && tensor == prev_tensor.get();
      });
}

}  // namespace

GraphImplDml::GraphBufferBindingInfo::GraphBufferBindingInfo() = default;
GraphImplDml::GraphBufferBindingInfo::~GraphBufferBindingInfo() = default;

GraphImplDml::GraphBufferBindingInfo::GraphBufferBindingInfo(
    const GraphBufferBindingInfo&) = default;
GraphImplDml::GraphBufferBindingInfo&
GraphImplDml::GraphBufferBindingInfo::operator=(const GraphBufferBindingInfo&) =
    default;

GraphImplDml::GraphBufferBindingInfo::GraphBufferBindingInfo(
    GraphBufferBindingInfo&&) = default;
GraphImplDml::GraphBufferBindingInfo&
GraphImplDml::GraphBufferBindingInfo::operator=(GraphBufferBindingInfo&&) =
    default;

// static
scoped_refptr<GraphImplDml::PersistentResource>
GraphImplDml::PersistentResource::Create(
    uint64_t persistent_buffer_byte_length,
    ComPtr<ID3D12Resource> persistent_buffer) {
  CHECK_GT(persistent_buffer_byte_length, 0u);
  CHECK_NE(persistent_buffer.Get(), nullptr);
  return base::WrapRefCounted(new PersistentResource(
      persistent_buffer_byte_length, std::move(persistent_buffer)));
}

GraphImplDml::PersistentResource::PersistentResource(
    uint64_t persistent_buffer_byte_length,
    ComPtr<ID3D12Resource> persistent_buffer)
    : persistent_buffer_(std::move(persistent_buffer)) {
  persistent_buffer_binding_ =
      DML_BUFFER_BINDING{.Buffer = persistent_buffer_.Get(),
                         .Offset = 0,
                         .SizeInBytes = persistent_buffer_byte_length};
  persistent_buffer_binding_desc_ = DML_BINDING_DESC{
      .Type = DML_BINDING_TYPE_BUFFER, .Desc = &persistent_buffer_binding_};
}

GraphImplDml::PersistentResource::~PersistentResource() = default;

GraphImplDml::GraphResources::GraphResources(
    ComPtr<ID3D12DescriptorHeap> descriptor_heap,
    uint64_t temporary_buffer_byte_length,
    ComPtr<ID3D12Resource> temporary_resource)
    : descriptor_heap(std::move(descriptor_heap)),
      temporary_buffer(std::move(temporary_resource)) {
  if (temporary_buffer_byte_length > 0) {
    CHECK_NE(temporary_buffer.Get(), nullptr);
    temporary_buffer_binding =
        DML_BUFFER_BINDING{.Buffer = temporary_buffer.Get(),
                           .Offset = 0,
                           .SizeInBytes = temporary_buffer_byte_length};
    temporary_buffer_binding_desc =
        DML_BINDING_DESC{.Type = DML_BINDING_TYPE_BUFFER,
                         .Desc = &temporary_buffer_binding.value()};
  }
}

GraphImplDml::GraphResources::~GraphResources() = default;

// static
base::expected<std::unique_ptr<GraphImplDml::GraphResources>, HRESULT>
GraphImplDml::AllocateGraphResources(Adapter* adapter,
                                     IDMLCompiledOperator* compiled_operator) {
  TRACE_EVENT0("gpu", "GraphImplDml::AllocateGraphResources");
  // Create the descriptor heap.
  DML_BINDING_PROPERTIES execution_binding_properties =
      compiled_operator->GetBindingProperties();
  ComPtr<ID3D12DescriptorHeap> descriptor_heap;
  RETURN_UNEXPECTED_IF_FAILED(CreateDescriptorHeap(
      adapter->d3d12_device(),
      execution_binding_properties.RequiredDescriptorCount,
      L"WebNN_Descriptor_Heap_For_Execution", descriptor_heap));

  // Create and bind the temporary resource if the operator execution requires.
  ComPtr<ID3D12Resource> temporary_buffer;
  uint64_t temporary_buffer_byte_length =
      execution_binding_properties.TemporaryResourceSize;
  if (temporary_buffer_byte_length > 0) {
    RETURN_UNEXPECTED_IF_FAILED(CreateDefaultBuffer(
        adapter->d3d12_device(), temporary_buffer_byte_length,
        L"WebNN_Temporary_Buffer_For_Execution", temporary_buffer));
  }

  return base::WrapUnique(new GraphResources(std::move(descriptor_heap),
                                             temporary_buffer_byte_length,
                                             std::move(temporary_buffer)));
}

GraphImplDml::GraphImplDml(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    scoped_refptr<Adapter> adapter,
    ContextImplDml* context,
    std::unique_ptr<CommandRecorder> command_recorder,
    scoped_refptr<PersistentResource> persistent_resource,
    ComPtr<IDMLCompiledOperator> compiled_operator,
    ComputeResourceInfo compute_resource_info,
    GraphBufferBindingInfo graph_buffer_binding_info,
    std::unique_ptr<GraphResources> graph_resources,
    std::vector<mojom::Device> devices)
    : WebNNGraphImpl(std::move(receiver),
                     context,
                     std::move(compute_resource_info),
                     std::move(devices)),
      persistent_resource_(std::move(persistent_resource)),
      adapter_(std::move(adapter)),
      context_(context),
      command_recorder_(std::move(command_recorder)),
      compiled_operator_(std::move(compiled_operator)),
      graph_buffer_binding_info_(std::move(graph_buffer_binding_info)),
      graph_resources_(std::move(graph_resources)) {}

//  Notice that it's the CommandQueue's responsibility to wait for all of the
//  queued work to complete before destructing itself.
GraphImplDml::~GraphImplDml() = default;

base::expected<ComPtr<IDMLCompiledOperator>, HRESULT>
GraphImplDml::CompileOnBackgroundThread(GraphBuilderDml graph_builder,
                                        DML_EXECUTION_FLAGS flags) {
  TRACE_EVENT0("gpu", "dml::GraphImplDml::CompileOnBackgroundThread");
  return graph_builder.Compile(flags);
}

// static
HRESULT GraphImplDml::ExecuteAndWaitSyncOnBackgroundThread(
    std::unique_ptr<CommandRecorder> init_command_recorder_for_npu) {
  TRACE_EVENT0("gpu",
               "dml::GraphImplDml::ExecuteAndWaitSyncOnBackgroundThread");
  RETURN_IF_FAILED(init_command_recorder_for_npu->Execute());
  RETURN_IF_FAILED(init_command_recorder_for_npu->command_queue()->WaitSync());
  return S_OK;
}

// static
void GraphImplDml::OnCompilationComplete(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    scoped_refptr<Adapter> adapter,
    base::WeakPtr<ContextImplDml> context,
    WebNNContextImpl::CreateGraphImplCallback callback,
    absl::flat_hash_map<OperandId, uint32_t> constant_id_to_input_index_map,
    GraphBufferBindingInfo graph_buffer_binding_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    base::expected<ComPtr<IDMLCompiledOperator>, HRESULT> compilation_result) {
  TRACE_EVENT0("gpu", "dml::GraphImplDml::OnCompilationComplete");

  if (!context) {
    std::move(callback).Run(base::unexpected(CreateError(
        mojom::Error::Code::kUnknownError,
        "Failed to create graph because the context was destroyed.")));
    return;
  }

  if (!compilation_result.has_value()) {
    // Handle the unsupported error on NPU gracefully since it's expected.
    if (adapter->IsNPU() &&
        compilation_result.error() == DXGI_ERROR_UNSUPPORTED) {
      LOG(ERROR)
          << "[WebNN] Failed to compile graph on NPU. Model is not supported.";
      std::move(callback).Run(base::unexpected(CreateError(
          mojom::Error::Code::kUnknownError,
          "Failed to compile graph on NPU. Model is not supported.")));
    } else {
      HandleGraphCreationFailure("Failed to compile the graph.",
                                 std::move(callback), context.get(),
                                 compilation_result.error());
    }
    return;
  }
  ComPtr<IDMLCompiledOperator> compiled_operator =
      std::move(compilation_result.value());

  CommandQueue* command_queue = adapter->IsNPU()
                                    ? adapter->init_command_queue_for_npu()
                                    : adapter->command_queue();
  ASSIGN_OR_RETURN(
      std::unique_ptr<CommandRecorder> initialization_command_recorder,
      CommandRecorder::Create(command_queue, adapter->dml_device()),
      &HandleGraphCreationFailure,
      "Failed to create command recorder for graph initialization.",
      std::move(callback), context.get());

  HRESULT hr = initialization_command_recorder->Open();
  if (FAILED(hr)) {
    HandleGraphCreationFailure("Failed to open the command recorder.",
                               std::move(callback), context.get(), hr);
    return;
  }

  // Create the input resource binding for graph initialization. The number of
  // bindings must exactly match the number of inputs (including constants) of
  // the graph, only the constant resource needs to be bound, the inputs for
  // computation supply nullptr for `Buffer` member to indicate 'no binding'.
  //
  // The constant tensor specifying DML_TENSOR_FLAG_OWNED_BY_DML need to bind
  // the resource in the buffer binding (DML_BUFFER_BINDING) array, the index
  // of constant in the array is DML_INPUT_GRAPH_EDGE_DESC.GraphInputIndex which
  // is got from `constant_id_to_input_index_map`.
  //
  // The inputs tensors without the DML_TENSOR_FLAG_OWNED_BY_DML flag is
  // expected to be bound during execution, and not during initialization.
  std::vector<DML_BUFFER_BINDING> input_buffer_binding(
      graph_buffer_binding_info.input_buffer_binding_count,
      DML_BUFFER_BINDING{.Buffer = nullptr, .Offset = 0, .SizeInBytes = 0});
  if (!constant_operands.empty()) {
    std::optional<AlignedByteLength<OperandId>>
        aligned_byte_length_of_constants =
            CalculateAlignedByteLength(constant_operands);
    if (!aligned_byte_length_of_constants) {
      std::move(callback).Run(base::unexpected(CreateError(
          mojom::Error::Code::kUnknownError,
          "Failed to calculate the aligned byte length of constants.")));
      return;
    }

    size_t total_byte_length_of_constants =
        aligned_byte_length_of_constants.value().total_byte_length;
    std::variant<UploadAndDefaultBuffers, ComPtr<ID3D12Resource>>
        buffer_variant;
    if (adapter->IsUMA()) {
      // For GPU supports UMA, create the custom heap with CPU memory pool, and
      // create a resource to map the heap. CPU writes constants into this
      // resource which will be bound as graph input for GPU reading during
      // initialization.
      ComPtr<ID3D12Resource> cpu_buffer;
      hr = CreateCustomUploadBuffer(
          adapter->d3d12_device(), total_byte_length_of_constants,
          L"WebNN_Custom_Upload_Buffer_Constants", cpu_buffer);
      if (FAILED(hr)) {
        HandleGraphCreationFailure(
            "Failed to create custom upload buffer for constants.",
            std::move(callback), context.get(), hr);
        return;
      }
      buffer_variant = std::move(cpu_buffer);
    } else {
      // Create the upload heap that can be written by CPU and read from GPU,
      // and create a resource to map the heap.
      ComPtr<ID3D12Resource> upload_buffer;
      hr = CreateUploadBuffer(adapter->d3d12_device(),
                              total_byte_length_of_constants,
                              L"WebNN_Upload_Buffer_Constants", upload_buffer);
      if (FAILED(hr)) {
        HandleGraphCreationFailure(
            "Failed to create upload buffer for constants.",
            std::move(callback), context.get(), hr);
        return;
      }
      // Create the default heap that only can be accessed by GPU not provide
      // CPU access, and create a resource to map the heap.
      ComPtr<ID3D12Resource> default_buffer;
      hr = CreateDefaultBuffer(
          adapter->d3d12_device(), total_byte_length_of_constants,
          L"WebNN_Default_Buffer_Constants", default_buffer);
      if (FAILED(hr)) {
        HandleGraphCreationFailure(
            "Failed to create default input buffer for constants.",
            std::move(callback), context.get(), hr);
        return;
      }
      buffer_variant =
          UploadAndDefaultBuffers{.upload_buffer = std::move(upload_buffer),
                                  .default_buffer = std::move(default_buffer)};
    }

    ASSIGN_OR_RETURN(
        (absl::flat_hash_map<OperandId, DML_BUFFER_BINDING>
             constant_buffer_binding),
        UploadAndCreateConstantBufferBinding(
            initialization_command_recorder.get(), constant_operands,
            aligned_byte_length_of_constants.value(),
            std::move(buffer_variant)),
        &HandleGraphCreationFailure, "Failed to upload constant weight data.",
        std::move(callback), context.get());

    // The constant tensor must be bound to the binding table during operator
    // initialization, and not during execution.
    for (auto& [constant_id, buffer_binding] : constant_buffer_binding) {
      // Get the graph input index with the constant id.
      const auto graph_input_index_iterator =
          constant_id_to_input_index_map.find(constant_id);
      CHECK(graph_input_index_iterator != constant_id_to_input_index_map.end());
      input_buffer_binding[graph_input_index_iterator->second] =
          std::move(buffer_binding);
    }
  }

  // The tensors used for constants must be bound during operator initialization
  // and not during execution.
  for (auto& [constant_id, constant_tensor] : constant_tensor_operands) {
    TensorImplDml* constant_tensor_impl =
        static_cast<TensorImplDml*>(constant_tensor);
    // Get the graph input index with the constant id.
    const auto graph_input_index_iterator =
        constant_id_to_input_index_map.find(constant_id);
    CHECK(graph_input_index_iterator != constant_id_to_input_index_map.end());
    input_buffer_binding[graph_input_index_iterator->second] =
        DML_BUFFER_BINDING{
            .Buffer = constant_tensor_impl->buffer(),
            .Offset = 0,
            .SizeInBytes = constant_tensor_impl->PackedByteLength()};
  }

  DML_BUFFER_ARRAY_BINDING input_buffer_array_binding{
      .BindingCount = base::checked_cast<uint32_t>(input_buffer_binding.size()),
      .Bindings = input_buffer_binding.data()};
  DML_BINDING_DESC input_buffer_binding_desc = {DML_BINDING_TYPE_BUFFER_ARRAY,
                                                &input_buffer_array_binding};

  // Create the persistent resource which is bound as output of operator
  // initializer.
  scoped_refptr<PersistentResource> persistent_resource;
  std::optional<DML_BINDING_DESC> persistent_buffer_binding_desc;
  DML_BINDING_PROPERTIES execution_binding_properties =
      compiled_operator->GetBindingProperties();
  uint64_t persistent_buffer_size =
      execution_binding_properties.PersistentResourceSize;
  if (persistent_buffer_size) {
    ComPtr<ID3D12Resource> persistent_buffer;
    hr = CreateDefaultBuffer(adapter->d3d12_device(), persistent_buffer_size,
                             L"WebNN_Default_Persistent_Buffer",
                             persistent_buffer);
    if (FAILED(hr)) {
      HandleGraphCreationFailure(
          "Failed to create the default buffer for persistent resource.",
          std::move(callback), context.get(), hr);
      return;
    }

    persistent_resource = PersistentResource::Create(
        persistent_buffer_size, std::move(persistent_buffer));
    CHECK(persistent_resource);
    persistent_buffer_binding_desc =
        persistent_resource->persistent_buffer_binding_desc();
  }

  hr = initialization_command_recorder->InitializeOperator(
      compiled_operator.Get(), input_buffer_binding_desc,
      persistent_buffer_binding_desc);
  if (FAILED(hr)) {
    HandleGraphCreationFailure("Failed to initialize the operator.",
                               std::move(callback), context.get(), hr);
    return;
  }

  hr = initialization_command_recorder->Close();
  if (FAILED(hr)) {
    HandleGraphCreationFailure("Failed to close the command list.",
                               std::move(callback), context.get(), hr);
    return;
  }

  // TODO(crbug.com/344921705): Move other graph initialization tasks to the
  // background thread: records the graph initialization onto the command list,
  // binds all required resources and closes the command list.
  if (adapter->IsNPU()) {
    adapter->init_task_runner_for_npu()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&GraphImplDml::ExecuteAndWaitSyncOnBackgroundThread,
                       std::move(initialization_command_recorder)),
        base::BindOnce(
            &GraphImplDml::OnInitializationComplete, std::move(receiver),
            std::move(adapter), std::move(context),
            std::move(persistent_resource), std::move(compiled_operator),
            std::move(compute_resource_info),
            std::move(graph_buffer_binding_info), std::move(callback)));
    return;
  }

  hr = initialization_command_recorder->Execute();
  if (FAILED(hr)) {
    HandleGraphCreationFailure("Failed to execute the command list.",
                               std::move(callback), context.get(), hr);
    return;
  }

  // Since the initialization command recorder has given all of the resources
  // needed for graph initialization to the command queue to hold onto until
  // they're no longer needed, it won't need to be passed to
  // `OnInitializationComplete()`.
  initialization_command_recorder->command_queue()->WaitAsync(base::BindOnce(
      &GraphImplDml::OnInitializationComplete, std::move(receiver),
      std::move(adapter), std::move(context), std::move(persistent_resource),
      std::move(compiled_operator), std::move(compute_resource_info),
      std::move(graph_buffer_binding_info), std::move(callback)));
}

// static
void GraphImplDml::CreateWebNNGraphImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    scoped_refptr<Adapter> adapter,
    base::WeakPtr<ContextImplDml> context,
    scoped_refptr<PersistentResource> persistent_resource,
    ComPtr<IDMLCompiledOperator> compiled_operator,
    ComputeResourceInfo compute_resource_info,
    GraphBufferBindingInfo graph_buffer_binding_info,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  if (!context) {
    std::move(callback).Run(base::unexpected(CreateError(
        mojom::Error::Code::kUnknownError,
        "Failed to create graph because the context was destroyed.")));
    return;
  }

  // Create a new command recorder and pass it to `GraphImplDml` for
  // `dispatch()`.
  ASSIGN_OR_RETURN(
      std::unique_ptr<CommandRecorder> command_recorder_for_dispatch,
      CommandRecorder::Create(adapter->command_queue(), adapter->dml_device()),
      &HandleGraphCreationFailure,
      "Failed to create the command recorder for dispatch.",
      std::move(callback), context.get());

  ASSIGN_OR_RETURN(
      std::unique_ptr<GraphResources> graph_resources,
      AllocateGraphResources(adapter.get(), compiled_operator.Get()),
      &HandleGraphCreationFailure,
      "Failed to create the graph resource for dispatch.", std::move(callback),
      context.get());

  HRESULT hr = command_recorder_for_dispatch->Open();
  if (FAILED(hr)) {
    HandleGraphCreationFailure("Failed to open the command recorder.",
                               std::move(callback), context.get(), hr);
    return;
  }

  std::optional<DML_BINDING_DESC> persistent_buffer_binding_desc;
  if (persistent_resource) {
    persistent_buffer_binding_desc =
        persistent_resource->persistent_buffer_binding_desc();
  }

  // Execute the graph with temporary and persistent bindings, as inputs and
  // outputs will be late bound.
  hr = command_recorder_for_dispatch->ExecuteOperator(
      compiled_operator, graph_resources->descriptor_heap,
      persistent_buffer_binding_desc,
      graph_resources->temporary_buffer_binding_desc);
  if (FAILED(hr)) {
    HandleGraphCreationFailure(
        "Failed to record graph execution for late binding.",
        std::move(callback), context.get(), hr);
    return;
  }

  hr = command_recorder_for_dispatch->Close();
  if (FAILED(hr)) {
    HandleGraphCreationFailure("Failed to close the command recorder.",
                               std::move(callback), context.get(), hr);
    return;
  }

  // The receiver bound to GraphImplDml.
  std::move(callback).Run(base::WrapUnique(new GraphImplDml(
      std::move(receiver), std::move(adapter), context.get(),
      std::move(command_recorder_for_dispatch), std::move(persistent_resource),
      std::move(compiled_operator), std::move(compute_resource_info),
      std::move(graph_buffer_binding_info), std::move(graph_resources),
      {adapter->IsNPU() ? mojom::Device::kNpu : mojom::Device::kGpu})));
}

// static
void GraphImplDml::OnInitializationComplete(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    scoped_refptr<Adapter> adapter,
    base::WeakPtr<ContextImplDml> context,
    scoped_refptr<PersistentResource> persistent_resource,
    ComPtr<IDMLCompiledOperator> compiled_operator,
    ComputeResourceInfo compute_resource_info,
    GraphBufferBindingInfo graph_buffer_binding_info,
    WebNNContextImpl::CreateGraphImplCallback callback,
    HRESULT hr) {
  TRACE_EVENT0("gpu", "dml::GraphImplDml::OnInitializationComplete");

  if (!context) {
    std::move(callback).Run(base::unexpected(CreateError(
        mojom::Error::Code::kUnknownError,
        "Failed to create graph because the context was destroyed.")));
    return;
  }

  if (FAILED(hr)) {
    HandleGraphCreationFailure(
        "Failed to wait for the initialization to complete.",
        std::move(callback), context.get(), hr);
    return;
  }

  CreateWebNNGraphImpl(
      std::move(receiver), std::move(adapter), std::move(context),
      std::move(persistent_resource), std::move(compiled_operator),
      std::move(compute_resource_info), std::move(graph_buffer_binding_info),
      std::move(callback));
}

// static
base::expected<void, mojom::ErrorPtr> GraphImplDml::CreateAndBuildInternal(
    const ContextProperties& context_properties,
    scoped_refptr<Adapter> adapter,
    mojom::GraphInfoPtr& graph_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    const base::flat_map<OperandId, WebNNTensorImpl*>& constant_tensor_operands,
    GraphBuilderDml& graph_builder,
    absl::flat_hash_map<OperandId, uint32_t>& constant_id_to_input_index_map,
    GraphBufferBindingInfo& graph_buffer_binding_info) {
  IdToNodeOutputMap id_to_node_output_map;
  const auto& operands = graph_info->operands;
  // Add inputs.
  for (OperandId input_id : graph_info->input_operands) {
    uint32_t graph_input_index = CreateInputNode(
        operands, input_id, graph_builder, id_to_node_output_map);
    const OperandPtr& operand = operands.at(input_id.value());
    CHECK(operand);
    graph_buffer_binding_info
        .graph_input_name_to_index_map[operand->name.value()] =
        graph_input_index;
  }

  // Retrieve all the constant ids for iteration, since `CreateConstantNode`
  // might erase elements from `constant_operands` which could invalidate the
  // iterators.
  base::flat_set<OperandId> constant_ids;
  constant_ids.reserve(constant_operands.size());
  for (const auto& [constant_id, _] : constant_operands) {
    constant_ids.insert(constant_id);
  }

  // Add constants.
  for (OperandId constant_id : constant_ids) {
    CreateConstantNode(adapter.get(), constant_id, constant_operands,
                       graph_builder, id_to_node_output_map,
                       constant_id_to_input_index_map);
  }

  // Add constant tensors which are considered read-only inputs that must be
  // bound during graph initialization.
  for (const auto& [constant_id, tensor_impl] : constant_tensor_operands) {
    const Node* node = graph_builder.CreateInputNode();
    constant_id_to_input_index_map[constant_id] =
        node->AsInputNode()->GetGraphInputIndex();
    TensorDesc tensor_desc(GetTensorDataType(tensor_impl->data_type()),
                           DML_TENSOR_FLAG_OWNED_BY_DML, tensor_impl->shape());
    const NodeOutput* output =
        graph_builder.CreateNodeOutput(node, std::move(tensor_desc));
    CHECK(id_to_node_output_map.try_emplace(constant_id, output).second);
  }

  // Fuse the operations in `mojom::GraphInfo` wherever possible to optimize the
  // graph's compute performance.
  //
  // 1. Go through all operations from the last one to the first one, record the
  // output edges count from each operation.
  // 2. Find the fusible operations and record them in `GraphFusionInfo`. For
  // example, activations (such as relu/sigmoid) that can be fused into
  // preceding operations that can support activation fusion (such as
  // conv2d/batch_norm), or transposes that can be fused into following matmul
  // operation.
  // 3. Go through all operations again, create corresponding DirectML operators
  // and add them into the final DirectML graph. During the process, the
  // `GraphFusionInfo` will be passed to DirectML operator creation methods to
  // configure the operator fusion and re-wire the input/output edges. The fused
  // operations will be skipped and no DirectML operators will be created for
  // them.
  GraphFusionInfo graph_fusion_info = GetGraphFusionInfo(graph_info);
  // Add operations.
  for (auto& operation : graph_info->operations) {
    // Skip the operations which are fused into another operation.
    if (graph_fusion_info.fusible_operations_set.contains(operation.get())) {
      continue;
    }

    // For operators that deal with DML API, there is a chance that operator
    // creation will fail. Use `mojom::ErrorPtr` to hold the given error
    // message.
    base::expected<void, mojom::ErrorPtr> create_operator_result;
    switch (operation->which()) {
      case Operation::Tag::kArgMinMax: {
        CreateOperatorNodeForArgMinMax(operands, operation->get_arg_min_max(),
                                       graph_builder, id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kBatchNormalization: {
        CreateOperatorNodeForBatchNormalization(
            adapter.get(), context_properties, operation.get(),
            graph_fusion_info.operation_to_fusible_standalone_activation_map,
            graph_info, constant_operands, graph_builder, id_to_node_output_map,
            constant_id_to_input_index_map);
        break;
      }
      case Operation::Tag::kClamp: {
        CreateOperatorNodeForClamp(adapter.get(), context_properties, operands,
                                   operation->get_clamp(), graph_builder,
                                   id_to_node_output_map);
        break;
      }
      case Operation::Tag::kConcat: {
        CreateOperatorNodeForConcat(context_properties, operands,
                                    operation->get_concat(), graph_builder,
                                    id_to_node_output_map);
        break;
      }
      case Operation::Tag::kConv2d: {
        CreateOperatorNodeForConv2d(
            context_properties, operands, operation.get(),
            graph_fusion_info.operation_to_fusible_standalone_activation_map,
            graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kCumulativeSum: {
        CreateOperatorNodeForCumulativeSum(
            context_properties, operands, operation->get_cumulative_sum(),
            graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kDequantizeLinear: {
        if (adapter->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_6_3)) {
          create_operator_result =
              CreateOperatorNodeForDequantizeOrQuantizeLinear<
                  DML_DEQUANTIZE_OPERATOR_DESC>(
                  context_properties, operands,
                  operation->get_dequantize_linear(), graph_builder,
                  DML_OPERATOR_DEQUANTIZE, id_to_node_output_map);
        } else {
          create_operator_result =
              CreateOperatorNodeForDequantizeOrQuantizeLinear<
                  DML_ELEMENT_WISE_DEQUANTIZE_LINEAR_OPERATOR_DESC>(
                  context_properties, operands,
                  operation->get_dequantize_linear(), graph_builder,
                  DML_OPERATOR_ELEMENT_WISE_DEQUANTIZE_LINEAR,
                  id_to_node_output_map);
        }
        break;
      }
      case mojom::Operation::Tag::kElementWiseBinary: {
        CreateOperatorNodeForBinary(
            context_properties, operands, operation.get(),
            graph_fusion_info.operation_to_fusible_standalone_activation_map,
            graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kElu: {
        CreateOperatorNodeForElu(operands, operation->get_elu(), graph_builder,
                                 id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kElementWiseUnary: {
        CreateOperatorNodeForElementWiseUnary(
            context_properties, operands, operation->get_element_wise_unary(),
            graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kExpand: {
        CreateOperatorNodeForExpand(context_properties, operands,
                                    operation->get_expand(), graph_builder,
                                    id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kGather: {
        create_operator_result = CreateOperatorNodeForGather(
            context_properties, operands, operation->get_gather(),
            graph_builder, id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kGatherElements: {
        CreateOperatorNodeForGatherElements(
            context_properties, operands, operation->get_gather_elements(),
            graph_builder, id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kGatherNd: {
        CreateOperatorNodeForGatherND(context_properties, operands,
                                      operation->get_gather_nd(), graph_builder,
                                      id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kGelu: {
        CreateOperatorNodeForGelu(
            context_properties, adapter.get(), operation->get_gelu(),
            graph_info, constant_operands, graph_builder, id_to_node_output_map,
            constant_id_to_input_index_map);
        break;
      }
      case mojom::Operation::Tag::kGemm: {
        CreateOperatorNodeForGemm(
            context_properties, operands, operation.get(),
            graph_fusion_info.operation_to_fusible_standalone_activation_map,
            graph_builder, id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kGru: {
        create_operator_result = CreateOperatorNodeForGru<mojom::GruPtr>(
            adapter.get(), context_properties, operation->get_gru(), graph_info,
            constant_operands, graph_builder, id_to_node_output_map,
            constant_id_to_input_index_map);
        break;
      }
      case mojom::Operation::Tag::kGruCell: {
        create_operator_result = CreateOperatorNodeForGru<mojom::GruCellPtr>(
            adapter.get(), context_properties, operation->get_gru_cell(),
            graph_info, constant_operands, graph_builder, id_to_node_output_map,
            constant_id_to_input_index_map);
        break;
      }
      case mojom::Operation::Tag::kHardSigmoid: {
        CreateOperatorNodeForHardSigmoid(operands,
                                         operation->get_hard_sigmoid(),
                                         graph_builder, id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kHardSwish: {
        CreateOperatorNodeForHardSwish(adapter.get(), operands,
                                       operation->get_hard_swish(),
                                       graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kInstanceNormalization: {
        // The axes along which to calculate the Mean and Variance.
        CHECK_EQ(context_properties.input_operand_layout,
                 InputOperandLayout::kNchw);
        std::array<uint32_t, 2> mean_variance_axes = {2, 3};
        std::array<uint32_t, 1> scale_bias_broadcast_axes = {1};
        create_operator_result = CreateOperatorNodeForMeanVarianceNormalization(
            adapter.get(), context_properties,
            operation->get_instance_normalization(), operation.get(),
            graph_fusion_info.operation_to_fusible_standalone_activation_map,
            graph_info, constant_operands, graph_builder, id_to_node_output_map,
            constant_id_to_input_index_map, mean_variance_axes,
            scale_bias_broadcast_axes, Operation::Tag::kInstanceNormalization);
        break;
      }
      case Operation::Tag::kLayerNormalization: {
        const auto& layer_normalization = operation->get_layer_normalization();
        const auto axes = layer_normalization->axes;
        create_operator_result = CreateOperatorNodeForMeanVarianceNormalization(
            adapter.get(), context_properties, layer_normalization,
            operation.get(),
            graph_fusion_info.operation_to_fusible_standalone_activation_map,
            graph_info, constant_operands, graph_builder, id_to_node_output_map,
            constant_id_to_input_index_map, axes, axes,
            Operation::Tag::kLayerNormalization);
        break;
      }
      case Operation::Tag::kLeakyRelu: {
        CreateOperatorNodeForLeakyRelu(operands, operation->get_leaky_relu(),
                                       graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kLinear: {
        CreateOperatorNodeForLinear(context_properties, operands,
                                    operation->get_linear(), graph_builder,
                                    id_to_node_output_map);
        break;
      }
      case Operation::Tag::kLstm: {
        create_operator_result = CreateOperatorNodeForLstm<mojom::Lstm>(
            adapter.get(), context_properties, *operation->get_lstm(),
            graph_info, constant_operands, graph_builder, id_to_node_output_map,
            constant_id_to_input_index_map);
        break;
      }
      case Operation::Tag::kLstmCell: {
        create_operator_result = CreateOperatorNodeForLstm<mojom::LstmCell>(
            adapter.get(), context_properties, *operation->get_lstm_cell(),
            graph_info, constant_operands, graph_builder, id_to_node_output_map,
            constant_id_to_input_index_map);
        break;
      }
      case mojom::Operation::Tag::kMatmul: {
        create_operator_result = CreateOperatorNodeForMatmul(
            context_properties, operands, operation.get(),
            graph_fusion_info.operation_to_fusible_standalone_activation_map,
            graph_fusion_info.output_id_to_fusible_transpose_map, graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kPad: {
        CreateOperatorNodeForPad(context_properties, operands,
                                 operation->get_pad(), graph_builder,
                                 id_to_node_output_map);
        break;
      }
      case Operation::Tag::kPool2d: {
        create_operator_result = CreateOperatorNodeForPool2d(
            context_properties, operands, operation->get_pool2d(),
            graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kPrelu: {
        CreateOperatorNodeForPrelu(context_properties, operands,
                                   operation->get_prelu(), graph_builder,
                                   id_to_node_output_map);
        break;
      }
      case Operation::Tag::kQuantizeLinear: {
        if (adapter->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_6_3)) {
          create_operator_result =
              CreateOperatorNodeForDequantizeOrQuantizeLinear<
                  DML_QUANTIZE_OPERATOR_DESC>(
                  context_properties, operands,
                  operation->get_quantize_linear(), graph_builder,
                  DML_OPERATOR_QUANTIZE, id_to_node_output_map);
        } else {
          create_operator_result =
              CreateOperatorNodeForDequantizeOrQuantizeLinear<
                  DML_ELEMENT_WISE_QUANTIZE_LINEAR_OPERATOR_DESC>(
                  context_properties, operands,
                  operation->get_quantize_linear(), graph_builder,
                  DML_OPERATOR_ELEMENT_WISE_QUANTIZE_LINEAR,
                  id_to_node_output_map);
        }
        break;
      }
      case Operation::Tag::kReduce: {
        CreateOperatorNodeForReduce(context_properties, operands,
                                    operation->get_reduce(), graph_builder,
                                    id_to_node_output_map);
        break;
      }
      case Operation::Tag::kRelu: {
        CreateOperatorNodeForUnary<DML_ACTIVATION_RELU_OPERATOR_DESC,
                                   DML_OPERATOR_ACTIVATION_RELU>(
            operands, operation->get_relu(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kResample2d: {
        CreateOperatorNodeForResample2d(context_properties, operands,
                                        operation->get_resample2d(),
                                        graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kReshape: {
        CreateOperatorNodeForReshape(context_properties, operands,
                                     operation->get_reshape(), graph_builder,
                                     id_to_node_output_map);
        break;
      }
      case Operation::Tag::kReverse: {
        CreateOperatorNodeForReverse(context_properties, operands,
                                     *operation->get_reverse(), graph_builder,
                                     id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kScatterElements: {
        CreateOperatorNodeForScatterElements(
            context_properties, operands, operation->get_scatter_elements(),
            graph_builder, id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kScatterNd: {
        CreateOperatorNodeForScatterND(context_properties, operands,
                                       operation->get_scatter_nd(),
                                       graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kSigmoid: {
        CreateOperatorNodeForUnary<DML_ACTIVATION_SIGMOID_OPERATOR_DESC,
                                   DML_OPERATOR_ACTIVATION_SIGMOID>(
            operands, operation->get_sigmoid(), graph_builder,
            id_to_node_output_map);

        break;
      }
      case Operation::Tag::kSlice: {
        CreateOperatorNodeForSlice(operands, operation->get_slice(),
                                   graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kSoftmax: {
        create_operator_result = CreateOperatorNodeForSoftmax(
            adapter.get(), operands, operation->get_softmax(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kSoftplus: {
        CreateOperatorNodeForSoftplus(operands, operation->get_softplus(),
                                      graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kSoftsign: {
        CreateOperatorNodeForUnary<DML_ACTIVATION_SOFTSIGN_OPERATOR_DESC,
                                   DML_OPERATOR_ACTIVATION_SOFTSIGN>(
            operands, operation->get_softsign(), graph_builder,
            id_to_node_output_map);

        break;
      }
      case mojom::Operation::Tag::kSplit: {
        CreateOperatorNodeForSplit(operands, operation->get_split(),
                                   graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kTanh: {
        CreateOperatorNodeForUnary<DML_ACTIVATION_TANH_OPERATOR_DESC,
                                   DML_OPERATOR_ACTIVATION_TANH>(
            operands, operation->get_tanh(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kTile: {
        CreateOperatorNodeForTile(context_properties, operands,
                                  operation->get_tile(), graph_builder,
                                  id_to_node_output_map);
        break;
      }
      case Operation::Tag::kTranspose: {
        CreateOperatorNodeForTranspose(context_properties, operands,
                                       operation->get_transpose(),
                                       graph_builder, id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kTriangular: {
        create_operator_result = CreateOperatorNodeForTriangular(
            context_properties, adapter.get(), operation->get_triangular(),
            graph_info, constant_operands, graph_builder, id_to_node_output_map,
            constant_id_to_input_index_map);
        break;
      }
      case Operation::Tag::kWhere: {
        CreateOperatorNodeForWhere(operands, operation->get_where(),
                                   graph_builder, id_to_node_output_map);
        break;
      }
      default: {
        std::string error_message = NotSupportedOperatorError(*operation);
        create_operator_result = base::unexpected(CreateError(
            mojom::Error::Code::kNotSupportedError, std::move(error_message)));
      }
    }

    if (!create_operator_result.has_value()) {
      return create_operator_result;
    }
  }

  for (auto& output_id : graph_info->output_operands) {
    const auto output_iterator = id_to_node_output_map.find(output_id);
    CHECK(output_iterator != id_to_node_output_map.end());
    const NodeOutput* output = output_iterator->second;
    CHECK(output);
    // TODO: A DML graph's output tensor may have adjusted strides rather than
    // default strides which are calculated by its' dimensions. For example,
    // dimensions [1,2,3,4] should have default strides [24,12,4,1] according to
    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/dml-helper-functions#calculatestrides,
    // but the strides may be adjusted for supporting some ops such as
    // transpose. Append an identity operator to consume the adjusted strides to
    // ensure a correct output result.

    // Appending an identity operator DML_OPERATOR_ELEMENT_WISE_IDENTITY which
    // effectively copies input tensor to the output tensor to avoid directly
    // using graph input as output.
    if (output->GetNode().GetType() == Node::Type::kInput) {
      output = AppendIdentityNode(graph_builder, output);
    }

    std::string name = operands.at(output_id.value())->name.value();
    graph_buffer_binding_info.graph_output_name_to_index_map[std::move(name)] =
        graph_builder.CreateOutputEdge(output);
  }

  graph_buffer_binding_info.input_buffer_binding_count =
      constant_id_to_input_index_map.size() +
      graph_buffer_binding_info.graph_input_name_to_index_map.size();

  return base::ok();
}

// static
void GraphImplDml::CreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    scoped_refptr<Adapter> adapter,
    base::WeakPtr<ContextImplDml> context,
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    WebNNContextImpl::CreateGraphImplCallback callback,
    const bool disable_dml_meta_commands_for_gpu) {
  TRACE_EVENT0("gpu", "dml::GraphImplDml::CreateAndBuild");
  GraphBuilderDml graph_builder(adapter->dml_device());
  absl::flat_hash_map<OperandId, uint32_t> constant_id_to_input_index_map;
  GraphBufferBindingInfo graph_buffer_binding_info;
  base::expected<void, mojom::ErrorPtr> create_operator_result =
      GraphImplDml::CreateAndBuildInternal(
          context->properties(), adapter, graph_info, constant_operands,
          constant_tensor_operands, graph_builder,
          constant_id_to_input_index_map, graph_buffer_binding_info);

  // TODO(crbug.com/349649099): Handle context lost for operator creation
  // failures.
  if (!create_operator_result.has_value()) {
    std::move(callback).Run(
        base::unexpected(std::move(create_operator_result.error())));
    return;
  }

  // Use `DML_EXECUTION_FLAG_DESCRIPTORS_VOLATILE` to allow late bindings on the
  // compiled operator which you've already recorded onto the command list,
  // before you submit the command list for execution.
  DML_EXECUTION_FLAGS flags = DML_EXECUTION_FLAG_DESCRIPTORS_VOLATILE;
  // Only apply `DML_EXECUTION_FLAG_DISABLE_META_COMMANDS` for GPU, because NPU
  // currently requires metacommands to be enabled.
  if (disable_dml_meta_commands_for_gpu && !adapter->IsNPU()) {
    flags |= DML_EXECUTION_FLAG_DISABLE_META_COMMANDS;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GraphImplDml::CompileOnBackgroundThread,
                     std::move(graph_builder), flags),
      base::BindOnce(
          &GraphImplDml::OnCompilationComplete, std::move(receiver),
          std::move(adapter), std::move(context), std::move(callback),
          std::move(constant_id_to_input_index_map),
          std::move(graph_buffer_binding_info),
          std::move(compute_resource_info), std::move(constant_operands),
          std::move(constant_tensor_operands)));
}

void GraphImplDml::HandleDispatchFailure(std::string_view error_message,
                                         HRESULT hr) {
  command_recorder_.reset();

  // Clear out previous buffers recorded for dispatch() so we don't mistakenly
  // skip recording on failure.
  previous_input_tensors_.clear();
  previous_output_tensors_.clear();
  context_->HandleContextLostOrCrash(error_message, hr);
}

GraphImplDml::IoBindings::IoBindings(
    std::vector<DML_BUFFER_BINDING> buffer_bindings,
    base::FixedArray<DML_BINDING_DESC> buffer_binding_desc)
    : buffer_bindings(std::move(buffer_bindings)),
      buffer_binding_desc(std::move(buffer_binding_desc)) {}
GraphImplDml::IoBindings::~IoBindings() = default;

GraphImplDml::IoBindings GraphImplDml::CreateAndCacheInputBindings(
    const base::flat_map<std::string, WebNNTensorImpl*>& named_inputs) {
  TRACE_EVENT0("gpu", "dml::GraphImplDml::CreateAndCacheInputBindings");
  // Create the MLTensor input bindings needed for graph execution.
  std::vector<DML_BUFFER_BINDING> graph_input_buffer_bindings(
      graph_buffer_binding_info_.input_buffer_binding_count,
      DML_BUFFER_BINDING{.Buffer = nullptr, .Offset = 0, .SizeInBytes = 0});

  previous_input_tensors_.reserve(named_inputs.size());

  // The graph input tensors must be bound to the binding table during the
  // graph execution.
  base::FixedArray<DML_BINDING_DESC> input_buffer_binding_desc(
      graph_buffer_binding_info_.input_buffer_binding_count,
      DML_BINDING_DESC{.Type = DML_BINDING_TYPE_NONE, .Desc = nullptr});

  for (auto& [name, input_tensor] : named_inputs) {
    TensorImplDml* input_tensor_impl =
        static_cast<TensorImplDml*>(input_tensor);
    // Get the graph input index for the name.
    const size_t graph_input_index =
        graph_buffer_binding_info_.graph_input_name_to_index_map.at(
            std::string(name));
    graph_input_buffer_bindings[graph_input_index] = DML_BUFFER_BINDING{
        .Buffer = input_tensor_impl->buffer(),
        .Offset = 0,
        .SizeInBytes = input_tensor_impl->PackedByteLength()};
    input_buffer_binding_desc[graph_input_index] = {
        DML_BINDING_TYPE_BUFFER,
        &graph_input_buffer_bindings[graph_input_index]};
    previous_input_tensors_[std::string(name)] =
        input_tensor_impl->GetWeakPtr();
  }
  return IoBindings(std::move(graph_input_buffer_bindings),
                    std::move(input_buffer_binding_desc));
}

GraphImplDml::IoBindings GraphImplDml::CreateAndCacheOutputBindings(
    const base::flat_map<std::string, WebNNTensorImpl*>& named_outputs) {
  TRACE_EVENT0("gpu", "dml::GraphImplDml::CreateAndCacheOutputBindings");
  // TODO(crbug.com/40278771): consider pre-computing the output binding
  // count.
  const size_t output_buffer_binding_count =
      graph_buffer_binding_info_.graph_output_name_to_index_map.size();

  // Create the MLTensor output bindings needed for graph execution.
  std::vector<DML_BUFFER_BINDING> graph_output_buffer_bindings(
      output_buffer_binding_count,
      DML_BUFFER_BINDING{.Buffer = nullptr, .Offset = 0, .SizeInBytes = 0});

  // The graph output tensors must be bound to the binding table during the
  // graph execution.
  base::FixedArray<DML_BINDING_DESC> output_buffer_binding_desc(
      output_buffer_binding_count,
      DML_BINDING_DESC{.Type = DML_BINDING_TYPE_NONE, .Desc = nullptr});

  previous_output_tensors_.reserve(named_outputs.size());

  for (auto& [name, output_tensor] : named_outputs) {
    TensorImplDml* output_tensor_impl =
        static_cast<TensorImplDml*>(output_tensor);
    // Get the graph output index with the name.
    const size_t graph_output_index =
        graph_buffer_binding_info_.graph_output_name_to_index_map.at(
            std::string(name));
    graph_output_buffer_bindings[graph_output_index] = DML_BUFFER_BINDING{
        .Buffer = output_tensor_impl->buffer(),
        .Offset = 0,
        .SizeInBytes = output_tensor_impl->PackedByteLength()};
    output_buffer_binding_desc[graph_output_index] = {
        DML_BINDING_TYPE_BUFFER,
        &graph_output_buffer_bindings[graph_output_index]};
    previous_output_tensors_[std::string(name)] =
        output_tensor_impl->GetWeakPtr();
  }
  return IoBindings(std::move(graph_output_buffer_bindings),
                    std::move(output_buffer_binding_desc));
}

void GraphImplDml::DispatchImpl(
    base::flat_map<std::string, WebNNTensorImpl*> named_inputs,
    base::flat_map<std::string, WebNNTensorImpl*> named_outputs) {
  TRACE_EVENT0("gpu", "dml::GraphImplDml::DispatchImpl");

  // It indicates whether we need to record commands and bind resources again.
  // If either the `command_recorder_` or `graph_resources_` is not available
  // during the graph execution, it must be set to true.
  bool is_command_recording_needed = false;

  if (!command_recorder_) {
    ASSIGN_OR_RETURN(command_recorder_,
                     CommandRecorder::Create(adapter_->command_queue(),
                                             adapter_->dml_device()),
                     &GraphImplDml::HandleDispatchFailure, this,
                     "Failed to create the command recorder.");
    is_command_recording_needed = true;
  }

  // Use the existing graph resource if it is available, else allocate a new
  // one.
  std::unique_ptr<GraphResources> graph_resources = std::move(graph_resources_);
  if (!graph_resources) {
    base::expected<std::unique_ptr<GraphResources>, HRESULT> result =
        AllocateGraphResources(adapter_.get(), compiled_operator_.Get());
    if (!result.has_value()) {
      HandleDispatchFailure("Failed to allocate graph resources.",
                            std::move(result.error()));
      return;
    }
    graph_resources = std::move(result.value());
    is_command_recording_needed = true;
  }
  CHECK(graph_resources);

  HRESULT hr = S_OK;

  // Indicate whether we need to bind the inputs/outputs. If the inputs/outputs
  // were not bound in the previous `DispatchImpl()` or if the commands need to
  // be recorded again, we need to bind them.
  bool is_inputs_binding_needed = false;
  bool is_outputs_binding_needed = false;
  if (is_command_recording_needed) {
    hr = command_recorder_->Open();
    if (FAILED(hr)) {
      HandleDispatchFailure("Failed to open the command recorder.", hr);
      return;
    }

    std::optional<DML_BINDING_DESC> persistent_buffer_binding_desc;
    if (persistent_resource_) {
      persistent_buffer_binding_desc =
          persistent_resource_->persistent_buffer_binding_desc();
    }

    // Execute the graph with temporary and persistent bindings, as inputs and
    // outputs will be late bound.
    hr = command_recorder_->ExecuteOperator(
        compiled_operator_.Get(), graph_resources->descriptor_heap,
        persistent_buffer_binding_desc,
        graph_resources->temporary_buffer_binding_desc);
    if (FAILED(hr)) {
      HandleDispatchFailure("Failed to record execute operator.", hr);
      return;
    }

    hr = command_recorder_->Close();
    if (FAILED(hr)) {
      HandleDispatchFailure("Failed to close the command recorder.", hr);
      return;
    }

    is_inputs_binding_needed = true;
    is_outputs_binding_needed = true;
  } else {
    if (!IsDispatchBindingValid(named_inputs, previous_input_tensors_)) {
      is_inputs_binding_needed = true;
    }
    if (!IsDispatchBindingValid(named_outputs, previous_output_tensors_)) {
      is_outputs_binding_needed = true;
    }
  }

  if (is_inputs_binding_needed) {
    IoBindings input_bindings = CreateAndCacheInputBindings(named_inputs);
    hr = command_recorder_->BindInputs(input_bindings.buffer_binding_desc);
    if (FAILED(hr)) {
      HandleDispatchFailure("Failed to bind inputs.", hr);
      return;
    }
  }

  if (is_outputs_binding_needed) {
    IoBindings output_bindings = CreateAndCacheOutputBindings(named_outputs);
    hr = command_recorder_->BindOutputs(output_bindings.buffer_binding_desc);
    if (FAILED(hr)) {
      HandleDispatchFailure("Failed to bind outputs.", hr);
      return;
    }
  }

  // Submit the command list for execution.
  hr = command_recorder_->Execute();
  if (FAILED(hr)) {
    HandleDispatchFailure("Failed to execute the command recorder.", hr);
    return;
  }

  // Reference the named inputs and outputs resources for late binding into
  // `CommandQueue` to keep them alive until the graph has been executed on the
  // GPU, also set the last submission fence value of the resources to track the
  // GPU progress.
  CommandQueue* command_queue = command_recorder_->command_queue();
  uint64_t last_submitted_fence_value = command_queue->GetLastFenceValue();
  for (auto& [name, input_tensor] : named_inputs) {
    TensorImplDml* input_tensor_impl =
        static_cast<TensorImplDml*>(input_tensor);
    input_tensor_impl->SetLastSubmissionFenceValue(last_submitted_fence_value);
    command_queue->ReferenceUntilCompleted(input_tensor_impl->buffer());
  }
  for (auto& [name, output_tensor] : named_outputs) {
    TensorImplDml* output_tensor_impl =
        static_cast<TensorImplDml*>(output_tensor);
    output_tensor_impl->SetLastSubmissionFenceValue(last_submitted_fence_value);
    command_queue->ReferenceUntilCompleted(output_tensor_impl->buffer());
  }

  // Prepare for the next dispatch.
  command_queue->WaitAsync(base::BindOnce(&GraphImplDml::OnDispatchComplete,
                                          weak_factory_.GetWeakPtr(),
                                          std::move(graph_resources)));
}

void GraphImplDml::OnDispatchComplete(
    std::unique_ptr<GraphResources> graph_resources,
    HRESULT hr) {
  TRACE_EVENT0("gpu", "dml::GraphImplDml::OnDispatchComplete");
  if (FAILED(hr)) {
    HandleDispatchFailure("Failed to wait for the dispatch to complete.", hr);
    return;
  }

  // If there is an existing available graph resources, release the graph
  // resources. Otherwise, recycle the graph resources for the next call.
  if (!graph_resources_) {
    graph_resources_ = std::move(graph_resources);
  }
}
}  // namespace webnn::dml
