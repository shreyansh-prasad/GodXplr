#include "godxplr/core/entry_key.h"
#include <sstream>
#include <iomanip>

namespace gx {

std::string to_string(const EntryKey& key) {
    std::ostringstream oss;
    oss << "EntryKey{prov=" << static_cast<int>(key.provider)
        << ", vol=0x" << std::hex << key.volumeOrProviderId
        << ", obj=0x" << key.objectId
        << ", inc=" << std::dec << key.incarnation << "}";
    return oss.str();
}

} // namespace gx
