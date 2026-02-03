import {
  H5vccUpdater, H5vccUpdaterReceiver,
  H5vccUpdaterSideloading, H5vccUpdaterSideloadingReceiver
} from '/gen/cobalt/browser/h5vcc_updater/public/mojom/h5vcc_updater.mojom.m.js';

// Implementation of h5vcc_updater.mojom.H5vccUpdater and H5vccUpdaterSideloading
class MockH5vccUpdater {
  constructor() {
    this.interceptor_ =
      new MojoInterfaceInterceptor(H5vccUpdater.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);

    this.sideloading_interceptor_ =
      new MojoInterfaceInterceptor(H5vccUpdaterSideloading.$interfaceName);
    this.sideloading_interceptor_.oninterfacerequest = e => this.bindSideloading(e.handle);

    this.receiver_ = new H5vccUpdaterReceiver(this);
    this.sideloading_receiver_ = new H5vccUpdaterSideloadingReceiver(this);

    this.reset();
  }

  start() {
    this.interceptor_.start();
    this.sideloading_interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
    this.sideloading_interceptor_.stop();
  }

  reset() {
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
    if (this.sideloading_receiver_) {
        this.sideloading_receiver_.$.close();
    }
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  bindSideloading(handle) {
    this.sideloading_receiver_.$.bindHandle(handle);
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
    this.installation_index_ = index;
  }

  async getInstallationIndex() {
    return { index: this.installation_index_ };
  }

  setMockAllowSelfSignedPackages(allow) {
    this.allow_self_signed_packages_ = allow;
  }

  async getAllowSelfSignedPackages() {
    return {
        allowSelfSignedPackages: this.allow_self_signed_packages_,
        allow_self_signed_packages: this.allow_self_signed_packages_
    };
  }

  setMockUpdateServerUrl(url) {
    this.update_server_url_ = url;
  }

  async getUpdateServerUrl() {
    return {
        updateServerUrl: this.update_server_url_,
        update_server_url: this.update_server_url_
    };
  }

  setMockRequireNetworkEncryption(require) {
    this.require_network_encryption_ = require;
  }

  async getRequireNetworkEncryption() {
    return {
        requireNetworkEncryption: this.require_network_encryption_,
        require_network_encryption: this.require_network_encryption_
    };
  }

  setMockLibrarySha256(index, sha256) {
    this.library_sha256_.set(index, sha256);
  }

  async getLibrarySha256(index) {
    const sha256 = this.library_sha256_.get(index) || "";
    return { sha256: sha256 };
  }

  // --- H5vccUpdaterSideloading Interface Methods ---

  async setAllowSelfSignedPackages(allow) {
    this.allow_self_signed_packages_ = allow;
  }

  async setUpdateServerUrl(url) {
    this.update_server_url_ = url;
  }

  async setRequireNetworkEncryption(require) {
    this.require_network_encryption_ = require;
  }
}

export const mockH5vccUpdater = new MockH5vccUpdater();
