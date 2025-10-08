/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_RPC_HTTPD_H_
#define SRC_TRACE_PROCESSOR_RPC_HTTPD_H_

#include <memory>
#include <string>

<<<<<<< HEAD
namespace perfetto::trace_processor {
=======
namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

class TraceProcessor;

// Starts a RPC server that handles requests using protobuf-over-HTTP.
// It takes control of the calling thread and does not return.
<<<<<<< HEAD
// preloaded_instance is optional. If non-null, the HTTP server will adopt
// an existing instance with a pre-loaded trace. If null, it will create a new
// instance when pushing data into the /parse endpoint.
// listen_ip is the ip address which http server will listen on,
// it can be an ipv4 or an ipv6 or a domain.
// port_number is the port which http server will listen on.
void RunHttpRPCServer(std::unique_ptr<TraceProcessor> preloaded_instance,
                      bool is_preloaded_eof,
                      const std::string& listen_ip,
                      const std::string& port_number);

}  // namespace perfetto::trace_processor
=======
// The unique_ptr argument is optional. If non-null, the HTTP server will adopt
// an existing instance with a pre-loaded trace. If null, it will create a new
// instance when pushing data into the /parse endpoint.
void RunHttpRPCServer(std::unique_ptr<TraceProcessor>, std::string);

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_RPC_HTTPD_H_
