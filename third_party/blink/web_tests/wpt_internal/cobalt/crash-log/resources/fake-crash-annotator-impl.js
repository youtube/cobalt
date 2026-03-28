import {CrashAnnotator, CrashAnnotatorReceiver} from '/gen/cobalt/browser/crash_annotator/public/mojom/crash_annotator.mojom.m.js';

// Implementation of crash_annotator.mojom.CrashAnnotator.
class FakeCrashAnnotatorImpl {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(CrashAnnotator.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new CrashAnnotatorReceiver(this);
    this.stub_result_ = null;
    this.annotations_ = new Map();
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.stub_result_ = null;
    this.annotations_ = new Map();
  }

  async setString(key, value) {
    this.annotations_.set(key, value);
    return {
      result: this.stub_result_
    };
  }

  getTestValueSync() {
    console.log("Fake getTestValueSync");
    return {
      result: true
    };
  }

  // Added for stubbing setString() result in tests.
  stubResult(stub_result) {
    this.stub_result_ = stub_result;
  }

  // Added for testing setString() interactions.
  getAnnotation(key) {
    return this.annotations_.get(key);
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

}

export const fakeCrashAnnotatorImpl = new FakeCrashAnnotatorImpl();
