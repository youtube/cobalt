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

#include "cobalt/cobalt_web_contents_observer.h"

namespace cobalt {

CobaltWebContentsObserver::CobaltWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  LOG(INFO)
      << "CobaltWebContentsObserver::CobaltWebContentsObserver() constructor.";
  // Create browser-side mojo service component
  js_communication_host_ =
      std::make_unique<js_injection::JsCommunicationHost>(web_contents);

  // Inject a script at document start for all origins
  // const std::u16string script(u"console.log('Hello from JS injection');");

  const std::u16string script =
      uR"(
function arrayBufferToBase64(buffer) {
    const bytes = new Uint8Array(buffer);
    let binary = '';
    for (let i = 0; i < bytes.byteLength; i++) {
        binary += String.fromCharCode(bytes[i]);
    }
    return window.btoa(binary);  // Encode binary string to Base64
}
function base64ToArrayBuffer(base64) {
    const binaryString = window.atob(base64); // Decode Base64 string to binary string
    const bytes = new Uint8Array(binaryString.length);
    for (let i = 0; i < binaryString.length; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    return bytes.buffer;
}
class PlatformServiceClient {
    constructor(name) {
        this.name = name;
    }
    send(data) {
        // convert the ArrayBuffer to base64 string because java bridge only takes primitive types as input.
        const convertToB64 = arrayBufferToBase64(data);
        const responseData = Android_H5vccPlatformService.platformServiceSend(this.name, convertToB64);
        if (responseData === "") {
            return null;
        }
        // response_data has the synchronize response data converted to base64 string.
        // convert it to ArrayBuffer, and return the ArrayBuffer to client.
        return base64ToArrayBuffer(responseData);
    }
    close() {
        Android_H5vccPlatformService.closePlatformService(this.name);
    }
}
function initializeH5vccPlatformService() {
    console.log('initializeH5vccPlatformService');
    if (typeof Android_H5vccPlatformService === 'undefined') {
        return;
    }
    // On Chrobalt, register window.H5vccPlatformService
    window.H5vccPlatformService = {
        // Holds the callback functions for the platform services when open() is called.
        callbacks: {
        },
        callbackFromAndroid: (serviceID, dataFromJava) => {
            const arrayBuffer = base64ToArrayBuffer(dataFromJava);
            window.H5vccPlatformService.callbacks[serviceID].callback(serviceID, arrayBuffer);
        },
        has: (name) => {
            return Android_H5vccPlatformService.hasPlatformService(name);
        },
        open: function(name, callback) {
            if (typeof callback !== 'function') {
                throw new Error("window.H5vccPlatformService.open(), missing or invalid callback function.");
            }
            const serviceID = Object.keys(this.callbacks).length + 1;
            // Store the callback with the service ID, name, and callback
            window.H5vccPlatformService.callbacks[serviceID] = {
                name: name,
                callback: callback
            };
            Android_H5vccPlatformService.openPlatformService(serviceID, name);
            return new PlatformServiceClient(name);
        },
    }
}
initializeH5vccPlatformService();
)";

  const std::vector<std::string> allowed_origins({"*"});
  auto result = js_communication_host_->AddDocumentStartJavaScript(
      script, allowed_origins);
  CHECK(!result.error_message);
}

// Placeholder for a WebContentsObserver override
void CobaltWebContentsObserver::PrimaryMainDocumentElementAvailable() {
  LOG(INFO) << "Cobalt::PrimaryMainDocumentElementAvailable";
}

}  // namespace cobalt
