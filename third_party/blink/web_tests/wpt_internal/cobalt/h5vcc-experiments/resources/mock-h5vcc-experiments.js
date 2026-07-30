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

  STUB_KEY_ACTIVE_CONFIG_DATA = "activeConfigData";
  STUB_KEY_CONFIG_CONFIG_HASH = 'latestConfigHash';

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.called_reset_experiment_state_ = false;
    this.called_set_experiment_state_ = false;
    this.called_set_latest_experiment_config_hash_data = false;
    this.stub_result_ = new Map();
    this.experiment_ids_ = [];
    this.receiver_.$.close();
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  stubActiveConfigData(config_data) {
    this.stubResult(this.STUB_KEY_ACTIVE_CONFIG_DATA, config_data);
  }

  stubConfigHashData(hash_data) {
    this.stubResult(this.STUB_KEY_CONFIG_CONFIG_HASH, hash_data);
    this.called_set_latest_experiment_config_hash_data = true;
  }

  // Added for stubbing getFeature() result in tests.
  stubResult(key, value) {
    this.stub_result_.set(key, value);
  }

  async getActiveExperimentConfigData() {
    return {
      activeExperimentConfigData: this.stub_result_.get(this.STUB_KEY_ACTIVE_CONFIG_DATA)
    };
  }

  getConfigHash() {
    return this.STUB_KEY_CONFIG_CONFIG_HASH;
  }

  async getFeature(feature_name) {
    return {
      featureValue: this.stub_result_.get(feature_name)
    };
  }

  // Helper function to get stubbed feature state.
  getStubResult(key) {
    return this.stub_result_.get(key);
  }

  async getLatestExperimentConfigHashData() {
    return {
      latestExperimentConfigHashData: this.stub_result_.get(this.STUB_KEY_CONFIG_CONFIG_HASH)
    };
  }

  hasCalledResetExperimentState() {
    return this.called_reset_experiment_state_;
  }

  hasCalledSetExperimentState() {
    return this.called_set_experiment_state_;
  }

  hasCalledSetLatestExperimentConfigHashData() {
    return this.called_set_latest_experiment_config_hash_data;
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

    // Process active experiment config data.
    const config_data = root_storage['experiment_config.active_experiment_config_data']?.stringValue?.storage;
    if (config_data) {
      this.stubActiveConfigData(config_data);
    }

    // Process latest experiment config hash data.
    const config_hash = root_storage['experiment_config.latest_experiment_config_hash_data']?.stringValue?.storage;
    if (config_hash) {
      this.stubConfigHashData(config_hash);
    }

    this.called_set_experiment_state_ = true;
    return;
  }

  async setFinchParameters(settings) {
    const root_storage = settings.storage;
    if (!root_storage) {
      return;
    }

    for (const key of Object.keys(root_storage)) {
      const value_obj = root_storage[key];
      const value = value_obj?.stringValue ?? value_obj?.boolValue ?? value_obj?.intValue;
      this.stubResult(key, value);
    }
    return;
  }

  async setLatestExperimentConfigHashData(hash_data) {
    this.stubConfigHashData(hash_data);
    return;
  }
}


export const mockH5vccExperiments = new MockH5vccExperiments();
