//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "starboard/system.h"

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "starboard/common/log.h"

namespace {

bool IsValidLocaleIdBcp47(const char* id) {
  return id && *id &&
         strncmp(id, "C", 2) != 0 &&
         strncmp(id, "POSIX", 6) != 0;
}

// Normalise a POSIX locale string to a BCP-47 language tag in place.
// e.g. "fr_FR.UTF-8" → "fr-FR", "en_US@euro" → "en-US".
void NormalizePosixLocaleToBcp47(char* buf) {
  // Strip modifier (@...).
  char* at = strchr(buf, '@');
  if (at) *at = '\0';
  // Strip codeset (....UTF-8).
  char* dot = strchr(buf, '.');
  if (dot) *dot = '\0';
  // Replace underscores with hyphens.
  for (char* p = buf; *p; ++p) {
    if (*p == '_') *p = '-';
  }
}

// Cache the BCP-47 locale string so SbSystemGetLocaleId() returns a stable
// pointer. A sentinel '\x01' means "already searched, nothing found".
const char* GetCachedLocaleBcp47() {
  static char cached[64] = {};
  if (cached[0] == '\x01')
    return NULL;
  if (cached[0])
    return cached;

  // 1. Check setlocale (works if the process environment was set before init).
  const char* id = setlocale(LC_MESSAGES, NULL);
  if (IsValidLocaleIdBcp47(id)) {
    strncpy(cached, id, sizeof(cached) - 1);
    NormalizePosixLocaleToBcp47(cached);
    return cached;
  }

  // 2. Check $LC_ALL / $LC_MESSAGES / $LANG environment variables.
  for (const char* var : { "LC_ALL", "LC_MESSAGES", "LANG" }) {
    id = getenv(var);
    if (IsValidLocaleIdBcp47(id)) {
      strncpy(cached, id, sizeof(cached) - 1);
      NormalizePosixLocaleToBcp47(cached);
      return cached;
    }
  }

  // 3. RDK-specific fallback: read LANG= from /etc/locale.conf (written by
  //    localectl or the platform locale manager). WPEFramework is typically
  //    started with LANG=C so this file is not reflected in the process
  //    environment even after localectl set-locale.
  FILE* f = fopen("/etc/locale.conf", "r");
  if (f) {
    char line[128];
    while (fgets(line, sizeof(line), f)) {
      if (strncmp(line, "LANG=", 5) == 0) {
        char* val = line + 5;
        // Strip optional surrounding quotes (LANG="en_US.UTF-8").
        if (*val == '"') ++val;
        // Trim trailing whitespace, newline, and closing quote.
        size_t len = strlen(val);
        while (len > 0 && (val[len - 1] == '\n' || val[len - 1] == '\r' ||
                            val[len - 1] == ' '  || val[len - 1] == '"'))
          val[--len] = '\0';
        if (IsValidLocaleIdBcp47(val)) {
          strncpy(cached, val, sizeof(cached) - 1);
          NormalizePosixLocaleToBcp47(cached);
          SB_LOG(INFO) << "SbSystemGetLocaleId: read from /etc/locale.conf: "
                       << cached;
          fclose(f);
          return cached;
        }
      }
    }
    fclose(f);
  }

  SB_LOG(WARNING) << "SbSystemGetLocaleId: no valid locale found, returning NULL";
  cached[0] = '\x01';
  return NULL;
}

}  // namespace

const char* SbSystemGetLocaleId() {
  return GetCachedLocaleBcp47();
}
