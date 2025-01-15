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

#include "cobalt/cobalt_shell.h"
#include "base/command_line.h"
#include "content/public/browser/presentation_receiver_flags.h"
#include "content/public/common/content_switches.h"

// This file defines a Cobalt specific shell entry point, that delegates most
// of its functionality to content::Shell. This allows overrides to
// WebContentsDelegate and WebContentsObserver interfaces.
//
// We expect to fully migrate away from depending on content::Shell in the
// future.

namespace cobalt {

// std::unique_ptr<js_injection::JsCommunicationHost>
//     CobaltShell::js_communication_host_;

CobaltShell::CobaltShell(std::unique_ptr<content::WebContents> web_contents)
    : content::Shell(std::move(web_contents), false) {
  LOG(INFO) << "Colin: CobaltShell::CobaltShell() ";
}

// static
content::Shell* CobaltShell::CreateShell(
    std::unique_ptr<content::WebContents> web_contents,
    const gfx::Size& initial_size,
    bool should_set_delegate) {
  LOG(INFO) << "Colin: CobaltShell::CreateShell() should_set_delegate is "
            << should_set_delegate;
  content::WebContents* raw_web_contents = web_contents.get();
  // Create a Cobalt specific shell instance
  CobaltShell* shell = new CobaltShell(std::move(web_contents));
  if (should_set_delegate) {
    LOG(INFO) << "Colin: CobaltShell::CreateShell() call "
                 "shell->web_contents()->SetDelegate(shell); shell is "
              << shell << ", shell->web_contents() is "
              << shell->web_contents();
    shell->web_contents()->SetDelegate(shell);
  }

  LOG(INFO) << "Colin: CobaltShell is " << shell;

  // Delegate the rest of Shell setup to content::Shell
  return content::Shell::CreateShellFromPointer(shell, raw_web_contents,
                                                initial_size, false);
}

// static
content::Shell* CobaltShell::CreateNewWindow(
    content::BrowserContext* browser_context,
    const GURL& url,
    const scoped_refptr<content::SiteInstance>& site_instance,
    const gfx::Size& initial_size) {
  LOG(INFO) << "Colin: CobaltShell::CreateNewWindow()";
  content::WebContents::CreateParams create_params(browser_context,
                                                   site_instance);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePresentationReceiverForTesting)) {
    create_params.starting_sandbox_flags =
        content::kPresentationReceiverSandboxFlags;
  }
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);
  content::Shell* shell =
      CreateShell(std::move(web_contents), AdjustWindowSize(initial_size),
                  true /* should_set_delegate */);

  if (!url.is_empty()) {
    shell->LoadURL(url);
  }
  return shell;
}

// Placeholder for a WebContentsObserver override
void CobaltShell::PrimaryMainDocumentElementAvailable() {
  LOG(INFO) << "CobaltShell::PrimaryMainDocumentElementAvailable";
}

void CobaltShell::WebContentsCreated(content::WebContents* source_contents,
                                     int opener_render_process_id,
                                     int opener_render_frame_id,
                                     const std::string& frame_name,
                                     const GURL& target_url,
                                     content::WebContents* new_contents) {
  LOG(INFO) << "CobaltShell::WebContentsCreated";

  // Quick hack to demo injecting scripts
  // Create browser-side mojo service component

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

  std::unique_ptr<js_injection::JsCommunicationHost> js_communication_host =
      std::make_unique<js_injection::JsCommunicationHost>(new_contents);

  const std::vector<std::string> allowed_origins({"*"});
  auto result = js_communication_host->AddDocumentStartJavaScript(
      script, allowed_origins);
  CHECK(!result.error_message);
  // End inject
}

void CobaltShell::PortalWebContentsCreated(
    content::WebContents* portal_web_contents) {
  LOG(INFO) << "CobaltShell::PortalWebContentsCreated";
}

}  // namespace cobalt
