// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/video_capture_device_win.h"

#include <algorithm>
#include <list>

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/win/scoped_variant.h"

using base::win::ScopedComPtr;
using base::win::ScopedVariant;

namespace {

// Finds and creates a DirectShow Video Capture filter matching the device_name.
HRESULT GetDeviceFilter(const media::VideoCaptureDevice::Name& device_name,
                        IBaseFilter** filter) {
  DCHECK(filter);

  ScopedComPtr<ICreateDevEnum> dev_enum;
  HRESULT hr = dev_enum.CreateInstance(CLSID_SystemDeviceEnum, NULL,
                                       CLSCTX_INPROC);
  if (FAILED(hr))
    return hr;

  ScopedComPtr<IEnumMoniker> enum_moniker;
  hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       enum_moniker.Receive(), 0);
  // CreateClassEnumerator returns S_FALSE on some Windows OS
  // when no camera exist. Therefore the FAILED macro can't be used.
  if (hr != S_OK)
    return NULL;

  ScopedComPtr<IMoniker> moniker;
  ScopedComPtr<IBaseFilter> capture_filter;
  DWORD fetched = 0;
  while (enum_moniker->Next(1, moniker.Receive(), &fetched) == S_OK) {
    ScopedComPtr<IPropertyBag> prop_bag;
    hr = moniker->BindToStorage(0, 0, IID_IPropertyBag, prop_bag.ReceiveVoid());
    if (FAILED(hr)) {
      moniker.Release();
      continue;
    }

    // Find the description or friendly name.
    static const wchar_t* kPropertyNames[] = {
      L"DevicePath", L"Description", L"FriendlyName"
    };
    ScopedVariant name;
    for (size_t i = 0;
         i < arraysize(kPropertyNames) && name.type() != VT_BSTR; ++i) {
      prop_bag->Read(kPropertyNames[i], name.Receive(), 0);
    }
    if (name.type() == VT_BSTR) {
      std::string device_path(base::SysWideToUTF8(V_BSTR(&name)));
      if (device_path.compare(device_name.unique_id) == 0) {
        // We have found the requested device
        hr = moniker->BindToObject(0, 0, IID_IBaseFilter,
                                   capture_filter.ReceiveVoid());
        DVPLOG_IF(2, FAILED(hr)) << "Failed to bind camera filter.";
        break;
      }
    }
    moniker.Release();
  }

  *filter = capture_filter.Detach();
  if (!*filter && SUCCEEDED(hr))
    hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

  return hr;
}

// Check if a Pin matches a category.
bool PinMatchesCategory(IPin* pin, REFGUID category) {
  DCHECK(pin);
  bool found = false;
  ScopedComPtr<IKsPropertySet> ks_property;
  HRESULT hr = ks_property.QueryFrom(pin);
  if (SUCCEEDED(hr)) {
    GUID pin_category;
    DWORD return_value;
    hr = ks_property->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0,
                          &pin_category, sizeof(pin_category), &return_value);
    if (SUCCEEDED(hr) && (return_value == sizeof(pin_category))) {
      found = (pin_category == category);
    }
  }
  return found;
}

// Finds a IPin on a IBaseFilter given the direction an category.
HRESULT GetPin(IBaseFilter* filter, PIN_DIRECTION pin_dir, REFGUID category,
               IPin** pin) {
  DCHECK(pin);
  ScopedComPtr<IEnumPins> pin_emum;
  HRESULT hr = filter->EnumPins(pin_emum.Receive());
  if (pin_emum == NULL)
    return hr;

  // Get first unconnected pin.
  hr = pin_emum->Reset();  // set to first pin
  while ((hr = pin_emum->Next(1, pin, NULL)) == S_OK) {
    PIN_DIRECTION this_pin_dir = static_cast<PIN_DIRECTION>(-1);
    hr = (*pin)->QueryDirection(&this_pin_dir);
    if (pin_dir == this_pin_dir) {
      if (category == GUID_NULL || PinMatchesCategory(*pin, category))
        return S_OK;
    }
    (*pin)->Release();
  }

  return E_FAIL;
}

// Release the format block for a media type.
// http://msdn.microsoft.com/en-us/library/dd375432(VS.85).aspx
void FreeMediaType(AM_MEDIA_TYPE* mt) {
  if (mt->cbFormat != 0) {
    CoTaskMemFree(mt->pbFormat);
    mt->cbFormat = 0;
    mt->pbFormat = NULL;
  }
  if (mt->pUnk != NULL) {
    NOTREACHED();
    // pUnk should not be used.
    mt->pUnk->Release();
    mt->pUnk = NULL;
  }
}

// Delete a media type structure that was allocated on the heap.
// http://msdn.microsoft.com/en-us/library/dd375432(VS.85).aspx
void DeleteMediaType(AM_MEDIA_TYPE* mt) {
  if (mt != NULL) {
    FreeMediaType(mt);
    CoTaskMemFree(mt);
  }
}

// Help structure used for comparing video capture capabilities.
struct ResolutionDiff {
  int capability_index;
  int diff_height;
  int diff_width;
  int diff_frame_rate;
  media::VideoCaptureCapability::Format color;
};

bool CompareHeight(const ResolutionDiff& item1, const ResolutionDiff& item2) {
  return abs(item1.diff_height) < abs(item2.diff_height);
}

bool CompareWidth(const ResolutionDiff& item1, const ResolutionDiff& item2) {
  return abs(item1.diff_width) < abs(item2.diff_width);
}

bool CompareFrameRate(const ResolutionDiff& item1,
                      const ResolutionDiff& item2) {
  return abs(item1.diff_frame_rate) < abs(item2.diff_frame_rate);
}

bool CompareColor(const ResolutionDiff& item1, const ResolutionDiff& item2) {
  return (item1.color < item2.color);
}

}  // namespace

namespace media {

// Name of a fake DirectShow filter that exist on computers with
// GTalk installed.
static const char kGoogleCameraAdapter[] = "google camera adapter";

// Gets the names of all video capture devices connected to this computer.
void VideoCaptureDevice::GetDeviceNames(Names* device_names) {
  DCHECK(device_names);

  ScopedComPtr<ICreateDevEnum> dev_enum;
  HRESULT hr = dev_enum.CreateInstance(CLSID_SystemDeviceEnum, NULL,
                                       CLSCTX_INPROC);
  if (FAILED(hr))
    return;

  ScopedComPtr<IEnumMoniker> enum_moniker;
  hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       enum_moniker.Receive(), 0);
  // CreateClassEnumerator returns S_FALSE on some Windows OS
  // when no camera exist. Therefore the FAILED macro can't be used.
  if (hr != S_OK)
    return;

  device_names->clear();

  // Enumerate all video capture devices.
  ScopedComPtr<IMoniker> moniker;
  int index = 0;
  while (enum_moniker->Next(1, moniker.Receive(), NULL) == S_OK) {
    Name device;
    ScopedComPtr<IPropertyBag> prop_bag;
    hr = moniker->BindToStorage(0, 0, IID_IPropertyBag, prop_bag.ReceiveVoid());
    if (FAILED(hr)) {
      moniker.Release();
      continue;
    }

    // Find the description or friendly name.
    ScopedVariant name;
    hr = prop_bag->Read(L"Description", name.Receive(), 0);
    if (FAILED(hr))
      hr = prop_bag->Read(L"FriendlyName", name.Receive(), 0);

    if (SUCCEEDED(hr) && name.type() == VT_BSTR) {
      // Ignore all VFW drivers and the special Google Camera Adapter.
      // Google Camera Adapter is not a real DirectShow camera device.
      // VFW is very old Video for Windows drivers that can not be used.
      const wchar_t* str_ptr = V_BSTR(&name);
      const int name_length = arraysize(kGoogleCameraAdapter) - 1;

      if ((wcsstr(str_ptr, L"(VFW)") == NULL) &&
          lstrlenW(str_ptr) < name_length ||
          (!(LowerCaseEqualsASCII(str_ptr, str_ptr + name_length,
                                  kGoogleCameraAdapter)))) {
        device.device_name = base::SysWideToUTF8(str_ptr);
        name.Reset();
        hr = prop_bag->Read(L"DevicePath", name.Receive(), 0);
        if (FAILED(hr)) {
          device.unique_id = device.device_name;
        } else if (name.type() == VT_BSTR) {
          device.unique_id = base::SysWideToUTF8(V_BSTR(&name));
        }

        device_names->push_back(device);
      }
    }
    moniker.Release();
  }
}

VideoCaptureDevice* VideoCaptureDevice::Create(const Name& device_name) {
  VideoCaptureDeviceWin* self = new VideoCaptureDeviceWin(device_name);
  if (self  && self->Init())
    return self;

  delete self;
  return NULL;
}

VideoCaptureDeviceWin::VideoCaptureDeviceWin(const Name& device_name)
    : device_name_(device_name),
      state_(kIdle),
      observer_(NULL) {
  DetachFromThread();
}

VideoCaptureDeviceWin::~VideoCaptureDeviceWin() {
  DCHECK(CalledOnValidThread());
  if (media_control_)
    media_control_->Stop();

  if (graph_builder_) {
    if (sink_filter_) {
      graph_builder_->RemoveFilter(sink_filter_);
      sink_filter_ = NULL;
    }

    if (capture_filter_)
      graph_builder_->RemoveFilter(capture_filter_);

    if (mjpg_filter_)
      graph_builder_->RemoveFilter(mjpg_filter_);
  }
}

bool VideoCaptureDeviceWin::Init() {
  DCHECK(CalledOnValidThread());
  HRESULT hr = GetDeviceFilter(device_name_, capture_filter_.Receive());
  if (!capture_filter_) {
    DVLOG(2) << "Failed to create capture filter.";
    return false;
  }

  hr = GetPin(capture_filter_, PINDIR_OUTPUT, PIN_CATEGORY_CAPTURE,
              output_capture_pin_.Receive());
  if (!output_capture_pin_) {
    DVLOG(2) << "Failed to get capture output pin";
    return false;
  }

  // Create the sink filter used for receiving Captured frames.
  sink_filter_ = new SinkFilter(this);
  if (sink_filter_ == NULL) {
    DVLOG(2) << "Failed to create send filter";
    return false;
  }

  input_sink_pin_ = sink_filter_->GetPin(0);

  hr = graph_builder_.CreateInstance(CLSID_FilterGraph, NULL,
                                     CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create graph builder.";
    return false;
  }

  hr = graph_builder_.QueryInterface(media_control_.Receive());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create media control builder.";
    return false;
  }

  hr = graph_builder_->AddFilter(capture_filter_, NULL);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to add the capture device to the graph.";
    return false;
  }

  hr = graph_builder_->AddFilter(sink_filter_, NULL);
  if (FAILED(hr)) {
    DVLOG(2)<< "Failed to add the send filter to the graph.";
    return false;
  }

  return CreateCapabilityMap();
}

void VideoCaptureDeviceWin::Allocate(
    int width,
    int height,
    int frame_rate,
    VideoCaptureDevice::EventHandler* observer) {
  DCHECK(CalledOnValidThread());
  if (state_ != kIdle)
    return;

  observer_ = observer;
  // Get the camera capability that best match the requested resolution.
  const int capability_index = GetBestMatchedCapability(width, height,
                                                        frame_rate);
  VideoCaptureCapability capability = capabilities_[capability_index];

  // Reduce the frame rate if the requested frame rate is lower
  // than the capability.
  if (capability.frame_rate > frame_rate)
    capability.frame_rate = frame_rate;

  AM_MEDIA_TYPE* pmt = NULL;
  VIDEO_STREAM_CONFIG_CAPS caps;

  ScopedComPtr<IAMStreamConfig> stream_config;
  HRESULT hr = output_capture_pin_.QueryInterface(stream_config.Receive());
  if (FAILED(hr)) {
    SetErrorState("Can't get the Capture format settings");
    return;
  }

  // Get the windows capability from the capture device.
  hr = stream_config->GetStreamCaps(capability_index, &pmt,
                                    reinterpret_cast<BYTE*>(&caps));
  if (SUCCEEDED(hr)) {
    if (pmt->formattype == FORMAT_VideoInfo) {
      VIDEOINFOHEADER* h = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);
      if (capability.frame_rate > 0)
        h->AvgTimePerFrame = kSecondsToReferenceTime / capability.frame_rate;
    }
    // Set the sink filter to request this capability.
    sink_filter_->SetRequestedMediaCapability(capability);
    // Order the capture device to use this capability.
    hr = stream_config->SetFormat(pmt);
  }

  if (FAILED(hr))
    SetErrorState("Failed to set capture device output format");

  if (capability.color == VideoCaptureCapability::kMJPEG &&
      !mjpg_filter_.get()) {
    // Create MJPG filter if we need it.
    hr = mjpg_filter_.CreateInstance(CLSID_MjpegDec, NULL, CLSCTX_INPROC);

    if (SUCCEEDED(hr)) {
      GetPin(mjpg_filter_, PINDIR_INPUT, GUID_NULL, input_mjpg_pin_.Receive());
      GetPin(mjpg_filter_, PINDIR_OUTPUT, GUID_NULL,
             output_mjpg_pin_.Receive());
      hr = graph_builder_->AddFilter(mjpg_filter_, NULL);
    }

    if (FAILED(hr)) {
      mjpg_filter_.Release();
      input_mjpg_pin_.Release();
      output_mjpg_pin_.Release();
    }
  }

  if (capability.color == VideoCaptureCapability::kMJPEG &&
      mjpg_filter_.get()) {
    // Connect the camera to the MJPEG decoder.
    hr = graph_builder_->ConnectDirect(output_capture_pin_, input_mjpg_pin_,
                                       NULL);
    // Connect the MJPEG filter to the Capture filter.
    hr += graph_builder_->ConnectDirect(output_mjpg_pin_, input_sink_pin_,
                                        NULL);
  } else {
    hr = graph_builder_->ConnectDirect(output_capture_pin_, input_sink_pin_,
                                       NULL);
  }

  if (FAILED(hr)) {
    SetErrorState("Failed to connect the Capture graph.");
    return;
  }

  hr = media_control_->Pause();
  if (FAILED(hr)) {
    SetErrorState("Failed to Pause the Capture device. "
                  "Is it already occupied?");
    return;
  }

  // Get the capability back from the sink filter after the filter have been
  // connected.
  const VideoCaptureCapability& used_capability
      = sink_filter_->ResultingCapability();
  observer_->OnFrameInfo(used_capability);

  state_ = kAllocated;
}

void VideoCaptureDeviceWin::Start() {
  DCHECK(CalledOnValidThread());
  if (state_ != kAllocated)
    return;

  HRESULT hr = media_control_->Run();
  if (FAILED(hr)) {
    SetErrorState("Failed to start the Capture device.");
    return;
  }

  state_ = kCapturing;
}

void VideoCaptureDeviceWin::Stop() {
  DCHECK(CalledOnValidThread());
  if (state_ != kCapturing)
    return;

  HRESULT hr = media_control_->Stop();
  if (FAILED(hr)) {
    SetErrorState("Failed to stop the capture graph.");
    return;
  }

  state_ = kAllocated;
}

void VideoCaptureDeviceWin::DeAllocate() {
  DCHECK(CalledOnValidThread());
  if (state_ == kIdle)
    return;

  HRESULT hr = media_control_->Stop();
  graph_builder_->Disconnect(output_capture_pin_);
  graph_builder_->Disconnect(input_sink_pin_);

  // If the _mjpg filter exist disconnect it even if it has not been used.
  if (mjpg_filter_) {
    graph_builder_->Disconnect(input_mjpg_pin_);
    graph_builder_->Disconnect(output_mjpg_pin_);
  }

  if (FAILED(hr)) {
    SetErrorState("Failed to Stop the Capture device");
    return;
  }

  state_ = kIdle;
}

const VideoCaptureDevice::Name& VideoCaptureDeviceWin::device_name() {
  DCHECK(CalledOnValidThread());
  return device_name_;
}

// Implements SinkFilterObserver::SinkFilterObserver.
void VideoCaptureDeviceWin::FrameReceived(const uint8* buffer,
                                          int length) {
  observer_->OnIncomingCapturedFrame(buffer, length, base::Time::Now());
}

bool VideoCaptureDeviceWin::CreateCapabilityMap() {
  DCHECK(CalledOnValidThread());
  ScopedComPtr<IAMStreamConfig> stream_config;
  HRESULT hr = output_capture_pin_.QueryInterface(stream_config.Receive());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to get IAMStreamConfig interface from "
                "capture device";
    return false;
  }

  // Get interface used for getting the frame rate.
  ScopedComPtr<IAMVideoControl> video_control;
  hr = capture_filter_.QueryInterface(video_control.Receive());
  DVLOG_IF(2, FAILED(hr)) << "IAMVideoControl Interface NOT SUPPORTED";

  AM_MEDIA_TYPE* media_type = NULL;
  VIDEO_STREAM_CONFIG_CAPS caps;
  int count, size;

  hr = stream_config->GetNumberOfCapabilities(&count, &size);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to GetNumberOfCapabilities";
    return false;
  }

  for (int i = 0; i < count; ++i) {
    hr = stream_config->GetStreamCaps(i, &media_type,
                                      reinterpret_cast<BYTE*>(&caps));
    if (FAILED(hr)) {
      DVLOG(2) << "Failed to GetStreamCaps";
      return false;
    }

    if (media_type->majortype == MEDIATYPE_Video &&
        media_type->formattype == FORMAT_VideoInfo) {
      VideoCaptureCapability capability;
      REFERENCE_TIME time_per_frame = 0;

      VIDEOINFOHEADER* h =
          reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
      capability.width = h->bmiHeader.biWidth;
      capability.height = h->bmiHeader.biHeight;
      time_per_frame = h->AvgTimePerFrame;

      // Try to get the max frame rate from IAMVideoControl.
      if (video_control.get()) {
        LONGLONG* max_fps_ptr;
        LONG list_size;
        SIZE size;
        size.cx = capability.width;
        size.cy = capability.height;

        // GetFrameRateList doesn't return max frame rate always
        // eg: Logitech Notebook. This may be due to a bug in that API
        // because GetFrameRateList array is reversed in the above camera. So
        // a util method written. Can't assume the first value will return
        // the max fps.
        hr = video_control->GetFrameRateList(output_capture_pin_, i, size,
                                             &list_size, &max_fps_ptr);

        if (SUCCEEDED(hr) && list_size > 0) {
          int min_time =  *std::min_element(max_fps_ptr,
                                            max_fps_ptr + list_size);
          capability.frame_rate = (min_time > 0) ?
              kSecondsToReferenceTime / min_time : 0;
        } else {
          // Get frame rate from VIDEOINFOHEADER.
          capability.frame_rate = (time_per_frame > 0) ?
              static_cast<int>(kSecondsToReferenceTime / time_per_frame) : 0;
        }
      } else {
        // Get frame rate from VIDEOINFOHEADER since IAMVideoControl is
        // not supported.
        capability.frame_rate = (time_per_frame > 0) ?
            static_cast<int>(kSecondsToReferenceTime / time_per_frame) : 0;
      }

      // We can't switch MEDIATYPE :~(.
      if (media_type->subtype == kMediaSubTypeI420) {
        capability.color = VideoCaptureCapability::kI420;
      } else if (media_type->subtype == MEDIASUBTYPE_IYUV) {
        // This is identical to kI420.
        capability.color = VideoCaptureCapability::kI420;
      } else if (media_type->subtype == MEDIASUBTYPE_RGB24) {
        capability.color = VideoCaptureCapability::kRGB24;
      } else if (media_type->subtype == MEDIASUBTYPE_YUY2) {
        capability.color = VideoCaptureCapability::kYUY2;
      } else if (media_type->subtype == MEDIASUBTYPE_MJPG) {
        capability.color = VideoCaptureCapability::kMJPEG;
      } else {
        WCHAR guid_str[128];
        StringFromGUID2(media_type->subtype, guid_str, arraysize(guid_str));
        DVLOG(2) << "Device support unknown media type " << guid_str;
        continue;
      }
      capabilities_[i] = capability;
    }
    DeleteMediaType(media_type);
    media_type = NULL;
  }

  return capabilities_.size() > 0;
}

// Loops through the list of capabilities and returns an index of the best
// matching capability.
// The algorithm prioritize height, width, frame rate and color format in that
// order.
int VideoCaptureDeviceWin::GetBestMatchedCapability(int requested_width,
                                                    int requested_height,
                                                    int requested_frame_rate) {
  DCHECK(CalledOnValidThread());
  std::list<ResolutionDiff> diff_list;

  // Loop through the candidates to create a list of differentials between the
  // requested resolution and the camera capability.
  for (CapabilityMap::iterator iterator = capabilities_.begin();
       iterator != capabilities_.end();
       ++iterator) {
    VideoCaptureCapability capability = iterator->second;

    ResolutionDiff diff;
    diff.capability_index = iterator->first;
    diff.diff_width = capability.width - requested_width;
    diff.diff_height = capability.height - requested_height;
    diff.diff_frame_rate = capability.frame_rate - requested_frame_rate;
    diff.color = capability.color;
    diff_list.push_back(diff);
  }

  // Sort the best height candidates.
  diff_list.sort(&CompareHeight);
  int best_diff = diff_list.front().diff_height;
  for (std::list<ResolutionDiff>::iterator it = diff_list.begin();
       it != diff_list.end(); ++it) {
    if (it->diff_height != best_diff) {
      // Remove all candidates but the best.
      diff_list.erase(it, diff_list.end());
      break;
    }
  }

  // Sort the best width candidates.
  diff_list.sort(&CompareWidth);
  best_diff = diff_list.front().diff_width;
  for (std::list<ResolutionDiff>::iterator it = diff_list.begin();
       it != diff_list.end(); ++it) {
    if (it->diff_width != best_diff) {
      // Remove all candidates but the best.
      diff_list.erase(it, diff_list.end());
      break;
    }
  }

  // Sort the best frame rate candidates.
  diff_list.sort(&CompareFrameRate);
  best_diff = diff_list.front().diff_frame_rate;
  for (std::list<ResolutionDiff>::iterator it = diff_list.begin();
       it != diff_list.end(); ++it) {
    if (it->diff_frame_rate != best_diff) {
      diff_list.erase(it, diff_list.end());
      break;
    }
  }

  // Decide the best color format.
  diff_list.sort(&CompareColor);
  return diff_list.front().capability_index;
}

void VideoCaptureDeviceWin::SetErrorState(const char* reason) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << reason;
  state_ = kError;
  observer_->OnError();
}

}  // namespace media
