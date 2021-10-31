/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "glimp/shaders/hash_glsl_source.h"

namespace glimp {
namespace shaders {

namespace {
// Takes the current hash and combines it with a new character to produce a
// new hash.
uint32_t UpdateHash(uint32_t hash, char update_char) {
  hash += update_char;
  hash += (hash << 10);
  hash ^= (hash >> 6);
  return hash;
}

// Applies some additional calculations to the hash after all characters
// have been accumulated.
uint32_t FinalizeHash(uint32_t hash) {
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  return hash;
}

const char* ScanUntilNonWhitespace(const char* source) {
  uint32_t i = 0;
  for (; source[i] != '\0'; ++i) {
    if (source[i] == ' ' || source[i] == '\t' || source[i] == '\n') {
      continue;
    }

    if (source[i] == '/' && source[i + 1] == '/') {
      // If we see a comment, leave it out of the hash.
      while (source[i] != '\n' && source[i] != '\0') {
        ++i;
      }
      if (source[i] == '\0') {
        break;
      } else {
        continue;
      }
    }
    break;
  }

  return &source[i];
}

const char* ScanUntilHashableCharacter(const char* source) {
  while (true) {
    const char* next_char = ScanUntilNonWhitespace(source);

    // If we find empty scopes (e.g:
    //
    // {
    //   // Stage 0: Texture
    // }
    //
    // Skip past them without hashing those characters.
    if (*next_char == '{') {
      const char* next_next_char = ScanUntilNonWhitespace(next_char + 1);
      if (*next_next_char == '}') {
        source = next_next_char + 1;
        continue;
      }
    }
    return next_char;
  }
}
}  // namespace

uint32_t HashGLSLSource(const char* source) {
  uint32_t hash = 0;
  const char* cur_char = source;
  while (true) {
    cur_char = ScanUntilHashableCharacter(cur_char);
    if (*cur_char == '\0') {
      break;
    }

    hash = UpdateHash(hash, *cur_char);
    ++cur_char;
  }
  return FinalizeHash(hash);
}

}  // namespace shaders
}  // namespace glimp
