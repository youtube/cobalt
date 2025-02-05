import {H5vccSystem, H5vccSystemReceiver} from '/gen/cobalt/browser/h5vcc_system/public/mojom/h5vcc_system.mojom.m.js';

// Implementation of h5vcc_system.mojom.H5vccSystem.
class FakeH5vccSystemImpl {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(H5vccSystem.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccSystemReceiver(this);
    this.stub_result_ = null;
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.stub_result_ = '';
  }

  async getAdvertisingId() {
    return {
      result: this.stub_result_
    };
  }

  // Added for stubbing setString() result in tests.
  stubResult(stub_result) {
    this.stub_result_ = stub_result;
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

}

export const fakeH5vccSystemImpl = new FakeH5vccSystemImpl();
