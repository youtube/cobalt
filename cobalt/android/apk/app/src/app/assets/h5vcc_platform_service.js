function arrayBufferToBase64(buffer) {
    const bytes = new Uint8Array(buffer);
    let binary = '';
    for (let i = 0; i < bytes.byteLength; i++) {
        binary += String.fromCharCode(bytes[i]);
    }
    return window.btoa(binary);  // Encode binary string to Base64
}

var platform_services = {
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
        console.log('platformService.has(' + name + ')');
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
                const decoder = new TextDecoder('utf-8');
                const text = decoder.decode(data);
                console.log('1 platformService.send(' + text + ')');
                var convert_to_b64 = arrayBufferToBase64(data);
                console.log('sending as b64:' + convert_to_b64);
                Android_H5vccPlatformService.platform_service_send(name, convert_to_b64);
            },
            close: () => {
                console.log('1 platformService.close()');
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

window.H5vccPlatformService = platform_services;
