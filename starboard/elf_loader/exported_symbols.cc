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

#include "starboard/accessibility.h"
#include "starboard/audio_sink.h"
#include "starboard/blitter.h"
#include "starboard/byte_swap.h"
#include "starboard/character.h"
#include "starboard/condition_variable.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/cpu_features.h"
#include "starboard/decode_target.h"
#include "starboard/directory.h"
#include "starboard/double.h"
#include "starboard/egl.h"
#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/gles.h"
#include "starboard/image.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/memory_reporter.h"
#include "starboard/microphone.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/player.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#if SB_API_VERSION < 13
#include "starboard/speech_recognizer.h"
#endif
#include "starboard/speech_synthesis.h"
#include "starboard/storage.h"
#include "starboard/string.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#include "starboard/time_zone.h"
#include "starboard/ui_navigation.h"
#include "starboard/window.h"

#define REGISTER_SYMBOL(s)                        \
  do {                                            \
    map_[#s] = reinterpret_cast<const void*>(&s); \
  } while (0)

namespace starboard {
namespace elf_loader {

ExportedSymbols::ExportedSymbols(
    const void* (*custom_get_extension)(const char* name)) {
  REGISTER_SYMBOL(SbAccessibilityGetDisplaySettings);
  REGISTER_SYMBOL(SbAccessibilityGetTextToSpeechSettings);
  REGISTER_SYMBOL(SbAudioSinkCreate);
  REGISTER_SYMBOL(SbAudioSinkDestroy);
  REGISTER_SYMBOL(SbAudioSinkGetMaxChannels);
  REGISTER_SYMBOL(SbAudioSinkGetNearestSupportedSampleFrequency);
  REGISTER_SYMBOL(SbAudioSinkIsAudioFrameStorageTypeSupported);
  REGISTER_SYMBOL(SbAudioSinkIsAudioSampleTypeSupported);
  REGISTER_SYMBOL(SbAudioSinkIsValid);
  REGISTER_SYMBOL(SbByteSwapS16);
  REGISTER_SYMBOL(SbByteSwapS32);
  REGISTER_SYMBOL(SbByteSwapS64);
  REGISTER_SYMBOL(SbByteSwapU16);
  REGISTER_SYMBOL(SbByteSwapU32);
  REGISTER_SYMBOL(SbByteSwapU64);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbCharacterIsAlphanumeric);
  REGISTER_SYMBOL(SbCharacterIsDigit);
  REGISTER_SYMBOL(SbCharacterIsHexDigit);
  REGISTER_SYMBOL(SbCharacterIsSpace);
  REGISTER_SYMBOL(SbCharacterIsUpper);
  REGISTER_SYMBOL(SbCharacterToLower);
  REGISTER_SYMBOL(SbCharacterToUpper);
#endif  // SB_API_VERSION < 13
  REGISTER_SYMBOL(SbConditionVariableBroadcast);
  REGISTER_SYMBOL(SbConditionVariableCreate);
  REGISTER_SYMBOL(SbConditionVariableDestroy);
  REGISTER_SYMBOL(SbConditionVariableSignal);
  REGISTER_SYMBOL(SbConditionVariableWait);
  REGISTER_SYMBOL(SbConditionVariableWaitTimed);
  REGISTER_SYMBOL(SbDecodeTargetGetInfo);
  REGISTER_SYMBOL(SbDecodeTargetRelease);
  REGISTER_SYMBOL(SbDirectoryCanOpen);
  REGISTER_SYMBOL(SbDirectoryClose);
  REGISTER_SYMBOL(SbDirectoryCreate);
  REGISTER_SYMBOL(SbDirectoryGetNext);
  REGISTER_SYMBOL(SbDirectoryOpen);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbDoubleAbsolute);
  REGISTER_SYMBOL(SbDoubleExponent);
  REGISTER_SYMBOL(SbDoubleFloor);
  REGISTER_SYMBOL(SbDoubleIsFinite);
  REGISTER_SYMBOL(SbDoubleIsNan);
#endif  // SB_API_VERSION < 13
  REGISTER_SYMBOL(SbDrmCloseSession);
  REGISTER_SYMBOL(SbDrmCreateSystem);
  REGISTER_SYMBOL(SbDrmDestroySystem);
  REGISTER_SYMBOL(SbDrmGenerateSessionUpdateRequest);
#if SB_API_VERSION >= 12
  REGISTER_SYMBOL(SbDrmGetMetrics);
#endif  // SB_API_VERSION >= 12
  REGISTER_SYMBOL(SbDrmUpdateSession);
  REGISTER_SYMBOL(SbEventCancel);
  REGISTER_SYMBOL(SbEventSchedule);
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
  REGISTER_SYMBOL(SbMediaGetAudioConfiguration);
  REGISTER_SYMBOL(SbMediaGetAudioOutputCount);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbMediaIsSupported);
#endif  // SB_API_VERSION < 13
  REGISTER_SYMBOL(SbMemoryAllocate);
  REGISTER_SYMBOL(SbMemoryAllocateAligned);
  REGISTER_SYMBOL(SbMemoryAllocateNoReport);
  REGISTER_SYMBOL(SbMemoryDeallocate);
  REGISTER_SYMBOL(SbMemoryDeallocateAligned);
  REGISTER_SYMBOL(SbMemoryDeallocateNoReport);
  REGISTER_SYMBOL(SbMemoryReallocate);
  REGISTER_SYMBOL(SbMemorySetReporter);
  REGISTER_SYMBOL(SbImageDecode);
  REGISTER_SYMBOL(SbImageIsDecodeSupported);
  REGISTER_SYMBOL(SbLog);
  REGISTER_SYMBOL(SbLogFlush);
  REGISTER_SYMBOL(SbLogFormat);
  REGISTER_SYMBOL(SbLogIsTty);
  REGISTER_SYMBOL(SbLogRaw);
  REGISTER_SYMBOL(SbLogRawDumpStack);
  REGISTER_SYMBOL(SbLogRawFormat);
  REGISTER_SYMBOL(SbMediaCanPlayMimeAndKeySystem);
  REGISTER_SYMBOL(SbMemoryAllocateAlignedUnchecked);
  REGISTER_SYMBOL(SbMemoryAllocateUnchecked);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbMemoryCompare);
  REGISTER_SYMBOL(SbMemoryCopy);
  REGISTER_SYMBOL(SbMemoryFindByte);
#endif
  REGISTER_SYMBOL(SbMemoryFree);
  REGISTER_SYMBOL(SbMemoryFreeAligned);
  REGISTER_SYMBOL(SbMemoryGetStackBounds);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbMemoryMove);
#endif
  REGISTER_SYMBOL(SbMemoryReallocateUnchecked);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbMemorySet);
#endif
  REGISTER_SYMBOL(SbMutexAcquire);
  REGISTER_SYMBOL(SbMutexAcquireTry);
  REGISTER_SYMBOL(SbMutexCreate);
  REGISTER_SYMBOL(SbMutexDestroy);
  REGISTER_SYMBOL(SbMutexRelease);
  REGISTER_SYMBOL(SbOnce);
  REGISTER_SYMBOL(SbPlayerCreate);
  REGISTER_SYMBOL(SbPlayerDestroy);
  REGISTER_SYMBOL(SbPlayerGetCurrentFrame);
  REGISTER_SYMBOL(SbPlayerSetBounds);
  REGISTER_SYMBOL(SbPlayerSetPlaybackRate);
  REGISTER_SYMBOL(SbPlayerSetVolume);
  REGISTER_SYMBOL(SbPlayerWriteEndOfStream);
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
  REGISTER_SYMBOL(SbStorageCloseRecord);
  REGISTER_SYMBOL(SbStorageDeleteRecord);
  REGISTER_SYMBOL(SbStorageGetRecordSize);
  REGISTER_SYMBOL(SbStorageOpenRecord);
  REGISTER_SYMBOL(SbStorageReadRecord);
  REGISTER_SYMBOL(SbStorageWriteRecord);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbStringCompare);
  REGISTER_SYMBOL(SbStringCompareAll);
#endif
  REGISTER_SYMBOL(SbStringCompareNoCase);
  REGISTER_SYMBOL(SbStringCompareNoCaseN);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbStringCompareWide);
  REGISTER_SYMBOL(SbStringConcat);
  REGISTER_SYMBOL(SbStringConcatWide);
  REGISTER_SYMBOL(SbStringCopy);
  REGISTER_SYMBOL(SbStringCopyWide);
#endif  // SB_API_VERSION < 13
  REGISTER_SYMBOL(SbStringDuplicate);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbStringFindCharacter);
  REGISTER_SYMBOL(SbStringFindLastCharacter);
  REGISTER_SYMBOL(SbStringFindString);
#endif
  REGISTER_SYMBOL(SbStringFormat);
  REGISTER_SYMBOL(SbStringFormatWide);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbStringGetLength);
  REGISTER_SYMBOL(SbStringGetLengthWide);
  REGISTER_SYMBOL(SbStringParseDouble);
  REGISTER_SYMBOL(SbStringParseSignedInteger);
  REGISTER_SYMBOL(SbStringParseUInt64);
  REGISTER_SYMBOL(SbStringParseUnsignedInteger);
#endif
  REGISTER_SYMBOL(SbStringScan);
#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbSystemBinarySearch);
#endif
  REGISTER_SYMBOL(SbSystemBreakIntoDebugger);
  REGISTER_SYMBOL(SbSystemClearLastError);
  REGISTER_SYMBOL(SbSystemGetConnectionType);
  REGISTER_SYMBOL(SbSystemGetDeviceType);
  REGISTER_SYMBOL(SbSystemGetErrorString);
  REGISTER_SYMBOL(SbSystemGetLastError);
  REGISTER_SYMBOL(SbSystemGetLocaleId);
  REGISTER_SYMBOL(SbSystemGetNumberOfProcessors);
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
  REGISTER_SYMBOL(SbSystemRaisePlatformError);
#if SB_API_VERSION >= 13
  REGISTER_SYMBOL(SbSystemRequestBlur);
  REGISTER_SYMBOL(SbSystemRequestConceal);
  REGISTER_SYMBOL(SbSystemRequestFocus);
  REGISTER_SYMBOL(SbSystemRequestFreeze);
  REGISTER_SYMBOL(SbSystemRequestReveal);
  REGISTER_SYMBOL(SbSystemRequestStop);
#else
  REGISTER_SYMBOL(SbSystemRequestPause);
  REGISTER_SYMBOL(SbSystemRequestStop);
  REGISTER_SYMBOL(SbSystemRequestSuspend);
  REGISTER_SYMBOL(SbSystemRequestUnpause);
#endif  // SB_API_VERSION >= 13

#if SB_API_VERSION < 13
  REGISTER_SYMBOL(SbSystemSort);
#endif  // SB_API_VERSION < 13

  REGISTER_SYMBOL(SbSystemSymbolize);
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
  REGISTER_SYMBOL(SbThreadSetLocalValue);
  REGISTER_SYMBOL(SbThreadSetName);
  REGISTER_SYMBOL(SbThreadSleep);
  REGISTER_SYMBOL(SbThreadYield);
  REGISTER_SYMBOL(SbTimeGetMonotonicNow);
  REGISTER_SYMBOL(SbTimeGetNow);
  REGISTER_SYMBOL(SbTimeZoneGetCurrent);
  REGISTER_SYMBOL(SbTimeZoneGetName);
  REGISTER_SYMBOL(SbUserGetCurrent);
  REGISTER_SYMBOL(SbUserGetProperty);
  REGISTER_SYMBOL(SbUserGetPropertySize);
  REGISTER_SYMBOL(SbUserGetSignedIn);
  REGISTER_SYMBOL(SbWindowCreate);
  REGISTER_SYMBOL(SbWindowDestroy);
  REGISTER_SYMBOL(SbWindowGetPlatformHandle);
  REGISTER_SYMBOL(SbWindowGetSize);
  REGISTER_SYMBOL(SbWindowSetDefaultOptions);
  REGISTER_SYMBOL(SbSystemGetPath);
  REGISTER_SYMBOL(SbGetEglInterface);
  REGISTER_SYMBOL(SbGetGlesInterface);

#if SB_API_VERSION >= 12
  REGISTER_SYMBOL(SbFileAtomicReplace);
#endif  // SB_API_VERSION >= 12

#if SB_CAN(MAP_EXECUTABLE_MEMORY)
  REGISTER_SYMBOL(SbMemoryFlush);
#endif  // SB_CAN(MAP_EXECUTABLE_MEMORY)

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  REGISTER_SYMBOL(SbPlayerGetPreferredOutputMode);
#else
  REGISTER_SYMBOL(SbPlayerOutputModeSupported);
#endif

#if SB_API_VERSION >= 12 || SB_HAS(MMAP)
  REGISTER_SYMBOL(SbMemoryMap);
  REGISTER_SYMBOL(SbMemoryUnmap);
  REGISTER_SYMBOL(SbMemoryProtect);
#endif  // SB_HAS(MMAP)

#if SB_API_VERSION >= 12
  REGISTER_SYMBOL(SbUiNavGetInterface);
#endif  // SB_API_VERSION >= 12

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
  REGISTER_SYMBOL(SbWindowBlurOnScreenKeyboard);
  REGISTER_SYMBOL(SbWindowFocusOnScreenKeyboard);
  REGISTER_SYMBOL(SbWindowGetOnScreenKeyboardBoundingRect);
  REGISTER_SYMBOL(SbWindowHideOnScreenKeyboard);
  REGISTER_SYMBOL(SbWindowIsOnScreenKeyboardShown);
  REGISTER_SYMBOL(SbWindowSetOnScreenKeyboardKeepFocus);
  REGISTER_SYMBOL(SbWindowShowOnScreenKeyboard);
  REGISTER_SYMBOL(SbWindowOnScreenKeyboardSuggestionsSupported);
  REGISTER_SYMBOL(SbWindowUpdateOnScreenKeyboardSuggestions);
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

#if SB_API_VERSION >= 12
  REGISTER_SYMBOL(SbWindowOnScreenKeyboardIsSupported);
#endif  // SB_API_VERSION >= 12

#if SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)
  REGISTER_SYMBOL(SbAccessibilityGetCaptionSettings);
  REGISTER_SYMBOL(SbAccessibilitySetCaptionsEnabled);
#endif  // SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)

#if SB_API_VERSION >= 12 || SB_HAS(MICROPHONE)
  REGISTER_SYMBOL(SbMicrophoneClose);
  REGISTER_SYMBOL(SbMicrophoneCreate);
  REGISTER_SYMBOL(SbMicrophoneDestroy);
  REGISTER_SYMBOL(SbMicrophoneGetAvailable);
  REGISTER_SYMBOL(SbMicrophoneIsSampleRateSupported);
  REGISTER_SYMBOL(SbMicrophoneOpen);
  REGISTER_SYMBOL(SbMicrophoneRead);
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(MICROPHONE)

#if SB_API_VERSION >= 12
  REGISTER_SYMBOL(SbSocketIsIpv6Supported);
#endif

#if SB_API_VERSION >= 12 || SB_HAS(SPEECH_SYNTHESIS)
  REGISTER_SYMBOL(SbSpeechSynthesisCancel);
  REGISTER_SYMBOL(SbSpeechSynthesisSpeak);
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(SPEECH_SYNTHESIS)

#if SB_API_VERSION >= 12 || SB_HAS(TIME_THREAD_NOW)
  REGISTER_SYMBOL(SbTimeGetMonotonicThreadNow);
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(TIME_THREAD_NOW)

#if SB_API_VERSION == 12
  REGISTER_SYMBOL(SbSpeechRecognizerIsSupported);
#endif
#if SB_API_VERSION == 12 || (SB_API_VERSION < 12 && SB_HAS(SPEECH_RECOGNIZER))
  REGISTER_SYMBOL(SbSpeechRecognizerCreate);
  REGISTER_SYMBOL(SbSpeechRecognizerDestroy);
  REGISTER_SYMBOL(SbSpeechRecognizerStart);
  REGISTER_SYMBOL(SbSpeechRecognizerStop);
  REGISTER_SYMBOL(SbSpeechRecognizerCancel);
#endif  // SB_API_VERSION >= 12 ||
        // (SB_API_VERSION < 12 && SB_HAS(SPEECH_RECOGNIZER))

#if SB_API_VERSION >= 12
  REGISTER_SYMBOL(SbSpeechSynthesisIsSupported);
#endif

#if SB_API_VERSION >= 12
  REGISTER_SYMBOL(SbTimeIsTimeThreadNowSupported);
#endif

  REGISTER_SYMBOL(SbDrmIsServerCertificateUpdatable);
  REGISTER_SYMBOL(SbDrmUpdateServerCertificate);
  REGISTER_SYMBOL(SbMediaGetAudioBufferBudget);
  REGISTER_SYMBOL(SbMediaGetBufferAlignment);
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
  REGISTER_SYMBOL(SbPlayerGetInfo2);
  REGISTER_SYMBOL(SbPlayerGetMaximumNumberOfSamplesPerWrite);
  REGISTER_SYMBOL(SbPlayerSeek2);
  REGISTER_SYMBOL(SbPlayerWriteSample2);
  REGISTER_SYMBOL(SbSystemSupportsResume);
  REGISTER_SYMBOL(SbAudioSinkGetMinBufferSizeInFrames);
  REGISTER_SYMBOL(SbCPUFeaturesGet);
  REGISTER_SYMBOL(SbMediaSetAudioWriteDuration);
  REGISTER_SYMBOL(SbSystemGetExtension);
  REGISTER_SYMBOL(SbSystemSignWithCertificationSecretKey);
  REGISTER_SYMBOL(SbThreadContextGetPointer);
  REGISTER_SYMBOL(SbThreadSamplerCreate);
  REGISTER_SYMBOL(SbThreadSamplerDestroy);
  REGISTER_SYMBOL(SbThreadSamplerFreeze);
  REGISTER_SYMBOL(SbThreadSamplerIsSupported);
  REGISTER_SYMBOL(SbThreadSamplerThaw);
  REGISTER_SYMBOL(SbWindowGetDiagonalSizeInInches);

#if SB_API_VERSION >= 12
  REGISTER_SYMBOL(kSbDefaultMmapThreshold);
  REGISTER_SYMBOL(kSbFileMaxName);
  REGISTER_SYMBOL(kSbFileMaxOpen);
  REGISTER_SYMBOL(kSbFileAltSepChar);
  REGISTER_SYMBOL(kSbFileAltSepString);
  REGISTER_SYMBOL(kSbFileMaxPath);
  REGISTER_SYMBOL(kSbFileSepChar);
  REGISTER_SYMBOL(kSbFileSepString);
  REGISTER_SYMBOL(kSbHasAc3Audio);
  REGISTER_SYMBOL(kSbHasMediaWebmVp9Support);
  REGISTER_SYMBOL(kSbHasThreadPrioritySupport);
  REGISTER_SYMBOL(kSbMallocAlignment);
  REGISTER_SYMBOL(kSbMaxThreadLocalKeys);
  REGISTER_SYMBOL(kSbMaxThreadNameLength);
  REGISTER_SYMBOL(kSbMediaMaxAudioBitrateInBitsPerSecond);
  REGISTER_SYMBOL(kSbMediaMaxVideoBitrateInBitsPerSecond);
  REGISTER_SYMBOL(kSbMediaVideoFrameAlignment);
  REGISTER_SYMBOL(kSbMemoryLogPath);
  REGISTER_SYMBOL(kSbMemoryPageSize);
  REGISTER_SYMBOL(kSbNetworkReceiveBufferSize);
  REGISTER_SYMBOL(kSbMaxThreads);
  REGISTER_SYMBOL(kSbPathSepChar);
  REGISTER_SYMBOL(kSbPathSepString);
  REGISTER_SYMBOL(kSbPreferredRgbaByteOrder);
  REGISTER_SYMBOL(kSbUserMaxSignedIn);
#endif  // SB_API_VERSION >= 12

#if SB_API_VERSION >= 13
  REGISTER_SYMBOL(SbSystemNetworkIsDisconnected);
#endif  // SB_API_VERSION >= 13
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
