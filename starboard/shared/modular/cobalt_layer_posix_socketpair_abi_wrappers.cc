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

extern "C" {

int __abi_wrap_socketpair(int domain,
                          int type,
                          int protocol,
                          int socket_vector[2]);

int socketpair(int domain, int type, int protocol, int socket_vector[2]) {
  return __abi_wrap_socketpair(domain, type, protocol, socket_vector);
}
}
