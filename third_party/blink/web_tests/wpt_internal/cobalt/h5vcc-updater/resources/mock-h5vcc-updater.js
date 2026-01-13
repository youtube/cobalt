import {
  H5vccUpdater, H5vccUpdaterReceiver
} from '/gen/cobalt/browser/h5vcc_updater/public/mojom/h5vcc_updater.mojom.m.js';

// Implementation of h5vcc_updater.mojom.H5vccUpdater
class MockH5vccUpdater {
  constructor() {
    this.interceptor_ =
      new MojoInterfaceInterceptor(H5vccUpdater.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new H5vccUpdaterReceiver(this);
    this.stub_result_ = new Map();
    this.called_reset_installations_ = false;
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.stub_result_ = new Map();
    this.called_reset_installations_ = false;
    this.receiver_.$.close();
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  hasCalledResetInstallations() {
    return this.called_reset_installations_;
  }

  async resetInstallations() {
    this.called_reset_installations_ = true;
  }

  // --- H5vccUpdater Interface Methods Stubbed ---

  async setUpdaterChannel(channel) {
    throw new Error('Test not implemented yet');
  }

  async getUpdaterChannel() {
    throw new Error('Test not implemented yet');
  }

  async getUpdateStatus() {
    throw new Error('Test not implemented yet');
  }

  async getInstallationIndex() {
    throw new Error('Test not implemented yet');
  }

  async getAllowSelfSignedPackages() {
    throw new Error('Test not implemented yet');
  }

  async setAllowSelfSignedPackages(allow) {
    throw new Error('Test not implemented yet');
  }

  async getUpdateServerUrl() {
    throw new Error('Test not implemented yet');
  }

  async setUpdateServerUrl(url) {
    throw new Error('Test not implemented yet');
  }

  async getRequireNetworkEncryption() {
    throw new Error('Test not implemented yet');
  }

  async setRequireNetworkEncryption(require) {
    throw new Error('Test not implemented yet');
  }

  async getLibrarySha256(index) {
    throw new Error('Test not implemented yet');
  }
}

export const mockH5vccUpdater = new MockH5vccUpdater();
