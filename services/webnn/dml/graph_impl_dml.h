// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_GRAPH_IMPL_DML_H_
#define SERVICES_WEBNN_DML_GRAPH_IMPL_DML_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/fixed_array.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/microsoft_dxheaders/include/directml.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

namespace webnn::dml {

class Adapter;
class CommandRecorder;
class ContextImplDml;
class GraphBuilderDml;

// Record the total byte length of buffers and the D3D12_RANGE for each
// buffer, all with the required alignment.
template <typename Key>
struct AlignedByteLength {
  size_t total_byte_length = 0;
  std::map<Key, D3D12_RANGE> key_to_d3d12_range_map;
};

// GraphImplDml inherits WebNNGraphImpl to represent a DML graph implementation.
// It is mainly responsible for building and compiling a DML graph from
// mojom::GraphInfo via GraphBuilderDml, then initializing and executing the
// graph represented by an IDMLCompiledOperator.
class GraphImplDml final : public WebNNGraphImpl {
 public:
  // It records the graph's buffer binding info to create the buffer binding
  // (DML_BUFFER_BINDING) for the graph execution.
  struct GraphBufferBindingInfo {
    GraphBufferBindingInfo();
    ~GraphBufferBindingInfo();

    GraphBufferBindingInfo(const GraphBufferBindingInfo&);
    GraphBufferBindingInfo& operator=(const GraphBufferBindingInfo&);

    GraphBufferBindingInfo(GraphBufferBindingInfo&&);
    GraphBufferBindingInfo& operator=(GraphBufferBindingInfo&&);

    // The count of input buffer bindings for the graph execution should equal
    // to the the number of both constants and inputs.
    size_t input_buffer_binding_count = 0;
    // The map is used to bind input buffers for the graph execution in
    // order.
    // The index is the DML_INPUT_GRAPH_EDGE_DESC::GraphInputIndex when
    // creating the DML_GRAPH_DESC.
    std::unordered_map<std::string, uint32_t> graph_input_name_to_index_map;
    // The map is used to bind output buffers for the graph execution in
    // order.
    // The index is the DML_OUTPUT_GRAPH_EDGE_DESC::GraphOutputIndex when
    // creating the DML_GRAPH_DESC.
    std::unordered_map<std::string, uint32_t> graph_output_name_to_index_map;
  };
  static base::expected<void, mojom::ErrorPtr> CreateAndBuildInternal(
      const ContextProperties& context_properties,
      scoped_refptr<Adapter> adapter,
      mojom::GraphInfoPtr& graph_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
          constant_operands,
      GraphBuilderDml& graph_builder,
      std::unordered_map<uint64_t, uint32_t>& constant_id_to_input_index_map,
      GraphBufferBindingInfo& graph_buffer_binding_info);

  // This method builds and compiles a DML graph from mojom::GraphInfo via
  // GraphBuilderDml, and then calls the CommandRecorder::InitializeOperator
  // method to initialize the DML graph. Next, it calls CommandQueue::WaitAsync
  // method to wait for the initialization work to be completed on GPU. The
  // GraphImplDml instance will only be created and bound to the mojom receiver
  // in GraphImplDml::OnInitializationComplete method.
  static void CreateAndBuild(
      scoped_refptr<Adapter> adapter,
      base::WeakPtr<ContextImplDml> context,
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      WebNNContextImpl::CreateGraphImplCallback callback,
      bool disable_dml_meta_commands_for_gpu);

  GraphImplDml(const GraphImplDml&) = delete;
  GraphImplDml& operator=(const GraphImplDml&) = delete;
  ~GraphImplDml() override;

 private:
  // Contains the persistent resource for the graph initialization and execution
  // if the graph needs it. The resource should be kept alive until the GPU has
  // completed the execution.
  class PersistentResource final
      : public base::RefCountedThreadSafe<PersistentResource> {
   public:
    static scoped_refptr<PersistentResource> Create(
        uint64_t persistent_buffer_byte_length,
        Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer);

    PersistentResource(const PersistentResource&) = delete;
    PersistentResource& operator=(const PersistentResource&) = delete;

    DML_BINDING_DESC persistent_buffer_binding_desc() const {
      return persistent_buffer_binding_desc_;
    }

   private:
    friend class base::RefCountedThreadSafe<PersistentResource>;
    PersistentResource(
        uint64_t persistent_buffer_byte_length,
        Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer);
    ~PersistentResource();

    Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer_;
    DML_BUFFER_BINDING persistent_buffer_binding_;
    DML_BINDING_DESC persistent_buffer_binding_desc_;
  };

  // Contains the GPU descriptor heap and temporary buffer for graph
  // execution. These resources should be kept alive until the GPU has completed
  // the execution. After that, the resources could be reused for next graph
  // execution or be released.
  struct GraphResources {
    GraphResources(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap,
                   uint64_t temporary_buffer_byte_length,
                   Microsoft::WRL::ComPtr<ID3D12Resource> temporary_resource);
    ~GraphResources();
    GraphResources(const GraphResources&) = delete;
    GraphResources& operator=(const GraphResources&) = delete;
    GraphResources(GraphResources&&) = delete;
    GraphResources& operator=(GraphResources&&) = delete;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;

    // Temporary buffers can be reused between DML dispatches. However,
    // they cannot be used between multiple queues at a time.
    // https://learn.microsoft.com/en-us/windows/ai/directml/dml-binding
    Microsoft::WRL::ComPtr<ID3D12Resource> temporary_buffer;
    std::optional<DML_BUFFER_BINDING> temporary_buffer_binding;
    std::optional<DML_BINDING_DESC> temporary_buffer_binding_desc;
  };

  static base::expected<std::unique_ptr<GraphResources>, HRESULT>
  AllocateGraphResources(Adapter* adapter,
                         IDMLCompiledOperator* compiled_operator);

  // `ExecuteAndWaitSyncOnBackgroundThread` accepts a `CommandRecorder` which
  // keeps a reference to the `init_command_queue_for_npu_` in `Adapter`. The
  // method submits the command list for execution and synchronously wait for
  // initialization to complete. Since `ID3D12CommandQueue::ExecuteCommandLists`
  // called in this method may take long time on some adapters e.g. NPU, this
  // method should run on non-gpuMain threads to avoid blocking the compositor.
  //
  // CommandQueue is not a thread-safe object and should only be used by one
  // task runner at a time to avoid race conditions with its member variables.
  static HRESULT ExecuteAndWaitSyncOnBackgroundThread(
      std::unique_ptr<CommandRecorder> init_command_recorder_for_npu);

  static void CreateWebNNGraphImpl(
      scoped_refptr<Adapter> adapter,
      base::WeakPtr<ContextImplDml> context,
      scoped_refptr<PersistentResource> persistent_resource,
      Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
      ComputeResourceInfo compute_resource_info,
      GraphBufferBindingInfo graph_buffer_binding_info,
      WebNNContextImpl::CreateGraphImplCallback callback);

  GraphImplDml(scoped_refptr<Adapter> adapter,
               ContextImplDml* context,
               std::unique_ptr<CommandRecorder> command_recorder,
               scoped_refptr<PersistentResource> persistent_resource,
               Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
               ComputeResourceInfo compute_resource_info,
               GraphBufferBindingInfo graph_buffer_binding_info,
               std::unique_ptr<GraphResources> graph_resources);

  // The method compiles all DML operators into an IDMLCompiledOperator
  // which can be dispatched to GPU. Since IDMLDevice1::CompileGraph called in
  // this method may take long time to compile shaders (if not cached before),
  // this method should run on a background thread rather than the current GPU
  // main thread to avoid blocking.
  static base::expected<Microsoft::WRL::ComPtr<IDMLCompiledOperator>, HRESULT>
  CompileOnBackgroundThread(GraphBuilderDml graph_builder,
                            DML_EXECUTION_FLAGS flags);

  // After the CompileOnBackgroundThread task is completed on a background
  // thread, the OnCompilationComplete method should run back on the GPU main
  // thread since graph initialization commands are submitted to GPU. Notice
  // that the compiled_operator might be nullptr if the graph compilation fails.
  //
  // The `constant_id_to_input_index_map` is used to bind constant buffers
  // for the graph initialization in order. The constant id is the key for
  // `id_to_operand_map` of `mojom::GraphInfo` interface, the input index is the
  // DML_INPUT_GRAPH_EDGE_DESC::GraphInputIndex when creating the
  // DML_GRAPH_DESC. DirectML graph treats both input tensors and constant
  // tensors to be graph inputs. The difference is the data of the constant
  // tensor is owned by DirectML and should be uploaded during the graph
  // initialization, while the data of the input tensor is uploaded for every
  // graph execution.
  static void OnCompilationComplete(
      scoped_refptr<Adapter> adapter,
      base::WeakPtr<ContextImplDml> context,
      WebNNContextImpl::CreateGraphImplCallback callback,
      std::unordered_map<uint64_t, uint32_t> constant_id_to_input_index_map,
      GraphBufferBindingInfo graph_buffer_binding_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::expected<Microsoft::WRL::ComPtr<IDMLCompiledOperator>, HRESULT>
          compilation_result);

  // This method creates the GraphImplDml instance and binds it to the
  // mojom::WebNNGraph receiver, then runs callback to send the pending remote
  // to the renderer process. Notice that the `persistent_resource` could be
  // nullptr which means it isn't required by the graph.
  static void OnInitializationComplete(
      scoped_refptr<Adapter> adapter,
      base::WeakPtr<ContextImplDml> context,
      scoped_refptr<PersistentResource> persistent_resource,
      Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
      ComputeResourceInfo compute_resource_info,
      GraphBufferBindingInfo graph_buffer_binding_info,
      WebNNContextImpl::CreateGraphImplCallback callback,
      HRESULT hr);

  // After the dispatch is completed, recycle the graph resources for another
  // dispatch.
  void OnDispatchComplete(std::unique_ptr<GraphResources> graph_resources,
                          HRESULT hr);

  // If GraphImplDml::DispatchImpl fails, report and log an error message and
  // release the command recorder since it may haven't been closed normally by
  // CommandRecorder::CloseAndExecute.
  void HandleDispatchFailure(std::string_view error_message, HRESULT hr);

  // Represent the input or output bindings for the graph execution.
  struct IoBindings {
    IoBindings(std::vector<DML_BUFFER_BINDING> buffer_bindings,
               base::FixedArray<DML_BINDING_DESC> buffer_binding_desc);
    ~IoBindings();

    IoBindings(const IoBindings&) = delete;
    IoBindings& operator=(const IoBindings&) = delete;

    IoBindings(IoBindings&&) = delete;
    IoBindings& operator=(IoBindings&&) = delete;

    // Ensure this object remains alive because `buffer_binding_desc` contains
    // pointers to its data.
    std::vector<DML_BUFFER_BINDING> buffer_bindings;
    base::FixedArray<DML_BINDING_DESC> buffer_binding_desc;
  };

  IoBindings CreateAndCacheInputBindings(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs);

  IoBindings CreateAndCacheOutputBindings(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs);

  // Execute the compiled platform graph asynchronously. The inputs were
  // validated in base class so we can use them to compute directly.
  void DispatchImpl(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs)
      override;

  // The persistent resource is allocated after the compilation work is
  // completed for the graph initialization and will be used for the following
  // graph executions. It could be nullptr which means it isn't required by the
  // graph and won't need to be bound for graph executions.
  scoped_refptr<PersistentResource> persistent_resource_;

  // Adapter used to create the built graph.
  scoped_refptr<Adapter> adapter_;

  // ContextImplDml owns this object.
  raw_ptr<ContextImplDml> context_;

  // The command_recorder is created for the graph execution and recycled
  // after graph execution has completed. It avoids the resource allocation
  // overhead for the first execution and following executions when it is
  // available. A graph execution takes its ownership during the execution and
  // returns the ownership once the GPU has completed the execution. If it is
  // unavailable, e.g., being taken by previous uncompleted execution, a graph
  // execution will create a new one and release it after the execution is
  // done.
  std::unique_ptr<CommandRecorder> command_recorder_;
  // IDMLCompiledOperator represents a compiled and initialized DML graph to be
  // executed on GPU.
  Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator_;
  GraphBufferBindingInfo graph_buffer_binding_info_;

  // Graph resources are allocated after graph initialization and
  // recycled after graph execution has completed. It avoids the resource
  // allocation overhead for the first execution and following executions when
  // it is available. A graph execution takes its ownership during the execution
  // and returns the ownership once the GPU has completed the execution. If it
  // is unavailable, e.g., being taken by previous uncompleted execution, a
  // graph execution will allocate a new one and release it after the execution
  // is done.
  std::unique_ptr<GraphResources> graph_resources_;

  base::flat_map<std::string, base::WeakPtr<const WebNNTensorImpl>>
      previous_input_tensors_;
  base::flat_map<std::string, base::WeakPtr<const WebNNTensorImpl>>
      previous_output_tensors_;

  base::WeakPtrFactory<GraphImplDml> weak_factory_{this};
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_GRAPH_IMPL_DML_H_
