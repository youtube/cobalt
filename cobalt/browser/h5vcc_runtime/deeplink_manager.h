#ifndef COBALT_BROWSER_H5VCC_RUNTIME_DEEPLINK_MANAGER_H_
#define COBALT_BROWSER_H5VCC_RUNTIME_DEEPLINK_MANAGER_H_

#include <string>

#include "base/no_destructor.h"
#include "cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace cobalt {
namespace browser {

class DeepLinkManager {
 public:
  // Get the singleton instance.
  static DeepLinkManager* GetInstance();

  DeepLinkManager(const DeepLinkManager&) = delete;
  DeepLinkManager& operator=(const DeepLinkManager&) = delete;

  void SetDeepLink(const std::string& deeplink);
  const std::string& GetDeepLink() const;
  void AddListener(mojo::Remote<h5vcc_runtime::mojom::DeepLinkListener> listener_remote);

  void OnWarmStartupDeepLink(const std::string& deeplink);

 private:
  friend class base::NoDestructor<DeepLinkManager>;

  DeepLinkManager();
  ~DeepLinkManager();


  mojo::RemoteSet<h5vcc_runtime::mojom::DeepLinkListener> listeners_;
  std::string deeplink_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_H5VCC_RUNTIME_DEEPLINK_MANAGER_H_