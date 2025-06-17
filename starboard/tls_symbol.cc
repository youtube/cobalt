thread_local int thread_specific_data = 0;

// This function will be exported from our shared library.
// Its code will contain the reference that requires __tls_get_addr.
extern "C" __attribute__((visibility("default"))) void set_data(int value) {
  thread_specific_data = value;
}

extern "C" __attribute__((visibility("default"))) int get_data() {
  return thread_specific_data;
}

int main() {}
