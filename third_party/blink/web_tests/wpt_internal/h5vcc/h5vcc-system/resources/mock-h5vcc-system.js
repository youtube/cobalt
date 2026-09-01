import {H5vccSystem, H5vccSystemReceiver} from '/gen/cobalt/browser/h5vcc_system/public/mojom/h5vcc_system.mojom.m.js';

// Implementation of h5vcc_system.mojom.H5vccSystem.
class MockH5vccSystem {
  constructor() {
/*
    this.receiver_ = new H5vccSystemReceiver(this);
    this.interceptor_ =
        new MojoInterfaceInterceptor(H5vccSystem.$interfaceName);
    this.interceptor_.oninterfacerequest = e =>
        this.receiver_.$.bindHandle(e.handle);
*/
    this.interceptor_ =
        new MojoInterfaceInterceptor(H5vccSystem.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccSystemReceiver(this);

    this.reset();
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.id_ = 'defaultFakeAdvertisingID'
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  setAdvertisingIDForTesting(id) {
    console.log('setAdvertisingIDForTesting(): ' + id);
    this.id_ = id;
  }

  // h5vcc_system.mojom.H5vccSystem impl.
  getAdvertisingId() {
    // VERY IMPORTANT: this should return (a resolved Promise with) a dictionary
    // with the same "key" as the mojom definition, in this case |id|.
    return Promise.resolve({ id: this.id_ });
  }
}

export const mockH5vccSystem = new MockH5vccSystem();
