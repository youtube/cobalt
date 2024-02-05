#include "cobalt/network/disk_cache/cobalt_backend_impl.h"
#include "cobalt/network/disk_cache/resource_type.h"

namespace cobalt {
namespace network {
namespace disk_cache {

namespace defaults {
std::string GetSubdirectory(ResourceType resource_type) { return ""; }

uint32_t GetQuota(ResourceType resource_type) { return 0; }
}  // namespace defaults

namespace settings {
uint32_t GetQuota(ResourceType resource_type) { return 0; }
void SetQuota(ResourceType resource_type, uint32_t value) {}
bool GetCacheEnabled() { return false; }
void SetCacheEnabled(bool value) {}
}  // namespace settings

}  // namespace disk_cache
}  // namespace network
}  // namespace cobalt
