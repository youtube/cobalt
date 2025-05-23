import {
  H5vccExperiments,
  H5vccExperimentsReceiver,
} from '/gen/cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom.m.js';

// Implementation of h5vcc_experiments.mojom.H5vccExperiments.
class MockH5vccExperiments {
  constructor() {
    this.interceptor_ =
      new MojoInterfaceInterceptor(H5vccExperiments.$interfaceName);
    this.receiver_ = new H5vccExperimentsReceiver(this);
    this.interceptor_.oninterfacerequest = e => {
      if (this.should_close_pipe_on_request_)
        e.handle.close();
      else
        this.bind(e.handle);
      // this.receiver_.$.bindHandle(e.handle);
    }
    this.interceptor_.start();
    this.should_close_pipe_on_request_ = false;
    this.stub_result_ = new Map();
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.should_close_pipe_on_request_ = false;
    this.stub_result_ = new Map();
  }

  // simulate a 'no implementation available' case
  simulateNoImplementation() {
    this.should_close_pipe_on_request_ = true;
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  // Added for stubbing getFeature() and getFeatureParam() result in tests.
  // stubResult(key, value) {
  //   this.stub_result_.set(key, value);
  // }

  stubGetFeature(feature_name, stub_result) {
    // this.stubResult(feature_name, stub_result);
    this.stub_result_.set(feature_name, stub_result);
  }

  stubGetFeatureParam(feature_param_name, stub_result) {
    // this.stubResult(feature_param_name, stub_result);
    this.stub_result_.set(feature_param_name, stub_result)
  }

  getFeature(feature_name) {
    return this.stub_result_.get(feature_name);
  }

  async getFeatureParam(feature_param_name) {
    // return this.stub_result_.get(feature_param_name);
    // return Promise.resolve(this.stub_result_.get(feature_param_name));
    return {
      result: this.stub_result_.get(feature_param_name)
    };
  }

  async resetExperimentState() {
    return Promise.resolve();
  }

  async setExperimentState(experiment_config) {
    return Promise.resolve();
  }
}

export const mockH5vccExperiments = new MockH5vccExperiments();
