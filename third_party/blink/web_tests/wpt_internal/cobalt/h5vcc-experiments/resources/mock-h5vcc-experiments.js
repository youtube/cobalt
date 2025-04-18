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
    this.feature_param_value_ = null;
  }

  STUB_FEATURE_STATE = true;
  STUB_FEATURE_PARAM_VALUE = "feature_param";

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

  stubResult(key, value) {
    this.stub_result_.set(key, value);
  }

  stubGetFeature(feature_name) {
    this.stubResult(this.STUB_FEATURE_STATE, feature_name);
  }

  stubGetFeatureParam(feature_param_name) {
    this.stubResult(this.STUB_FEATURE_PARAM_VALUE, feature_param_name);
  }

  getFeature() {
    return this.stub_result_.get(this.STUB_FEATURE_STATE);
  }

  getFeatureParam() {
    return this.stub_result_.get(this.STUB_FEATURE_PARAM_VALUE);
  }

  // Added for stubbing getFeature() and getFeatureParam() result in tests.
  stubResult(stub_result) {
    this.stub_result_ = stub_result;
  }
}

export const mockH5vccExperiments = new MockH5vccExperiments();
