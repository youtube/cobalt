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
    this.experiment_ids_ = [];
    this.called_reset_experiment_state_ = false;
    this.called_set_experiment_state_ = false;
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.called_reset_experiment_state_ = false;
    this.called_set_experiment_state_ = false;
    this.stub_result_ = new Map();
    this.experiment_ids_ = [];
    this.receiver_.$.close();
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  // Added for stubbing getFeature() and getFeatureParam() result in tests.
  stubResult(key, value) {
    this.stub_result_.set(key, value);
  }

  getActiveExperimentIds() {
    throw new Error('Sync methods not supported in MojoJS (b/406809316');
  }

  // Added for verifying setExperimentState result in tests.
  getExperimentIds() {
    return this.experiment_ids_;
  }

  getFeature(feature_name) {
    throw new Error('Sync methods not supported in MojoJS (b/406809316');
  }

  getFeatureParam(feature_param_name) {
    throw new Error('Sync methods not supported in MojoJS (b/406809316');
  }

  getFeatureState(feature_name) {
    return this.stub_result_.get(feature_name);
  }

  hasCalledResetExperimentState() {
    return this.called_reset_experiment_state_;
  }

  hasCalledSetExperimentState() {
    return this.called_set_experiment_state_;
  }

  async resetExperimentState() {
    this.called_reset_experiment_state_ = true;
    return;
  }

  async setExperimentState(experiment_config) {
    const root_storage = experiment_config.storage;
    if (!root_storage) {
      return;
    }

    // Process Features
    const features_container = root_storage['experiment_config.features']?.dictionaryValue?.storage;
    if (features_container) {
      // The keys of this object are the feature names (e.g., "fake_feature").
      for (const feature_name of Object.keys(features_container)) {
        const value = features_container[feature_name]?.boolValue;
        this.stubResult(feature_name, value);
      }
    }

    // Process Feature Params
    const params_container = root_storage['experiment_config.feature_params']?.dictionaryValue?.storage;
    if (params_container) {
      // The keys are the param names (e.g., "fake_feature_param").
      for (const param_name of Object.keys(params_container)) {
        const param_object = params_container[param_name];
        // Feature param could be any of the following types.
        const value = param_object?.stringValue ?? param_object?.boolValue ?? param_object?.intValue;
        this.stubResult(param_name, value);
      }
    }

    // Process Experiment IDs
    const ids_container = root_storage['experiment_config.exp_ids']?.listValue?.storage;
    if (ids_container) {
      const id_list = [];
      for (const id_key of Object.keys(ids_container)) {
        const id_object = ids_container[id_key];
        // Mojom often stores numbers as strings.
        const value = id_object?.stringValue ?? id_object?.intValue;
        if (value !== undefined) {
          // Convert to number for consistency with the test.
          id_list.push(Number(value));
        }
      }
      this.experiment_ids_ = id_list;
    }

    this.called_set_experiment_state_ = true;
    return;
  }
}

export const mockH5vccExperiments = new MockH5vccExperiments();
