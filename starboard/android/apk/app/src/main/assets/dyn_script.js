console.log('dyn_script.js');


function debug_things() {
    console.log("debugging...")

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
    debug_things();
} else {
    console.log('Dynscript Init while waiting for DOMContentLoaded');
    document.addEventListener('DOMContentLoaded', function() {
        debug_things();
    });
}
