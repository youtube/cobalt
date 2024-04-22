console.log('static script');

document.addEventListener('keydown', function(event) {
    console.log('Static Captured at document: Key pressed:', event.key, 'KeyCode:', event.keyCode);
}, true); // Set to true to listen during the capture phase

var handleBackPress = () => {
    console.log('Back press was called');

    var event = new KeyboardEvent('keydown', {
        key: 'Escape',
        code: 'Escape',
        keyCode: 27, // Note: keyCode is deprecated
        which: 27, // Note: which is deprecated
        bubbles: true, // Make the event bubble
        cancelable: true // Make the event cancelable
    });
    document.dispatchEvent(event);
}

var accountmanager = {
    getAuthToken: () => {},
    requestPairing: () => {},
    requestUnpairing: () => {},
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
        keys: () => { [ 'uh', 'oh'] },
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
/*
        initialDeepLink: () => { console.log('runtime.initialDeepLink'); return "";},
        onDeepLink: () => { console.log('runtime.onDeepLink'); },
        onPause: () => { console.log('runtime.onPause'); },
        onResume: () => { console.log('runtime.onResume'); }
*/
    },
    settings: {
        set: (name, value) => {
            console.log('settings.set(' + name + ',' + value + ')');
        },
    },
    storage: {
        enableCache: () => { console.log('storage.enableCache()'); },
        disableCache: () => { console.log('storage.disableCache()'); },
        flush: () => { console.log('storage.flush()');},
        getCookiesEnabled: () => { console.log('storage.getCookiesEnabled()'); return true; },
        setCookiesEnabled: (value) => { console.log('storage.setCookiesEnabled(' + value +')');},
        getQuota: () => { console.log('storage.getQuota()'); return {} },
        setQuota: (dict) => { console.log('storage.setQuota(' + JSON.stringify(dict) + ')' ); }
    },
    system: {
        areKeysReversed: true,
        buildId: 'whee',
        platform: 'bar',
        region: 'us',
        version: 'yah',
        advertisingId: 'nope',
        limitAdTracking: false,
        userOnExitStrategy: 0, //could be 1 or 2
    }
};

if (document.readyState === 'complete' || document.readyState === 'interactive') {
    console.log('Init while page load complete');
    window.h5vcc = h5;
} else {
    console.log('Init while waiting for DOMContentLoaded');
    document.addEventListener('DOMContentLoaded', function() {
       window.h5vcc = h5;
       console.log('DOMContentLoaded');
       console.log('Native call:' + Android.getSystemProperty('http.agent', 'defaultUserAgent'));
       console.log('ro.product.brand:' + Android.getRestrictedSystemProperty('ro.product.brand', 'defaultBrand'));
       console.log('ro.product.model:' + Android.getRestrictedSystemProperty('ro.product.model', 'defaultModel'));
       console.log('ro.build.id:' + Android.getRestrictedSystemProperty('ro.build.id', 'defaultBuild'));
    });
}
