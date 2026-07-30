// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_D3D12_VIDEO_MOCKS_H_
#define MEDIA_BASE_WIN_D3D12_VIDEO_MOCKS_H_

#include <d3d12video.h>
#include <wrl.h>

#include "media/base/win/test_utils.h"

namespace media {

class D3D12VideoDevice3Mock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D12VideoDevice3> {
 public:
  D3D12VideoDevice3Mock();
  ~D3D12VideoDevice3Mock() override;

  MOCK_STDCALL_METHOD2(QueryInterface, HRESULT(REFIID riid, void** ppvObject));

  // Interfaces of ID3D12VideoDevice

  MOCK_STDCALL_METHOD3(
      CheckFeatureSupport,
      HRESULT(D3D12_FEATURE_VIDEO FeatureVideo,
              _Inout_updates_bytes_(
                  FeatureSupportDataSize) void* pFeatureSupportData,
              UINT FeatureSupportDataSize));
  MOCK_STDCALL_METHOD3(CreateVideoDecoder,
                       HRESULT(const D3D12_VIDEO_DECODER_DESC* pDesc,
                               REFIID riid,
                               void** ppVideoDecoder));
  MOCK_STDCALL_METHOD3(
      CreateVideoDecoderHeap,
      HRESULT(const D3D12_VIDEO_DECODER_HEAP_DESC* pVideoDecoderHeapDesc,
              REFIID riid,
              void** ppVideoDecoderHeap));
  MOCK_STDCALL_METHOD6(
      CreateVideoProcessor,
      HRESULT(UINT NodeMask,
              const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC* pOutputStreamDesc,
              UINT NumInputStreamDescs,
              const D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC* pInputStreamDescs,
              REFIID riid,
              void** ppVideoProcessor));

  // Interfaces of ID3D12VideoDevice1

  MOCK_STDCALL_METHOD4(
      CreateVideoMotionEstimator,
      HRESULT(const D3D12_VIDEO_MOTION_ESTIMATOR_DESC* pDesc,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoMotionEstimator));

  MOCK_STDCALL_METHOD4(
      CreateVideoMotionVectorHeap,
      HRESULT(const D3D12_VIDEO_MOTION_VECTOR_HEAP_DESC* pDesc,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoMotionVectorHeap));

  // Interfaces of ID3D12VideoDevice2

  MOCK_STDCALL_METHOD4(
      CreateVideoDecoder1,
      HRESULT(const D3D12_VIDEO_DECODER_DESC* pDesc,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoDecoder));

  MOCK_STDCALL_METHOD4(
      CreateVideoDecoderHeap1,
      HRESULT(const D3D12_VIDEO_DECODER_HEAP_DESC* pVideoDecoderHeapDesc,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoDecoderHeap));

  MOCK_STDCALL_METHOD7(
      CreateVideoProcessor1,
      HRESULT(UINT NodeMask,
              const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC* pOutputStreamDesc,
              UINT NumInputStreamDescs,
              const D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC* pInputStreamDescs,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoProcessor));

  MOCK_STDCALL_METHOD6(
      CreateVideoExtensionCommand,
      HRESULT(const D3D12_VIDEO_EXTENSION_COMMAND_DESC* pDesc,
              const void* pCreationParameters,
              SIZE_T CreationParametersDataSizeInBytes,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoExtensionCommand));

  MOCK_STDCALL_METHOD5(ExecuteExtensionCommand,
                       HRESULT(ID3D12VideoExtensionCommand* pExtensionCommand,
                               const void* pExecutionParameters,
                               SIZE_T ExecutionParametersSizeInBytes,
                               void* pOutputData,
                               SIZE_T OutputDataSizeInBytes));

  // Interfaces of ID3D12VideoDevice3

  MOCK_STDCALL_METHOD3(CreateVideoEncoder,
                       HRESULT(const D3D12_VIDEO_ENCODER_DESC* pDesc,
                               REFIID riid,
                               void** ppVideoEncoder));
  MOCK_STDCALL_METHOD3(CreateVideoEncoderHeap,
                       HRESULT(const D3D12_VIDEO_ENCODER_HEAP_DESC* pDesc,
                               REFIID riid,
                               void** ppVideoEncoderHeap));
};

class D3D12VideoProcessorMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D12VideoProcessor> {
 public:
  D3D12VideoProcessorMock();
  ~D3D12VideoProcessorMock() override;

  MOCK_METHOD(HRESULT,
              GetPrivateData,
              (REFGUID, UINT*, void*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              SetPrivateData,
              (REFGUID, UINT, const void*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              SetPrivateDataInterface,
              (REFGUID, const IUnknown*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, SetName, (LPCWSTR), (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetDevice,
              (REFIID, void**),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(UINT, GetNodeMask, (), (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(UINT, GetNumInputStreamDescs, (), (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetInputStreamDescs,
              (UINT, D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC,
              GetOutputStreamDesc,
              (),
              (Calltype(STDMETHODCALLTYPE)));
};

class D3D12VideoProcessCommandListMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D12VideoProcessCommandList> {
 public:
  D3D12VideoProcessCommandListMock();
  ~D3D12VideoProcessCommandListMock() override;

  MOCK_METHOD(HRESULT,
              GetPrivateData,
              (REFGUID, UINT*, void*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              SetPrivateData,
              (REFGUID, UINT, const void*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              SetPrivateDataInterface,
              (REFGUID, const IUnknown*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, SetName, (LPCWSTR), (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetDevice,
              (REFIID, void**),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(D3D12_COMMAND_LIST_TYPE,
              GetType,
              (),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, Close, (), (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              Reset,
              (ID3D12CommandAllocator*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void, ClearState, (), (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void,
              ResourceBarrier,
              (UINT, const D3D12_RESOURCE_BARRIER*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void,
              DiscardResource,
              (ID3D12Resource*, const D3D12_DISCARD_REGION*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void,
              BeginQuery,
              (ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void,
              EndQuery,
              (ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(
      void,
      ResolveQueryData,
      (ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT, UINT, ID3D12Resource*, UINT64),
      (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void,
              SetPredication,
              (ID3D12Resource*, UINT64, D3D12_PREDICATION_OP),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void,
              SetMarker,
              (UINT, const void*, UINT),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void,
              BeginEvent,
              (UINT, const void*, UINT),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void, EndEvent, (), (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void,
              ProcessFrames,
              (ID3D12VideoProcessor*,
               const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS*,
               UINT,
               const D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS*),
              (Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(void,
              WriteBufferImmediate,
              (UINT,
               const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER*,
               const D3D12_WRITEBUFFERIMMEDIATE_MODE*),
              (Calltype(STDMETHODCALLTYPE)));
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_D3D12_VIDEO_MOCKS_H_
