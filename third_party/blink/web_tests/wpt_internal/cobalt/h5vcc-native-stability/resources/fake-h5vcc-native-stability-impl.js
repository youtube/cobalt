import {
  H5vccNativeStability,
  H5vccNativeStabilityReceiver
} from '/gen/cobalt/browser/h5vcc_native_stability/public/mojom/h5vcc_native_stability.mojom.m.js';

// Implementation of h5vcc_native_stability.mojom.H5vccNativeStability.
class FakeH5vccNativeStabilityImpl {
  constructor() {
    this.interceptor_ =
      new MojoInterfaceInterceptor(H5vccNativeStability.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccNativeStabilityReceiver(this);
    this.reports_ = [];
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.reports_ = [];
  }

  // Added for stubbing getPendingReports() results in tests.
  stubReports(reports) {
    this.reports_ = reports;
  }

  async getPendingReports() {
    return {
      reports: this.reports_
    };
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }
}

export const fakeH5vccNativeStabilityImpl =
  new FakeH5vccNativeStabilityImpl();
