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

#ifndef COBALT_SHELL_COMMON_DEVICE_AUTHENTICATION_H_
#define COBALT_SHELL_COMMON_DEVICE_AUTHENTICATION_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "starboard/configuration.h"
#include "url/gurl.h"

namespace content {

// Takes a GURL and returns a new GURL with device authentication parameters
// appended as query strings. These parameters typically include a signature,
// certification scope, and timestamp to authenticate the device request.
GURL GetDeviceAuthenticationSignedURL(const GURL& url);

// Returns a base64 encoded SHA256 hash representing the device authentication
// signature, the device certification scope, and the current time as query
// parameters in a string. The signature is the result of signing a message,
// containing the certification scope and the current time, with a secret key
// provided by the device.
std::string GetDeviceAuthenticationSignedURLQueryString();

// The following methods are deterministic helper functions used in the
// implementation of the public method above.  They are declared here so that
// they can be tested in (white box) unit tests.

// Similar to the method above, but all platform queries are resolved to make
// this a deterministic and testable function.
std::string GetDeviceAuthenticationSignedURLQueryStringFromComponents(
    const std::string& cert_scope,
    const std::string& start_time,
    const std::string& base64_signature);

// Given the certification parameters, computes a message to be used as input
// to the signing process.
std::string ComputeMessage(const std::string& cert_scope,
                           const std::string& start_time);

// Given a message (arbitrary sequence of bytes) and a base64-encoded key
// key, compute the HMAC-SHA256 signature and store it in the |signature_hash|
// out parameter.  Note that 32 bytes will be written to the output hash, it is
// an error if |signature_hash_size_in_bytes| is less than 32.
void ComputeHMACSHA256SignatureWithProvidedKey(
    const std::string& message,
    const std::string& base64_key,
    uint8_t* signature_hash,
    size_t signature_hash_size_in_bytes);

}  // namespace content

#endif  // COBALT_SHELL_COMMON_DEVICE_AUTHENTICATION_H_
