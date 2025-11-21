// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License-for-dev at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_COMMON_LIBC_LOCALE_LOCALE_H_
#define COBALT_COMMON_LIBC_LOCALE_LOCALE_H_

#include <locale.h>

#ifdef __cplusplus
extern "C" {
#endif

char* setlocale(int category, const char* locale);
locale_t newlocale(int category_mask, const char* locale, locale_t base);
locale_t uselocale(locale_t newloc);
void freelocale(locale_t loc);
struct lconv* localeconv(void);
locale_t duplocale(locale_t loc);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_COMMON_LIBC_LOCALE_LOCALE_H_
