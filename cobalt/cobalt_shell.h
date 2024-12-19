#ifndef COBALT_COBALT_SHELL_H
#define COBALT_COBALT_SHELL_H

#include "content/shell/browser/shell.h"

namespace cobalt {

class CobaltShell : public content::Shell {
  // WebContentsObserver
  void PrimaryMainDocumentElementAvailable() override;
};

}  // namespace cobalt

#endif  // COBALT_COBALT_SHELL_H
