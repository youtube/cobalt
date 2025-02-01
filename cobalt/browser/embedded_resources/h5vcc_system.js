const advertisingId = await window.h5vcc.system.getAdvertisingId();

Object.defineProperty(window.h5vcc.system, "advertisingId", {
    value: advertisingId,
    writable: false,
    enumerable: true,
    configurable: false
});
