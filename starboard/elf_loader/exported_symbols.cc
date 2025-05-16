// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/elf_loader/exported_symbols.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <malloc.h>
#include <netdb.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/cpu_features.h"
#include "starboard/decode_target.h"
#include "starboard/egl.h"
#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/gles.h"
#include "starboard/log.h"
#include "starboard/microphone.h"
#include "starboard/player.h"
#include "starboard/shared/modular/starboard_layer_posix_directory_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_errno_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_mmap_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_pthread_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_socket_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_stat_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_uio_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_unistd_abi_wrappers.h"
#include "starboard/socket.h"
#include "starboard/speech_synthesis.h"
#include "starboard/storage.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#include "starboard/time_zone.h"
#include "starboard/window.h"

#define REGISTER_SYMBOL(s)                        \
  do {                                            \
    map_[#s] = reinterpret_cast<const void*>(&s); \
  } while (0)

namespace starboard {
namespace elf_loader {

ExportedSymbols::ExportedSymbols() {
  REGISTER_SYMBOL(kSbFileMaxName);
  REGISTER_SYMBOL(kSbFileMaxOpen);
  REGISTER_SYMBOL(kSbFileMaxPath);
  REGISTER_SYMBOL(kSbFileSepChar);
  REGISTER_SYMBOL(kSbFileSepString);
  REGISTER_SYMBOL(kSbHasThreadPrioritySupport);
  REGISTER_SYMBOL(kSbMaxSystemPathCacheDirectorySize);
  REGISTER_SYMBOL(kSbMaxThreadLocalKeys);
  REGISTER_SYMBOL(kSbMaxThreadNameLength);
  REGISTER_SYMBOL(kSbMaxThreads);
  REGISTER_SYMBOL(kSbMediaMaxAudioBitrateInBitsPerSecond);
  REGISTER_SYMBOL(kSbMediaMaxVideoBitrateInBitsPerSecond);
  REGISTER_SYMBOL(kSbMemoryPageSize);
  REGISTER_SYMBOL(kSbNetworkReceiveBufferSize);
  REGISTER_SYMBOL(kSbPathSepChar);
  REGISTER_SYMBOL(kSbPathSepString);
  REGISTER_SYMBOL(kSbCanMapExecutableMemory);
  REGISTER_SYMBOL(kHasPartialAudioFramesSupport);
  REGISTER_SYMBOL(SbAudioSinkCreate);
  REGISTER_SYMBOL(SbAudioSinkDestroy);
  REGISTER_SYMBOL(SbAudioSinkGetMaxChannels);
  REGISTER_SYMBOL(SbAudioSinkGetMinBufferSizeInFrames);
  REGISTER_SYMBOL(SbAudioSinkGetNearestSupportedSampleFrequency);
  REGISTER_SYMBOL(SbAudioSinkIsAudioFrameStorageTypeSupported);
  REGISTER_SYMBOL(SbAudioSinkIsAudioSampleTypeSupported);
  REGISTER_SYMBOL(SbAudioSinkIsValid);
  REGISTER_SYMBOL(SbCPUFeaturesGet);
  REGISTER_SYMBOL(SbDecodeTargetGetInfo);
  REGISTER_SYMBOL(SbDecodeTargetRelease);
  REGISTER_SYMBOL(SbDrmCloseSession);
  REGISTER_SYMBOL(SbDrmCreateSystem);
  REGISTER_SYMBOL(SbDrmDestroySystem);
  REGISTER_SYMBOL(SbDrmGenerateSessionUpdateRequest);
  REGISTER_SYMBOL(SbDrmGetMetrics);
  REGISTER_SYMBOL(SbDrmIsServerCertificateUpdatable);
  REGISTER_SYMBOL(SbDrmUpdateServerCertificate);
  REGISTER_SYMBOL(SbDrmUpdateSession);
  REGISTER_SYMBOL(SbEventCancel);
  REGISTER_SYMBOL(SbEventSchedule);
  REGISTER_SYMBOL(SbFileAtomicReplace);
  REGISTER_SYMBOL(SbGetEglInterface);
  REGISTER_SYMBOL(SbGetGlesInterface);
  REGISTER_SYMBOL(SbLog);
  REGISTER_SYMBOL(SbLogFlush);
  REGISTER_SYMBOL(SbLogFormat);
  REGISTER_SYMBOL(SbLogRaw);
  REGISTER_SYMBOL(SbLogRawDumpStack);
  REGISTER_SYMBOL(SbLogRawFormat);
  REGISTER_SYMBOL(SbMediaCanPlayMimeAndKeySystem);
  REGISTER_SYMBOL(SbMediaGetAudioBufferBudget);
  REGISTER_SYMBOL(SbMediaGetAudioConfiguration);
  REGISTER_SYMBOL(SbMediaGetAudioOutputCount);
  REGISTER_SYMBOL(SbMediaGetBufferAlignment);
  REGISTER_SYMBOL(SbMediaGetBufferAllocationUnit);
  REGISTER_SYMBOL(SbMediaGetBufferGarbageCollectionDurationThreshold);
  REGISTER_SYMBOL(SbMediaGetBufferPadding);
  REGISTER_SYMBOL(SbMediaGetInitialBufferCapacity);
  REGISTER_SYMBOL(SbMediaGetMaxBufferCapacity);
  REGISTER_SYMBOL(SbMediaGetProgressiveBufferBudget);
  REGISTER_SYMBOL(SbMediaGetVideoBufferBudget);
  REGISTER_SYMBOL(SbMediaIsBufferPoolAllocateOnDemand);
  REGISTER_SYMBOL(SbMediaIsBufferUsingMemoryPool);
  REGISTER_SYMBOL(SbMicrophoneClose);
  REGISTER_SYMBOL(SbMicrophoneCreate);
  REGISTER_SYMBOL(SbMicrophoneDestroy);
  REGISTER_SYMBOL(SbMicrophoneGetAvailable);
  REGISTER_SYMBOL(SbMicrophoneIsSampleRateSupported);
  REGISTER_SYMBOL(SbMicrophoneOpen);
  REGISTER_SYMBOL(SbMicrophoneRead);
  REGISTER_SYMBOL(SbPlayerCreate);
  REGISTER_SYMBOL(SbPlayerDestroy);
  REGISTER_SYMBOL(SbPlayerGetAudioConfiguration);
  REGISTER_SYMBOL(SbPlayerGetCurrentFrame);
  REGISTER_SYMBOL(SbPlayerGetInfo);
  REGISTER_SYMBOL(SbPlayerGetMaximumNumberOfSamplesPerWrite);
  REGISTER_SYMBOL(SbPlayerGetPreferredOutputMode);
  REGISTER_SYMBOL(SbPlayerSeek);
  REGISTER_SYMBOL(SbPlayerSetBounds);
  REGISTER_SYMBOL(SbPlayerSetPlaybackRate);
  REGISTER_SYMBOL(SbPlayerSetVolume);
  REGISTER_SYMBOL(SbPlayerWriteEndOfStream);
  REGISTER_SYMBOL(SbPlayerWriteSamples);
  REGISTER_SYMBOL(SbSpeechSynthesisCancel);
  REGISTER_SYMBOL(SbSpeechSynthesisIsSupported);
  REGISTER_SYMBOL(SbSpeechSynthesisSpeak);
  REGISTER_SYMBOL(SbStorageCloseRecord);
  REGISTER_SYMBOL(SbStorageDeleteRecord);
  REGISTER_SYMBOL(SbStorageGetRecordSize);
  REGISTER_SYMBOL(SbStorageOpenRecord);
  REGISTER_SYMBOL(SbStorageReadRecord);
  REGISTER_SYMBOL(SbStorageWriteRecord);
  REGISTER_SYMBOL(SbSystemBreakIntoDebugger);
  REGISTER_SYMBOL(SbSystemClearLastError);
  REGISTER_SYMBOL(SbSystemGetErrorString);
  REGISTER_SYMBOL(SbSystemGetExtension);
  REGISTER_SYMBOL(SbSystemGetLastError);
  REGISTER_SYMBOL(SbSystemGetLocaleId);
  REGISTER_SYMBOL(SbSystemGetNumberOfProcessors);
  REGISTER_SYMBOL(SbSystemGetPath);
  REGISTER_SYMBOL(SbSystemGetProperty);
  REGISTER_SYMBOL(SbSystemGetRandomData);
  REGISTER_SYMBOL(SbSystemGetRandomUInt64);
  REGISTER_SYMBOL(SbSystemGetStack);
  REGISTER_SYMBOL(SbSystemGetTotalCPUMemory);
  REGISTER_SYMBOL(SbSystemGetTotalGPUMemory);
  REGISTER_SYMBOL(SbSystemGetUsedCPUMemory);
  REGISTER_SYMBOL(SbSystemGetUsedGPUMemory);
  REGISTER_SYMBOL(SbSystemHasCapability);
  REGISTER_SYMBOL(SbSystemHideSplashScreen);
  REGISTER_SYMBOL(SbSystemIsDebuggerAttached);
  REGISTER_SYMBOL(SbSystemNetworkIsDisconnected);
  REGISTER_SYMBOL(SbSystemRaisePlatformError);
  REGISTER_SYMBOL(SbSystemRequestBlur);
  REGISTER_SYMBOL(SbSystemRequestConceal);
  REGISTER_SYMBOL(SbSystemRequestFocus);
  REGISTER_SYMBOL(SbSystemRequestFreeze);
  REGISTER_SYMBOL(SbSystemRequestReveal);
  REGISTER_SYMBOL(SbSystemRequestStop);
  REGISTER_SYMBOL(SbSystemSignWithCertificationSecretKey);
  REGISTER_SYMBOL(SbSystemSupportsResume);
  REGISTER_SYMBOL(SbSystemSymbolize);
  REGISTER_SYMBOL(SbThreadContextGetPointer);
  REGISTER_SYMBOL(SbThreadGetId);
  REGISTER_SYMBOL(SbThreadGetPriority);
  REGISTER_SYMBOL(SbThreadSamplerCreate);
  REGISTER_SYMBOL(SbThreadSamplerDestroy);
  REGISTER_SYMBOL(SbThreadSamplerFreeze);
  REGISTER_SYMBOL(SbThreadSamplerIsSupported);
  REGISTER_SYMBOL(SbThreadSamplerThaw);
  REGISTER_SYMBOL(SbThreadSetPriority);
  REGISTER_SYMBOL(SbTimeZoneGetCurrent);
  REGISTER_SYMBOL(SbTimeZoneGetName);
  REGISTER_SYMBOL(SbWindowCreate);
  REGISTER_SYMBOL(SbWindowDestroy);
  REGISTER_SYMBOL(SbWindowGetDiagonalSizeInInches);
  REGISTER_SYMBOL(SbWindowGetPlatformHandle);
  REGISTER_SYMBOL(SbWindowGetSize);
  REGISTER_SYMBOL(SbWindowSetDefaultOptions);

  // POSIX APIs
  REGISTER_SYMBOL(calloc);
  REGISTER_SYMBOL(close);
  REGISTER_SYMBOL(closedir);
  REGISTER_SYMBOL(dup);
  REGISTER_SYMBOL(dup2);
  REGISTER_SYMBOL(fcntl);
  REGISTER_SYMBOL(free);
  REGISTER_SYMBOL(freeifaddrs);
  REGISTER_SYMBOL(fsync);
  REGISTER_SYMBOL(getpeername);
  REGISTER_SYMBOL(getsockname);
  REGISTER_SYMBOL(getsockopt);
  REGISTER_SYMBOL(listen);
  REGISTER_SYMBOL(madvise);
  REGISTER_SYMBOL(malloc);
  REGISTER_SYMBOL(malloc_usable_size);
  REGISTER_SYMBOL(mkdir);
  REGISTER_SYMBOL(mkdtemp);
  REGISTER_SYMBOL(mkostemp);
  REGISTER_SYMBOL(mkstemp);
  REGISTER_SYMBOL(mprotect);
  REGISTER_SYMBOL(msync);
  REGISTER_SYMBOL(munmap);
  REGISTER_SYMBOL(open);
  REGISTER_SYMBOL(opendir);
  REGISTER_SYMBOL(posix_memalign);
  REGISTER_SYMBOL(pread);
  REGISTER_SYMBOL(pwrite);
  REGISTER_SYMBOL(read);
  REGISTER_SYMBOL(realloc);
  REGISTER_SYMBOL(recv);
  REGISTER_SYMBOL(recvfrom);
  REGISTER_SYMBOL(recvmsg);
  REGISTER_SYMBOL(rmdir);
  REGISTER_SYMBOL(sched_yield);
  REGISTER_SYMBOL(send);
  REGISTER_SYMBOL(sendto);
  REGISTER_SYMBOL(socket);
  REGISTER_SYMBOL(snprintf);
  REGISTER_SYMBOL(sprintf);
  REGISTER_SYMBOL(unlink);
  REGISTER_SYMBOL(usleep);
  REGISTER_SYMBOL(vfwprintf);
  REGISTER_SYMBOL(vsnprintf);
  REGISTER_SYMBOL(vsscanf);
  REGISTER_SYMBOL(write);

  // Custom mapped POSIX APIs to compatibility wrappers.
  // These will rely on Starboard-side implementations that properly translate
  // Platform-specific types with musl-based types. These wrappers are defined
  // in //starboard/shared/modular.
  // TODO: b/316603042 - Detect via NPLB and only add the wrapper if needed.
  map_["clock_gettime"] =
      reinterpret_cast<const void*>(&__abi_wrap_clock_gettime);
  if (errno_translation()) {
    map_["__errno_location"] =
        reinterpret_cast<const void*>(__abi_wrap___errno_location);
  } else {
    map_["__errno_location"] = reinterpret_cast<const void*>(__errno_location);
  }
  map_["fstat"] = reinterpret_cast<const void*>(&__abi_wrap_fstat);
  map_["ftruncate"] = reinterpret_cast<const void*>(&__abi_wrap_ftruncate);
  map_["gettimeofday"] =
      reinterpret_cast<const void*>(&__abi_wrap_gettimeofday);
  map_["gmtime_r"] = reinterpret_cast<const void*>(&__abi_wrap_gmtime_r);
  map_["lseek"] = reinterpret_cast<const void*>(&__abi_wrap_lseek);
  map_["mmap"] = reinterpret_cast<const void*>(&__abi_wrap_mmap);

  map_["pthread_attr_init"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_init);
  map_["pthread_attr_destroy"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_destroy);
  map_["pthread_attr_getdetachstate"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_getdetachstate);
  map_["pthread_attr_getschedpolicy"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_getschedpolicy);
  map_["pthread_attr_getscope"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_getscope);
  map_["pthread_attr_getstack"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_getstack);
  map_["pthread_attr_getstacksize"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_getstacksize);
  map_["pthread_attr_setdetachstate"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_setdetachstate);
  map_["pthread_attr_setschedpolicy"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_setschedpolicy);
  map_["pthread_attr_setscope"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_setscope);
  map_["pthread_attr_setstack"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_setstack);
  map_["pthread_attr_setstacksize"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_attr_setstacksize);
  map_["pthread_cond_broadcast"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_cond_broadcast);
  map_["pthread_cond_destroy"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_cond_destroy);
  map_["pthread_cond_init"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_cond_init);
  map_["pthread_cond_signal"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_cond_signal);
  map_["pthread_cond_timedwait"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_cond_timedwait);
  map_["pthread_cond_wait"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_cond_wait);
  map_["pthread_condattr_destroy"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_condattr_destroy);
  map_["pthread_condattr_getclock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_condattr_getclock);
  map_["pthread_condattr_init"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_condattr_init);
  map_["pthread_condattr_setclock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_condattr_setclock);
  map_["pthread_create"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_create);
  map_["pthread_detach"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_detach);
  map_["pthread_equal"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_equal);
  map_["pthread_getname_np"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_getname_np);
  map_["pthread_getattr_np"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_getattr_np);
  map_["pthread_getschedparam"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_getschedparam);
  map_["pthread_getspecific"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_getspecific);
  map_["pthread_join"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_join);
  map_["pthread_key_create"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_key_create);
  map_["pthread_key_delete"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_key_delete);
  map_["pthread_kill"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_kill);
  map_["pthread_mutex_destroy"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutex_destroy);
  map_["pthread_mutex_init"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutex_init);
  map_["pthread_mutex_lock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutex_lock);
  map_["pthread_mutex_unlock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutex_unlock);
  map_["pthread_mutex_trylock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutex_trylock);
  map_["pthread_mutexattr_destroy"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutexattr_destroy);
  map_["pthread_mutexattr_gettype"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutexattr_gettype);
  map_["pthread_mutexattr_getpshared"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutexattr_getpshared);
  map_["pthread_mutexattr_init"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutexattr_init);
  map_["pthread_mutexattr_settype"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutexattr_settype);
  map_["pthread_mutexattr_setpshared"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_mutexattr_setpshared);
  map_["pthread_once"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_once);
  map_["pthread_rwlock_destroy"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_rwlock_destroy);
  map_["pthread_rwlock_init"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_rwlock_init);
  map_["pthread_rwlock_rdlock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_rwlock_rdlock);
  map_["pthread_rwlock_tryrdlock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_rwlock_tryrdlock);
  map_["pthread_rwlock_trywrlock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_rwlock_trywrlock);
  map_["pthread_rwlock_unlock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_rwlock_unlock);
  map_["pthread_rwlock_wrlock"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_rwlock_wrlock);
  map_["pthread_self"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_self);
  map_["pthread_setspecific"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_setspecific);
  map_["pthread_setname_np"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_setname_np);
  map_["pthread_setschedparam"] =
      reinterpret_cast<const void*>(&__abi_wrap_pthread_setschedparam);
  map_["readdir_r"] = reinterpret_cast<const void*>(&__abi_wrap_readdir_r);
  map_["stat"] = reinterpret_cast<const void*>(&__abi_wrap_stat);
  map_["time"] = reinterpret_cast<const void*>(&__abi_wrap_time);
  map_["accept"] = reinterpret_cast<const void*>(&__abi_wrap_accept);
  map_["bind"] = reinterpret_cast<const void*>(&__abi_wrap_bind);
  map_["connect"] = reinterpret_cast<const void*>(&__abi_wrap_connect);
  map_["getaddrinfo"] = reinterpret_cast<const void*>(&__abi_wrap_getaddrinfo);
  map_["freeaddrinfo"] =
      reinterpret_cast<const void*>(&__abi_wrap_freeaddrinfo);
  map_["getifaddrs"] = reinterpret_cast<const void*>(&__abi_wrap_getifaddrs);
  map_["setsockopt"] = reinterpret_cast<const void*>(&__abi_wrap_setsockopt);
  map_["shutdown"] = reinterpret_cast<const void*>(&__abi_wrap_shutdown);
  map_["writev"] = reinterpret_cast<const void*>(&__abi_wrap_writev);

  REGISTER_SYMBOL(vswprintf);

}  // NOLINT

const void* ExportedSymbols::Lookup(const char* name) {
  const void* address = map_[name];
  // Any symbol that is not registered as part of the Starboard API in the
  // constructor of this class is a leak, and is an error.
  if (!address) {
    SB_LOG(ERROR) << "Failed to retrieve the address of '" << name << "'.";
  }
  return address;
}

}  // namespace elf_loader
}  // namespace starboard
