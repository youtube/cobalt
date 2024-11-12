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
        const decoder = new TextDecoder('utf-8');
        const text = decoder.decode(data);
        var convert_to_b64 = arrayBufferToBase64(data);
        const response_data = Android_H5vccPlatformService.platform_service_send(this.name, convert_to_b64);
        if (response_data === "") {
            return null;
        }

        // handle synchronize response
        return base64ToArrayBuffer(response_data);
    }

    close() {
        Android_H5vccPlatformService.close_platform_service(this.name);
    }
}

export function initializeH5vccPlatformService() {
    if (typeof Android_H5vccPlatformService === 'undefined') {
        return;
    }

    // On Chrobalt
    window.H5vccPlatformService = {
        callbacks: {
        },
        callback_from_android: (serviceID, dataFromJava) => {
            // TODO(b/372558900): make async callback work.
            const binaryString = window.atob(dataFromJava);
            const len = binaryString.length;
            const bytes = new Uint8Array(len);
            for (let i = 0; i < len; i++) {
                bytes[i] = binaryString.charCodeAt(i);
            }
            const arrayBuffer = bytes.buffer;
            window.H5vccPlatformService.callbacks[serviceID].callback(arrayBuffer);
        },
        has: (name) => {
            return Android_H5vccPlatformService.has_platform_service(name);
        },
        open: function(name, callback) {
            if (typeof callback !== 'function') {
                throw new Error("Missing or invalid callback function.");
            }

            const serviceId = Object.keys(this.callbacks).length + 1;
            // Store the callback with the service ID, name, and callback
            window.H5vccPlatformService.callbacks[serviceId] = {
                name: name,
                callback: callback
            };
            Android_H5vccPlatformService.open_platform_service(serviceId, name);
            return new PlatformServiceClient(name);
        },
    }
}
