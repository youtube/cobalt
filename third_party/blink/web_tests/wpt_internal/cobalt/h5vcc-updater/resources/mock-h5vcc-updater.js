import {
  H5vccUpdater, H5vccUpdaterReceiver
} from '/gen/cobalt/browser/h5vcc_updater/public/mojom/h5vcc_updater.mojom.m.js';


// Implementation of h5vcc_updater.mojom.H5vccUpdater.
class MockH5vccUpdater {
  constructor() {
    this.interceptor_ =
      new MojoInterfaceInterceptor(H5vccUpdater.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccUpdaterReceiver(this);

    this.stub_result_ = new Map();
  }

  STUB_KEY_UPDATE_SERVER_URL = 'testUpdateServerUrl';

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

  stubGetUpdateServerUrl(updateServerUrl) {
    this.stubResult(this.STUB_KEY_UPDATE_SERVER_URL, updateServerUrl);
  }

  exit() {
    incrementExitCallCount();
  }
}

export const mockH5vccUpdater = new MockH5vccUpdater();
