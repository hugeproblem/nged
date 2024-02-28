/// Functions that I wish C++ has provided, but not (yet).
/// by iiif.

#pragma once

#include <algorithm>
#include <numeric>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <cassert>
#include <cmath>
#include <cstdlib>

namespace utils {

template<typename Container, typename Compare>
void argsort(std::vector<size_t>& order, Container const& arr, Compare const& cmp)
{
  order.resize(arr.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&arr, &cmp = cmp](size_t a, size_t b) {
    return cmp(arr[a], arr[b]);
  });
}

template<typename Container, typename Compare>
std::vector<size_t> argsort(Container const& arr, Compare const& cmp)
{
  std::vector<size_t> order;
  argsort(order, arr, cmp);
  return order;
}

template<typename Container>
void argsort(std::vector<size_t>& order, Container const& arr)
{
  order.resize(arr.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&arr](size_t a, size_t b) { return arr[a] < arr[b]; });
}

template<typename Container>
std::vector<size_t> argsort(Container const& arr)
{
  std::vector<size_t> order;
  argsort(order, arr);
  return order;
}

template<class Container, class Elem>
bool contains(Container const& c, Elem const& e)
{
  if (auto itr = c.find(e); itr != c.end())
    return true;
  return false;
}

template<class Container, class Key, class Elem>
auto get_or(Container const& c, Key&& key, Elem&& v)
{
  if (auto itr = c.find(key); itr != c.end())
    return itr->second;
  return std::forward<Elem>(v);
}

template<typename Container, typename Order>
Container reorder(Container const& arr, Order const& order)
{
  if (arr.size() != order.size()) {
    assert(!"order has different length with array");
    return Container{};
  }
  Container result;
  result.reserve(arr.size());
  for (size_t i = 0, n = order.size(); i < n; ++i)
    result.emplace_back(arr[order[i]]);
  return result;
}

inline
std::vector<std::string_view>
strsplit(std::string_view const& str, std::string_view const& delim)
{
  std::vector<std::string_view> parts;
  size_t s=0, e=str.find(delim), el=delim.length();
  for (; e!=str.npos; s=e+el, e=str.find(delim, e+el)) {
    parts.push_back(std::string_view(str.data()+s, e-s));
  }
  if (s<str.length())
    parts.push_back(std::string_view(str.data()+s, str.length()-s));
  return parts;
}

inline
std::string_view
strstrip(std::string_view const& str, std::string_view const& ignorechars=" \t\n\r")
{
  size_t start = 0, end = str.size();
  for(; start<end && ignorechars.find(str[start])!=std::string_view::npos; ++start)
    ;
  for(; start<end && ignorechars.find(str[end-1])!=std::string_view::npos; --end)
    ;
  return str.substr(start, end-start);
}

inline bool
startswith(std::string_view const& str, std::string_view const& prefix)
{
  return str.length() >= prefix.length() && str.substr(0, prefix.length()) == prefix;
}

#ifdef _MSC_VER
inline uint32_t bswap(uint32_t u) { return _byteswap_ulong(u); }
inline uint64_t bswap(uint64_t u) { return _byteswap_uint64(u); }
#else
inline uint32_t bswap(uint32_t u) { return __builtin_bswap32(u); }
inline uint64_t bswap(uint64_t u) { return __builtin_bswap64(u); }
#endif

inline float pow2(float t) { return t * t; }
inline float pow3(float t) { return t * t * t; }
template<class V>
inline std::vector<V> bezierPath(V p1, V p2, V p3, V p4, int cnt = 20)
{
  std::vector<V> path(cnt);
  for (int i = 0; i < cnt; ++i) {
    float t = float(i) / (cnt - 1);
    auto  p =
      pow3(1 - t) * p1 + 3 * pow2(1 - t) * t * p2 + 3 * (1 - t) * pow2(t) * p3 + pow3(t) * p4;
    path[i] = p;
  }
  return path;
}

namespace ease {

#define DEFINE_EASE(name, code) \
  inline float name(float x) { return code; }

DEFINE_EASE(inLinear, x)
DEFINE_EASE(outLinear, x)
DEFINE_EASE(inOutLinear, x)

DEFINE_EASE(inQuad, x* x)
DEFINE_EASE(outQuad, 1 - (1 - x) * (1 - x))
DEFINE_EASE(inOutQuad, x < 0.5 ? 2 * x * x : 1 - pow2(-2 * x + 2) / 2)

DEFINE_EASE(inCubic, x* x* x)
DEFINE_EASE(outCubic, 1.f - pow3(1.f - x))
DEFINE_EASE(inOutCubic, x < 0.5 ? 4 * pow3(x) : 1 - pow3(-2 * x + 2) / 2)

DEFINE_EASE(inExpo, x <= 1e-10f ? 0.f : float(std::pow(2.0, 10.0 * x - 10.0)))
DEFINE_EASE(outExpo, x >= 1.f ? 1.f : float(std::pow(2.0, -10.0 * x)))
DEFINE_EASE(
  inOutExpo,
  x <= 1e-10f ? 0.f
  : x >= 1.f  ? 1.f
  : x < 0.5f  ? float(std::pow(2.0, 20.0 * x - 10.0)) / 2.f
              : (2.f - float(std::pow(2.0, -20.0 * x + 10.0))) / 2.f)
#undef DEFINE_EASE

} // namespace ease

// bit op for enum class {{{
template<class E>
inline E eor(E a, E b)
{
  using U = typename std::underlying_type<E>::type;
  return E(U(a) | U(b));
}
template<class E>
inline E eand(E a, E b)
{
  using U = typename std::underlying_type<E>::type;
  return E(U(a) & U(b));
}
template<class E>
inline bool echeck(E a, E b)
{
  using U = typename std::underlying_type<E>::type;
  return 0 != (U(a) & U(b));
}
// }}} bit op for enum class

} // namespace utils
