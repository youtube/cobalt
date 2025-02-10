import {H5vccSystem, H5vccSystemReceiver} from '/gen/cobalt/browser/h5vcc_system/public/mojom/h5vcc_system.mojom.m.js';

// Implementation of h5vcc_system.mojom.H5vccSystem.
class MockH5vccSystem {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(H5vccSystem.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccSystemReceiver(this);

    this.stub_result_ = new Map();
  }

  STUB_KEY_ADVERTISING_ID = 'advertisingId';
  STUB_KEY_LIMIT_AD_TRACKING = 'limitAdTracking';

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  reset() {
    this.stub_result_ = new Map();
  }

  stubResult(key, value) {
    this.stub_result_.set(key, value);
  }

  stubAdvertisingID(id) {
    this.stubResult(this.STUB_KEY_ADVERTISING_ID, id);
  }

  stubLimitAdTracking(limitAdTracking) {
    this.stubResult(this.STUB_KEY_LIMIT_AD_TRACKING, limitAdTracking);
  }

  // h5vcc_system.mojom.H5vccSystem impl.
  getAdvertisingId() {
    // VERY IMPORTANT: this should return (a resolved Promise with) a dictionary
    // with the same "key" as the mojom definition, find the name of the key in the generated js file imported on the top of this file.
    return Promise.resolve({ advertisingId: this.stub_result_.get(this.STUB_KEY_ADVERTISING_ID) });
  }

  getLimitAdTracking() {
    return Promise.resolve({ limitAdTracking: this.stub_result_.get(this.STUB_KEY_LIMIT_AD_TRACKING) });
  }
}

export const mockH5vccSystem = new MockH5vccSystem();
