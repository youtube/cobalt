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
    this.reset();
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
    this.updater_channel_ = "";
    this.update_status_ = "";
    this.installation_index_ = 0;
    this.library_sha256_ = new Map();
    this.allow_self_signed_packages_ = false;
    this.update_server_url_ = "";
    this.require_network_encryption_ = false;
    if (this.receiver_) {
        this.receiver_.$.close();
    }
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

  // --- H5vccUpdater Interface Methods ---

  async setUpdaterChannel(channel) {
    this.updater_channel_ = channel;
  }

  async getUpdaterChannel() {
    return { channel: this.updater_channel_ };
  }

  setMockUpdateStatus(status) {
    this.update_status_ = status;
  }

  async getUpdateStatus() {
    return { status: this.update_status_ };
  }

  setMockInstallationIndex(index) {
    console.log("setMockInstallationIndex");
    this.installation_index_ = index;
  }

  async getInstallationIndex() {
    return { index: this.installation_index_ };
  }

  async getAllowSelfSignedPackages() {
    return { allow_self_signed_packages: this.allow_self_signed_packages_ };
  }

  async setAllowSelfSignedPackages(allow) {
    throw new Error('Method is only supported for side-loading enabled release builds.');
  }

  async getUpdateServerUrl() {
    return { update_server_url: this.update_server_url_ };
  }

  async setUpdateServerUrl(url) {
    throw new Error('Method is only supported for side-loading enabled release builds.');
  }

  async getRequireNetworkEncryption() {
    return { require_network_encryption: this.require_network_encryption_ };
  }

  async setRequireNetworkEncryption(require) {
    throw new Error('Method is only supported for side-loading enabled release builds.');
  }

  setMockLibrarySha256(index, sha256) {
    this.library_sha256_.set(index, sha256);
  }

  async getLibrarySha256(index) {
    const sha256 = this.library_sha256_.get(index) || "";
    return { sha256: sha256 };
  }
}

export const mockH5vccUpdater = new MockH5vccUpdater();
