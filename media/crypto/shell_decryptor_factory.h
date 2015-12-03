/*
 * Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef MEDIA_CRYPTO_SHELL_DECRYPTOR_FACTORY_H_
#define MEDIA_CRYPTO_SHELL_DECRYPTOR_FACTORY_H_

#include <map>
#include <string>

#include "media/base/decryptor.h"
#include "media/base/decryptor_client.h"

namespace media {

// Serves as a registry and factory for decryptors.
// This allows a complete decoupling so that the media stack does not need
// explicit knowledge of any particular decryptor or how it should be created.
class MEDIA_EXPORT ShellDecryptorFactory {
 public:
  // Creates a decryptor.  Returns NULL on failure.
  typedef base::Callback<media::Decryptor*(media::DecryptorClient*)> CreateCB;

  static bool Supports(const std::string& key_system);

  static Decryptor* Create(const std::string& key_system,
                           DecryptorClient* client);

  static void RegisterDecryptor(const std::string& key_system,
                                const CreateCB& create_cb);

 private:
  typedef std::map<std::string, CreateCB> DecryptorRegistry;
  static DecryptorRegistry registry_;
};

}  // namespace media

#endif  // MEDIA_CRYPTO_SHELL_DECRYPTOR_FACTORY_H_
