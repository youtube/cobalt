// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef MINI_CHROMIUM_BASE_BASE_WRAPPER_H_
#define MINI_CHROMIUM_BASE_BASE_WRAPPER_H_

// Change the symbol name to avoid collisions with //base
#define Alias MAlias

#define AssertAcquired MAssertAcquired
#define FilePath MFilePath
#define CheckHeldAndUnmark MCheckHeldAndUnmark
#define CheckUnheldAndMark MCheckUnheldAndMark
#define ConditionVariable MConditionVariable
#define GetLogMessageHandler MGetLogMessageHandler
#define Lock MLock
#define LockImpl MLockImpl
#define LogMessage MLogMessage
#define PlatformThreadLocalStorage MPlatformThreadLocalStorage
#define RandBytes MRandBytes
#define RandBytesAsString MRandBytesAsString
#define RandDouble MRandDouble
#define RandGenerator MRandGenerator
#define RandInt MRandInt
#define RandUint64 MRandUint64
#define ReadUnicodeCharacter MReadUnicodeCharacter
#define SetLogMessageHandler MSetLogMessageHandler
#define StringAppendV MStringAppendV
#define StringPrintf MStringPrintf
#define ThreadLocalStorage MThreadLocalStorage
#define UTF16ToUTF8 MUTF16ToUTF8
#define UmaHistogramSparse MUmaHistogramSparse
#define UncheckedMalloc MUncheckedMalloc
#define WriteUnicodeCharacter MWriteUnicodeCharacter
#define c16len mc16len
#define utf8_nextCharSafeBody mutf8_nextCharSafeBody


#endif  // MINI_CHROMIUM_BASE_BASE_WRAPPER_H_
