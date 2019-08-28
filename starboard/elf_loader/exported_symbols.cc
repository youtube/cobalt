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
#include "starboard/byte_swap.h"
#include "starboard/character.h"
#include "starboard/condition_variable.h"
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
#include "starboard/microphone.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/player.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/speech_recognizer.h"
#include "starboard/speech_synthesis.h"
#include "starboard/storage.h"
#include "starboard/string.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#include "starboard/time_zone.h"
#include "starboard/ui_navigation.h"

namespace starboard {
namespace elf_loader {

ExportedSymbols::ExportedSymbols() {
  map_["SbAccessibilityGetDisplaySettings"] =
      reinterpret_cast<const void*>(SbAccessibilityGetDisplaySettings);
  map_["SbAccessibilityGetTextToSpeechSettings"] =
      reinterpret_cast<const void*>(SbAccessibilityGetTextToSpeechSettings);
  map_["SbAudioSinkCreate"] = reinterpret_cast<const void*>(SbAudioSinkCreate);
  map_["SbAudioSinkDestroy"] =
      reinterpret_cast<const void*>(SbAudioSinkDestroy);
#if SB_API_VERSION >= 11
  map_["SbAudioSinkGetMinBufferSizeInFrames"] =
      reinterpret_cast<const void*>(SbAudioSinkGetMinBufferSizeInFrames);
#endif
  map_["SbAudioSinkGetNearestSupportedSampleFrequency"] =
      reinterpret_cast<const void*>(
          SbAudioSinkGetNearestSupportedSampleFrequency);
  map_["SbAudioSinkIsAudioFrameStorageTypeSupported"] =
      reinterpret_cast<const void*>(
          SbAudioSinkIsAudioFrameStorageTypeSupported);
  map_["SbAudioSinkIsAudioSampleTypeSupported"] =
      reinterpret_cast<const void*>(SbAudioSinkIsAudioSampleTypeSupported);
  map_["SbAudioSinkIsValid"] =
      reinterpret_cast<const void*>(SbAudioSinkIsValid);
  map_["SbByteSwapU16"] = reinterpret_cast<const void*>(SbByteSwapU16);
  map_["SbByteSwapU32"] = reinterpret_cast<const void*>(SbByteSwapU32);
  map_["SbByteSwapU64"] = reinterpret_cast<const void*>(SbByteSwapU64);
  map_["SbCharacterIsDigit"] =
      reinterpret_cast<const void*>(SbCharacterIsDigit);
  map_["SbCharacterIsHexDigit"] =
      reinterpret_cast<const void*>(SbCharacterIsHexDigit);
  map_["SbCharacterIsSpace"] =
      reinterpret_cast<const void*>(SbCharacterIsSpace);
  map_["SbCharacterToLower"] =
      reinterpret_cast<const void*>(SbCharacterToLower);
  map_["SbCharacterToUpper"] =
      reinterpret_cast<const void*>(SbCharacterToUpper);
  map_["SbConditionVariableBroadcast"] =
      reinterpret_cast<const void*>(SbConditionVariableBroadcast);
  map_["SbConditionVariableCreate"] =
      reinterpret_cast<const void*>(SbConditionVariableCreate);
  map_["SbConditionVariableDestroy"] =
      reinterpret_cast<const void*>(SbConditionVariableDestroy);
  map_["SbConditionVariableSignal"] =
      reinterpret_cast<const void*>(SbConditionVariableSignal);
  map_["SbConditionVariableWait"] =
      reinterpret_cast<const void*>(SbConditionVariableWait);
  map_["SbConditionVariableWaitTimed"] =
      reinterpret_cast<const void*>(SbConditionVariableWaitTimed);

#if SB_API_VERSION >= 11
  map_["SbCPUFeaturesGet"] = reinterpret_cast<const void*>(SbCPUFeaturesGet);
#endif

  map_["SbDecodeTargetGetInfo"] =
      reinterpret_cast<const void*>(SbDecodeTargetGetInfo);
  map_["SbDecodeTargetRelease"] =
      reinterpret_cast<const void*>(SbDecodeTargetRelease);
  map_["SbDirectoryCanOpen"] =
      reinterpret_cast<const void*>(SbDirectoryCanOpen);
  map_["SbDirectoryClose"] = reinterpret_cast<const void*>(SbDirectoryClose);
  map_["SbDirectoryCreate"] = reinterpret_cast<const void*>(SbDirectoryCreate);
  map_["SbDirectoryGetNext"] =
      reinterpret_cast<const void*>(SbDirectoryGetNext);
  map_["SbDirectoryOpen"] = reinterpret_cast<const void*>(SbDirectoryOpen);
  map_["SbDoubleAbsolute"] = reinterpret_cast<const void*>(SbDoubleAbsolute);
  map_["SbDoubleExponent"] = reinterpret_cast<const void*>(SbDoubleExponent);
  map_["SbDoubleFloor"] = reinterpret_cast<const void*>(SbDoubleFloor);
  map_["SbDoubleIsFinite"] = reinterpret_cast<const void*>(SbDoubleIsFinite);
  map_["SbDoubleIsNan"] = reinterpret_cast<const void*>(SbDoubleIsNan);
  map_["SbDrmCloseSession"] = reinterpret_cast<const void*>(SbDrmCloseSession);
  map_["SbDrmCreateSystem"] = reinterpret_cast<const void*>(SbDrmCreateSystem);
  map_["SbDrmDestroySystem"] =
      reinterpret_cast<const void*>(SbDrmDestroySystem);
  map_["SbDrmGenerateSessionUpdateRequest"] =
      reinterpret_cast<const void*>(SbDrmGenerateSessionUpdateRequest);

#if SB_API_VERSION >= 10
  map_["SbDrmIsServerCertificateUpdatable"] =
      reinterpret_cast<const void*>(SbDrmIsServerCertificateUpdatable);
  map_["SbDrmUpdateServerCertificate"] =
      reinterpret_cast<const void*>(SbDrmUpdateServerCertificate);
#endif

  map_["SbDrmUpdateSession"] =
      reinterpret_cast<const void*>(SbDrmUpdateSession);
  map_["SbEventCancel"] = reinterpret_cast<const void*>(SbEventCancel);
  map_["SbEventSchedule"] = reinterpret_cast<const void*>(SbEventSchedule);
  map_["SbFileCanOpen"] = reinterpret_cast<const void*>(SbFileCanOpen);
  map_["SbFileClose"] = reinterpret_cast<const void*>(SbFileClose);
  map_["SbFileDelete"] = reinterpret_cast<const void*>(SbFileDelete);
  map_["SbFileExists"] = reinterpret_cast<const void*>(SbFileExists);
  map_["SbFileFlush"] = reinterpret_cast<const void*>(SbFileFlush);
  map_["SbFileGetInfo"] = reinterpret_cast<const void*>(SbFileGetInfo);
  map_["SbFileGetPathInfo"] = reinterpret_cast<const void*>(SbFileGetPathInfo);
  map_["SbFileModeStringToFlags"] =
      reinterpret_cast<const void*>(SbFileModeStringToFlags);
  map_["SbFileOpen"] = reinterpret_cast<const void*>(SbFileOpen);
  map_["SbFileRead"] = reinterpret_cast<const void*>(SbFileRead);
  map_["SbFileSeek"] = reinterpret_cast<const void*>(SbFileSeek);
  map_["SbFileTruncate"] = reinterpret_cast<const void*>(SbFileTruncate);
  map_["SbFileWrite"] = reinterpret_cast<const void*>(SbFileWrite);
  map_["SbGetEglInterface"] = reinterpret_cast<const void*>(SbGetEglInterface);
  map_["SbGetGlesInterface"] =
      reinterpret_cast<const void*>(SbGetGlesInterface);
  map_["SbImageDecode"] = reinterpret_cast<const void*>(SbImageDecode);
  map_["SbImageIsDecodeSupported"] =
      reinterpret_cast<const void*>(SbImageIsDecodeSupported);
  map_["SbLog"] = reinterpret_cast<const void*>(SbLog);
  map_["SbLogFlush"] = reinterpret_cast<const void*>(SbLogFlush);
  map_["SbLogFormat"] = reinterpret_cast<const void*>(SbLogFormat);
  map_["SbLogIsTty"] = reinterpret_cast<const void*>(SbLogIsTty);
  map_["SbLogRaw"] = reinterpret_cast<const void*>(SbLogRaw);
  map_["SbLogRawFormat"] = reinterpret_cast<const void*>(SbLogRawFormat);
  map_["SbMediaCanPlayMimeAndKeySystem"] =
      reinterpret_cast<const void*>(SbMediaCanPlayMimeAndKeySystem);

#if SB_API_VERSION >= 10
  map_["SbMediaGetAudioBufferBudget"] =
      reinterpret_cast<const void*>(SbMediaGetAudioBufferBudget);
  map_["SbMediaGetBufferAlignment"] =
      reinterpret_cast<const void*>(SbMediaGetBufferAlignment);
  map_["SbMediaGetBufferAllocationUnit"] =
      reinterpret_cast<const void*>(SbMediaGetBufferAllocationUnit);
  map_["SbMediaGetBufferGarbageCollectionDurationThreshold"] =
      reinterpret_cast<const void*>(
          SbMediaGetBufferGarbageCollectionDurationThreshold);
  map_["SbMediaGetBufferPadding"] =
      reinterpret_cast<const void*>(SbMediaGetBufferPadding);
  map_["SbMediaGetInitialBufferCapacity"] =
      reinterpret_cast<const void*>(SbMediaGetInitialBufferCapacity);
  map_["SbMediaGetMaxBufferCapacity"] =
      reinterpret_cast<const void*>(SbMediaGetMaxBufferCapacity);
  map_["SbMediaGetProgressiveBufferBudget"] =
      reinterpret_cast<const void*>(SbMediaGetProgressiveBufferBudget);
  map_["SbMediaGetVideoBufferBudget"] =
      reinterpret_cast<const void*>(SbMediaGetVideoBufferBudget);
  map_["SbMediaIsBufferPoolAllocateOnDemand"] =
      reinterpret_cast<const void*>(SbMediaIsBufferPoolAllocateOnDemand);
  map_["SbMediaIsBufferUsingMemoryPool"] =
      reinterpret_cast<const void*>(SbMediaIsBufferUsingMemoryPool);
#endif

#if SB_API_VERSION >= 11
  map_["SbMediaSetAudioWriteDuration"] =
      reinterpret_cast<const void*>(SbMediaSetAudioWriteDuration);
#endif

  map_["SbMemoryAllocateAlignedUnchecked"] =
      reinterpret_cast<const void*>(SbMemoryAllocateAlignedUnchecked);
  map_["SbMemoryAllocateUnchecked"] =
      reinterpret_cast<const void*>(SbMemoryAllocateUnchecked);
  map_["SbMemoryCompare"] = reinterpret_cast<const void*>(SbMemoryCompare);
  map_["SbMemoryCopy"] = reinterpret_cast<const void*>(SbMemoryCopy);
  map_["SbMemoryFindByte"] = reinterpret_cast<const void*>(SbMemoryFindByte);
  map_["SbMemoryFree"] = reinterpret_cast<const void*>(SbMemoryFree);
  map_["SbMemoryFreeAligned"] =
      reinterpret_cast<const void*>(SbMemoryFreeAligned);

#if SB_HAS(MMAP)
  map_["SbMemoryMap"] = reinterpret_cast<const void*>(SbMemoryMap);
#endif
  map_["SbMemoryMove"] = reinterpret_cast<const void*>(SbMemoryMove);

#if SB_API_VERSION >= 10 && SB_HAS(MMAP)
  map_["SbMemoryProtect"] = reinterpret_cast<const void*>(SbMemoryProtect);
#endif

  map_["SbMemoryReallocateUnchecked"] =
      reinterpret_cast<const void*>(SbMemoryReallocateUnchecked);
  map_["SbMemorySet"] = reinterpret_cast<const void*>(SbMemorySet);

#if SB_HAS(MMAP)
  map_["SbMemoryUnmap"] = reinterpret_cast<const void*>(SbMemoryUnmap);
#endif

#if SB_HAS(MICROPHONE)
  map_["SbMicrophoneClose"] = reinterpret_cast<const void*>(SbMicrophoneClose);
  map_["SbMicrophoneCreate"] =
      reinterpret_cast<const void*>(SbMicrophoneCreate);
  map_["SbMicrophoneDestroy"] =
      reinterpret_cast<const void*>(SbMicrophoneDestroy);
  map_["SbMicrophoneGetAvailable"] =
      reinterpret_cast<const void*>(SbMicrophoneGetAvailable);
  map_["SbMicrophoneIsSampleRateSupported"] =
      reinterpret_cast<const void*>(SbMicrophoneIsSampleRateSupported);
  map_["SbMicrophoneOpen"] = reinterpret_cast<const void*>(SbMicrophoneOpen);
  map_["SbMicrophoneRead"] = reinterpret_cast<const void*>(SbMicrophoneRead);
#endif

  map_["SbMutexAcquire"] = reinterpret_cast<const void*>(SbMutexAcquire);
  map_["SbMutexAcquireTry"] = reinterpret_cast<const void*>(SbMutexAcquireTry);
  map_["SbMutexCreate"] = reinterpret_cast<const void*>(SbMutexCreate);
  map_["SbMutexDestroy"] = reinterpret_cast<const void*>(SbMutexDestroy);
  map_["SbMutexRelease"] = reinterpret_cast<const void*>(SbMutexRelease);
  map_["SbOnce"] = reinterpret_cast<const void*>(SbOnce);

  map_["SbPlayerCreate"] = reinterpret_cast<const void*>(SbPlayerCreate);
  map_["SbPlayerDestroy"] = reinterpret_cast<const void*>(SbPlayerDestroy);
  map_["SbPlayerGetCurrentFrame"] =
      reinterpret_cast<const void*>(SbPlayerGetCurrentFrame);

#if SB_API_VERSION >= 10
  map_["SbPlayerGetInfo2"] = reinterpret_cast<const void*>(SbPlayerGetInfo2);
  map_["SbPlayerGetMaximumNumberOfSamplesPerWrite"] =
      reinterpret_cast<const void*>(SbPlayerGetMaximumNumberOfSamplesPerWrite);

#endif

  map_["SbPlayerOutputModeSupported"] =
      reinterpret_cast<const void*>(SbPlayerOutputModeSupported);

#if SB_API_VERSION >= 10
  map_["SbPlayerSeek2"] = reinterpret_cast<const void*>(SbPlayerSeek2);
#endif

  map_["SbPlayerSetBounds"] = reinterpret_cast<const void*>(SbPlayerSetBounds);
  map_["SbPlayerSetPlaybackRate"] =
      reinterpret_cast<const void*>(SbPlayerSetPlaybackRate);
  map_["SbPlayerSetVolume"] = reinterpret_cast<const void*>(SbPlayerSetVolume);
  map_["SbPlayerWriteEndOfStream"] =
      reinterpret_cast<const void*>(SbPlayerWriteEndOfStream);

#if SB_API_VERSION >= 10
  map_["SbPlayerWriteSample2"] =
      reinterpret_cast<const void*>(SbPlayerWriteSample2);
#endif

  map_["SbSocketAccept"] = reinterpret_cast<const void*>(SbSocketAccept);
  map_["SbSocketBind"] = reinterpret_cast<const void*>(SbSocketBind);
  map_["SbSocketClearLastError"] =
      reinterpret_cast<const void*>(SbSocketClearLastError);
  map_["SbSocketConnect"] = reinterpret_cast<const void*>(SbSocketConnect);
  map_["SbSocketCreate"] = reinterpret_cast<const void*>(SbSocketCreate);
  map_["SbSocketDestroy"] = reinterpret_cast<const void*>(SbSocketDestroy);
  map_["SbSocketFreeResolution"] =
      reinterpret_cast<const void*>(SbSocketFreeResolution);
  map_["SbSocketGetInterfaceAddress"] =
      reinterpret_cast<const void*>(SbSocketGetInterfaceAddress);
  map_["SbSocketGetLastError"] =
      reinterpret_cast<const void*>(SbSocketGetLastError);
  map_["SbSocketGetLocalAddress"] =
      reinterpret_cast<const void*>(SbSocketGetLocalAddress);
  map_["SbSocketIsConnected"] =
      reinterpret_cast<const void*>(SbSocketIsConnected);
  map_["SbSocketIsConnectedAndIdle"] =
      reinterpret_cast<const void*>(SbSocketIsConnectedAndIdle);
  map_["SbSocketJoinMulticastGroup"] =
      reinterpret_cast<const void*>(SbSocketJoinMulticastGroup);
  map_["SbSocketListen"] = reinterpret_cast<const void*>(SbSocketListen);
  map_["SbSocketReceiveFrom"] =
      reinterpret_cast<const void*>(SbSocketReceiveFrom);
  map_["SbSocketResolve"] = reinterpret_cast<const void*>(SbSocketResolve);
  map_["SbSocketSendTo"] = reinterpret_cast<const void*>(SbSocketSendTo);
  map_["SbSocketSetBroadcast"] =
      reinterpret_cast<const void*>(SbSocketSetBroadcast);
  map_["SbSocketSetReceiveBufferSize"] =
      reinterpret_cast<const void*>(SbSocketSetReceiveBufferSize);
  map_["SbSocketSetReuseAddress"] =
      reinterpret_cast<const void*>(SbSocketSetReuseAddress);
  map_["SbSocketSetSendBufferSize"] =
      reinterpret_cast<const void*>(SbSocketSetSendBufferSize);
  map_["SbSocketSetTcpKeepAlive"] =
      reinterpret_cast<const void*>(SbSocketSetTcpKeepAlive);
  map_["SbSocketSetTcpNoDelay"] =
      reinterpret_cast<const void*>(SbSocketSetTcpNoDelay);
  map_["SbSocketSetTcpWindowScaling"] =
      reinterpret_cast<const void*>(SbSocketSetTcpWindowScaling);
  map_["SbSocketWaiterAdd"] = reinterpret_cast<const void*>(SbSocketWaiterAdd);
  map_["SbSocketWaiterCreate"] =
      reinterpret_cast<const void*>(SbSocketWaiterCreate);
  map_["SbSocketWaiterDestroy"] =
      reinterpret_cast<const void*>(SbSocketWaiterDestroy);
  map_["SbSocketWaiterRemove"] =
      reinterpret_cast<const void*>(SbSocketWaiterRemove);
  map_["SbSocketWaiterWait"] =
      reinterpret_cast<const void*>(SbSocketWaiterWait);
  map_["SbSocketWaiterWaitTimed"] =
      reinterpret_cast<const void*>(SbSocketWaiterWaitTimed);
  map_["SbSocketWaiterWakeUp"] =
      reinterpret_cast<const void*>(SbSocketWaiterWakeUp);

#if SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5
  map_["SbSpeechRecognizerCreate"] =
      reinterpret_cast<const void*>(SbSpeechRecognizerCreate);
  map_["SbSpeechRecognizerDestroy"] =
      reinterpret_cast<const void*>(SbSpeechRecognizerDestroy);
  map_["SbSpeechRecognizerStart"] =
      reinterpret_cast<const void*>(SbSpeechRecognizerStart);
  map_["SbSpeechRecognizerStop"] =
      reinterpret_cast<const void*>(SbSpeechRecognizerStop);
#endif

#if SB_HAS(SPEECH_SYNTHESIS)
  map_["SbSpeechSynthesisCancel"] =
      reinterpret_cast<const void*>(SbSpeechSynthesisCancel);
  map_["SbSpeechSynthesisSpeak"] =
      reinterpret_cast<const void*>(SbSpeechSynthesisSpeak);
#endif

  map_["SbStorageCloseRecord"] =
      reinterpret_cast<const void*>(SbStorageCloseRecord);
  map_["SbStorageDeleteRecord"] =
      reinterpret_cast<const void*>(SbStorageDeleteRecord);
  map_["SbStorageGetRecordSize"] =
      reinterpret_cast<const void*>(SbStorageGetRecordSize);
  map_["SbStorageOpenRecord"] =
      reinterpret_cast<const void*>(SbStorageOpenRecord);
  map_["SbStorageReadRecord"] =
      reinterpret_cast<const void*>(SbStorageReadRecord);
  map_["SbStorageWriteRecord"] =
      reinterpret_cast<const void*>(SbStorageWriteRecord);
  map_["SbStringCompare"] = reinterpret_cast<const void*>(SbStringCompare);
  map_["SbStringCompareAll"] =
      reinterpret_cast<const void*>(SbStringCompareAll);
  map_["SbStringCompareNoCase"] =
      reinterpret_cast<const void*>(SbStringCompareNoCase);
  map_["SbStringCompareNoCaseN"] =
      reinterpret_cast<const void*>(SbStringCompareNoCaseN);
  map_["SbStringConcat"] = reinterpret_cast<const void*>(SbStringConcat);
  map_["SbStringCopy"] = reinterpret_cast<const void*>(SbStringCopy);
  map_["SbStringDuplicate"] = reinterpret_cast<const void*>(SbStringDuplicate);
  map_["SbStringFindCharacter"] =
      reinterpret_cast<const void*>(SbStringFindCharacter);
  map_["SbStringFindLastCharacter"] =
      reinterpret_cast<const void*>(SbStringFindLastCharacter);
  map_["SbStringFindString"] =
      reinterpret_cast<const void*>(SbStringFindString);
  map_["SbStringFormat"] = reinterpret_cast<const void*>(SbStringFormat);
  map_["SbStringFormatWide"] =
      reinterpret_cast<const void*>(SbStringFormatWide);
  map_["SbStringGetLength"] = reinterpret_cast<const void*>(SbStringGetLength);
  map_["SbStringParseDouble"] =
      reinterpret_cast<const void*>(SbStringParseDouble);
  map_["SbStringParseSignedInteger"] =
      reinterpret_cast<const void*>(SbStringParseSignedInteger);
  map_["SbStringParseUInt64"] =
      reinterpret_cast<const void*>(SbStringParseUInt64);
  map_["SbStringParseUnsignedInteger"] =
      reinterpret_cast<const void*>(SbStringParseUnsignedInteger);
  map_["SbStringScan"] = reinterpret_cast<const void*>(SbStringScan);
  map_["SbSystemBinarySearch"] =
      reinterpret_cast<const void*>(SbSystemBinarySearch);
  map_["SbSystemBreakIntoDebugger"] =
      reinterpret_cast<const void*>(SbSystemBreakIntoDebugger);
  map_["SbSystemClearLastError"] =
      reinterpret_cast<const void*>(SbSystemClearLastError);
  map_["SbSystemGetConnectionType"] =
      reinterpret_cast<const void*>(SbSystemGetConnectionType);
  map_["SbSystemGetDeviceType"] =
      reinterpret_cast<const void*>(SbSystemGetDeviceType);
  map_["SbSystemGetErrorString"] =
      reinterpret_cast<const void*>(SbSystemGetErrorString);

#if SB_API_VERSION >= 11
  map_["SbSystemGetExtension"] =
      reinterpret_cast<const void*>(SbSystemGetExtension);
#endif

  map_["SbSystemGetLastError"] =
      reinterpret_cast<const void*>(SbSystemGetLastError);
  map_["SbSystemGetLocaleId"] =
      reinterpret_cast<const void*>(SbSystemGetLocaleId);
  map_["SbSystemGetNumberOfProcessors"] =
      reinterpret_cast<const void*>(SbSystemGetNumberOfProcessors);
  map_["SbSystemGetPath"] = reinterpret_cast<const void*>(SbSystemGetPath);
  map_["SbSystemGetProperty"] =
      reinterpret_cast<const void*>(SbSystemGetProperty);
  map_["SbSystemGetRandomData"] =
      reinterpret_cast<const void*>(SbSystemGetRandomData);
  map_["SbSystemGetRandomUInt64"] =
      reinterpret_cast<const void*>(SbSystemGetRandomUInt64);
  map_["SbSystemGetStack"] = reinterpret_cast<const void*>(SbSystemGetStack);
  map_["SbSystemGetTotalCPUMemory"] =
      reinterpret_cast<const void*>(SbSystemGetTotalCPUMemory);
  map_["SbSystemGetTotalGPUMemory"] =
      reinterpret_cast<const void*>(SbSystemGetTotalGPUMemory);
  map_["SbSystemGetUsedCPUMemory"] =
      reinterpret_cast<const void*>(SbSystemGetUsedCPUMemory);
  map_["SbSystemGetUsedGPUMemory"] =
      reinterpret_cast<const void*>(SbSystemGetUsedGPUMemory);
  map_["SbSystemHasCapability"] =
      reinterpret_cast<const void*>(SbSystemHasCapability);
  map_["SbSystemHideSplashScreen"] =
      reinterpret_cast<const void*>(SbSystemHideSplashScreen);
  map_["SbSystemIsDebuggerAttached"] =
      reinterpret_cast<const void*>(SbSystemIsDebuggerAttached);
  map_["SbSystemRaisePlatformError"] =
      reinterpret_cast<const void*>(SbSystemRaisePlatformError);
  map_["SbSystemRequestPause"] =
      reinterpret_cast<const void*>(SbSystemRequestPause);
  map_["SbSystemRequestStop"] =
      reinterpret_cast<const void*>(SbSystemRequestStop);
  map_["SbSystemRequestSuspend"] =
      reinterpret_cast<const void*>(SbSystemRequestSuspend);
  map_["SbSystemRequestUnpause"] =
      reinterpret_cast<const void*>(SbSystemRequestUnpause);

#if SB_API_VERSION >= 11
  map_["SbSystemSignWithCertificationSecretKey"] =
      reinterpret_cast<const void*>(SbSystemSignWithCertificationSecretKey);
#endif

  map_["SbSystemSort"] = reinterpret_cast<const void*>(SbSystemSort);

#if SB_API_VERSION >= 10
  map_["SbSystemSupportsResume"] =
      reinterpret_cast<const void*>(SbSystemSupportsResume);
#endif

  map_["SbSystemSymbolize"] = reinterpret_cast<const void*>(SbSystemSymbolize);

#if SB_API_VERSION >= 11
  map_["SbThreadContextGetPointer"] =
      reinterpret_cast<const void*>(SbThreadContextGetPointer);
#endif

  map_["SbThreadCreate"] = reinterpret_cast<const void*>(SbThreadCreate);
  map_["SbThreadCreateLocalKey"] =
      reinterpret_cast<const void*>(SbThreadCreateLocalKey);
  map_["SbThreadDestroyLocalKey"] =
      reinterpret_cast<const void*>(SbThreadDestroyLocalKey);
  map_["SbThreadDetach"] = reinterpret_cast<const void*>(SbThreadDetach);
  map_["SbThreadGetCurrent"] =
      reinterpret_cast<const void*>(SbThreadGetCurrent);
  map_["SbThreadGetId"] = reinterpret_cast<const void*>(SbThreadGetId);
  map_["SbThreadGetLocalValue"] =
      reinterpret_cast<const void*>(SbThreadGetLocalValue);
  map_["SbThreadIsEqual"] = reinterpret_cast<const void*>(SbThreadIsEqual);
  map_["SbThreadJoin"] = reinterpret_cast<const void*>(SbThreadJoin);

#if SB_API_VERSION >= 11
  map_["SbThreadSamplerCreate"] =
      reinterpret_cast<const void*>(SbThreadSamplerCreate);

  map_["SbThreadSamplerDestroy"] =
      reinterpret_cast<const void*>(SbThreadSamplerDestroy);
  map_["SbThreadSamplerFreeze"] =
      reinterpret_cast<const void*>(SbThreadSamplerFreeze);
  map_["SbThreadSamplerThaw"] =
      reinterpret_cast<const void*>(SbThreadSamplerThaw);
#endif

  map_["SbThreadSetLocalValue"] =
      reinterpret_cast<const void*>(SbThreadSetLocalValue);
  map_["SbThreadSetName"] = reinterpret_cast<const void*>(SbThreadSetName);
  map_["SbThreadSleep"] = reinterpret_cast<const void*>(SbThreadSleep);
  map_["SbThreadYield"] = reinterpret_cast<const void*>(SbThreadYield);
  map_["SbTimeGetMonotonicNow"] =
      reinterpret_cast<const void*>(SbTimeGetMonotonicNow);

#if SB_HAS(TIME_THREAD_NOW)
  map_["SbTimeGetMonotonicThreadNow"] =
      reinterpret_cast<const void*>(SbTimeGetMonotonicThreadNow);
#endif

  map_["SbTimeGetNow"] = reinterpret_cast<const void*>(SbTimeGetNow);
  map_["SbTimeZoneGetCurrent"] =
      reinterpret_cast<const void*>(SbTimeZoneGetCurrent);
  map_["SbTimeZoneGetName"] = reinterpret_cast<const void*>(SbTimeZoneGetName);

#if SB_API_VERSION >= SB_UI_NAVIGATION_VERSION
  map_["SbUiNavGetInterface"] =
      reinterpret_cast<const void*>(SbUiNavGetInterface);
#endif

  map_["SbUserGetCurrent"] = reinterpret_cast<const void*>(SbUserGetCurrent);
  map_["SbUserGetProperty"] = reinterpret_cast<const void*>(SbUserGetProperty);
  map_["SbUserGetPropertySize"] =
      reinterpret_cast<const void*>(SbUserGetPropertySize);
  map_["SbWindowCreate"] = reinterpret_cast<const void*>(SbWindowCreate);
  map_["SbWindowDestroy"] = reinterpret_cast<const void*>(SbWindowDestroy);

#if SB_API_VERSION >= 11
  map_["SbWindowGetDiagonalSizeInInches"] =
      reinterpret_cast<const void*>(SbWindowGetDiagonalSizeInInches);
#endif

  map_["SbWindowGetPlatformHandle"] =
      reinterpret_cast<const void*>(SbWindowGetPlatformHandle);
  map_["SbWindowGetSize"] = reinterpret_cast<const void*>(SbWindowGetSize);
  map_["SbWindowSetDefaultOptions"] =
      reinterpret_cast<const void*>(SbWindowSetDefaultOptions);
}

const void* ExportedSymbols::Lookup(const char* name) {
  const void* ret = map_[name];
  SB_CHECK(ret) << name;
  return ret;
}
}  // namespace elf_loader
}  // namespace starboard
