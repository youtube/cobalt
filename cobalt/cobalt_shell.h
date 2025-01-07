#ifndef COBALT_COBALT_SHELL_H
#define COBALT_COBALT_SHELL_H

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"

namespace cobalt {

class CobaltShell : public content::Shell {
 public:
  // Override content::Shell entry point
  static content::Shell* CreateNewWindow(
      content::BrowserContext* browser_context,
      const GURL& url,
      const scoped_refptr<content::SiteInstance>& site_instance,
      const gfx::Size& initial_size);

 private:
  CobaltShell(std::unique_ptr<content::WebContents> web_contents,
              bool should_set_delegate);

  // Overridden from content::Shell
  static content::Shell* CreateShell(
      std::unique_ptr<content::WebContents> web_contents,
      const gfx::Size& initial_size,
      bool should_set_delegate);

  // WebContentsObserver interface
  void PrimaryMainDocumentElementAvailable() override;
};

}  // namespace cobalt

#endif  // COBALT_COBALT_SHELL_H
