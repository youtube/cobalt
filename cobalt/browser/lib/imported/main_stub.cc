#include "cobalt/browser/lib/imported/main.h"

// Empty implementations so that 'lib' targets can be compiled into executables
// by the builder without missing symbol definitions.
void CbLibOnCobaltInitialized() {}
bool CbLibHandleEvent(const SbEvent* event) {
  (void) event;
  return false;
}
