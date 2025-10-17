// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/mojom/webnn_device.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include <limits.h>

#include <array>
#include <numeric>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/features.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_hard_sigmoid_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_linear_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operator_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_recurrent_network_activation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_triangular_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/ml_graph_transformer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace blink_mojom = webnn::mojom::blink;

class FakeWebNNTensor;

namespace {

static constexpr webnn::SupportedRanks kMaxRank =
    webnn::SupportedRanks::UpTo(8);

// BuildResult is returned by Build() method. If the graph building is
// successful, `graph` points to the MLGraph and `error_name` and
// `error_message` are null. Otherwise, `graph` is a nullptr and
// `error_name` and `error_message` are populated from the JS error or
// DOMException.
struct BuildResult {
  Persistent<MLGraph> graph;
  String error_name;
  String error_message;
};

// Helper struct to create faked mojom result of inference.
struct ComputeResult {
  WTF::HashMap<WTF::String, WTF::Vector<uint8_t>> output;
};

template <typename T>
struct OperandInfo {
  V8MLOperandDataType::Enum data_type;
  Vector<uint32_t> dimensions;
  Vector<T> values;
};

webnn::OperandDescriptor ToDescriptor(webnn::OperandDataType data_type,
                                      base::span<const uint32_t> shape) {
  return webnn::OperandDescriptor::UnsafeCreateForTesting(data_type, shape);
}

template <typename T>
T* V8ToObject(V8TestingScope* scope, ScriptValue value) {
  return NativeValueTraits<T>::NativeValue(scope->GetIsolate(), value.V8Value(),
                                           scope->GetExceptionState());
}

String ExceptionCodeToString(ExceptionCode exception_code) {
  switch (static_cast<ESErrorType>(exception_code)) {
    case ESErrorType::kTypeError:
      return "TypeError";
    default:
      NOTREACHED();
  }
}

std::pair<String, String> GetErrorNameAndMessage(V8TestingScope* scope,
                                                 ScriptValue value) {
  v8::Local<v8::Object> object;
  if (!value.V8Value()
           ->ToObject(scope->GetScriptState()->GetContext())
           .ToLocal(&object)) {
    return {"undefined", "undefined"};
  }
  const auto& Get = [&scope, object](const String& key) -> String {
    v8::Local<v8::Value> prop_value;
    if (!object
             ->Get(scope->GetScriptState()->GetContext(),
                   V8AtomicString(scope->GetScriptState()->GetIsolate(), key))
             .ToLocal(&prop_value)) {
      return "undefined";
    }
    return ToCoreStringWithUndefinedOrNullCheck(
        scope->GetScriptState()->GetIsolate(), prop_value);
  };
  return {Get("name"), Get("message")};
}

// Helper function to set the data of an ArrayBufferView from a vector.
template <typename T>
void SetArrayBufferViewValues(MaybeShared<DOMArrayBufferView> array_buffer_view,
                              const Vector<T>& values) {
  DCHECK_EQ(array_buffer_view->byteLength(), values.size() * sizeof(T));
  memcpy(array_buffer_view->BaseAddress(), values.data(),
         values.size() * sizeof(T));
}

// Helper function to create an ArrayBufferView given an operand.
MaybeShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand) {
  return CreateDOMArrayBufferView(operand->NumberOfElements(),
                                  operand->dataType().AsEnum());
}

// Overrode helper function to create an ArrayBufferView given an operand and
// set its data from a vector.
template <typename T>
MaybeShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand,
    const Vector<T>& values) {
  auto array_buffer_view = CreateArrayBufferViewForOperand(operand);
  SetArrayBufferViewValues(array_buffer_view, values);
  return array_buffer_view;
}

// Helper function to get the data of an ArrayBufferView into a vector.
template <typename T>
Vector<T> GetArrayBufferViewValues(
    MaybeShared<DOMArrayBufferView> array_buffer_view) {
  Vector<T> values(base::checked_cast<wtf_size_t>(
      array_buffer_view->byteLength() / array_buffer_view->TypeSize()));
  memcpy(values.data(), array_buffer_view->BaseAddress(),
         array_buffer_view->byteLength());
  return values;
}

MLContext* CreateContext(V8TestingScope& scope, MLContextOptions* options) {
  auto* ml = MakeGarbageCollected<ML>(scope.GetExecutionContext());
  ScriptPromiseTester tester(scope.GetScriptState(),
                             ml->createContext(scope.GetScriptState(), options,
                                               scope.GetExceptionState()));
  tester.WaitUntilSettled();
  CHECK(tester.IsFulfilled());

  return NativeValueTraits<MLContext>::NativeValue(
      scope.GetIsolate(), tester.Value().V8Value(), scope.GetExceptionState());
}

MLOperand* BuildGemm(V8TestingScope& scope,
                     MLGraphBuilder* builder,
                     MLOperand* a,
                     MLOperand* b,
                     MLGemmOptions* options = MLGemmOptions::Create()) {
  auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), a->DataType());
  auto* gemm = output->Operator();
  EXPECT_THAT(gemm, testing::NotNull());
  EXPECT_EQ(gemm->Kind(), webnn::mojom::blink::Operation::Tag::kGemm);
  EXPECT_THAT(gemm->Options(), testing::NotNull());
  return output;
}

MLOperand* BuildElementWiseBinaryOperator(
    MLGraphBuilder* builder,
    V8TestingScope& scope,
    MLOperand* a,
    MLOperand* b,
    webnn::mojom::blink::ElementWiseBinary::Kind kind,
    MLOperatorOptions* options) {
  switch (kind) {
    case webnn::mojom::blink::ElementWiseBinary::Kind::kAdd:
      return builder->add(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kSub:
      return builder->sub(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMul:
      return builder->mul(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kDiv:
      return builder->div(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMin:
      return builder->min(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMax:
      return builder->max(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kPow:
      return builder->pow(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kEqual:
      return builder->equal(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreater:
      return builder->greater(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual:
      return builder->greaterOrEqual(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesser:
      return builder->lesser(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual:
      return builder->lesserOrEqual(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kNotEqual:
      return builder->notEqual(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalAnd:
      return builder->logicalAnd(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalOr:
      return builder->logicalOr(a, b, options, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalXor:
      return builder->logicalXor(a, b, options, scope.GetExceptionState());
  }
}

MLOperand* BuildElementWiseBinary(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    webnn::mojom::blink::ElementWiseBinary::Kind kind,
    MLOperand* a,
    MLOperand* b,
    MLOperatorOptions* options = MLOperatorOptions::Create()) {
  MLOperand* output =
      BuildElementWiseBinaryOperator(builder, scope, a, b, kind, options);
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);

  if (IsLogicalBinaryOperator(kind)) {
    EXPECT_EQ(output->dataType().AsEnum(), V8MLOperandDataType::Enum::kUint8);
  } else {
    EXPECT_EQ(output->DataType(), a->DataType());
  }

  auto* op = output->Operator();
  EXPECT_THAT(op, testing::NotNull());
  EXPECT_EQ(op->Kind(),
            webnn::mojom::blink::Operation::Tag::kElementWiseBinary);
  EXPECT_EQ(op->SubKind<webnn::mojom::blink::ElementWiseBinary::Kind>(), kind);
  return output;
}

}  // namespace

class MLGraphTest : public testing::Test {
 public:
  void SetGraphInfo(blink_mojom::GraphInfoPtr graph_info) {
    graph_info_ = std::move(graph_info);
  }

  blink_mojom::GraphInfoPtr GetGraphInfo() { return std::move(graph_info_); }

  void SetComputeResult(const ComputeResult& compute_result) {
    compute_result_ = std::move(compute_result);
  }

  const ComputeResult& GetComputeResult() const { return compute_result_; }

  void SetInputArrayBuffers(HashMap<String, mojo_base::BigBuffer> buffers) {
    input_array_buffers_ = std::move(buffers);
  }

  const HashMap<String, mojo_base::BigBuffer>& GetInputArrayBuffers() const {
    return input_array_buffers_;
  }

  BuildResult BuildGraph(V8TestingScope& scope,
                         MLGraphBuilder* builder,
                         MLNamedOperands& named_operands) {
    ScriptPromise<MLGraph> build_promise = builder->build(
        scope.GetScriptState(), named_operands, scope.GetExceptionState());
    // An empty promise will be returned if `build()` synchronously rejects.
    if (build_promise.IsEmpty()) {
      return BuildResult{
          .error_name = ExceptionCodeToString(scope.GetExceptionState().Code()),
          .error_message = scope.GetExceptionState().Message()};
    }

    ScriptPromiseTester tester(scope.GetScriptState(), build_promise);
    tester.WaitUntilSettled();
    if (tester.IsFulfilled()) {
      return BuildResult{.graph = V8ToObject<MLGraph>(&scope, tester.Value())};
    } else {
      auto [name, message] = GetErrorNameAndMessage(&scope, tester.Value());
      return BuildResult{.error_name = name, .error_message = message};
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      webnn::mojom::features::kWebMachineLearningNeuralNetwork};
  test::TaskEnvironment task_environment_;

  blink_mojom::GraphInfoPtr graph_info_;
  HashMap<String, mojo_base::BigBuffer> input_array_buffers_;
  ComputeResult compute_result_;
};

class WebNNContextHelper {
 public:
  WebNNContextHelper() = default;
  ~WebNNContextHelper() = default;

  void ConnectWebNNTensorImpl(const blink::WebNNTensorToken& handle,
                              std::unique_ptr<FakeWebNNTensor> tensor) {
    const auto it = tensor_impls_.find(handle);
    ASSERT_TRUE(it == tensor_impls_.end());
    tensor_impls_.try_emplace(handle, std::move(tensor));
  }

  void DisconnectAndDestroyWebNNTensorImpl(
      const blink::WebNNTensorToken& handle) {
    tensor_impls_.erase(handle);
  }

 private:
  std::map<blink::WebNNTensorToken, std::unique_ptr<FakeWebNNTensor>>
      tensor_impls_;

  mojo::UniqueAssociatedReceiverSet<blink_mojom::WebNNGraphBuilder> builders_;
};

class FakeWebNNGraph : public blink_mojom::WebNNGraph {
 public:
  explicit FakeWebNNGraph(MLGraphTest& helper) : helper_(helper) {}
  FakeWebNNGraph(const FakeWebNNGraph&) = delete;
  FakeWebNNGraph(FakeWebNNGraph&&) = delete;
  ~FakeWebNNGraph() override = default;

 private:
  // Just return for testing the validation of inputs and outputs.
  void Dispatch(
      const HashMap<WTF::String, blink::WebNNTensorToken>& named_inputs,
      const HashMap<WTF::String, blink::WebNNTensorToken>& named_outputs)
      override {}

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<MLGraphTest, DanglingUntriaged> helper_;
};

class FakeWebNNTensor : public blink_mojom::WebNNTensor {
 public:
  FakeWebNNTensor(
      WebNNContextHelper& helper,
      mojo::PendingAssociatedReceiver<blink_mojom::WebNNTensor> receiver,
      const blink::WebNNTensorToken& tensor_handle,
      blink_mojom::TensorInfoPtr tensor_info)
      : helper_(helper),
        receiver_(this, std::move(receiver)),
        handle_(tensor_handle) {
    buffer_ = mojo_base::BigBuffer(tensor_info->descriptor.PackedByteLength());
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &FakeWebNNTensor::OnConnectionError, WTF::Unretained(this)));
  }

  ~FakeWebNNTensor() override = default;

  FakeWebNNTensor(const FakeWebNNTensor&) = delete;
  FakeWebNNTensor(FakeWebNNTensor&&) = delete;

  const blink::WebNNTensorToken& handle() const { return handle_; }

 private:
  void ReadTensor(ReadTensorCallback callback) override {
    mojo_base::BigBuffer dst_buffer(buffer_.byte_span());

    std::move(callback).Run(
        blink_mojom::ReadTensorResult::NewBuffer(std::move(dst_buffer)));
  }

  void WriteTensor(mojo_base::BigBuffer src_buffer) override {
    ASSERT_LE(src_buffer.size(), buffer_.size());
    base::span(buffer_).copy_prefix_from(src_buffer);
  }

  void OnConnectionError() {
    helper_->DisconnectAndDestroyWebNNTensorImpl(handle());
  }

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<WebNNContextHelper, DanglingUntriaged> helper_;

  mojo::AssociatedReceiver<blink_mojom::WebNNTensor> receiver_;

  const blink::WebNNTensorToken handle_;

  mojo_base::BigBuffer buffer_;
};

class FakeWebNNGraphBuilder : public blink_mojom::WebNNGraphBuilder {
 public:
  explicit FakeWebNNGraphBuilder(MLGraphTest& helper) : helper_(helper) {}
  FakeWebNNGraphBuilder(const FakeWebNNGraphBuilder&) = delete;
  FakeWebNNGraphBuilder(FakeWebNNGraphBuilder&&) = delete;
  ~FakeWebNNGraphBuilder() override = default;

 private:
  // webnn::mojom::blink::WebNNGraphBuilder:
  void CreateGraph(blink_mojom::GraphInfoPtr graph_info,
                   CreateGraphCallback callback) override {
    helper_->SetGraphInfo(std::move(graph_info));

    mojo::PendingAssociatedRemote<blink_mojom::WebNNGraph> blink_remote;
    // The receiver bind to FakeWebNNGraph.
    mojo::MakeSelfOwnedAssociatedReceiver<blink_mojom::WebNNGraph>(
        std::make_unique<FakeWebNNGraph>(*helper_),
        blink_remote.InitWithNewEndpointAndPassReceiver());

    auto success = blink_mojom::CreateGraphSuccess::New(
        std::move(blink_remote), WTF::Vector<blink_mojom::Device>());
    std::move(callback).Run(std::move(success));
  }

  void CreatePendingConstant(const WebNNPendingConstantToken& constant_handle,
                             webnn::OperandDataType data_type,
                             mojo_base::BigBuffer data) override {
    NOTIMPLEMENTED();
  }

  void IsValidGraphForTesting(
      const webnn::ContextProperties& context_properties,
      webnn::mojom::blink::GraphInfoPtr graph_info,
      IsValidGraphForTestingCallback callback) override {
    NOTIMPLEMENTED();
  }

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<MLGraphTest, DanglingUntriaged> helper_;
};

class FakeWebNNContext : public blink_mojom::WebNNContext {
 public:
  explicit FakeWebNNContext(MLGraphTest& helper) : helper_(helper) {}
  FakeWebNNContext(const FakeWebNNContext&) = delete;
  FakeWebNNContext(FakeWebNNContext&&) = delete;
  ~FakeWebNNContext() override = default;

 private:
  // Override methods from webnn::mojom::WebNNContext.
  void CreateGraphBuilder(
      mojo::PendingAssociatedReceiver<blink_mojom::WebNNGraphBuilder> receiver)
      override {
    mojo::MakeSelfOwnedAssociatedReceiver<blink_mojom::WebNNGraphBuilder>(
        std::make_unique<FakeWebNNGraphBuilder>(*helper_), std::move(receiver));
  }

  void CreateTensor(blink_mojom::TensorInfoPtr tensor_info,
                    mojo_base::BigBuffer tensor_data,
                    CreateTensorCallback callback) override {
    mojo::PendingAssociatedRemote<blink_mojom::WebNNTensor> blink_remote;
    auto blink_receiver = blink_remote.InitWithNewEndpointAndPassReceiver();
    blink::WebNNTensorToken tensor_handle;
    context_helper_.ConnectWebNNTensorImpl(
        tensor_handle, std::make_unique<FakeWebNNTensor>(
                           context_helper_, std::move(blink_receiver),
                           tensor_handle, std::move(tensor_info)));

    auto success = blink_mojom::CreateTensorSuccess::New(
        std::move(blink_remote), std::move(tensor_handle));
    std::move(callback).Run(
        blink_mojom::CreateTensorResult::NewSuccess(std::move(success)));
  }

  // TODO(crbug.com/354741414): Fix this dangling pointer.
  const raw_ref<MLGraphTest, DanglingUntriaged> helper_;

  WebNNContextHelper context_helper_;
};

class FakeWebNNContextProvider : public blink_mojom::WebNNContextProvider {
 public:
  explicit FakeWebNNContextProvider(MLGraphTest& helper)
      : helper_(helper), receiver_(this) {}
  FakeWebNNContextProvider(const FakeWebNNContextProvider&) = delete;
  FakeWebNNContextProvider(FakeWebNNContextProvider&&) = delete;
  ~FakeWebNNContextProvider() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(mojo::PendingReceiver<blink_mojom::WebNNContextProvider>(
        std::move(handle)));
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &FakeWebNNContextProvider::OnConnectionError, WTF::Unretained(this)));
  }

  bool IsBound() const { return receiver_.is_bound(); }

  void OnConnectionError() { receiver_.reset(); }

 private:
  // Override methods from webnn::mojom::WebNNContextProvider.
  void CreateWebNNContext(blink_mojom::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override {
    mojo::PendingRemote<blink_mojom::WebNNContext> blink_remote;
    // The receiver bind to FakeWebNNContext.
    mojo::MakeSelfOwnedReceiver<blink_mojom::WebNNContext>(
        std::make_unique<FakeWebNNContext>(*helper_),
        blink_remote.InitWithNewPipeAndPassReceiver());

    webnn::ContextProperties context_properties(
        webnn::InputOperandLayout::kNchw, webnn::Resample2DAxes::kAny,
        webnn::BatchNormalizationAxis::kAny,
        /*tensor_byte_length_limit=*/INT_MAX,
        {/*input=*/webnn::SupportedDataTypes::All(),
         /*constant=*/webnn::SupportedDataTypes::All(),
         /*arg_min_max_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*arg_min_max_output=*/
         webnn::SupportedDataTypes::All(),
         /*batch_normalization_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*batch_normalization_mean=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*cast_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*clamp_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*concat_inputs=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*conv2d_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*conv2d_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*conv_transpose2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*conv_transpose2d_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*cumulative_sum_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*dequantize_linear_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*dequantize_linear_scale=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*dequantize_linear_zero_point=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*add_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*sub_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*mul_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*div_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*max_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*min_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*pow_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*equal_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*greater_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*greater_or_equal_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*lesser_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*lesser_or_equal_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*not_equal_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_and_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_or_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_xor_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_not_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*logical_output=*/webnn::SupportedDataTypes::All(),
         /*abs_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*ceil_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*cos_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*erf_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*exp_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*floor_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*identity_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*log_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*neg_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reciprocal_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*sign_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*sin_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*sqrt_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*tan_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*elu_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*expand_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_indices=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_elements_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_elements_indices=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_nd_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gather_nd_indices=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gelu_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gemm_a=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gemm_c=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gru_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gru_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gru_cell_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*gru_cell_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*hard_sigmoid_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*hard_swish_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*instance_normalization_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*instance_normalization_scale=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*layer_normalization_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*leaky_relu_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*linear_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*lstm_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*lstm_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*lstm_cell_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*lstm_cell_bias=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*matmul_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*pad_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*average_pool2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*l2_pool2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*max_pool2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*prelu_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*quantize_linear_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*quantize_linear_zero_point=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_l1_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_l2_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_log_sum_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_log_sum_exp_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_max_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_mean_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_min_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_product_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_sum_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reduce_sum_square_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*relu_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*resample2d_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reshape_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*reverse_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_elements_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_elements_indices=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_nd_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_nd_indices=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*scatter_nd_updates=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*sigmoid_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*slice_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*softmax_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*softplus_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*softsign_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*split_input=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*tanh_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*tile_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*transpose_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*triangular_input=*/
         {webnn::SupportedDataTypes::All(), kMaxRank},
         /*where_condition=*/{webnn::SupportedDataTypes::All(), kMaxRank},
         /*where_value=*/{webnn::SupportedDataTypes::All(), kMaxRank}});
    auto success = blink_mojom::CreateContextSuccess::New(
        std::move(blink_remote), std::move(context_properties),
        blink::WebNNContextToken());
    std::move(callback).Run(
        blink_mojom::CreateContextResult::NewSuccess(std::move(success)));
  }

  const raw_ref<MLGraphTest> helper_;
  mojo::Receiver<blink_mojom::WebNNContextProvider> receiver_;
};

class ScopedWebNNServiceBinder {
 public:
  explicit ScopedWebNNServiceBinder(MLGraphTest& helper, V8TestingScope& scope)
      : fake_webnn_context_provider_(
            std::make_unique<FakeWebNNContextProvider>(helper)),
        interface_broker_(
            scope.GetExecutionContext()->GetBrowserInterfaceBroker()) {
    interface_broker_->SetBinderForTesting(
        blink_mojom::WebNNContextProvider::Name_,
        WTF::BindRepeating(
            &FakeWebNNContextProvider::BindRequest,
            WTF::Unretained(fake_webnn_context_provider_.get())));
  }

  ~ScopedWebNNServiceBinder() {
    interface_broker_->SetBinderForTesting(
        blink_mojom::WebNNContextProvider::Name_, base::NullCallback());
  }

  bool IsWebNNContextBound() const {
    return fake_webnn_context_provider_->IsBound();
  }

 private:
  std::unique_ptr<FakeWebNNContextProvider> fake_webnn_context_provider_;
  const raw_ref<const BrowserInterfaceBrokerProxy> interface_broker_;
};

// Build a simple MLGraph asynchronously with only one relu operator.
ScriptPromise<MLGraph> BuildSimpleGraph(V8TestingScope& scope,
                                        MLContextOptions* context_options) {
  auto* context = CreateContext(scope, context_options);
  auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                         scope.GetExceptionState());
  if (builder == nullptr) {
    return ScriptPromise<MLGraph>::RejectWithDOMException(
        scope.GetScriptState(),
        DOMException::Create(
            "Unable to create graph builder.",
            DOMException::GetErrorName(DOMExceptionCode::kOperationError)));
  }

  auto* lhs_operand = BuildInput(scope.GetScriptState(), builder, "lhs",
                                 {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
  auto* rhs_operand = BuildInput(scope.GetScriptState(), builder, "rhs",
                                 {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
  MLOperatorOptions* options = MLOperatorOptions::Create();
  auto* output = builder->add(lhs_operand, rhs_operand, options,
                              scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  MLNamedOperands named_outputs = {{"output", output}};
  return builder->build(scope.GetScriptState(), named_outputs,
                        scope.GetExceptionState());
}

bool IsBufferDataEqual(DOMArrayBuffer* array_buffer,
                       base::span<const uint8_t> expected_data) {
  return array_buffer->ByteSpan() == expected_data;
}

// Checks the contents of a MLTensor.
// Returns false if unable to download or the tensor data did not match
// expected.
bool DownloadMLTensorAndCheck(V8TestingScope& scope,
                              MLContext* context,
                              MLTensor* src_tensor,
                              base::span<const uint8_t> expected_data) {
  auto* script_state = scope.GetScriptState();
  ScriptPromiseTester tester(
      script_state,
      context->readTensor(script_state, src_tensor, scope.GetExceptionState()));
  tester.WaitUntilSettled();
  if (tester.IsRejected()) {
    return false;
  }
  EXPECT_TRUE(tester.IsFulfilled());
  auto* array_buffer = V8ToObject<DOMArrayBuffer>(&scope, tester.Value());
  return IsBufferDataEqual(array_buffer, expected_data);
}

MLTensor* CreateMLTensorForOperand(V8TestingScope& scope,
                                   MLContext* ml_context,
                                   const MLOperand* operand) {
  auto array_buffer_view = CreateArrayBufferViewForOperand(operand);
  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(operand->dataType());
  desc->setShape(operand->shape());
  desc->setReadable(true);
  desc->setWritable(true);

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      ml_context->createTensor(scope.GetScriptState(), desc,
                               scope.GetExceptionState()));
  tester.WaitUntilSettled();
  CHECK(tester.IsFulfilled());

  MLTensor* ml_tensor = V8ToObject<MLTensor>(&scope, tester.Value());

  auto* src_data =
      MakeGarbageCollected<AllowSharedBufferSource>(array_buffer_view);
  ml_context->writeTensor(scope.GetScriptState(), ml_tensor, src_data,
                          scope.GetExceptionState());
  return ml_tensor;
}

Vector<uint8_t> GetMLTensorValues(V8TestingScope& scope,
                                  MLContext* ml_context,
                                  MLTensor* ml_tensor) {
  ScriptPromiseTester tester(
      scope.GetScriptState(),
      ml_context->readTensor(scope.GetScriptState(), ml_tensor,
                             scope.GetExceptionState()));
  tester.WaitUntilSettled();
  if (tester.IsRejected()) {
    return {};
  }
  auto* array_buffer = V8ToObject<DOMArrayBuffer>(&scope, tester.Value());
  return GetArrayBufferViewValues<uint8_t>(
      MaybeShared<DOMArrayBufferView>(blink::DOMUint8Array::Create(
          array_buffer, /*byte_offset=*/0, ml_tensor->PackedByteLength())));
}

TEST_F(MLGraphTest, BuildTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  MLContext* context = CreateContext(scope, MLContextOptions::Create());
  {
    // Test throwing exception if the named outputs is empty.
    DummyExceptionStateForTesting exception_state;
    MLNamedOperands named_outputs;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message, "At least one output needs to be provided.");
  }
  {
    // Test throwing exception if the named output is an input operand.
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* input =
        BuildInput(scope.GetScriptState(), builder, "input", {3, 4, 5},
                   V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLNamedOperands named_outputs = {{
        "output",
        input,
    }};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "The operand with name \"output\" is not an output operand.");
  }
  {
    // Test throwing exception if two inputs have the same name.
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* b = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* c = builder->add(a, b, options, exception_state);
    ASSERT_THAT(c, testing::NotNull());

    MLNamedOperands named_outputs = {{"c", c}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message, "The input name \"a\" is duplicated.");
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
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* output = builder->add(a, a, options, exception_state);
    ASSERT_THAT(output, testing::NotNull());
    MLNamedOperands named_outputs = {{"b", output}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("b"), output->Descriptor());
  }
  {
    // Test building a graph with two operators sharing a same input:
    //      [a]
    //     /   \
    //  relu   sigmoid
    //    |      |
    //   [b]    [c]
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* b = builder->relu(a, options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* c = builder->sigmoid(a, options, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    MLNamedOperands named_outputs = {{"b", b}, {"c", c}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(*outputs.at("b"), b->Descriptor());
    EXPECT_EQ(*outputs.at("c"), c->Descriptor());
  }
  {
    // Test building a fake graph with two inputs, one gemm operation and one
    // output.
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* b = BuildInput(scope.GetScriptState(), builder, "b", {4, 3},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* c = BuildGemm(scope, builder, a, b);

    MLNamedOperands named_outputs = {{"c", c}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    EXPECT_EQ(*inputs.at("b"), b->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("c"), c->Descriptor());
  }
}

TEST_F(MLGraphTest, CreateWebNNTensorTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kFloat32);
  desc->setShape({2, 2});

  ScriptPromiseTester tensor_tester(
      script_state,
      ml_context->createTensor(script_state, desc, scope.GetExceptionState()));
  tensor_tester.WaitUntilSettled();
  EXPECT_TRUE(tensor_tester.IsFulfilled());

  MLTensor* ml_tensor = V8ToObject<MLTensor>(&scope, tensor_tester.Value());
  ASSERT_THAT(ml_tensor, testing::NotNull());
  EXPECT_EQ(ml_tensor->dataType(), desc->dataType());
  EXPECT_EQ(ml_tensor->shape(), desc->shape());
}

TEST_F(MLGraphTest, WriteWebNNTensorTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  constexpr size_t kTensorSize = 4ull;
  const Vector<uint32_t> kTensorShape{2, 2};

  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kUint8);
  desc->setShape(kTensorShape);
  desc->setReadable(true);
  desc->setWritable(true);

  ScriptPromiseTester tensor_tester(
      script_state,
      ml_context->createTensor(script_state, desc, scope.GetExceptionState()));
  tensor_tester.WaitUntilSettled();
  EXPECT_TRUE(tensor_tester.IsFulfilled());

  MLTensor* ml_tensor = V8ToObject<MLTensor>(&scope, tensor_tester.Value());
  ASSERT_THAT(ml_tensor, testing::NotNull());

  std::array<const uint8_t, kTensorSize> input_data = {0xAA, 0xAA, 0xAA, 0xAA};
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(input_data);
  ASSERT_THAT(array_buffer, testing::NotNull());

  // Write data to the tensor.
  auto* src_data = MakeGarbageCollected<AllowSharedBufferSource>(array_buffer);
  ml_context->writeTensor(script_state, ml_tensor, src_data,
                          scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(
      DownloadMLTensorAndCheck(scope, ml_context, ml_tensor, input_data));

  // Write different data to the tensor.
  std::array<const uint8_t, kTensorSize> new_data = {0xAA, 0xCC, 0xBB, 0xBB};
  DOMArrayBuffer* new_array_buffer = DOMArrayBuffer::Create(new_data);
  ASSERT_THAT(new_array_buffer, testing::NotNull());
  auto* new_src_data =
      MakeGarbageCollected<AllowSharedBufferSource>(new_array_buffer);
  ml_context->writeTensor(script_state, ml_tensor, new_src_data,
                          scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(DownloadMLTensorAndCheck(scope, ml_context, ml_tensor, new_data));
}

// Writing data from an array buffer to a destroyed MLTensor should not crash.
TEST_F(MLGraphTest, WriteWebNNTensorThenDestroyTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kUint8);
  desc->setShape({2, 2});
  desc->setWritable(true);

  ScriptPromiseTester tensor_tester(
      script_state,
      ml_context->createTensor(script_state, desc, scope.GetExceptionState()));
  tensor_tester.WaitUntilSettled();
  EXPECT_TRUE(tensor_tester.IsFulfilled());

  MLTensor* ml_tensor = V8ToObject<MLTensor>(&scope, tensor_tester.Value());
  ASSERT_THAT(ml_tensor, testing::NotNull());

  ml_tensor->destroy();

  auto* src_data =
      MakeGarbageCollected<AllowSharedBufferSource>(CreateDOMArrayBufferView(
          ml_tensor->PackedByteLength(), V8MLOperandDataType::Enum::kUint8));
  ml_context->writeTensor(script_state, ml_tensor, src_data,
                          scope.GetExceptionState());
}

// Reading data from an array buffer to a destroyed MLTensor should not crash.
TEST_F(MLGraphTest, ReadWebNNTensorThenDestroyTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* script_state = scope.GetScriptState();

  MLContext* ml_context = CreateContext(scope, options);

  auto* desc = MLTensorDescriptor::Create();
  desc->setDataType(V8MLOperandDataType::Enum::kFloat32);
  desc->setShape({2, 2});
  desc->setReadable(true);

  ScriptPromiseTester create_tensor_tester(
      script_state,
      ml_context->createTensor(script_state, desc, scope.GetExceptionState()));
  create_tensor_tester.WaitUntilSettled();
  EXPECT_TRUE(create_tensor_tester.IsFulfilled());

  MLTensor* ml_tensor =
      V8ToObject<MLTensor>(&scope, create_tensor_tester.Value());
  ASSERT_THAT(ml_tensor, testing::NotNull());

  ml_tensor->destroy();

  ScriptPromise<DOMArrayBuffer> read_promise = ml_context->readTensor(
      script_state, ml_tensor, scope.GetExceptionState());
  EXPECT_TRUE(read_promise.IsEmpty());
}

TEST_F(MLGraphTest, WebNNGraphDispatchTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  MLContext* ml_context = CreateContext(scope, options);
  auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), ml_context,
                                         scope.GetExceptionState());
  ASSERT_THAT(builder, testing::NotNull());
  const Vector<uint32_t> dimensions = {3, 5};
  const wtf_size_t number_of_elements = 15;

  // Build the graph.
  auto* lhs_operand =
      BuildInput(scope.GetScriptState(), builder, "lhs", dimensions,
                 V8MLOperandDataType::Enum::kUint8, scope.GetExceptionState());
  auto* rhs_operand =
      BuildInput(scope.GetScriptState(), builder, "rhs", dimensions,
                 V8MLOperandDataType::Enum::kUint8, scope.GetExceptionState());
  auto* output_operand = BuildElementWiseBinary(
      scope, builder, webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
      lhs_operand, rhs_operand);
  MLNamedOperands named_outputs = {{"output", output_operand}};
  auto [graph, error_message, build_exception] =
      BuildGraph(scope, builder, named_outputs);
  ASSERT_THAT(graph, testing::NotNull());

  MLTensor* input_tensor =
      CreateMLTensorForOperand(scope, ml_context, lhs_operand);
  ASSERT_THAT(input_tensor, testing::NotNull());

  MLNamedTensors inputs(
      {{"lhs", input_tensor},
       {"rhs", CreateMLTensorForOperand(scope, ml_context, rhs_operand)}});
  MLNamedTensors outputs({{"output", CreateMLTensorForOperand(
                                         scope, ml_context, output_operand)}});

  {
    // Dispatch successfully.
    ml_context->dispatch(scope.GetScriptState(), graph, inputs, outputs,
                         scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kNoError));
    Vector<uint8_t> results =
        GetMLTensorValues(scope, ml_context, outputs[0].second);
    EXPECT_EQ(results, Vector<uint8_t>(number_of_elements, 0));

    // Dispatch again successfully.
    ml_context->dispatch(scope.GetScriptState(), graph, inputs, outputs,
                         scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kNoError));
    results = GetMLTensorValues(scope, ml_context, outputs[0].second);
    EXPECT_EQ(results, Vector<uint8_t>(number_of_elements, 0));
  }
}

TEST_F(MLGraphTest, CreateWebNNGraphTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* script_state = scope.GetScriptState();
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);

  {
    ScriptPromiseTester tester(script_state, BuildSimpleGraph(scope, options));
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    MLGraph* ml_graph = V8ToObject<MLGraph>(&scope, tester.Value());
    ASSERT_THAT(ml_graph, testing::NotNull());
    EXPECT_TRUE(scoped_setup_binder.IsWebNNContextBound());
  }
}

struct ClampOptions {
  std::optional<float> min_value;
  std::optional<float> max_value;
};

struct SoftmaxTester {
  OperandInfo<float> input;
  webnn::OperandDescriptor expected_descriptor;

  void Test(MLGraphTest& helper, V8TestingScope& scope, MLContext* context) {
    // Build the graph.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* input_operand =
        BuildInput(scope.GetScriptState(), builder, "input", input.dimensions,
                   input.data_type, scope.GetExceptionState());
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* output_operand =
        builder->softmax(input_operand, options, scope.GetExceptionState());
    MLNamedOperands named_outputs = {{"output", output_operand}};
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_softmax());
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    ASSERT_LT(output_operand_id.value(), graph_info->operands.size());
    EXPECT_EQ(graph_info->operands[output_operand_id.value()]->descriptor,
              expected_descriptor);
  }
};

TEST_F(MLGraphTest, SoftmaxTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  MLContext* context = CreateContext(scope, options);

  {
    // Test building softmax with float32 input.
    SoftmaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 4}},
        .expected_descriptor = ToDescriptor(webnn::OperandDataType::kFloat32,
                                            std::array<uint32_t, 2>{2, 4})}
        .Test(*this, scope, context);
  }
  {
    // Test building softmax with float16 input.
    SoftmaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 5}},
        .expected_descriptor = ToDescriptor(webnn::OperandDataType::kFloat16,
                                            std::array<uint32_t, 2>{1, 5})}
        .Test(*this, scope, context);
  }
}

struct CastTester {
  OperandInfo<float> input;
  V8MLOperandDataType::Enum output_data_type;
  webnn::OperandDescriptor expected_descriptor;

  void Test(MLGraphTest& helper, V8TestingScope& scope, MLContext* context) {
    // Build the graph.
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           scope.GetExceptionState());
    ASSERT_THAT(builder, testing::NotNull());
    auto* input_operand =
        BuildInput(scope.GetScriptState(), builder, "input", input.dimensions,
                   input.data_type, scope.GetExceptionState());
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* output_operand =
        builder->cast(input_operand, V8MLOperandDataType(output_data_type),
                      options, scope.GetExceptionState());
    MLNamedOperands named_outputs = {{"output", output_operand}};
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_element_wise_unary());
    webnn::mojom::blink::ElementWiseUnaryPtr& element_wise_unary =
        operation->get_element_wise_unary();
    EXPECT_EQ(element_wise_unary->kind,
              blink_mojom::ElementWiseUnary::Kind::kCast);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    ASSERT_LT(output_operand_id.value(), graph_info->operands.size());
    EXPECT_EQ(graph_info->operands[output_operand_id.value()]->descriptor,
              expected_descriptor);
  }
};

TEST_F(MLGraphTest, CastTester) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  MLContext* context = CreateContext(scope, options);

  const std::array<uint32_t, 2> shape{2, 2};
  const Vector<uint32_t> wtf_shape(shape);
  {
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kUint8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kFloat16,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kFloat16, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt8,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt8, shape)}
        .Test(*this, scope, context);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = wtf_shape},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_descriptor =
                   ToDescriptor(webnn::OperandDataType::kInt32, shape)}
        .Test(*this, scope, context);
  }
}

TEST_F(MLGraphTest, MLTransformTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  MLContext* context = CreateContext(scope, MLContextOptions::Create());

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());

    //   [a]
    //   / \
    //   \ /
    //   add
    //    |
    //   [b]
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    MLOperatorOptions* options = MLOperatorOptions::Create();
    auto* b = builder->add(a, a, options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    //  Transform the graph to:
    // [a]  [c]
    //  \   /
    //   \ /
    //   add
    //    |
    //   [b]

    auto* c =
        BuildConstant(scope.GetScriptState(), builder, {3, 4, 5},
                      V8MLOperandDataType::Enum::kFloat32, exception_state);
    ASSERT_THAT(c, testing::NotNull());
    MLGraphTransformer::Disconnect(a, b->Operator(), 1);
    MLGraphTransformer::Connect(c, b->Operator(), 1);
    EXPECT_EQ(b->Operator()->Inputs()[0], a);
    EXPECT_EQ(b->Operator()->Inputs()[1], c);
    // Build the transformed graph.
    MLNamedOperands named_outputs = {{"b", b}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("b"), b->Descriptor());
  }

  {
    DummyExceptionStateForTesting exception_state;
    auto* builder = MLGraphBuilder::Create(scope.GetScriptState(), context,
                                           exception_state);
    ASSERT_THAT(builder, testing::NotNull());
    // [a] -> transpose -> [b] -> relu -> [c]
    auto* a = BuildInput(scope.GetScriptState(), builder, "a", {3, 4, 5},
                         V8MLOperandDataType::Enum::kFloat32, exception_state);
    auto* transpose_options = MLTransposeOptions::Create();
    transpose_options->setPermutation({0, 2, 1});
    auto* b = builder->transpose(a, transpose_options, exception_state);
    ASSERT_THAT(b, testing::NotNull());
    auto* relu_options = MLOperatorOptions::Create();
    auto* c = builder->relu(b, relu_options, exception_state);
    ASSERT_THAT(c, testing::NotNull());

    EXPECT_EQ(c->Shape(), std::vector<uint32_t>({3, 5, 4}));
    // Transform the graph to:
    // [a] -> relu -> [c]
    MLGraphTransformer::Disconnect(a, b->Operator(), 0);
    MLGraphTransformer::Disconnect(b, c->Operator(), 0);
    MLGraphTransformer::Connect(a, c->Operator(), 0);
    // update shape of c
    auto* updated_c =
        MLGraphTransformer::ReplaceOperandWithNewShape(c, {3, 4, 5});
    EXPECT_EQ(updated_c->Shape(), std::vector<uint32_t>({3, 4, 5}));

    // Build the transformed graph.
    MLNamedOperands named_outputs = {{"c", updated_c}};
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputConstraints();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*inputs.at("a"), a->Descriptor());
    const auto& outputs = graph->GetOutputConstraints();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(*outputs.at("c"), updated_c->Descriptor());
  }
}

}  // namespace blink
