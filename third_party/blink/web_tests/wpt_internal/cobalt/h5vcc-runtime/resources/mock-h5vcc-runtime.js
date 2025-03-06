import {H5vccRuntime, H5vccRuntimeReceiver} from '/gen/cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom.m.js';

// Implementation of h5vcc_runtime.mojom.H5vccRuntime.
class MockH5vccRuntime {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(H5vccRuntime.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccRuntimeReceiver(this);

    this.stub_result_ = new Map();
  }

  STUB_KEY_INITIAL_DEEP_LINK = 'initialDeepLink';

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  reset() {
    this.stub_result_ = new Map();
  }

  stubResult(key, value) {
    this.stub_result_.set(key, value);
  }

  stubInitialDeepLink(initialDeepLink) {
    this.stubResult(this.STUB_KEY_INITIAL_DEEP_LINK, initialDeepLink);
  }

  // h5vcc_runtime.mojom.H5vccRuntime impl.
  getInitialDeepLink() {
    const value = this.stub_result_.get(this.STUB_KEY_INITIAL_DEEP_LINK);
    stubInitialDeepLink('');

    // VERY IMPORTANT: this should return (a resolved Promise with) a dictionary
    // with the same "key" as the mojom definition, it's the variable name in the mojom file in CamelCase.
    // Double check the name in the generated js file imported on the top of this file.
    return Promise.resolve({ initialDeepLink: value });
  }
}

export const mockH5vccRuntime = new MockH5vccRuntime();
