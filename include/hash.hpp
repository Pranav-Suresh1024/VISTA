#ifndef VISTA_HASH_HPP
#define VISTA_HASH_HPP

#include <string>
#include <string_view>

namespace vista {

std::string sha256_hex(std::string_view input);

}  // namespace vista

#endif
