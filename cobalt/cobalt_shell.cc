#include "cobalt/cobalt_shell.h"

namespace cobalt {

// Placeholder for a WebContentsObserver override
void CobaltShell::PrimaryMainDocumentElementAvailable() {
  LOG(INFO) << "Cobalt::PrimaryMainDocumentElementAvailable";
}

}  // namespace cobalt
