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
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/decode_target.h"
#include "starboard/egl.h"
#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/gles.h"
#include "starboard/log.h"
#include "starboard/microphone.h"
#include "starboard/player.h"
#include "starboard/shared/modular/starboard_layer_posix_auxv_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_directory_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_errno_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_eventfd_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_fcntl_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_mmap_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_pipe2_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_poll_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_prctl_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_pthread_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_resource_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_sched_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_semaphore_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_signal_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_socket_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_socketpair_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_stat_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_statvfs_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_uio_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_uname_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_unistd_abi_wrappers.h"
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

#define REGISTER_WRAPPER(s)                                    \
  do {                                                         \
    map_[#s] = reinterpret_cast<const void*>(&__abi_wrap_##s); \
  } while (0)

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
  REGISTER_SYMBOL(SbTimeZoneGetName);
  REGISTER_SYMBOL(SbWindowCreate);
  REGISTER_SYMBOL(SbWindowDestroy);
  REGISTER_SYMBOL(SbWindowGetDiagonalSizeInInches);
  REGISTER_SYMBOL(SbWindowGetPlatformHandle);
  REGISTER_SYMBOL(SbWindowGetSize);
  REGISTER_SYMBOL(SbWindowSetDefaultOptions);

  // POSIX APIs
  REGISTER_SYMBOL(aligned_alloc);
  REGISTER_SYMBOL(calloc);
  REGISTER_SYMBOL(close);
  REGISTER_SYMBOL(fdatasync);
  REGISTER_SYMBOL(dup);
  REGISTER_SYMBOL(dup2);
  REGISTER_SYMBOL(epoll_create);
  REGISTER_SYMBOL(epoll_create1);
  REGISTER_SYMBOL(epoll_ctl);
  REGISTER_SYMBOL(epoll_wait);
  REGISTER_SYMBOL(free);
  REGISTER_SYMBOL(freeifaddrs);
  REGISTER_SYMBOL(fsync);
  REGISTER_SYMBOL(getcwd);
  REGISTER_SYMBOL(getpeername);
  REGISTER_SYMBOL(getsockname);
  REGISTER_SYMBOL(getsockopt);
  REGISTER_SYMBOL(isatty);
  REGISTER_SYMBOL(kill);
  REGISTER_SYMBOL(link);
  REGISTER_SYMBOL(listen);
  REGISTER_SYMBOL(madvise);
  REGISTER_SYMBOL(malloc);
  REGISTER_SYMBOL(malloc_usable_size);
  REGISTER_SYMBOL(mincore);
  REGISTER_SYMBOL(mkdir);
  REGISTER_SYMBOL(mkdtemp);
  REGISTER_SYMBOL(mkostemp);
  REGISTER_SYMBOL(mkstemp);
  REGISTER_SYMBOL(mprotect);
  REGISTER_SYMBOL(msync);
  REGISTER_SYMBOL(munmap);
  REGISTER_SYMBOL(pause);
  REGISTER_SYMBOL(pipe);
  REGISTER_SYMBOL(posix_memalign);
  REGISTER_SYMBOL(pread);
  REGISTER_SYMBOL(pwrite);
  REGISTER_SYMBOL(raise);
  REGISTER_SYMBOL(rand);
  REGISTER_SYMBOL(rand_r);
  REGISTER_SYMBOL(read);
  REGISTER_SYMBOL(readlink);
  REGISTER_SYMBOL(realloc);
  REGISTER_SYMBOL(realpath);
  REGISTER_SYMBOL(recv);
  REGISTER_SYMBOL(recvfrom);
  REGISTER_SYMBOL(recvmsg);
  REGISTER_SYMBOL(rename);
  REGISTER_SYMBOL(sched_get_priority_max);
  REGISTER_SYMBOL(sched_get_priority_min);
  REGISTER_SYMBOL(sched_yield);
  REGISTER_SYMBOL(select);
  REGISTER_SYMBOL(send);
  REGISTER_SYMBOL(sendto);
  REGISTER_SYMBOL(signal);
  REGISTER_SYMBOL(socket);
  REGISTER_SYMBOL(srand);
  REGISTER_SYMBOL(symlink);
  REGISTER_SYMBOL(usleep);
  REGISTER_SYMBOL(write);

  // Linux APIs
  REGISTER_SYMBOL(recvmmsg);

  // Custom mapped POSIX APIs to compatibility wrappers.
  // These will rely on Starboard-side implementations that properly translate
  // Platform-specific types with musl-based types. These wrappers are defined
  // in //starboard/shared/modular.
  // TODO: b/316603042 - Detect via NPLB and only add the wrapper if needed.

  REGISTER_WRAPPER(accept);
  REGISTER_WRAPPER(access);
  REGISTER_WRAPPER(bind);
  REGISTER_WRAPPER(chmod);
  REGISTER_WRAPPER(clock_gettime);
  REGISTER_WRAPPER(closedir);
  REGISTER_WRAPPER(clock_nanosleep);
  REGISTER_WRAPPER(connect);
  if (errno_translation()) {
    REGISTER_WRAPPER(__errno_location);
  } else {
    REGISTER_SYMBOL(__errno_location);
  }
  REGISTER_WRAPPER(eventfd);
  REGISTER_WRAPPER(fchmod);
  REGISTER_WRAPPER(fchown);
  REGISTER_WRAPPER(fcntl);
  REGISTER_WRAPPER(fstat);
  REGISTER_WRAPPER(fstatat);
  REGISTER_WRAPPER(freeaddrinfo);
  REGISTER_WRAPPER(ftruncate);
  REGISTER_WRAPPER(gai_strerror);
  REGISTER_WRAPPER(getaddrinfo);
  REGISTER_WRAPPER(getauxval);
  REGISTER_WRAPPER(geteuid);
  REGISTER_WRAPPER(getifaddrs);
  REGISTER_WRAPPER(getpid);
  REGISTER_WRAPPER(getpriority);
  REGISTER_WRAPPER(getrlimit);
  REGISTER_WRAPPER(lseek);
  REGISTER_WRAPPER(mmap);
  REGISTER_WRAPPER(openat);
  REGISTER_WRAPPER(opendir);
  REGISTER_WRAPPER(pathconf);
  REGISTER_WRAPPER(pipe2);
  REGISTER_WRAPPER(poll);
  REGISTER_WRAPPER(prctl);
  REGISTER_WRAPPER(pthread_attr_init);
  REGISTER_WRAPPER(pthread_attr_destroy);
  REGISTER_WRAPPER(pthread_attr_getdetachstate);
  REGISTER_WRAPPER(pthread_attr_getschedpolicy);
  REGISTER_WRAPPER(pthread_attr_getscope);
  REGISTER_WRAPPER(pthread_attr_getstack);
  REGISTER_WRAPPER(pthread_attr_getstacksize);
  REGISTER_WRAPPER(pthread_attr_init);
  REGISTER_WRAPPER(pthread_attr_setdetachstate);
  REGISTER_WRAPPER(pthread_attr_setschedpolicy);
  REGISTER_WRAPPER(pthread_attr_setscope);
  REGISTER_WRAPPER(pthread_attr_setstack);
  REGISTER_WRAPPER(pthread_attr_setstacksize);
  REGISTER_WRAPPER(pthread_cond_broadcast);
  REGISTER_WRAPPER(pthread_cond_destroy);
  REGISTER_WRAPPER(pthread_cond_init);
  REGISTER_WRAPPER(pthread_cond_signal);
  REGISTER_WRAPPER(pthread_cond_timedwait);
  REGISTER_WRAPPER(pthread_cond_wait);
  REGISTER_WRAPPER(pthread_condattr_destroy);
  REGISTER_WRAPPER(pthread_condattr_getclock);
  REGISTER_WRAPPER(pthread_condattr_init);
  REGISTER_WRAPPER(pthread_condattr_setclock);
  REGISTER_WRAPPER(pthread_create);
  REGISTER_WRAPPER(pthread_detach);
  REGISTER_WRAPPER(pthread_equal);
  REGISTER_WRAPPER(pthread_getattr_np);
  REGISTER_WRAPPER(pthread_getname_np);
  REGISTER_WRAPPER(pthread_getschedparam);
  REGISTER_WRAPPER(pthread_getspecific);
  REGISTER_WRAPPER(pthread_join);
  REGISTER_WRAPPER(pthread_key_create);
  REGISTER_WRAPPER(pthread_key_delete);
  REGISTER_WRAPPER(pthread_kill);
  REGISTER_WRAPPER(pthread_mutex_destroy);
  REGISTER_WRAPPER(pthread_mutex_init);
  REGISTER_WRAPPER(pthread_mutex_lock);
  REGISTER_WRAPPER(pthread_mutex_trylock);
  REGISTER_WRAPPER(pthread_mutex_unlock);
  REGISTER_WRAPPER(pthread_mutexattr_destroy);
  REGISTER_WRAPPER(pthread_mutexattr_getpshared);
  REGISTER_WRAPPER(pthread_mutexattr_gettype);
  REGISTER_WRAPPER(pthread_mutexattr_init);
  REGISTER_WRAPPER(pthread_mutexattr_setpshared);
  REGISTER_WRAPPER(pthread_mutexattr_settype);
  REGISTER_WRAPPER(pthread_once);
  REGISTER_WRAPPER(pthread_rwlock_destroy);
  REGISTER_WRAPPER(pthread_rwlock_init);
  REGISTER_WRAPPER(pthread_rwlock_rdlock);
  REGISTER_WRAPPER(pthread_rwlock_tryrdlock);
  REGISTER_WRAPPER(pthread_rwlock_trywrlock);
  REGISTER_WRAPPER(pthread_rwlock_unlock);
  REGISTER_WRAPPER(pthread_rwlock_wrlock);
  REGISTER_WRAPPER(pthread_self);
  REGISTER_WRAPPER(pthread_setname_np);
  REGISTER_WRAPPER(pthread_setschedparam);
  REGISTER_WRAPPER(pthread_setspecific);
  REGISTER_WRAPPER(pthread_sigmask);
  REGISTER_WRAPPER(readdir);
  REGISTER_WRAPPER(readdir_r);
  REGISTER_WRAPPER(sched_getaffinity);
  REGISTER_WRAPPER(readv);
  REGISTER_WRAPPER(setsockopt);
  REGISTER_WRAPPER(sem_destroy);
  REGISTER_WRAPPER(sem_init);
  REGISTER_WRAPPER(sem_post);
  REGISTER_WRAPPER(sem_timedwait);
  REGISTER_WRAPPER(sem_wait);
  REGISTER_WRAPPER(sendmsg);
  REGISTER_WRAPPER(setpriority);
  REGISTER_WRAPPER(shutdown);
  REGISTER_WRAPPER(sigaction);
  REGISTER_WRAPPER(socketpair);
  REGISTER_WRAPPER(statvfs);
  REGISTER_WRAPPER(sysconf);
  REGISTER_WRAPPER(uname);
  REGISTER_WRAPPER(unlinkat);
  REGISTER_WRAPPER(utimensat);
  REGISTER_WRAPPER(writev);

}  // NOLINT

const void* ExportedSymbols::Lookup(const char* name) {
  // Don't report an error here, as the symbol might be a valid weak symbol.
  return map_[name];
}

}  // namespace elf_loader
