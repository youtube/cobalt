import {H5vccMetrics, H5vccMetricsReceiver} from '/gen/cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom.m.js';

// Implementation of h5vcc_metrics.mojom.H5vccMetrics.
class MockH5vccMetrics {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(H5vccMetrics.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccMetricsReceiver(this);

    this.stub_result_ = new Map();
  }

  STUB_KEY_ENABLE_STATE = false;

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  bind(handle) {
    this.receiver_.bindHandle(handle);
  }

  reset() {
    this.stub_result_ = new Map();
  }

  stubResult(key, value) {
    this.stub_result_.set(key, value);
  }

  stubEnableState(enableState) {
    this.stubResult(this.STUB_KEY_ENABLE_STATE, enableState);
  }

  // h5vcc_metrics.mojom.H5vccMetrics impl.
  isEnabled() {
    return this.stub_result_.get(this.STUB_KEY_ENABLE_STATE);
  }
}

export const mockH5vccMetrics = new MockH5vccMetrics();
