import {CrashAnnotator, CrashAnnotatorReceiver} from '/gen/third_party/blink/public/mojom/crash/crash_annotator.mojom.m.js';

// Implementation of blink.mojom.CrashAnnotator.
class FakeCrashAnnotator {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(CrashAnnotator.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new CrashAnnotatorReceiver(this);
    this.stub_result_ = null;
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.stub_result_ = null;
  }

  async setString(key, value) {
    return {
      result: this.stub_result_
    };
  }

  // Added for stubbing setString() in tests.
  stubResult(stub_result) {
    this.stub_result_ = stub_result;
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

}

export const fakeCrashAnnotator = new FakeCrashAnnotator();
