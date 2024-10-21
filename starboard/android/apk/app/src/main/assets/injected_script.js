console.log('static script');

const blackImageDataUrl = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/wcAAgMBAF+jA2MAAAAASUVORK5CYII=";

// Hack to null out 'poster' attribute on all videos to prevent default "play" button from showing
function set_black_video_poster_image() {
    var videos = document.getElementsByTagName('video');
    for (var i = 0; i < videos.length; i++) {
        if (!videos[i].hasAttribute('poster')) {
            videos[i].setAttribute('poster', blackImageDataUrl);
        }
    }
}

document.addEventListener('keydown', function (event) {
    console.log('Static Captured at document: Key pressed:', event.key, 'KeyCode:', event.keyCode);
    if (event.keyCode == 13) {
        set_black_video_poster_image();
    }
}, true); // Set to true to listen during the capture phase

var injectBackKeyPress = () => {
    console.log('Back press was called');
    // Other keycodes: 'Esccape': 27, 'Backspace': 8, Back: 166

    var backEvent = new KeyboardEvent('keydown', {
        key: 'Back',
        code: 'Back',
        keyCode: 166,
        charCode: 166,
        which: 166,
        bubbles: true,
        cancelable: false
    });
    document.dispatchEvent(backEvent);

}

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


var accountmanager = {
    getAuthToken: () => { },
    requestPairing: () => { },
    requestUnpairing: () => { },
};
var h5 = {
    accessibility: {
        highContrastText: false,
        textToSpeech: false,
        builtInScreenReader: false,
    },
    audioConfig: {
        connector: "huh",
        latencyMs: 100,
        codingType: "idk",
        numberOfChannels: 4,
        samplingFrequency: 100,
    },
    /*
        crashLog: {
            setString: (key, value) => {
                console.log('crashLog.setString(' + key + ',' + value + ')');
            },
            register: (name, description) => {
                console.log('crashLog.register(' + name + ',' + description + ')');
            },
            setPersistentSettingWatchdogEnable: (enable) => {
                console.log('crashLog.setPersistentSettingWatchdogEnable(' + enable + ')');
            },
            setPersistentSettingWatchdogCrash: (trigger) => {
                console.log('crashLog.setPersistentSettingWatchdogCrash(' + trigger + ')');
            }
        },
     */
    cVal: {
        keys: () => { ['uh', 'oh'] },
        getValue: (name) => {
            //console.log('cVal.getValue(' + name + ')');
        },
        getPrettyValue: (name) => {
            console.log('cVal.getValue(' + name + ')');
        }
    },
    metrics: {
        enable: () => { console.log('metrics.enable()'); },
        disable: () => { console.log('metrics.disable()'); },
        isEnabled: () => { console.log('metrics.isEnabled()'); return false; }
    },
    runtime: {
        initialDeepLink: "https://youtube.com/tv",
        onDeepLink: {
            addListener: (callback) => { console.log("Adding deeplink listerner");}
        },
        onPause: {
            addListener: (callback) => { console.log("Adding onPause listerner");}
        },
        onResume: {
            addListener: (callback) => { console.log("Adding onResume listerner");}
        }
    },
    settings: {
        set: (name, value) => {
            console.log('settings.set(' + name + ',' + value + ')');
        },
    },
    storage: {
        enableCache: () => { console.log('storage.enableCache()'); },
        disableCache: () => { console.log('storage.disableCache()'); },
        flush: () => { console.log('storage.flush()'); },
        getCookiesEnabled: () => { console.log('storage.getCookiesEnabled()'); return true; },
        setCookiesEnabled: (value) => { console.log('storage.setCookiesEnabled(' + value + ')'); },
        getQuota: () => { console.log('storage.getQuota()'); return {} },
        setQuota: (dict) => { console.log('storage.setQuota(' + JSON.stringify(dict) + ')'); }
    },
    system: {
        areKeysReversed: true,
        buildId: 'bogus_build_id',
        platform: 'bogus_platform',
        region: 'us',
        version: 'bogys_version',
        advertisingId: Android.getAdvertisingId(),
        limitAdTracking: Android.getLimitAdTracking(),
        userOnExitStrategy: 0, //could be 1 or 2
    }
};

// Intercept calls to isTypeSupported
function intercept_is_type_supported() {
    const originalIsTypeSupported = MediaSource.isTypeSupported;
    MediaSource.isTypeSupported = function(type) {
        // Filter out unwanted formats
        for(const s of ['9999', 'catavision', 'invalidformat', '7680']) {
            if (type.includes(s)) {
                console.log('IsTypeSupported: rejecting ' + type + ' <' + s + '>' );
                return false;
            }
        }
        var result = originalIsTypeSupported.call(this, type);
        if (!result) {
            console.log('IsTypeSupported: failed with type:', type, ' result:', result);
        }
        return result;
    };
}

function inject() {
    window.h5vcc = h5;
    window.H5vccPlatformService = platform_services;
    intercept_is_type_supported();
    set_black_video_poster_image();
}

if (document.readyState === 'complete' || document.readyState === 'interactive') {
    console.log('Init while page load complete');
    inject();
} else {
    console.log('Init while waiting for DOMContentLoaded');
    document.addEventListener('DOMContentLoaded', function () {
        inject();
        console.log('DOMContentLoaded');
        console.log('Native call:' + Android.getSystemProperty('http.agent', 'defaultUserAgent'));
        console.log('ro.product.brand:' + Android.getRestrictedSystemProperty('ro.product.brand', 'defaultBrand'));
        console.log('ro.product.model:' + Android.getRestrictedSystemProperty('ro.product.model', 'defaultModel'));
        console.log('ro.build.id:' + Android.getRestrictedSystemProperty('ro.build.id', 'defaultBuild'));
    });
}
