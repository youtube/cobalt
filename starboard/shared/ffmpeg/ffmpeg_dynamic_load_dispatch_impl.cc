// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

// This file implements the FFMPEGDispatch interface with dynamic loading of
// the libraries.

#include <dlfcn.h>

#include <map>

#include "starboard/common/log.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/common/string.h"
#include "starboard/once.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/shared/starboard/lazy_initialization_internal.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {

const char kAVCodecLibraryName[] = "libavcodec.so";
const char kAVFormatLibraryName[] = "libavformat.so";
const char kAVUtilLibraryName[] = "libavutil.so";

SbOnceControl g_dynamic_load_once = SB_ONCE_INITIALIZER;

struct LibraryMajorVersions {
  int avcodec;
  int avformat;
  int avutil;
  LibraryMajorVersions(int avcodec, int avformat, int avutil)
      : avcodec(avcodec), avformat(avformat), avutil(avutil) {}
};

// Map containing the major version for all combinations of libraries in
// supported ffmpeg installations. The index is the specialization version
// matching the template parameter for the available specialization.
typedef std::map<int, LibraryMajorVersions> LibraryMajorVersionsMap;

class FFMPEGDispatchImpl {
 public:
  FFMPEGDispatchImpl();
  ~FFMPEGDispatchImpl();

  static FFMPEGDispatchImpl* GetInstance();

  bool RegisterSpecialization(int specialization,
                              int avcodec,
                              int avformat,
                              int avutil);

  bool is_valid() const { return avcodec_ && avformat_ && avutil_; }

  FFMPEGDispatch* get_ffmpeg_dispatch();

 private:
  SbMutex mutex_;
  FFMPEGDispatch* ffmpeg_;
  // Load the ffmpeg shared libraries, return true if successful.
  bool OpenLibraries();
  // Load the symbols from the shared libraries.
  void LoadSymbols();

  void* avcodec_;
  void* avformat_;
  void* avutil_;
  LibraryMajorVersionsMap versions_;
};

FFMPEGDispatchImpl* g_ffmpeg_dispatch_impl;

void construct_ffmpeg_dispatch_impl() {
  g_ffmpeg_dispatch_impl = new FFMPEGDispatchImpl();
}

FFMPEGDispatchImpl::FFMPEGDispatchImpl()
    : mutex_(SB_MUTEX_INITIALIZER),
      ffmpeg_(NULL),
      avcodec_(NULL),
      avformat_(NULL),
      avutil_(NULL) {}

FFMPEGDispatchImpl::~FFMPEGDispatchImpl() {
  delete ffmpeg_;
  if (avformat_) {
    dlclose(avformat_);
  }
  if (avcodec_) {
    dlclose(avcodec_);
  }
  if (avutil_) {
    dlclose(avutil_);
  }
}

// static
FFMPEGDispatchImpl* FFMPEGDispatchImpl::GetInstance() {
  bool initialized =
      SbOnce(&g_dynamic_load_once, construct_ffmpeg_dispatch_impl);
  SB_DCHECK(initialized);
  return g_ffmpeg_dispatch_impl;
}

bool FFMPEGDispatchImpl::RegisterSpecialization(int specialization,
                                                int avcodec,
                                                int avformat,
                                                int avutil) {
  SbMutexAcquire(&mutex_);
  auto result = versions_.insert(std::make_pair(
      specialization, LibraryMajorVersions(avcodec, avformat, avutil)));
  bool success = result.second;
  if (!success) {
    // Element was not inserted because an entry with the same key already
    // exists. Registration is still successful if the parameters are the same.
    const LibraryMajorVersions& existing_versions = result.first->second;
    success = existing_versions.avcodec == avcodec &&
              existing_versions.avformat == avformat &&
              existing_versions.avutil == avutil;
  }
  SbMutexRelease(&mutex_);
  return success;
}

FFMPEGDispatch* FFMPEGDispatchImpl::get_ffmpeg_dispatch() {
  SbMutexAcquire(&mutex_);
  if (!ffmpeg_) {
    ffmpeg_ = new FFMPEGDispatch();
    // Dynamically load the libraries and retrieve the function pointers.
    if (OpenLibraries()) {
      LoadSymbols();
      ffmpeg_->av_register_all();
    }
  }
  SbMutexRelease(&mutex_);
  return ffmpeg_;
}

const int kMaxVersionedLibraryNameLength = 32;

std::string GetVersionedLibraryName(const char* name, const int version) {
  char s[kMaxVersionedLibraryNameLength];
  SbStringFormatF(s, kMaxVersionedLibraryNameLength, "%s.%d", name, version);
  return std::string(s);
}

bool FFMPEGDispatchImpl::OpenLibraries() {
  // Helper lambda to ensure all av libraries are closed and reset.
  const auto reset_av_libraries = [this]() {
    if (avformat_) {
      dlclose(avformat_);
      avformat_ = NULL;
    }
    if (avcodec_) {
      dlclose(avcodec_);
      avcodec_ = NULL;
    }
    if (avutil_) {
      dlclose(avutil_);
      avutil_ = NULL;
    }
    SB_DCHECK(!is_valid());
  };

  for (auto version_iterator = versions_.rbegin();
       version_iterator != versions_.rend(); ++version_iterator) {
    LibraryMajorVersions& versions = version_iterator->second;
    std::string library_file =
        GetVersionedLibraryName(kAVUtilLibraryName, versions.avutil);
    avutil_ = dlopen(library_file.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!avutil_) {
      SB_DLOG(WARNING) << "Unable to open shared library " << library_file;
      reset_av_libraries();
      continue;
    }

    library_file =
        GetVersionedLibraryName(kAVCodecLibraryName, versions.avcodec);
    avcodec_ = dlopen(library_file.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!avcodec_) {
      SB_DLOG(WARNING) << "Unable to open shared library " << library_file;
      reset_av_libraries();
      continue;
    }

    library_file =
        GetVersionedLibraryName(kAVFormatLibraryName, versions.avformat);
    avformat_ = dlopen(library_file.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!avformat_) {
      SB_DLOG(WARNING) << "Unable to open shared library " << library_file;
      reset_av_libraries();
      continue;
    }
    SB_DCHECK(is_valid());
    break;
  }

  if (is_valid()) {
    return true;
  }

  SB_LOG(WARNING) << "Failed to load FFMPEG libs with specified versions. "
                  << "Trying to load FFMPEG libs without version numbers.";
  // Attempt to load the libraries without a version number.
  // This allows loading of the libraries on machines where a versioned and
  // supported library is not available. Additionally, if this results in a
  // library version being loaded that is not supported, then the decoder
  // instantiation can output a more informative log message.
  avutil_ = dlopen(kAVUtilLibraryName, RTLD_NOW | RTLD_GLOBAL);
  if (!avutil_) {
    SB_DLOG(WARNING) << "Unable to open shared library " << kAVUtilLibraryName;
    reset_av_libraries();
    return false;
  }

  avcodec_ = dlopen(kAVCodecLibraryName, RTLD_NOW | RTLD_GLOBAL);
  if (!avcodec_) {
    SB_DLOG(WARNING) << "Unable to open shared library " << kAVCodecLibraryName;
    reset_av_libraries();
    return false;
  }

  avformat_ = dlopen(kAVFormatLibraryName, RTLD_NOW | RTLD_GLOBAL);
  if (!avformat_) {
    SB_DLOG(WARNING) << "Unable to open shared library "
                     << kAVFormatLibraryName;
    reset_av_libraries();
    return false;
  }

  SB_DCHECK(is_valid());
  return true;
}

void FFMPEGDispatchImpl::LoadSymbols() {
  SB_DCHECK(is_valid());
  // Load the desired symbols from the shared libraries. Note: If a symbol is
  // listed as a '.text' entry in the output of 'objdump -T' on the shared
  // library file, then it is directly available from it.
  char* errstr;
  errstr = dlerror();
  if (errstr != NULL) {
    SB_LOG(INFO) << "LoadSymbols - checking dlerror shows:" << errstr;
  }

#define INITSYMBOL(library, symbol)                                     \
  ffmpeg_->symbol = reinterpret_cast<decltype(FFMPEGDispatch::symbol)>( \
      dlsym(library, #symbol));                                         \
  errstr = dlerror();                                                   \
  if (errstr != NULL) {                                                 \
    SB_LOG(INFO) << "Load Symbols ran into error:" << errstr;           \
  }

  // Load symbols from the avutil shared library.
  INITSYMBOL(avutil_, avutil_version);
  SB_DCHECK(ffmpeg_->avutil_version);
  SB_LOG(INFO) << "Opened avutil  - version is:" << ffmpeg_->avutil_version()
               << std::endl;
  INITSYMBOL(avutil_, av_malloc);
  INITSYMBOL(avutil_, av_freep);
  INITSYMBOL(avutil_, av_free);
  INITSYMBOL(avutil_, av_rescale_rnd);
  INITSYMBOL(avutil_, av_samples_get_buffer_size);
  INITSYMBOL(avutil_, av_opt_set_int);
  INITSYMBOL(avutil_, av_image_check_size);
  if (ffmpeg_->avutil_version() > kAVUtilSupportsBufferCreate) {
    INITSYMBOL(avutil_, av_buffer_create);
  }

  // Load symbols from the avcodec shared library.
  INITSYMBOL(avcodec_, avcodec_version);
  SB_DCHECK(ffmpeg_->avcodec_version);
  SB_LOG(INFO) << "Opened libavcodec - version is:"
               << ffmpeg_->avcodec_version() << std::endl;

  if (ffmpeg_->avcodec_version() > kAVCodecSupportsAvFrameAlloc) {
    INITSYMBOL(avcodec_, av_frame_alloc);
    INITSYMBOL(avcodec_, av_frame_unref);
    INITSYMBOL(avcodec_, av_frame_free);
    INITSYMBOL(avcodec_, av_dict_get);
  } else {
    INITSYMBOL(avcodec_, avcodec_alloc_frame);
    INITSYMBOL(avcodec_, avcodec_get_frame_defaults);
  }

  INITSYMBOL(avcodec_, avcodec_alloc_context3);
  if (ffmpeg_->avcodec_version() > kAVCodecSupportsAvcodecFreeContext) {
    INITSYMBOL(avcodec_, avcodec_free_context);
  }
  INITSYMBOL(avcodec_, avcodec_find_decoder);
  INITSYMBOL(avcodec_, avcodec_close);
  INITSYMBOL(avcodec_, avcodec_open2);
  INITSYMBOL(avcodec_, av_init_packet);
  INITSYMBOL(avcodec_, avcodec_decode_audio4);
  INITSYMBOL(avcodec_, avcodec_decode_video2);
  INITSYMBOL(avcodec_, avcodec_flush_buffers);
  INITSYMBOL(avcodec_, avcodec_align_dimensions2);

  if (ffmpeg_->avcodec_version() > kAVCodecSupportsAvPacketAlloc) {
    INITSYMBOL(avcodec_, av_packet_alloc);
    INITSYMBOL(avcodec_, av_packet_free);
    INITSYMBOL(avcodec_, av_packet_unref);
    INITSYMBOL(avcodec_, avcodec_parameters_to_context);
  } else {
    INITSYMBOL(avcodec_, av_free_packet);
  }

  // Load symbols from the avformat shared library.
  INITSYMBOL(avformat_, avformat_version);
  SB_DCHECK(ffmpeg_->avformat_version);
  SB_LOG(INFO) << "Opened libavformat - version is:"
               << ffmpeg_->avformat_version() << std::endl;
  INITSYMBOL(avformat_, av_register_all);
  SB_DCHECK(ffmpeg_->av_register_all);
  INITSYMBOL(avformat_, av_read_frame);
  INITSYMBOL(avformat_, av_seek_frame);
  INITSYMBOL(avformat_, avformat_open_input);
  INITSYMBOL(avformat_, avformat_close_input);
  INITSYMBOL(avformat_, avformat_alloc_context);
  INITSYMBOL(avformat_, avformat_find_stream_info);
  INITSYMBOL(avformat_, avio_alloc_context);

#undef INITSYMBOL
}
}  // namespace

FFMPEGDispatch::FFMPEGDispatch() {}
FFMPEGDispatch::~FFMPEGDispatch() {}

// static
FFMPEGDispatch* FFMPEGDispatch::GetInstance() {
  return FFMPEGDispatchImpl::GetInstance()->get_ffmpeg_dispatch();
}

// static
bool FFMPEGDispatch::RegisterSpecialization(int specialization,
                                            int avcodec,
                                            int avformat,
                                            int avutil) {
  return FFMPEGDispatchImpl::GetInstance()->RegisterSpecialization(
      specialization, avcodec, avformat, avutil);
}

bool FFMPEGDispatch::is_valid() const {
  return FFMPEGDispatchImpl::GetInstance()->is_valid();
}

int FFMPEGDispatch::specialization_version() const {
  // Return the specialization version, which is the major version of the
  // avcodec library plus a single digit indicating if it's ffmpeg or libav,
  // derived from the libavcodec micro version number.
  return (avcodec_version() >> 16) * 10 + ((avcodec_version() & 0xFF) >= 100);
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
