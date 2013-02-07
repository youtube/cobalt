// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/video_capture_device_mf_win.h"

#include <mfapi.h>
#include <mferror.h>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/video/capture/win/capability_list_win.h"

using base::win::ScopedCoMem;
using base::win::ScopedComPtr;

namespace media {
namespace {

class MFInitializerSingleton {
 public:
  MFInitializerSingleton() { MFStartup(MF_VERSION, MFSTARTUP_LITE); }
  ~MFInitializerSingleton() { MFShutdown(); }
};

static base::LazyInstance<MFInitializerSingleton> g_mf_initialize =
    LAZY_INSTANCE_INITIALIZER;

void EnsureMFInit() {
  g_mf_initialize.Get();
}

bool PrepareVideoCaptureAttributes(IMFAttributes** attributes, int count) {
  EnsureMFInit();

  if (FAILED(MFCreateAttributes(attributes, count)))
    return false;

  return SUCCEEDED((*attributes)->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
}

bool EnumerateVideoDevices(IMFActivate*** devices,
                           UINT32* count) {
  ScopedComPtr<IMFAttributes> attributes;
  if (!PrepareVideoCaptureAttributes(attributes.Receive(), 1))
    return false;

  return SUCCEEDED(MFEnumDeviceSources(attributes, devices, count));
}

bool CreateVideoCaptureDevice(const char* sym_link, IMFMediaSource** source) {
  ScopedComPtr<IMFAttributes> attributes;
  if (!PrepareVideoCaptureAttributes(attributes.Receive(), 2))
    return false;

  attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                        base::SysUTF8ToWide(sym_link).c_str());

  return SUCCEEDED(MFCreateDeviceSource(attributes, source));
}

bool FormatFromGuid(const GUID& guid, VideoCaptureCapability::Format* format) {
  struct {
    const GUID& guid;
    const VideoCaptureCapability::Format format;
  } static const kFormatMap[] = {
    { MFVideoFormat_I420, VideoCaptureCapability::kI420 },
    { MFVideoFormat_YUY2, VideoCaptureCapability::kYUY2 },
    { MFVideoFormat_UYVY, VideoCaptureCapability::kUYVY },
    { MFVideoFormat_RGB24, VideoCaptureCapability::kRGB24 },
    { MFVideoFormat_ARGB32, VideoCaptureCapability::kARGB },
    { MFVideoFormat_MJPG, VideoCaptureCapability::kMJPEG },
    { MFVideoFormat_YV12, VideoCaptureCapability::kYV12 },
  };

  for (int i = 0; i < arraysize(kFormatMap); ++i) {
    if (kFormatMap[i].guid == guid) {
      *format = kFormatMap[i].format;
      return true;
    }
  }

  return false;
}

bool GetFrameSize(IMFMediaType* type, int* width, int* height) {
  UINT32 width32, height32;
  if (FAILED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width32, &height32)))
    return false;
  *width = width32;
  *height = height32;
  return true;
}

bool GetFrameRate(IMFMediaType* type, int* frame_rate) {
  UINT32 numerator, denominator;
  if (FAILED(MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator,
                                 &denominator))) {
    return false;
  }

  *frame_rate = denominator ? numerator / denominator : 0;

  return true;
}

bool FillCapabilitiesFromType(IMFMediaType* type,
                              VideoCaptureCapability* capability) {
  GUID type_guid;
  if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &type_guid)) ||
      !FormatFromGuid(type_guid, &capability->color) ||
      !GetFrameSize(type, &capability->width, &capability->height) ||
      !GetFrameRate(type, &capability->frame_rate)) {
    return false;
  }

  capability->expected_capture_delay = 0;  // Currently not used.
  capability->interlaced = false;  // Currently not used.

  return true;
}

HRESULT FillCapabilities(IMFSourceReader* source,
                         CapabilityList* capabilities) {
  DWORD stream_index = 0;
  ScopedComPtr<IMFMediaType> type;
  HRESULT hr;
  while (SUCCEEDED(hr = source->GetNativeMediaType(
      MF_SOURCE_READER_FIRST_VIDEO_STREAM, stream_index, type.Receive()))) {
    VideoCaptureCapabilityWin capability(stream_index++);
    if (FillCapabilitiesFromType(type, &capability))
      capabilities->Add(capability);
    type.Release();
  }

  if (SUCCEEDED(hr) && capabilities->empty())
    hr = HRESULT_FROM_WIN32(ERROR_EMPTY);

  return (hr == MF_E_NO_MORE_TYPES) ? S_OK : hr;
}

bool LoadMediaFoundationDlls() {
  static const wchar_t* const kMfDLLs[] = {
    L"%WINDIR%\\system32\\mf.dll",
    L"%WINDIR%\\system32\\mfplat.dll",
    L"%WINDIR%\\system32\\mfreadwrite.dll",
  };

  for (int i = 0; i < arraysize(kMfDLLs); ++i) {
    wchar_t path[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(kMfDLLs[i], path, arraysize(path));
    if (!LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
      return false;
  }

  return true;
}

}  // namespace

class MFReaderCallback
    : public base::RefCountedThreadSafe<MFReaderCallback>,
      public IMFSourceReaderCallback {
 public:
  MFReaderCallback(VideoCaptureDeviceMFWin* observer)
      : observer_(observer), wait_event_(NULL) {
  }

  void SetSignalOnFlush(base::WaitableEvent* event) {
    wait_event_ = event;
  }

  STDMETHOD(QueryInterface)(REFIID riid, void** object) {
    if (riid != IID_IUnknown && riid != IID_IMFSourceReaderCallback)
      return E_NOINTERFACE;
    *object = static_cast<IMFSourceReaderCallback*>(this);
    AddRef();
    return S_OK;
  }

  STDMETHOD_(ULONG, AddRef)() {
    base::RefCountedThreadSafe<MFReaderCallback>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() {
    base::RefCountedThreadSafe<MFReaderCallback>::Release();
    return 1U;
  }

  STDMETHOD(OnReadSample)(HRESULT status, DWORD stream_index,
      DWORD stream_flags, LONGLONG time_stamp, IMFSample* sample) {
    base::Time stamp(base::Time::Now());
    if (!sample) {
      observer_->OnIncomingCapturedFrame(NULL, 0, stamp);
      return S_OK;
    }

    DWORD count = 0;
    sample->GetBufferCount(&count);

    for (DWORD i = 0; i < count; ++i) {
      ScopedComPtr<IMFMediaBuffer> buffer;
      sample->GetBufferByIndex(i, buffer.Receive());
      if (buffer) {
        DWORD length = 0, max_length = 0;
        BYTE* data = NULL;
        buffer->Lock(&data, &max_length, &length);
        observer_->OnIncomingCapturedFrame(data, length, stamp);
        buffer->Unlock();
      }
    }
    return S_OK;
  }

  STDMETHOD(OnFlush)(DWORD stream_index) {
    if (wait_event_) {
      wait_event_->Signal();
      wait_event_ = NULL;
    }
    return S_OK;
  }

  STDMETHOD(OnEvent)(DWORD stream_index, IMFMediaEvent* event) {
    NOTIMPLEMENTED();
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<MFReaderCallback>;
  ~MFReaderCallback() {}

  VideoCaptureDeviceMFWin* observer_;
  base::WaitableEvent* wait_event_;
};

// static
bool VideoCaptureDeviceMFWin::PlatformSupported() {
  // Even though the DLLs might be available on Vista, we get crashes
  // when running our tests on the build bots.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  static bool g_dlls_available = LoadMediaFoundationDlls();
  return g_dlls_available;
}

// static
void VideoCaptureDeviceMFWin::GetDeviceNames(Names* device_names) {
  ScopedCoMem<IMFActivate*> devices;
  UINT32 count;
  if (!EnumerateVideoDevices(&devices, &count))
    return;

  HRESULT hr;
  for (UINT32 i = 0; i < count; ++i) {
    UINT32 name_size, id_size;
    ScopedCoMem<wchar_t> name, id;
    if (SUCCEEDED(hr = devices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &name_size)) &&
        SUCCEEDED(hr = devices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &id,
            &id_size))) {
      std::wstring name_w(name, name_size), id_w(id, id_size);
      Name device;
      device.device_name = base::SysWideToUTF8(name_w);
      device.unique_id = base::SysWideToUTF8(id_w);
      device_names->push_back(device);
    } else {
      DLOG(WARNING) << "GetAllocatedString failed: " << std::hex << hr;
    }
    devices[i]->Release();
  }
}

VideoCaptureDeviceMFWin::VideoCaptureDeviceMFWin(const Name& device_name)
    : name_(device_name), observer_(NULL), capture_(0) {
  DetachFromThread();
}

VideoCaptureDeviceMFWin::~VideoCaptureDeviceMFWin() {
  DCHECK(CalledOnValidThread());
}

bool VideoCaptureDeviceMFWin::Init() {
  DCHECK(CalledOnValidThread());
  DCHECK(!reader_);

  ScopedComPtr<IMFMediaSource> source;
  if (!CreateVideoCaptureDevice(name_.unique_id.c_str(), source.Receive()))
    return false;

  ScopedComPtr<IMFAttributes> attributes;
  MFCreateAttributes(attributes.Receive(), 1);
  DCHECK(attributes);

  callback_ = new MFReaderCallback(this);
  attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, callback_.get());

  return SUCCEEDED(MFCreateSourceReaderFromMediaSource(source, attributes,
                                                       reader_.Receive()));
}

void VideoCaptureDeviceMFWin::Allocate(
    int width,
    int height,
    int frame_rate,
    VideoCaptureDevice::EventHandler* observer) {
  DCHECK(CalledOnValidThread());

  base::AutoLock lock(lock_);

  if (observer_) {
    DCHECK_EQ(observer, observer_);
    return;
  }

  observer_ = observer;
  DCHECK_EQ(capture_, false);

  CapabilityList capabilities;
  HRESULT hr = S_OK;
  if (!reader_ || FAILED(hr = FillCapabilities(reader_, &capabilities))) {
    OnError(hr);
    return;
  }

  const VideoCaptureCapabilityWin& found_capability =
      capabilities.GetBestMatchedCapability(width, height, frame_rate);

  ScopedComPtr<IMFMediaType> type;
  if (FAILED(hr = reader_->GetNativeMediaType(
          MF_SOURCE_READER_FIRST_VIDEO_STREAM, found_capability.stream_index,
          type.Receive())) ||
      FAILED(hr = reader_->SetCurrentMediaType(
          MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type))) {
    OnError(hr);
    return;
  }

  observer_->OnFrameInfo(found_capability);
}

void VideoCaptureDeviceMFWin::Start() {
  DCHECK(CalledOnValidThread());

  base::AutoLock lock(lock_);
  if (!capture_) {
    capture_ = true;
    HRESULT hr;
    if (FAILED(hr = reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                        NULL, NULL, NULL, NULL))) {
      OnError(hr);
      capture_ = false;
    }
  }
}

void VideoCaptureDeviceMFWin::Stop() {
  DCHECK(CalledOnValidThread());
  base::WaitableEvent flushed(false, false);
  bool wait = false;
  {
    base::AutoLock lock(lock_);
    if (capture_) {
      capture_ = false;
      callback_->SetSignalOnFlush(&flushed);
      HRESULT hr = reader_->Flush(MF_SOURCE_READER_ALL_STREAMS);
      wait = SUCCEEDED(hr);
      if (!wait) {
        callback_->SetSignalOnFlush(NULL);
        OnError(hr);
      }
    }
  }

  if (wait)
    flushed.Wait();
}

void VideoCaptureDeviceMFWin::DeAllocate() {
  DCHECK(CalledOnValidThread());

  Stop();

  base::AutoLock lock(lock_);
  observer_ = NULL;
}

const VideoCaptureDevice::Name& VideoCaptureDeviceMFWin::device_name() {
  DCHECK(CalledOnValidThread());
  return name_;
}

void VideoCaptureDeviceMFWin::OnIncomingCapturedFrame(
    const uint8* data,
    int length,
    const base::Time& time_stamp) {
  base::AutoLock lock(lock_);
  if (data && observer_)
    observer_->OnIncomingCapturedFrame(data, length, time_stamp);

  if (capture_) {
    HRESULT hr = reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                     NULL, NULL, NULL, NULL);
    if (FAILED(hr)) {
      // If running the *VideoCap* unit tests on repeat, this can sometimes
      // fail with HRESULT_FROM_WINHRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION).
      // It's not clear to me why this is, but it is possible that it has
      // something to do with this bug:
      // http://support.microsoft.com/kb/979567
      OnError(hr);
    }
  }
}

void VideoCaptureDeviceMFWin::OnError(HRESULT hr) {
  DLOG(ERROR) << "VideoCaptureDeviceMFWin: " << std::hex << hr;
  if (observer_)
    observer_->OnError();
}

}  // namespace media
