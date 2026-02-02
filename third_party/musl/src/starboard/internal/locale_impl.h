// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_MUSL_SRC_STARBOARD_INTERNAL_LOCALE_IMPL_H_
#define THIRD_PARTY_MUSL_SRC_STARBOARD_INTERNAL_LOCALE_IMPL_H_

#include <locale.h>
#include <stdlib.h>
#include "libc.h"

#define LOCALE_NAME_MAX 23

struct __locale_map {
	const void *map;
	size_t map_size;
	char name[LOCALE_NAME_MAX+1];
	const struct __locale_map *next;
};

extern hidden const struct __locale_map __c_dot_utf8;
extern hidden const struct __locale_struct __c_locale;
extern hidden const struct __locale_struct __c_dot_utf8_locale;

#define C_LOCALE ((locale_t)&__c_locale)
#define UTF8_LOCALE ((locale_t)&__c_dot_utf8_locale)

#define CURRENT_LOCALE (__pthread_self()->locale)

// Starboard supports UTF-16 and UTF-32, both of which have a maximum character
// size of 4 bytes.
#undef MB_CUR_MAX
#define MB_CUR_MAX 4

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_INTERNAL_LOCALE_IMPL_H_
