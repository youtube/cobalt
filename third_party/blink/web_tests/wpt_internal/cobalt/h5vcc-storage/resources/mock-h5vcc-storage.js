import {
  H5vccStorage,
  H5vccStorageReceiver,
} from '/gen/cobalt/browser/h5vcc_storage/public/mojom/h5vcc_storage.mojom.m.js';

const FLUSH_METHOD_NAME = 'flush';

// Implementation of h5vcc_storage.mojom.H5vccStorage.
class MockH5vccStorage {
  constructor() {
    this.interceptor_ =
      new MojoInterfaceInterceptor(H5vccStorage.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccStorageReceiver(this);

    this.callCount_ = { [FLUSH_METHOD_NAME]: 0 };
  }

  incrementFlushCallCount() {
    this.callCount_[FLUSH_METHOD_NAME] += 1;
  }

  flushCallCount() {
    return this.callCount_[FLUSH_METHOD_NAME];
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  flush() {
    incrementFlushCallCount();
    return Promise.resolve();
  }
}

export const mockH5vccStorage = new MockH5vccStorage();
