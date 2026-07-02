// FairPlay DRM support for HLS URL Player test page.
// Uses HLSTest.log() from hls-test-common.js.

// --- Axinom test credentials ---
// These are public test credentials for Axinom FairPlay DRM service, used for testing purposes only as per https://github.com/Axinom/public-test-vectors/blob/master/README.md
// Demo player : https://cdn.xn--am-gka.be/TestVectors/Cmaf/protected_1080p_h264_cbcs/native-hls.html
var DRM_CONFIG = {
    // https://github.com/Axinom/public-test-vectors/blob/master/README.md?plain=1#L25
    licenseUrl: 'https://drm-fairplay-licensing.axprod.net/AcquireLicense',

    // https://github.com/Axinom/public-test-vectors/blob/master/ContentKeys/Axinom-Encoding-Cmaf-H264-protected-SingleKey.txt
    licenseToken: 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.ewogICJ2ZXJzaW9uIjogMSwKICAiY29tX2tleV9pZCI6ICI2OWU1NDA4OC1lOWUwLTQ1MzAtOGMxYS0xZWI2ZGNkMGQxNGUiLAogICJtZXNzYWdlIjogewogICAgInR5cGUiOiAiZW50aXRsZW1lbnRfbWVzc2FnZSIsCiAgICAidmVyc2lvbiI6IDIsCiAgICAibGljZW5zZSI6IHsKICAgICAgImFsbG93X3BlcnNpc3RlbmNlIjogdHJ1ZQogICAgfSwKICAgICJjb250ZW50X2tleXNfc291cmNlIjogewogICAgICAiaW5saW5lIjogWwogICAgICAgIHsKICAgICAgICAgICJpZCI6ICIzMDJmODBkZC00MTFlLTQ4ODYtYmNhNS1iYjFmODAxOGEwMjQiLAogICAgICAgICAgImVuY3J5cHRlZF9rZXkiOiAicm9LQWcwdDdKaTFpNDNmd3YremZ0UT09IiwKICAgICAgICAgICJ1c2FnZV9wb2xpY3kiOiAiUG9saWN5IEEiCiAgICAgICAgfQogICAgICBdCiAgICB9LAogICAgImNvbnRlbnRfa2V5X3VzYWdlX3BvbGljaWVzIjogWwogICAgICB7CiAgICAgICAgIm5hbWUiOiAiUG9saWN5IEEiLAogICAgICAgICJwbGF5cmVhZHkiOiB7CiAgICAgICAgICAibWluX2RldmljZV9zZWN1cml0eV9sZXZlbCI6IDE1MCwKICAgICAgICAgICJwbGF5X2VuYWJsZXJzIjogWwogICAgICAgICAgICAiNzg2NjI3RDgtQzJBNi00NEJFLThGODgtMDhBRTI1NUIwMUE3IgogICAgICAgICAgXQogICAgICAgIH0KICAgICAgfQogICAgXQogIH0KfQ._NfhLVY7S6k8TJDWPeMPhUawhympnrk6WAZHOVjER6M',

    // Axinom public FPS test certificate (referenced in public-test-vectors README play links)
    certificateUrl: 'https://tools.axinom.com/FPScert/fairplay.cer',

    keySystems: ['com.youtube.fairplay', 'com.apple.fps', 'com.apple.fps.1_0', 'com.apple.fps.2_0']
};

// --- DRM Info panel helpers ---

function updateDrmInfo(field, value) {
    var el = document.getElementById(field);
    if (el) el.textContent = value;
}

// --- FairPlay init data packing ---

function packFairPlayInitData(skdUrl, contentId, certData) {
    var skdUtf16 = new ArrayBuffer(skdUrl.length * 2);
    var skdView = new Uint16Array(skdUtf16);
    for (var i = 0; i < skdUrl.length; i++) {
        skdView[i] = skdUrl.charCodeAt(i);
    }
    var encoder = new TextEncoder();
    var contentIdBytes = encoder.encode(contentId);

    var totalLen = 4 + skdUtf16.byteLength +
                   4 + contentIdBytes.byteLength +
                   4 + certData.byteLength;
    var packed = new ArrayBuffer(totalLen);
    var view = new DataView(packed);
    var offset = 0;

    view.setUint32(offset, skdUtf16.byteLength, true); offset += 4;
    new Uint8Array(packed, offset, skdUtf16.byteLength).set(new Uint8Array(skdUtf16));
    offset += skdUtf16.byteLength;

    view.setUint32(offset, contentIdBytes.byteLength, true); offset += 4;
    new Uint8Array(packed, offset, contentIdBytes.byteLength).set(contentIdBytes);
    offset += contentIdBytes.byteLength;

    view.setUint32(offset, certData.byteLength, true); offset += 4;
    new Uint8Array(packed, offset, certData.byteLength).set(new Uint8Array(certData));

    return packed;
}

function extractSkdUrlFromFairPlayInitData(initData) {
    if (!initData || initData.byteLength < 4) {
        throw new Error('Init data is too short');
    }
    var view = new DataView(initData);
    var len = view.getUint32(0, true);
    if (initData.byteLength < 4 + len) {
        throw new Error('Init data length mismatch');
    }
    var utf16 = new Uint16Array(initData, 4, len / 2);
    return String.fromCharCode.apply(null, utf16);
}

function extractContentId(skdUrl) {
    var idx = skdUrl.indexOf('://');
    return idx >= 0 ? skdUrl.substring(idx + 3) : skdUrl;
}

// --- EME session management ---

async function handleEncryptedEvent(event, mediaKeys, certificate) {
    try {
        HLSTest.log('Creating MediaKeySession for type=' + event.initDataType, 'ok');
        var session = mediaKeys.createSession();

        session.addEventListener('message', async function(msgEvent) {
            HLSTest.log('SESSION message: type=' + msgEvent.messageType +
                ' size=' + msgEvent.message.byteLength, 'ok');

            var licenseUrl = DRM_CONFIG.licenseUrl + '?AxDrmMessage=' + DRM_CONFIG.licenseToken;
            HLSTest.log('Sending license request...');

            try {
                var response = await fetch(licenseUrl, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/octet-stream' },
                    body: msgEvent.message
                });
                if (!response.ok) {
                    HLSTest.log('License server error: ' + response.status, 'error');
                    return;
                }
                var licenseData = await response.arrayBuffer();
                HLSTest.log('License received, size=' + licenseData.byteLength, 'ok');
                await session.update(new Uint8Array(licenseData));
                HLSTest.log('Session updated with license!', 'ok');
            } catch (fetchErr) {
                HLSTest.log('License fetch error: ' + fetchErr.message, 'error');
            }
        });

        session.addEventListener('keystatuseschange', function() {
            HLSTest.log('SESSION keystatuseschange:', 'ok');
            var statuses = [];
            session.keyStatuses.forEach(function(status, keyId) {
                var hex = Array.from(new Uint8Array(keyId))
                    .map(function(b) { return b.toString(16).padStart(2, '0'); }).join('');
                HLSTest.log('  Key ' + hex + ' status=' + status,
                    status === 'usable' ? 'ok' : 'warn');
                statuses.push(status);
            });
            updateDrmInfo('di-keystatus', statuses.join(', ') || '--');
            if (isFinite(session.expiration) && session.expiration > 0) {
                try {
                    updateDrmInfo('di-expiration', new Date(session.expiration).toISOString());
                } catch (e) {
                    updateDrmInfo('di-expiration', session.expiration);
                }
            } else {
                updateDrmInfo('di-expiration', 'none');
            }
        });

        var requestInitData;
        if (event.initDataType === 'fairplay') {
            var skdUrl = extractSkdUrlFromFairPlayInitData(event.initData);
            var contentId = extractContentId(skdUrl);
            HLSTest.log('FairPlay: skdUrl=' + skdUrl + ' contentId=' + contentId, 'ok');
            requestInitData = packFairPlayInitData(skdUrl, contentId, certificate);
        } else {
            requestInitData = event.initData;
        }

        HLSTest.log('generateRequest(' + event.initDataType + ', ' +
            requestInitData.byteLength + ' bytes)...');
        await session.generateRequest(event.initDataType, requestInitData);
        HLSTest.log('generateRequest completed', 'ok');
        if (session.sessionId) {
            updateDrmInfo('di-sessionid', session.sessionId);
        }
    } catch (error) {
        HLSTest.log('DRM error: ' + error.message, 'error');
    }
}

function setupFairPlay(video) {
    var certificate = null;
    var mediaKeys = null;
    var pendingEvents = [];
    var setupDone = false;
    var setupStarted = false;

    async function doSetup() {
        if (setupStarted) return;
        setupStarted = true;
        HLSTest.log('--- FairPlay EME Setup ---', 'ok');

        try {
            if (!navigator.requestMediaKeySystemAccess) {
                throw new Error('EME (requestMediaKeySystemAccess) is not supported on this platform.');
            }
            HLSTest.log('Fetching certificate...');
            var certResponse = await fetch(DRM_CONFIG.certificateUrl);
            if (!certResponse.ok) {
                HLSTest.log('Certificate fetch failed: ' + certResponse.status, 'error');
                setupStarted = false;
                return;
            }
            certificate = await certResponse.arrayBuffer();
            HLSTest.log('Certificate: ' + certificate.byteLength + ' bytes', 'ok');

            var access = null;
            for (var i = 0; i < DRM_CONFIG.keySystems.length; i++) {
                var ks = DRM_CONFIG.keySystems[i];
                try {
                    HLSTest.log('Trying "' + ks + '"...');
                    access = await navigator.requestMediaKeySystemAccess(ks, [{
                        initDataTypes: ['fairplay', 'skd', 'sinf', 'cenc'],
                        videoCapabilities: [{
                            contentType: 'video/mp4; codecs="avc1.42E01E"',
                            encryptionScheme: 'cbcs'
                        }],
                        audioCapabilities: [{
                            contentType: 'audio/mp4; codecs="mp4a.40.2"',
                            encryptionScheme: 'cbcs'
                        }]
                    }]);
                    HLSTest.log('Key system: ' + ks, 'ok');
                    // Extract encryption scheme from the accepted configuration
                    var config = access.getConfiguration();
                    var schemes = [];
                    if (config.videoCapabilities) {
                        config.videoCapabilities.forEach(function(c) {
                            if (c.encryptionScheme) schemes.push(c.encryptionScheme);
                        });
                    }
                    if (config.audioCapabilities) {
                        config.audioCapabilities.forEach(function(c) {
                            if (c.encryptionScheme && schemes.indexOf(c.encryptionScheme) === -1) schemes.push(c.encryptionScheme);
                        });
                    }
                    updateDrmInfo('di-scheme', schemes.length > 0 ? schemes.join(', ') : 'unknown');
                    updateDrmInfo('di-keysystem', ks);
                    if (config.initDataTypes) {
                        updateDrmInfo('di-initdatatypes', config.initDataTypes.join(', '));
                    }
                    if (config.sessionTypes) {
                        updateDrmInfo('di-sessiontype', config.sessionTypes.join(', '));
                    }
                    break;
                } catch (e) {
                    HLSTest.log('"' + ks + '" not supported', 'warn');
                }
            }
            if (!access) {
                HLSTest.log('No FairPlay key system found!', 'error');
                setupStarted = false;
                return;
            }

            mediaKeys = await access.createMediaKeys();
            await mediaKeys.setServerCertificate(new Uint8Array(certificate));
            HLSTest.log('Server certificate set', 'ok');

            await video.setMediaKeys(mediaKeys);
            HLSTest.log('MediaKeys attached', 'ok');

            setupDone = true;
            HLSTest.log('--- EME Setup Complete ---', 'ok');

            for (var i = 0; i < pendingEvents.length; i++) {
                await handleEncryptedEvent(pendingEvents[i], mediaKeys, certificate);
            }
            pendingEvents = [];
        } catch (error) {
            HLSTest.log('EME error: ' + error.message, 'error');
            setupStarted = false;
        }
    }

    video.addEventListener('encrypted', function(event) {
        HLSTest.log('EVENT: encrypted type=' + event.initDataType +
            ' size=' + (event.initData ? event.initData.byteLength : 'null'), 'ok');
        updateDrmInfo('di-initdata', event.initDataType);
        updateDrmInfo('di-encrypted', 'Yes');
        var siType = document.getElementById('si-type');
        if (siType) siType.textContent = 'HLS (Encrypted)';
        if (setupDone && mediaKeys) {
            handleEncryptedEvent(event, mediaKeys, certificate);
        } else {
            pendingEvents.push(event);
            doSetup();
        }
    });

    video.addEventListener('waitingforkey', function() {
        HLSTest.log('EVENT: waitingforkey', 'warn');
        doSetup();
    });

    setTimeout(function() {
        if (!setupStarted) {
            HLSTest.log('Timeout: proactive EME setup...', 'warn');
            doSetup();
        }
    }, 3000);
}
