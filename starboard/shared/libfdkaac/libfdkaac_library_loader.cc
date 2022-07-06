// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <dlfcn.h>

#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/shared/libfdkaac/libfdkaac_library_loader.h"

namespace starboard {
namespace shared {
namespace libfdkaac {

namespace {
const char kLibfdkaacLibraryName[] = "libfdk-aac.so";
}

SB_ONCE_INITIALIZE_FUNCTION(LibfdkaacHandle, LibfdkaacHandle::GetHandle);

LibfdkaacHandle::LibfdkaacHandle() {
  LoadLibrary();
}

LibfdkaacHandle::~LibfdkaacHandle() {
  if (handle_) {
    dlclose(handle_);
  }
}

bool LibfdkaacHandle::IsLoaded() const {
  return handle_;
}

void LibfdkaacHandle::ReportSymbolError() {
  SB_LOG(ERROR) << "libfdkaac load error: " << dlerror();
  dlclose(handle_);
  handle_ = NULL;
}

void LibfdkaacHandle::LoadLibrary() {
  SB_DCHECK(!handle_);
  handle_ = dlopen(kLibfdkaacLibraryName, RTLD_LAZY);
  if (!handle_) {
    return;
  }

#define INITSYMBOL(symbol)                                              \
  symbol = reinterpret_cast<decltype(symbol)>(dlsym(handle_, #symbol)); \
  if (!symbol) {                                                        \
    ReportSymbolError();                                                \
    return;                                                             \
  }

  INITSYMBOL(aacDecoder_GetStreamInfo);
  INITSYMBOL(aacDecoder_Close);
  INITSYMBOL(aacDecoder_Open);
  INITSYMBOL(aacDecoder_ConfigRaw);
  INITSYMBOL(aacDecoder_SetParam);
  INITSYMBOL(aacDecoder_AncDataInit);
  INITSYMBOL(aacDecoder_Fill);
  INITSYMBOL(aacDecoder_DecodeFrame);
}

CStreamInfo* (*aacDecoder_GetStreamInfo)(HANDLE_AACDECODER self);

void (*aacDecoder_Close)(HANDLE_AACDECODER self);

HANDLE_AACDECODER(*aacDecoder_Open)
(TRANSPORT_TYPE transportFmt, UINT nrOfLayers);

AAC_DECODER_ERROR(*aacDecoder_ConfigRaw)
(HANDLE_AACDECODER self, UCHAR* conf[], const UINT length[]);

AAC_DECODER_ERROR(*aacDecoder_SetParam)
(const HANDLE_AACDECODER self, const AACDEC_PARAM param, const INT value);

AAC_DECODER_ERROR(*aacDecoder_AncDataInit)
(HANDLE_AACDECODER self, UCHAR* buffer, int size);

AAC_DECODER_ERROR(*aacDecoder_Fill)
(HANDLE_AACDECODER self,
 UCHAR* pBuffer[],
 const UINT bufferSize[],
 UINT* bytesValid);

AAC_DECODER_ERROR(*aacDecoder_DecodeFrame)
(HANDLE_AACDECODER self,
 INT_PCM* pTimeData,
 const INT timeDataSize,
 const UINT flags);

}  // namespace libfdkaac
}  // namespace shared
}  // namespace starboard
