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
