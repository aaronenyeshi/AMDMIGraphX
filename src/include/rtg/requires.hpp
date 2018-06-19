#ifndef RTG_GUARD_RTGLIB_REQUIRES_HPP
#define RTG_GUARD_RTGLIB_REQUIRES_HPP

#include <type_traits>

namespace rtg {

template<bool... Bs>
struct and_
: std::is_same<and_<Bs...>, and_<(Bs || true)...>>
{};

#define RTG_REQUIRES(...) class = typename std::enable_if<and_<__VA_ARGS__, true>{}>::type

} // namespace rtg

#endif
