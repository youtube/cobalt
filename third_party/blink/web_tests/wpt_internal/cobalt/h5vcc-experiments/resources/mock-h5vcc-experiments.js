import {
  H5vccExperiments,
  H5vccExperimentsReceiver,
} from '/gen/cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom.m.js';

// Implementation of h5vcc_experiments.mojom.H5vccExperiments.
class MockH5vccExperiments {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(H5vccExperiments.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccExperimentsReceiver(this);
    this.stub_result_ = new Map();
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.stub_result_ = new Map();
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  // Added for stubbing getFeature() and getFeatureParam() result in tests.
  stubResult(key, value) {
    this.stub_result_.set(key, value);
  }

  stubGetFeature(feature_name, stub_result) {
    this.stubResult(feature_name, stub_result);
  }

  stubGetFeatureParam(feature_param_name, stub_result) {
    this.stubResult(feature_param_name, stub_result);
  }

  getFeature(feature_name) {
    return this.stub_result_.get(feature_name);
  }

  getFeatureParam(feature_param_name) {
    return this.stub_result_.get(feature_param_name);
  }

  resetExperimentState() {
    return Promise.resolve();
  }

  setExperimentState() {
    return Promise.resolve();
  }
}

export const mockH5vccExperiments = new MockH5vccExperiments();
