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
#define FilePath MFilePath
#define GetLogMessageHandler MGetLogMessageHandler
#define LogMessage MLogMessage
#define ReadUnicodeCharacter MReadUnicodeCharacter
#define SetLogMessageHandler MSetLogMessageHandler
#define UTF16ToUTF8 MUTF16ToUTF8
#define UmaHistogramSparse MUmaHistogramSparse
#define WriteUnicodeCharacter MWriteUnicodeCharacter
#define c16len mc16len

#endif  // MINI_CHROMIUM_BASE_BASE_WRAPPER_H_
