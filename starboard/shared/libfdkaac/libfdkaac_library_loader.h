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

#ifndef STARBOARD_SHARED_LIBFDKAAC_LIBFDKAAC_LIBRARY_LOADER_H_
#define STARBOARD_SHARED_LIBFDKAAC_LIBFDKAAC_LIBRARY_LOADER_H_

#include "starboard/shared/internal_only.h"
#include "third_party/libfdkaac/include/aacdecoder_lib.h"

namespace starboard {
namespace shared {
namespace libfdkaac {

class LibfdkaacHandle {
 public:
  LibfdkaacHandle();

  ~LibfdkaacHandle();

  static LibfdkaacHandle* GetHandle();

  bool IsLoaded() const;

 private:
  void ReportSymbolError();

  void LoadLibrary();

  void* handle_ = NULL;
};

extern CStreamInfo* (*aacDecoder_GetStreamInfo)(HANDLE_AACDECODER self);

extern void (*aacDecoder_Close)(HANDLE_AACDECODER self);

extern HANDLE_AACDECODER (*aacDecoder_Open)(TRANSPORT_TYPE transportFmt,
                                            UINT nrOfLayers);

extern AAC_DECODER_ERROR (*aacDecoder_ConfigRaw)(HANDLE_AACDECODER self,
                                                 UCHAR* conf[],
                                                 const UINT length[]);

extern AAC_DECODER_ERROR (*aacDecoder_SetParam)(const HANDLE_AACDECODER self,
                                                const AACDEC_PARAM param,
                                                const INT value);

extern AAC_DECODER_ERROR (*aacDecoder_AncDataInit)(HANDLE_AACDECODER self,
                                                   UCHAR* buffer,
                                                   int size);

extern AAC_DECODER_ERROR (*aacDecoder_Fill)(HANDLE_AACDECODER self,
                                            UCHAR* pBuffer[],
                                            const UINT bufferSize[],
                                            UINT* bytesValid);

extern AAC_DECODER_ERROR (*aacDecoder_DecodeFrame)(HANDLE_AACDECODER self,
                                                   INT_PCM* pTimeData,
                                                   const INT timeDataSize,
                                                   const UINT flags);

}  // namespace libfdkaac
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBFDKAAC_LIBFDKAAC_LIBRARY_LOADER_H_
