import {
  H5vccUpdater
} from '/gen/cobalt/browser/h5vcc_updater/public/mojom/h5vcc_updater.mojom.m.js';


// Implementation of h5vcc_system.mojom.H5vccUpdater.
class MockH5vccUpdater {
  constructor() {
    this.interceptor_ =
      new MojoInterfaceInterceptor(H5vccSystem.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccSystemReceiver(this);

    this.stub_result_ = new Map();
  }

  STUB_KEY_UPDATER_CHANNEL = 'updaterChannel';

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

  stubGetUpdaterChannel(updaterChannel) {
    this.stubResult(this.STUB_KEY_UPDATER_CHANNEL, updaterChannel);
  }

  exit() {
    incrementExitCallCount();
  }
}

export const mockH5vccUpdater = new MockH5vccUpdater();
