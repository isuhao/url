// Copyright 2017-18 Glyn Matthews.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef SKYR_DETAILS_URL_SCHEMES_INC
#define SKYR_DETAILS_URL_SCHEMES_INC

#include <vector>
#include <utility>
#include <string>
#include <string_view>
#include <cstdint>
#include <skyr/optional.hpp>

namespace skyr {
/// \exclude
namespace details {
using default_port_list = std::vector<std::pair<std::string, optional<std::uint16_t>>>;

/// \returns
const default_port_list &special_schemes() noexcept;

/// \param scheme
/// \returns
bool is_special(std::string_view scheme) noexcept;

/// \param scheme
/// \returns
optional<std::uint16_t> default_port(std::string_view scheme) noexcept;

/// \param scheme
/// \param port
/// \returns
bool is_default_port(std::string_view scheme, std::uint16_t port) noexcept;
}  // namespace details
}  // namespace skyr


#endif  // SKYR_DETAILS_URL_SCHEMES_INC
