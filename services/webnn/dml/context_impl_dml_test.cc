// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/context_impl_dml.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "services/webnn/webnn_test_utils.h"

namespace webnn::dml {

namespace {

void RemoveDeviceToDestroyAllContexts(ContextImplDml* context) {
  context->RemoveDeviceForTesting();
  context->HandleContextLostOrCrash("Testing for device removal.", S_OK);
}

// A fake WebNNGraph Mojo interface implementation that binds a pipe for
// computing graph message.
class FakeWebNNGraphImpl final : public WebNNGraphImpl {
 public:
  FakeWebNNGraphImpl(ContextImplDml* context,
                     ComputeResourceInfo compute_resource_info)
      : WebNNGraphImpl(context, std::move(compute_resource_info)),
        context_(context) {}
  ~FakeWebNNGraphImpl() override = default;

 private:
  void DispatchImpl(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs)
      override {
    RemoveDeviceToDestroyAllContexts(context_);
  }

  raw_ptr<ContextImplDml> context_;
};

// A fake WebNNTensor Mojo interface implementation that binds a pipe for
// tensor creation message.
class FakeWebNNTensorImpl final : public WebNNTensorImpl {
 public:
  FakeWebNNTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      ContextImplDml* context,
      mojom::TensorInfoPtr tensor_info)
      : WebNNTensorImpl(std::move(receiver), context, std::move(tensor_info)),
        context_(context) {}
  ~FakeWebNNTensorImpl() override = default;

 private:
  void ReadTensorImpl(ReadTensorCallback callback) override {
    std::move(callback).Run(ToError<mojom::ReadTensorResult>(
        mojom::Error::Code::kUnknownError, "Tesing for device removal."));
    RemoveDeviceToDestroyAllContexts(context_);
  }
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override {
    RemoveDeviceToDestroyAllContexts(context_);
  }

  raw_ptr<ContextImplDml> context_;
};

// Helper class to create the FakeWebNNGraphImpl that is intended to test
// the graph validation steps and computation resources.
class FakeWebNNBackend final : public ContextImplDml::BackendForTesting {
  void CreateGraphImpl(
      ContextImplDml* context,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      WebNNContextImpl::CreateGraphImplCallback callback) override {
    std::move(callback).Run(std::make_unique<FakeWebNNGraphImpl>(
        context, std::move(compute_resource_info)));
  }

  void CreateTensorImpl(
      ContextImplDml* context,
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      WebNNContextImpl::CreateTensorImplCallback callback) override {
    std::move(callback).Run(std::make_unique<FakeWebNNTensorImpl>(
        std::move(receiver), context, std::move(tensor_info)));
  }
};

struct CreateTensorSuccess {
  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  blink::WebNNTensorToken webnn_tensor_handle;
};

CreateTensorSuccess CreateWebNNTensor(
    mojo::Remote<mojom::WebNNContext>& webnn_context_remote,
    OperandDataType data_type,
    std::vector<uint32_t> shape) {
  base::test::TestFuture<mojom::CreateTensorResultPtr> create_tensor_future;
  webnn_context_remote->CreateTensor(
      mojom::TensorInfo::New(
          *OperandDescriptor::Create(webnn::GetContextPropertiesForTesting(),
                                     data_type, shape, "tensor"),
          MLTensorUsage{MLTensorUsageFlags::kWrite, MLTensorUsageFlags::kRead}),
      create_tensor_future.GetCallback());
  mojom::CreateTensorResultPtr create_tensor_result =
      create_tensor_future.Take();
  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  webnn_tensor_remote.Bind(
      std::move(create_tensor_result->get_success()->tensor_remote));
  return CreateTensorSuccess{
      std::move(webnn_tensor_remote),
      std::move(create_tensor_result->get_success()->tensor_handle)};
}

}  // namespace

class WebNNContextDMLImplTest : public TestBase {
 public:
  void SetUp() override;

 protected:
  WebNNContextDMLImplTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}
  ~WebNNContextDMLImplTest() override = default;

  bool CreateWebNNContext() {
    bool is_platform_supported = true;

    // Create the ContextImplDml through context provider.
    base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
    webnn_provider_remote_->CreateWebNNContext(
        mojom::CreateContextOptions::New(
            mojom::CreateContextOptions::Device::kGpu,
            mojom::CreateContextOptions::PowerPreference::kDefault),
        create_context_future.GetCallback());
    auto create_context_result = create_context_future.Take();
    if (create_context_result->is_success()) {
      webnn_context_remote_.Bind(
          std::move(create_context_result->get_success()->context_remote));
    } else {
      is_platform_supported = create_context_result->get_error()->code !=
                              mojom::Error::Code::kNotSupportedError;
    }
    return is_platform_supported;
  }

 protected:
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote_;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void WebNNContextDMLImplTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  Adapter::EnableDebugLayerForTesting();
  auto adapter_creation_result = Adapter::GetGpuInstanceForTesting();
  // If the adapter creation result has no value, it's most likely because
  // platform functions were not properly loaded.
  SKIP_TEST_IF(!adapter_creation_result.has_value());
  auto adapter = adapter_creation_result.value();
  // Graph compilation relies on IDMLDevice1::CompileGraph introduced in
  // DirectML version 1.2 or DML_FEATURE_LEVEL_2_1, so skip the tests if the
  // DirectML version doesn't support this feature.
  SKIP_TEST_IF(!adapter->IsDMLDeviceCompileGraphSupportedForTesting());
}

class WebNNFakeContextDMLImplTest : public WebNNContextDMLImplTest {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  WebNNFakeContextDMLImplTest() = default;
  ~WebNNFakeContextDMLImplTest() override = default;

 private:
  FakeWebNNBackend backend_for_testing_;
};

void WebNNFakeContextDMLImplTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  WebNNContextDMLImplTest::SetUp();
  ContextImplDml::SetBackendForTesting(&backend_for_testing_);
}
void WebNNFakeContextDMLImplTest::TearDown() {
  ContextImplDml::SetBackendForTesting(nullptr);
}

TEST_F(WebNNContextDMLImplTest, CreateGraphImplTest) {
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote_.BindNewPipeAndPassReceiver());
  SKIP_TEST_IF(!CreateWebNNContext());

  ASSERT_TRUE(webnn_context_remote_.is_bound());

  // Build a simple graph with relu operator.
  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote;
  webnn_context_remote_->CreateGraphBuilder(
      graph_builder_remote.BindNewEndpointAndPassReceiver());
  GraphInfoBuilder builder(graph_builder_remote);

  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 2, 3, 4}, OperandDataType::kFloat32);
  builder.BuildRelu(input_operand_id, output_operand_id);

  // The GraphImplDml should be built successfully.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  graph_builder_remote->CreateGraph(builder.TakeGraphInfo(),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  EXPECT_TRUE(create_graph_result->is_graph_remote());

  // Reset the remote to ensure `WebNNGraphImpl` is released.
  if (create_graph_result->is_graph_remote()) {
    create_graph_result->get_graph_remote().reset();
  }

  // Ensure `WebNNContextImpl::OnConnectionError()` is called and
  // `WebNNContextImpl` is released.
  webnn_context_remote_.reset();
  webnn_provider_remote_.reset();

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebNNFakeContextDMLImplTest, DeviceRemovalFromDispatch) {
  bool all_contexts_lost = false;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote_.BindNewPipeAndPassReceiver(),
      WebNNContextProviderImpl::WebNNStatus::kWebNNEnabled,
      base::BindOnce([](bool* all_contexts_lost) { *all_contexts_lost = true; },
                     base::Unretained(&all_contexts_lost)));
  SKIP_TEST_IF(!CreateWebNNContext());

  ASSERT_TRUE(webnn_context_remote_.is_bound());

  OperandDataType data_type = OperandDataType::kFloat32;
  std::vector<uint32_t> shape = {1, 2, 3, 4};
  // Build a simple graph with relu operator.
  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote;
  webnn_context_remote_->CreateGraphBuilder(
      graph_builder_remote.BindNewEndpointAndPassReceiver());
  GraphInfoBuilder builder(graph_builder_remote);
  uint64_t input_operand_id = builder.BuildInput("input", shape, data_type);
  uint64_t output_operand_id = builder.BuildOutput("output", shape, data_type);
  builder.BuildRelu(input_operand_id, output_operand_id);

  // The GraphImplDml should be built successfully.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  graph_builder_remote->CreateGraph(builder.TakeGraphInfo(),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  EXPECT_TRUE(create_graph_result->is_graph_remote());
  mojo::AssociatedRemote<mojom::WebNNGraph> webnn_graph_remote;
  webnn_graph_remote.Bind(std::move(create_graph_result->get_graph_remote()));

  CreateTensorSuccess input_tensor =
      CreateWebNNTensor(webnn_context_remote_, data_type, shape);
  CreateTensorSuccess output_tensor =
      CreateWebNNTensor(webnn_context_remote_, data_type, shape);
  // Assign tensors for the inputs.
  base::flat_map<std::string, blink::WebNNTensorToken> named_inputs;
  named_inputs.emplace("input", input_tensor.webnn_tensor_handle);
  // Assign tensors for the outputs.
  base::flat_map<std::string, blink::WebNNTensorToken> named_outputs;
  named_outputs.emplace("output", output_tensor.webnn_tensor_handle);
  webnn_graph_remote->Dispatch(named_inputs, named_outputs);

  webnn_graph_remote.reset();
  graph_builder_remote.reset();
  webnn_context_remote_.reset();
  webnn_provider_remote_.reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return all_contexts_lost; }));
}

TEST_F(WebNNFakeContextDMLImplTest, DeviceRemovalFromWritingTensor) {
  bool all_contexts_lost = false;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote_.BindNewPipeAndPassReceiver(),
      WebNNContextProviderImpl::WebNNStatus::kWebNNEnabled,
      base::BindOnce([](bool* all_contexts_lost) { *all_contexts_lost = true; },
                     base::Unretained(&all_contexts_lost)));
  SKIP_TEST_IF(!CreateWebNNContext());

  ASSERT_TRUE(webnn_context_remote_.is_bound());

  OperandDataType data_type = OperandDataType::kFloat32;
  std::vector<uint32_t> shape = {1};
  CreateTensorSuccess tensor =
      CreateWebNNTensor(webnn_context_remote_, data_type, shape);
  tensor.webnn_tensor_remote->WriteTensor(mojo_base::BigBuffer(sizeof(float)));

  tensor.webnn_tensor_remote.reset();
  webnn_context_remote_.reset();
  webnn_provider_remote_.reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return all_contexts_lost; }));
}

TEST_F(WebNNFakeContextDMLImplTest, DeviceRemovalFromReadingTensor) {
  bool all_contexts_lost = false;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote_.BindNewPipeAndPassReceiver(),
      WebNNContextProviderImpl::WebNNStatus::kWebNNEnabled,
      base::BindOnce([](bool* all_contexts_lost) { *all_contexts_lost = true; },
                     base::Unretained(&all_contexts_lost)));
  SKIP_TEST_IF(!CreateWebNNContext());

  ASSERT_TRUE(webnn_context_remote_.is_bound());

  OperandDataType data_type = OperandDataType::kFloat32;
  std::vector<uint32_t> shape = {1};
  CreateTensorSuccess tensor =
      CreateWebNNTensor(webnn_context_remote_, data_type, shape);
  base::test::TestFuture<mojom::ReadTensorResultPtr> future;
  tensor.webnn_tensor_remote->ReadTensor(future.GetCallback());

  tensor.webnn_tensor_remote.reset();
  webnn_context_remote_.reset();
  webnn_provider_remote_.reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return all_contexts_lost; }));
}

}  // namespace webnn::dml
