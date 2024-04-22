console.log('dyn_script.js');

function arrayBufferToBase64(buffer) {
    const bytes = new Uint8Array(buffer);
    let binary = '';
    for (let i = 0; i < bytes.byteLength; i++) {
        binary += String.fromCharCode(bytes[i]);
    }
    return window.btoa(binary);  // Encode binary string to Base64
}

var ps = {
    has: (name) => {
        console.log('platformService.has(' + name + ')');
        return Android.has_platform_service(name);
    },
    open: (name, callback) => {
        console.log('platformService.open(' + name + ',' + JSON.stringify(callback) + ')');
        Android.open_platform_service(name);
        // this needs to return an object with send
        return {
            'name' : name,
            'send' : function(data) {
                const decoder = new TextDecoder('utf-8');
                const text = decoder.decode(data);
                console.log('1 platformService.send(' + text + ')');
                var convert_to_b64 = arrayBufferToBase64(data);
                console.log('sending as b64:'+ convert_to_b64);
                Android.platform_service_send(name,convert_to_b64);
            },
            close: () => {
                console.log('1 platformService.close()');
                Android.close_platform_service(name);
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

function debug_things() {
    console.log("debugging...")
    console.log(typeof(MediaSource))

    const originalIsTypeSupported = MediaSource.isTypeSupported;
    MediaSource.isTypeSupported = function(type) {
        var result = originalIsTypeSupported.call(this, type);
        if(!result) {
            console.log('isTypeSupported called with type:', type, ' result:', result);
        }
        return result;
    };

    var valid_codecs = [
        'video/webm;codecs="vp8"',
        'video/webm;codecs="vorbis, vp8"',
        'AUDIO/WEBM;CODECS="vorbis"',
    ];
    var more_codecs = [
        'video/mp4;codecs="avc1.4d001e"', // H.264 Main Profile level 3.0
        'audio/mp4;codecs="mp4a.67"',     // MPEG2 AAC-LC
        'video/mp4;codecs="mp4a.40.2"',
        'video/mp4;codecs="mp4a.40.2 , avc1.4d001e "',
        'video/mp4;codecs="avc1.4d001e,mp4a.40.5"',
        'video/webm; codecs="vp09.02.51.10.01.09.16.09.00"; width=3840; height=2160; framerate=30; bitrate=21823784"'
    ]
    valid_codecs.push(...more_codecs);
    valid_codecs.forEach(function(codec) {
        var result = MediaSource.isTypeSupported(codec);
        console.log('Checking support for:' + codec + ' result:' + result);
    });
}


if (document.readyState === 'complete' || document.readyState === 'interactive') {
    console.log('Dynscript Init while page load complete');
    window.H5vccPlatformService = ps;
    debug_things();
} else {
    console.log('Dynscript Init while waiting for DOMContentLoaded');
    document.addEventListener('DOMContentLoaded', function() {
        window.H5vccPlatformService = ps;
        debug_things();
    });
}
