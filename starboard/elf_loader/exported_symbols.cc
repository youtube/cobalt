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

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "starboard/accessibility.h"
#include "starboard/audio_sink.h"
#if SB_API_VERSION < 16
#include "starboard/byte_swap.h"
#endif  // SB_API_VERSION < 16
#include "starboard/common/log.h"
#include "starboard/condition_variable.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/cpu_features.h"
#include "starboard/decode_target.h"
#include "starboard/directory.h"
#include "starboard/egl.h"
#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/gles.h"
#if SB_API_VERSION < 16
#include "starboard/image.h"
#endif  // SB_API_VERSION < 16
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/memory_reporter.h"
#include "starboard/microphone.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/player.h"
#if SB_API_VERSION >= 16
#include "starboard/shared/modular/posix_mmap_wrappers.h"
#include "starboard/shared/modular/posix_pthread_wrappers.h"
#include "starboard/shared/modular/posix_time_wrappers.h"

#endif  // SB_API_VERSION >= 16
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/speech_synthesis.h"
#include "starboard/storage.h"
#include "starboard/string.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#if SB_API_VERSION < 16
#include "starboard/time.h"
#endif  // SB_API_VERSION < 16
#include "starboard/time_zone.h"
#if SB_API_VERSION < 16
#include "starboard/ui_navigation.h"
#include "starboard/user.h"
#endif  // SB_API_VERSION < 16
#include "starboard/window.h"

#define REGISTER_SYMBOL(s)                        \
  do {                                            \
    map_[#s] = reinterpret_cast<const void*>(&s); \
  } while (0)

namespace starboard {
namespace elf_loader {

ExportedSymbols::ExportedSymbols() {
  REGISTER_SYMBOL(kSbDefaultMmapThreshold);
  REGISTER_SYMBOL(kSbFileAltSepChar);
  REGISTER_SYMBOL(kSbFileAltSepString);
  REGISTER_SYMBOL(kSbFileMaxName);
  REGISTER_SYMBOL(kSbFileMaxOpen);
  REGISTER_SYMBOL(kSbFileMaxPath);
  REGISTER_SYMBOL(kSbFileSepChar);
  REGISTER_SYMBOL(kSbFileSepString);
#if SB_API_VERSION < 15
  REGISTER_SYMBOL(kSbHasAc3Audio);
#endif  // SB_API_VERSION < 15
  REGISTER_SYMBOL(kSbHasMediaWebmVp9Support);
  REGISTER_SYMBOL(kSbHasThreadPrioritySupport);
  REGISTER_SYMBOL(kSbMallocAlignment);
#if SB_API_VERSION >= 14
  REGISTER_SYMBOL(kSbMaxSystemPathCacheDirectorySize);
#endif  // SB_API_VERSION >= 14
  REGISTER_SYMBOL(kSbMaxThreadLocalKeys);
  REGISTER_SYMBOL(kSbMaxThreadNameLength);
  REGISTER_SYMBOL(kSbMaxThreads);
  REGISTER_SYMBOL(kSbMediaMaxAudioBitrateInBitsPerSecond);
  REGISTER_SYMBOL(kSbMediaMaxVideoBitrateInBitsPerSecond);
  REGISTER_SYMBOL(kSbMediaVideoFrameAlignment);
  REGISTER_SYMBOL(kSbMemoryLogPath);
  REGISTER_SYMBOL(kSbMemoryPageSize);
  REGISTER_SYMBOL(kSbNetworkReceiveBufferSize);
  REGISTER_SYMBOL(kSbPathSepChar);
  REGISTER_SYMBOL(kSbPathSepString);
  REGISTER_SYMBOL(kSbPreferredRgbaByteOrder);
#if SB_API_VERSION < 16
  REGISTER_SYMBOL(kSbUserMaxSignedIn);
#endif  // SB_API_VERSION < 16
  REGISTER_SYMBOL(SbAccessibilityGetCaptionSettings);
  REGISTER_SYMBOL(SbAccessibilityGetDisplaySettings);
  REGISTER_SYMBOL(SbAccessibilityGetTextToSpeechSettings);
  REGISTER_SYMBOL(SbAccessibilitySetCaptionsEnabled);
  REGISTER_SYMBOL(SbAudioSinkCreate);
  REGISTER_SYMBOL(SbAudioSinkDestroy);
  REGISTER_SYMBOL(SbAudioSinkGetMaxChannels);
  REGISTER_SYMBOL(SbAudioSinkGetMinBufferSizeInFrames);
  REGISTER_SYMBOL(SbAudioSinkGetNearestSupportedSampleFrequency);
  REGISTER_SYMBOL(SbAudioSinkIsAudioFrameStorageTypeSupported);
  REGISTER_SYMBOL(SbAudioSinkIsAudioSampleTypeSupported);
  REGISTER_SYMBOL(SbAudioSinkIsValid);
#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbByteSwapS16);
  REGISTER_SYMBOL(SbByteSwapS32);
  REGISTER_SYMBOL(SbByteSwapS64);
  REGISTER_SYMBOL(SbByteSwapU16);
  REGISTER_SYMBOL(SbByteSwapU32);
  REGISTER_SYMBOL(SbByteSwapU64);
#endif  // SB_API_VERSION < 16
  REGISTER_SYMBOL(SbConditionVariableBroadcast);
  REGISTER_SYMBOL(SbConditionVariableCreate);
  REGISTER_SYMBOL(SbConditionVariableDestroy);
  REGISTER_SYMBOL(SbConditionVariableSignal);
  REGISTER_SYMBOL(SbConditionVariableWait);
  REGISTER_SYMBOL(SbConditionVariableWaitTimed);
  REGISTER_SYMBOL(SbCPUFeaturesGet);
  REGISTER_SYMBOL(SbDecodeTargetGetInfo);
  REGISTER_SYMBOL(SbDecodeTargetRelease);
  REGISTER_SYMBOL(SbDirectoryCanOpen);
  REGISTER_SYMBOL(SbDirectoryClose);
#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbDirectoryCreate);
#endif  // SB_API_VERSION < 16
  REGISTER_SYMBOL(SbDirectoryGetNext);
  REGISTER_SYMBOL(SbDirectoryOpen);
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
  REGISTER_SYMBOL(SbFileCanOpen);
  REGISTER_SYMBOL(SbFileClose);
  REGISTER_SYMBOL(SbFileDelete);
  REGISTER_SYMBOL(SbFileExists);
  REGISTER_SYMBOL(SbFileFlush);
  REGISTER_SYMBOL(SbFileGetInfo);
  REGISTER_SYMBOL(SbFileGetPathInfo);
  REGISTER_SYMBOL(SbFileModeStringToFlags);
  REGISTER_SYMBOL(SbFileOpen);
  REGISTER_SYMBOL(SbFileRead);
  REGISTER_SYMBOL(SbFileSeek);
  REGISTER_SYMBOL(SbFileTruncate);
  REGISTER_SYMBOL(SbFileWrite);
  REGISTER_SYMBOL(SbGetEglInterface);
  REGISTER_SYMBOL(SbGetGlesInterface);
#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbImageDecode);
  REGISTER_SYMBOL(SbImageIsDecodeSupported);
#endif  // SB_API_VERSION < 16
  REGISTER_SYMBOL(SbLog);
  REGISTER_SYMBOL(SbLogFlush);
  REGISTER_SYMBOL(SbLogFormat);
  REGISTER_SYMBOL(SbLogIsTty);
  REGISTER_SYMBOL(SbLogRaw);
  REGISTER_SYMBOL(SbLogRawDumpStack);
  REGISTER_SYMBOL(SbLogRawFormat);
  REGISTER_SYMBOL(SbMediaCanPlayMimeAndKeySystem);
  REGISTER_SYMBOL(SbMediaGetAudioBufferBudget);
  REGISTER_SYMBOL(SbMediaGetAudioConfiguration);
  REGISTER_SYMBOL(SbMediaGetAudioOutputCount);
#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbMediaGetBufferAlignment);
#endif  // SB_API_VERSION < 16
  REGISTER_SYMBOL(SbMediaGetBufferAllocationUnit);
  REGISTER_SYMBOL(SbMediaGetBufferGarbageCollectionDurationThreshold);
  REGISTER_SYMBOL(SbMediaGetBufferPadding);
  REGISTER_SYMBOL(SbMediaGetBufferStorageType);
  REGISTER_SYMBOL(SbMediaGetInitialBufferCapacity);
  REGISTER_SYMBOL(SbMediaGetMaxBufferCapacity);
  REGISTER_SYMBOL(SbMediaGetProgressiveBufferBudget);
  REGISTER_SYMBOL(SbMediaGetVideoBufferBudget);
  REGISTER_SYMBOL(SbMediaIsBufferPoolAllocateOnDemand);
  REGISTER_SYMBOL(SbMediaIsBufferUsingMemoryPool);
#if SB_API_VERSION < 15
  REGISTER_SYMBOL(SbMediaSetAudioWriteDuration);
#endif  // SB_API_VERSION < 15
#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbMemoryAllocate);
  REGISTER_SYMBOL(SbMemoryAllocateAligned);
  REGISTER_SYMBOL(SbMemoryAllocateAlignedUnchecked);
  REGISTER_SYMBOL(SbMemoryAllocateNoReport);
  REGISTER_SYMBOL(SbMemoryAllocateUnchecked);
  REGISTER_SYMBOL(SbMemoryDeallocate);
  REGISTER_SYMBOL(SbMemoryDeallocateAligned);
  REGISTER_SYMBOL(SbMemoryDeallocateNoReport);
#if SB_CAN(MAP_EXECUTABLE_MEMORY)
  REGISTER_SYMBOL(SbMemoryFlush);
#endif  // SB_CAN(MAP_EXECUTABLE_MEMORY)
  REGISTER_SYMBOL(SbMemoryFree);
  REGISTER_SYMBOL(SbMemoryFreeAligned);
#endif  // SB_API_VERSION < 16

#if SB_API_VERSION < 15
  REGISTER_SYMBOL(SbMemoryGetStackBounds);
#endif

#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbMemoryMap);
  REGISTER_SYMBOL(SbMemoryProtect);
  REGISTER_SYMBOL(SbMemoryReallocate);
  REGISTER_SYMBOL(SbMemoryReallocateUnchecked);
#endif  // SB_API_VERSION < 16

#if SB_API_VERSION < 15
  REGISTER_SYMBOL(SbMemorySetReporter);
#endif

#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbMemoryUnmap);
#endif  // SB_API_VERSION < 16

  REGISTER_SYMBOL(SbMicrophoneClose);
  REGISTER_SYMBOL(SbMicrophoneCreate);
  REGISTER_SYMBOL(SbMicrophoneDestroy);
  REGISTER_SYMBOL(SbMicrophoneGetAvailable);
  REGISTER_SYMBOL(SbMicrophoneIsSampleRateSupported);
  REGISTER_SYMBOL(SbMicrophoneOpen);
  REGISTER_SYMBOL(SbMicrophoneRead);
  REGISTER_SYMBOL(SbMutexAcquire);
  REGISTER_SYMBOL(SbMutexAcquireTry);
  REGISTER_SYMBOL(SbMutexCreate);
  REGISTER_SYMBOL(SbMutexDestroy);
  REGISTER_SYMBOL(SbMutexRelease);
  REGISTER_SYMBOL(SbOnce);
  REGISTER_SYMBOL(SbPlayerCreate);
  REGISTER_SYMBOL(SbPlayerDestroy);
#if SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerGetAudioConfiguration);
#endif  // SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerGetCurrentFrame);
#if SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerGetInfo);
#else   // SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerGetInfo2);
#endif  // SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerGetMaximumNumberOfSamplesPerWrite);
  REGISTER_SYMBOL(SbPlayerGetPreferredOutputMode);
#if SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerSeek);
#else   // SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerSeek2);
#endif  // SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerSetBounds);
  REGISTER_SYMBOL(SbPlayerSetPlaybackRate);
  REGISTER_SYMBOL(SbPlayerSetVolume);
  REGISTER_SYMBOL(SbPlayerWriteEndOfStream);
#if SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerWriteSamples);
#else   // SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbPlayerWriteSample2);
#endif  // SB_API_VERSION >= 15
  REGISTER_SYMBOL(SbSocketAccept);
  REGISTER_SYMBOL(SbSocketBind);
  REGISTER_SYMBOL(SbSocketClearLastError);
  REGISTER_SYMBOL(SbSocketConnect);
  REGISTER_SYMBOL(SbSocketCreate);
  REGISTER_SYMBOL(SbSocketDestroy);
  REGISTER_SYMBOL(SbSocketFreeResolution);
  REGISTER_SYMBOL(SbSocketGetInterfaceAddress);
  REGISTER_SYMBOL(SbSocketGetLastError);
  REGISTER_SYMBOL(SbSocketGetLocalAddress);
  REGISTER_SYMBOL(SbSocketIsConnected);
  REGISTER_SYMBOL(SbSocketIsConnectedAndIdle);
  REGISTER_SYMBOL(SbSocketIsIpv6Supported);
  REGISTER_SYMBOL(SbSocketJoinMulticastGroup);
  REGISTER_SYMBOL(SbSocketListen);
  REGISTER_SYMBOL(SbSocketReceiveFrom);
  REGISTER_SYMBOL(SbSocketResolve);
  REGISTER_SYMBOL(SbSocketSendTo);
  REGISTER_SYMBOL(SbSocketSetBroadcast);
  REGISTER_SYMBOL(SbSocketSetReceiveBufferSize);
  REGISTER_SYMBOL(SbSocketSetReuseAddress);
  REGISTER_SYMBOL(SbSocketSetSendBufferSize);
  REGISTER_SYMBOL(SbSocketSetTcpKeepAlive);
  REGISTER_SYMBOL(SbSocketSetTcpNoDelay);
  REGISTER_SYMBOL(SbSocketSetTcpWindowScaling);
  REGISTER_SYMBOL(SbSocketWaiterAdd);
  REGISTER_SYMBOL(SbSocketWaiterCreate);
  REGISTER_SYMBOL(SbSocketWaiterDestroy);
  REGISTER_SYMBOL(SbSocketWaiterRemove);
  REGISTER_SYMBOL(SbSocketWaiterWait);
  REGISTER_SYMBOL(SbSocketWaiterWaitTimed);
  REGISTER_SYMBOL(SbSocketWaiterWakeUp);
  REGISTER_SYMBOL(SbSpeechSynthesisCancel);
  REGISTER_SYMBOL(SbSpeechSynthesisIsSupported);
  REGISTER_SYMBOL(SbSpeechSynthesisSpeak);
  REGISTER_SYMBOL(SbStorageCloseRecord);
  REGISTER_SYMBOL(SbStorageDeleteRecord);
  REGISTER_SYMBOL(SbStorageGetRecordSize);
  REGISTER_SYMBOL(SbStorageOpenRecord);
  REGISTER_SYMBOL(SbStorageReadRecord);
  REGISTER_SYMBOL(SbStorageWriteRecord);
#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbStringCompareNoCase);
  REGISTER_SYMBOL(SbStringCompareNoCaseN);
  REGISTER_SYMBOL(SbStringDuplicate);
  REGISTER_SYMBOL(SbStringFormat);
  REGISTER_SYMBOL(SbStringFormatWide);
  REGISTER_SYMBOL(SbStringScan);
#endif  // SB_API_VERSION < 16
  REGISTER_SYMBOL(SbSystemBreakIntoDebugger);
  REGISTER_SYMBOL(SbSystemClearLastError);
#if SB_API_VERSION < 14
  REGISTER_SYMBOL(SbSystemGetConnectionType);
#endif
#if SB_API_VERSION < 15
  REGISTER_SYMBOL(SbSystemGetDeviceType);
#endif
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
  REGISTER_SYMBOL(SbThreadCreate);
  REGISTER_SYMBOL(SbThreadCreateLocalKey);
  REGISTER_SYMBOL(SbThreadDestroyLocalKey);
  REGISTER_SYMBOL(SbThreadDetach);
  REGISTER_SYMBOL(SbThreadGetCurrent);
  REGISTER_SYMBOL(SbThreadGetId);
  REGISTER_SYMBOL(SbThreadGetLocalValue);
  REGISTER_SYMBOL(SbThreadGetName);
  REGISTER_SYMBOL(SbThreadIsEqual);
  REGISTER_SYMBOL(SbThreadJoin);
  REGISTER_SYMBOL(SbThreadSamplerCreate);
  REGISTER_SYMBOL(SbThreadSamplerDestroy);
  REGISTER_SYMBOL(SbThreadSamplerFreeze);
  REGISTER_SYMBOL(SbThreadSamplerIsSupported);
  REGISTER_SYMBOL(SbThreadSamplerThaw);
  REGISTER_SYMBOL(SbThreadSetLocalValue);
  REGISTER_SYMBOL(SbThreadSetName);
  REGISTER_SYMBOL(SbThreadSleep);
  REGISTER_SYMBOL(SbThreadYield);
#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbTimeGetMonotonicNow);
  REGISTER_SYMBOL(SbTimeGetMonotonicThreadNow);
  REGISTER_SYMBOL(SbTimeGetNow);
  REGISTER_SYMBOL(SbTimeIsTimeThreadNowSupported);
#endif  // SB_API_VERSION < 16
  REGISTER_SYMBOL(SbTimeZoneGetCurrent);
  REGISTER_SYMBOL(SbTimeZoneGetName);
#if SB_API_VERSION < 16
  REGISTER_SYMBOL(SbUiNavGetInterface);
  REGISTER_SYMBOL(SbUserGetCurrent);
  REGISTER_SYMBOL(SbUserGetProperty);
  REGISTER_SYMBOL(SbUserGetPropertySize);
  REGISTER_SYMBOL(SbUserGetSignedIn);
#endif  // SB_API_VERSION < 16
  REGISTER_SYMBOL(SbWindowBlurOnScreenKeyboard);
  REGISTER_SYMBOL(SbWindowCreate);
  REGISTER_SYMBOL(SbWindowDestroy);
  REGISTER_SYMBOL(SbWindowFocusOnScreenKeyboard);
  REGISTER_SYMBOL(SbWindowGetDiagonalSizeInInches);
  REGISTER_SYMBOL(SbWindowGetOnScreenKeyboardBoundingRect);
  REGISTER_SYMBOL(SbWindowGetPlatformHandle);
  REGISTER_SYMBOL(SbWindowGetSize);
  REGISTER_SYMBOL(SbWindowHideOnScreenKeyboard);
  REGISTER_SYMBOL(SbWindowIsOnScreenKeyboardShown);
  REGISTER_SYMBOL(SbWindowOnScreenKeyboardIsSupported);
  REGISTER_SYMBOL(SbWindowOnScreenKeyboardSuggestionsSupported);
  REGISTER_SYMBOL(SbWindowSetDefaultOptions);
  REGISTER_SYMBOL(SbWindowSetOnScreenKeyboardKeepFocus);
  REGISTER_SYMBOL(SbWindowShowOnScreenKeyboard);
  REGISTER_SYMBOL(SbWindowUpdateOnScreenKeyboardSuggestions);

#if SB_API_VERSION >= 16
  // POSIX APIs
  REGISTER_SYMBOL(malloc);
  REGISTER_SYMBOL(realloc);
  REGISTER_SYMBOL(calloc);
  REGISTER_SYMBOL(posix_memalign);
  REGISTER_SYMBOL(free);
  REGISTER_SYMBOL(vsscanf);
  REGISTER_SYMBOL(mprotect);
  REGISTER_SYMBOL(munmap);
  REGISTER_SYMBOL(msync);
  REGISTER_SYMBOL(mkdir);

  // Custom mapped POSIX APIs to compatibility wrappers.
  // These will rely on Starboard-side implementations that properly translate
  // Platform-specific types with musl-based types. These wrappers are defined
  // in //starboard/shared/modular.
  // TODO: b/316603042 - Detect via NPLB and only add the wrapper if needed.
  map_["clock_gettime"] = reinterpret_cast<const void*>(&__wrap_clock_gettime);
  map_["gettimeofday"] = reinterpret_cast<const void*>(&__wrap_gettimeofday);
  map_["time"] = reinterpret_cast<const void*>(&__wrap_time);
  map_["gmtime_r"] = reinterpret_cast<const void*>(&__wrap_gmtime_r);
  map_["mmap"] = reinterpret_cast<const void*>(&__wrap_mmap);
  map_["pthread_mutex_destroy"] =
      reinterpret_cast<const void*>(&__wrap_pthread_mutex_destroy);
  map_["pthread_mutex_init"] =
      reinterpret_cast<const void*>(&__wrap_pthread_mutex_init);
  map_["pthread_mutex_lock"] =
      reinterpret_cast<const void*>(&__wrap_pthread_mutex_lock);
  map_["pthread_mutex_unlock"] =
      reinterpret_cast<const void*>(&__wrap_pthread_mutex_unlock);
  map_["pthread_mutex_trylock"] =
      reinterpret_cast<const void*>(&__wrap_pthread_mutex_trylock);

  REGISTER_SYMBOL(sprintf);
  REGISTER_SYMBOL(snprintf);
  REGISTER_SYMBOL(vfwprintf);
  REGISTER_SYMBOL(vsnprintf);
#if defined(_MSC_VER)
  // MSVC provides a template with the same name.
  // The cast helps the compiler to pick the correct C function pointer to be
  // used.
  REGISTER_SYMBOL(
      static_cast<int (*)(wchar_t* buffer, size_t count, const wchar_t* format,
                          va_list argptr)>(vswprintf));
#else
  REGISTER_SYMBOL(vswprintf);
#endif  // defined(_MSC_VER)

#endif  // SB_API_VERSION >= 16

}  // NOLINT

const void* ExportedSymbols::Lookup(const char* name) {
  const void* address = map_[name];
  // Any symbol that is not registered as part of the Starboard API in the
  // constructor of this class is a leak, and is an error.
  SB_CHECK(address) << "Failed to retrieve the address of '" << name << "'.";
  return address;
}

}  // namespace elf_loader
}  // namespace starboard
