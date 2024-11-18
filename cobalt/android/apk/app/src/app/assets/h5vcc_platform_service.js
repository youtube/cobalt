/**
 * @license
 * Copyright The Cobalt Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
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

window.H5vccPlatformService = {
    callbacks: {
    },
    callback_from_android: (serviceID, dataFromJava) => {
        console.log("Wrapper callback received:", name, dataFromJava);
        const binaryString = window.atob(dataFromJava);
        console.log("message:" + binaryString);
        const len = binaryString.length;
        const bytes = new Uint8Array(len);
        for (let i = 0; i < len; i++) {
            bytes[i] = binaryString.charCodeAt(i);
        }
        const arrayBuffer = bytes.buffer;
        window.H5vccPlatformService.callbacks[serviceID].callback(arrayBuffer);
    },
    has: (name) => {
        console.log('Colin test: platformService.has(' + name + ')');
        return Android_H5vccPlatformService.has_platform_service(name);
    },
    open: function(name, callback) {
        console.log('platformService.open(' + name + ',' +
                            JSON.stringify(callback) + ')');
        if (typeof callback !== 'function') {
            console.log("THROWING Missing or invalid callback function.")
            throw new Error("Missing or invalid callback function.");
        } else {
          console.log("callback was function!!!");
        }

        const serviceId = Object.keys(this.callbacks).length + 1;
        // Store the callback with the service ID, name, and callback
        window.H5vccPlatformService.callbacks[serviceId] = {
            name: name,
            callback: callback
        };
        Android_H5vccPlatformService.open_platform_service(serviceId, name);
        return {
            'name': name,
            'send': function (data) {
                // this is for log, but in production, the data may not be bytes of string
                const decoder = new TextDecoder('utf-8');
                const text = decoder.decode(data);
                console.log('platformService.send(' + text + ')');
                // ---end---
                var convert_to_b64 = arrayBufferToBase64(data);
                console.log('platformService send as b64:' + convert_to_b64);
                Android_H5vccPlatformService.platform_service_send(name, convert_to_b64);

                // -- new code
                // const response_data = Android_H5vccPlatformService.platform_service_send(name, convert_to_b64);
                // if (response_data) {
                //     console.log('platform_service_send response as b64:' + response_data);
                //     return base64ToArrayBuffer(response_data);
                // }
                // return null;
                // ---end---
            },
            close: () => {
                console.log('platformService.close()');
                Android_H5vccPlatformService.close_platform_service(name);
            },
        }
    },
    send: (data) => {
        console.log('platformService.send(' + JSON.stringify(data) + ')');
    },
    close: () => {
        console.log('platformService.close()');
    },
}

console.log('h5vcc_platform_service.js says window.H5vccPlatformService is ' + window.H5vccPlatformService);
