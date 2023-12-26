/// math for 2d graphics
/// by iiif

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace gmath {

static constexpr double pi = 3.1415926535897932384626;

// Math Foundation {{{
struct Vec2
{
  float x = 0.f, y = 0.f;

  constexpr Vec2()            = default;
  constexpr Vec2(Vec2 const&) = default;
  constexpr Vec2(Vec2&&)      = default;
  constexpr Vec2(float x, float y) : x(x), y(y) {}
  Vec2& operator=(Vec2 const&) = default;

  Vec2& operator+=(Vec2 a)
  {
    x += a.x;
    y += a.y;
    return *this;
  }
  Vec2& operator-=(Vec2 a)
  {
    x -= a.x;
    y -= a.y;
    return *this;
  }
  Vec2& operator*=(Vec2 a)
  {
    x *= a.x;
    y *= a.y;
    return *this;
  }
  Vec2& operator*=(float a)
  {
    x *= a;
    y *= a;
    return *this;
  }
  Vec2& operator/=(float a)
  {
    x /= a;
    y /= a;
    return *this;
  }
  Vec2 operator-() const { return Vec2{-x, -y}; }
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return Vec2{a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return Vec2{a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float b) { return Vec2{a.x * b, a.y * b}; }
inline Vec2 operator*(float b, Vec2 a) { return Vec2{a.x * b, a.y * b}; }
inline Vec2 operator*(Vec2 a, Vec2 b) { return Vec2{a.x * b.x, a.y * b.y}; }
inline Vec2 operator/(Vec2 a, float b) { return Vec2{a.x / b, a.y / b}; }
inline bool operator==(Vec2 a, Vec2 b)
{
  return fabs(a.x - b.x) < std::numeric_limits<float>::epsilon() &&
         fabs(a.y - b.y) < std::numeric_limits<float>::epsilon();
}
inline bool operator!=(Vec2 a, Vec2 b) { return !(a == b); }

struct Mat3
{
  float              m[3][3] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
  static inline Mat3 fromSRT(Vec2 scale, float rotate, Vec2 translate);
  static inline Mat3 fromRTS(Vec2 scale, float rotate, Vec2 translate);
  inline Vec2        transformPoint(Vec2 v) const;
  inline Vec2        transformVec(Vec2 v) const;
  inline float       det() const;
  inline Mat3        inverse() const;
  inline Mat3        operator*(Mat3 const& that) const;
};

struct LinearColor
{
  float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
};

struct HSLColor
{
  float h = 0.f, s = 1.f, l = 1.f, a = 1.f;
};

struct HSVColor
{
  float h = 0.f, s = 1.f, v = 1.f, a = 1.f;
};

struct sRGBColor
{
  uint8_t r = 255, g = 255, b = 255, a = 255;
};

struct FloatSRGBColor
{
  float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
};

inline sRGBColor      fromUint32sRGB(uint32_t uc);
inline sRGBColor      fromUint32sRGBA(uint32_t uc);
inline uint32_t       toUint32ABGR(sRGBColor c);
inline uint32_t       toUint32RGBA(sRGBColor c);
inline LinearColor    toLinear(sRGBColor const& c);
inline LinearColor    toLinear(HSLColor const& c);
inline LinearColor    toLinear(HSVColor const& c);
inline sRGBColor      toSRGB(LinearColor const& c);
inline HSLColor       toHSL(LinearColor const& c);
inline HSVColor       toHSV(LinearColor const& c);
inline HSLColor       toHSL(HSVColor const& c);
inline HSVColor       toHSV(HSLColor const& c);
inline std::string    toHexCode(sRGBColor const& c);
inline sRGBColor      hexCodeToSRGB(std::string_view c);
inline FloatSRGBColor toFloatSRGB(sRGBColor c);
inline sRGBColor      toSRGB(FloatSRGBColor const& c);

struct AABB
{
  Vec2 min, max;
  using value_type = decltype(min.x);

  AABB()
  {
    min = Vec2{std::numeric_limits<value_type>::max(), std::numeric_limits<value_type>::max()};
    max = Vec2{-std::numeric_limits<value_type>::max(), -std::numeric_limits<value_type>::max()};
  }
  AABB(Vec2 const& a) { min = max = a; }
  AABB(Vec2 const& a, Vec2 const& b) : AABB(a) { merge(b); }
  static inline AABB fromCenterAndSize(Vec2 const& center, Vec2 const& size);
  inline void        merge(Vec2 const& v);
  inline void        merge(AABB const& aabb);
  inline void        expand(float amount);
  inline AABB        expanded(float amount) const;
  inline void        move(Vec2 delta);
  inline AABB        moved(Vec2 delta) const;
  Vec2               center() const { return Vec2{(min.x + max.x) / 2, (min.y + max.y) / 2}; }
  Vec2               size() const { return Vec2{max.x - min.x, max.y - min.y}; }
  auto               width() const { return max.x - min.x; }
  auto               height() const { return max.y - min.y; }
  // test if the that is contained inside this
  inline bool contains(AABB const& that) const;
  // test if point is inside this
  inline bool contains(Vec2 const& pt) const;
  // test if the two AABBs has intersection
  inline bool intersects(AABB const& that) const;
  // test if AABB intersects with line segment
  inline bool intersects(Vec2 segstart, Vec2 segend) const;
  // test if AABB intersects with line segment, outputs interection
  inline bool intersects(Vec2* segstart, Vec2* segend) const;
};
// }}}

// inline math functions {{{
template<class Vec3>
static inline Vec3 cross(const Vec3& a, const Vec3& b)
{
  return Vec3{a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - b.x * a.y};
}
template<class Vec2>
static inline float dot(const Vec2& a, const Vec2& b)
{
  return a.x * b.x + a.y * b.y;
}
template<class Vec2>
static inline bool ccw(const Vec2& a, const Vec2& b, const Vec2& c)
{
  auto const ab = b - a, ac = c - a;
  struct Vec3
  {
    float x, y, z;
  };
  return cross(Vec3{ab.x, ab.y, 0.f}, Vec3{ac.x, ac.y, 0.f}).z > 0;
}
template<class T>
static inline T clamp(T v, T min, T max)
{
  return std::max(min, std::min(max, v));
}
template<class T>
static inline T lerp(T a, T b, float t)
{
  return a + (b - a) * t;
}
template<class Vec2>
inline auto distance2(Vec2 a, Vec2 b)
{
  float const dx = a.x - b.x;
  float const dy = a.y - b.y;
  return dx * dx + dy * dy;
}
template<class Vec2>
inline auto distance(Vec2 a, Vec2 b)
{
  return sqrt(distance2(a, b));
}
template<class Vec2>
inline auto length2(Vec2 v)
{
  return v.x * v.x + v.y * v.y;
}
template<class Vec2>
inline auto length(const Vec2& v)
{
  return sqrt(v.x * v.x + v.y * v.y);
}
template<class Vec2>
inline auto normalize(Vec2 v)
{
  v /= length(v);
  return v;
}

// matrix

inline Mat3 Mat3::fromSRT(Vec2 scale, float rotate, Vec2 translate)
{
  float const cr = cos(rotate);
  float const sr = sin(rotate);
  Mat3        srt;
  srt.m[0][0] = cr * scale.x;
  srt.m[0][1] = sr * scale.x;
  srt.m[1][0] = -sr * scale.y;
  srt.m[1][1] = cr * scale.y;
  srt.m[2][0] = translate.x;
  srt.m[2][1] = translate.y;
  return srt;
}

inline Mat3 Mat3::fromRTS(Vec2 scale, float rotate, Vec2 translate)
{
  float const cr = cos(rotate);
  float const sr = sin(rotate);
  Mat3        rts;
  rts.m[0][0] = cr * scale.x;
  rts.m[0][1] = sr * scale.x;
  rts.m[1][0] = -sr * scale.y;
  rts.m[1][1] = cr * scale.y;
  rts.m[2][0] = translate.x * scale.x;
  rts.m[2][1] = translate.y * scale.y;
  return rts;
}

inline Vec2 Mat3::transformPoint(Vec2 v) const
{
  // z = 1
  float x = v.x * m[0][0] + v.y * m[1][0] + m[2][0];
  float y = v.x * m[0][1] + v.y * m[1][1] + m[2][1];
  return Vec2{x, y};
}

inline Vec2 Mat3::transformVec(Vec2 v) const
{
  float x = v.x * m[0][0] + v.y * m[1][0];
  float y = v.x * m[0][1] + v.y * m[1][1];
  return Vec2{x, y};
}

inline float Mat3::det() const
{
  return +m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
         m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
         m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}
inline Mat3 Mat3::inverse() const
{
  float const recipDet = 1.f / det();

  Mat3 inv;
  inv.m[0][0] = +(m[1][1] * m[2][2] - m[1][2] * m[2][1]) * recipDet;
  inv.m[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * recipDet;
  inv.m[0][2] = +(m[0][1] * m[1][2] - m[0][2] * m[1][1]) * recipDet;
  inv.m[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * recipDet;
  inv.m[1][1] = +(m[0][0] * m[2][2] - m[0][2] * m[2][0]) * recipDet;
  inv.m[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * recipDet;
  inv.m[2][0] = +(m[1][0] * m[2][1] - m[1][1] * m[2][0]) * recipDet;
  inv.m[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * recipDet;
  inv.m[2][2] = +(m[0][0] * m[1][1] - m[0][1] * m[1][0]) * recipDet;

  return inv;
}
inline Mat3 Mat3::operator*(Mat3 const& that) const
{
  Mat3 result = {{0}};
  for (int j = 0; j < 3; ++j)
    for (int i = 0; i < 3; ++i)
      for (int k = 0; k < 3; ++k)
        result.m[j][i] += m[j][k] * that.m[k][i];
  return result;
}

// color

template <class T>
inline bool cequal(T const& a, T const& b)
{
  return memcmp(&a, &b, sizeof(T)) == 0;
}

#define MAKE_COLOR_CMP_OPERATOR(type) \
  inline bool operator==(type const& a, type const& b) { return cequal(a, b); } \
  inline bool operator!=(type const& a, type const& b) { return !cequal(a, b); }

MAKE_COLOR_CMP_OPERATOR(LinearColor)
MAKE_COLOR_CMP_OPERATOR(HSLColor)
MAKE_COLOR_CMP_OPERATOR(HSVColor)
MAKE_COLOR_CMP_OPERATOR(sRGBColor)
MAKE_COLOR_CMP_OPERATOR(FloatSRGBColor)

#undef MAKE_COLOR_CMP_OPERATOR

inline sRGBColor fromUint32sRGB(uint32_t uc)
{
  return {uint8_t((uc & 0xff0000) >> 16), uint8_t((uc & 0xff00) >> 8), uint8_t(uc & 0xff), 255};
}

inline sRGBColor fromUint32sRGBA(uint32_t uc)
{
  return {
    uint8_t((uc & 0xff000000) >> 24),
    uint8_t((uc & 0xff0000) >> 16),
    uint8_t((uc & 0xff00) >> 8),
    uint8_t(uc & 0xff)};
}

inline uint32_t toUint32RGBA(sRGBColor c)
{
  return (uint32_t(c.a)) | (uint32_t(c.b) << 8) | (uint32_t(c.g) << 16) | (uint32_t(c.r) << 24);
}

inline uint32_t toUint32ABGR(sRGBColor c)
{
  return (uint32_t(c.r)) | (uint32_t(c.g) << 8) | (uint32_t(c.b) << 16) | (uint32_t(c.a) << 24);
}

inline sRGBColor toSRGB(LinearColor const& c)
{
  sRGBColor srgb = {0, 0, 0, uint8_t(clamp(c.a * 255.f, 0.f, 255.f))};
  srgb.r         = uint8_t(clamp(255.f * float(pow(clamp(c.r, 0.f, 1.f), 2.2f)), 0.f, 255.f));
  srgb.g         = uint8_t(clamp(255.f * float(pow(clamp(c.g, 0.f, 1.f), 2.2f)), 0.f, 255.f));
  srgb.b         = uint8_t(clamp(255.f * float(pow(clamp(c.b, 0.f, 1.f), 2.2f)), 0.f, 255.f));
  return srgb;
}

inline LinearColor toLinear(sRGBColor const& c)
{
  return {
    float(pow(c.r / 255.f, 1 / 2.2f)),
    float(pow(c.g / 255.f, 1 / 2.2f)),
    float(pow(c.b / 255.f, 1 / 2.2f)),
    c.a / 255.f};
}

inline LinearColor toLinear(HSLColor const& c)
{
  float const hue       = c.h;
  float const satuation = c.s;
  float const luma      = c.l;
  float const chroma    = (1 - abs(2 * luma - 1)) * satuation;

  LinearColor rgba = {0, 0, 0, c.a};
  float const h    = fmod(hue, 1.f) * 6.f;
  float const x    = chroma * (1.f - abs(fmod(h, 2.0f) - 1.f));
  float const m    = luma - chroma / 2.f;
  if (h < 0) {
    // pass
  } else if (h < 1) {
    rgba.r = chroma;
    rgba.g = x;
  } else if (h < 2) {
    rgba.r = x;
    rgba.g = chroma;
  } else if (h < 3) {
    rgba.g = chroma;
    rgba.b = x;
  } else if (h < 4) {
    rgba.g = x;
    rgba.b = chroma;
  } else if (h < 5) {
    rgba.r = x;
    rgba.b = chroma;
  } else if (h < 6) {
    rgba.r = chroma;
    rgba.b = x;
  } else {
    // pass
  }
  rgba.r += m;
  rgba.g += m;
  rgba.b += m;
  return rgba;
}

inline LinearColor toLinear(HSVColor const& c) { return toLinear(toHSL(c)); }

inline HSLColor toHSL(HSVColor const& c)
{
  float const l = (2.0f - c.s) * c.v;
  return {c.h, c.s * c.v / (l < 1.f ? l : 2.0f - l), l / 2.f, c.a};
}

inline HSLColor toHSL(LinearColor const& rgba)
{
  HSLColor    hsla = {0, 0, 0, rgba.a};
  float const cmax = std::max({rgba.r, rgba.g, rgba.b});
  float const cmin = std::min({rgba.r, rgba.g, rgba.b});
  float const d    = cmax - cmin;
  if (d == 0) {
    hsla.h = 0;
  } else if (cmax == rgba.r) {
    hsla.h = fmod((rgba.g - rgba.b) / d, 6.f);
  } else if (cmax == rgba.g) {
    hsla.h = (rgba.b - rgba.r) / d + 2.f;
  } else {
    hsla.h = (rgba.r - rgba.g) / d + 4.f;
  }

  hsla.h = fmod(hsla.h / 6.f + 1.f, 1.f);
  hsla.l = (cmax + cmin) / 2.f;
  hsla.s = (d == 0.f ? 0.f : d / (1.f - fabs(2.f * hsla.l - 1.f)));
  return hsla;
}

inline HSVColor toHSV(HSLColor const& hsla)
{
  float const s = hsla.s * (hsla.l < 0.5f ? hsla.l : 1 - hsla.l);
  return {hsla.h, 2.f * s / (hsla.l + s), hsla.l + s, hsla.a};
}

inline HSVColor toHSV(LinearColor const& rgba) { return toHSV(toHSL(rgba)); }

inline std::string toHexCode(sRGBColor const& c)
{
  char formated[10] = {0};
  std::snprintf(formated, 10, "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
  return formated;
}

inline sRGBColor hexCodeToSRGB(std::string_view c)
{
  uint8_t r = 0, g = 0, b = 0, a = 255;
  auto    hexToDec = [](char h) -> uint8_t {
    if (h >= '0' && h <= '9')
      return h - '0';
    else if (h >= 'a' && h <= 'f')
      return h - 'a' + 10;
    else if (h >= 'A' && h <= 'F')
      return h - 'A' + 10;
    else
      return 0;
  };
  if (c.length() < 4 || c[0] != '#') {
    r = g = b = 255; // on error return white
  } else {
    switch (c.length()) {
    case 5: // #rgba format
      a = hexToDec(c[4]);
      a += a * 16;
      [[fallthrough]];
    case 4: // #rgb format
      r = hexToDec(c[1]);
      g = hexToDec(c[2]);
      b = hexToDec(c[3]);
      r += r * 16;
      g += g * 16;
      b += b * 16;
      break;
    case 9: // #rrggbbaa format
      a = hexToDec(c[7]) * 16 + hexToDec(c[8]);
      [[fallthrough]];
    case 7: // #rrggbb format or #rrggbbaa format
      r = hexToDec(c[1]) * 16 + hexToDec(c[2]);
      g = hexToDec(c[3]) * 16 + hexToDec(c[4]);
      b = hexToDec(c[5]) * 16 + hexToDec(c[6]);
      break;
    default: r = g = b = a = 255;
    }
  }
  return {r, g, b, a};
}

inline FloatSRGBColor toFloatSRGB(sRGBColor c)
{
  return {c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f};
}

inline sRGBColor toSRGB(FloatSRGBColor const& c)
{
  return {
    uint8_t(clamp(255.f * c.r, 0.f, 255.f)),
    uint8_t(clamp(255.f * c.g, 0.f, 255.f)),
    uint8_t(clamp(255.f * c.b, 0.f, 255.f)),
    uint8_t(clamp(255.f * c.a, 0.f, 255.f))};
}

// aabb

inline AABB AABB::fromCenterAndSize(Vec2 const& center, Vec2 const& size)
{
  return AABB(center - size * 0.5f, center + size * 0.5f);
}
inline void AABB::merge(Vec2 const& v)
{
  min.x = std::min(min.x, v.x);
  min.y = std::min(min.y, v.y);
  max.x = std::max(max.x, v.x);
  max.y = std::max(max.y, v.y);
}
inline void AABB::merge(AABB const& aabb)
{
  min.x = std::min(min.x, aabb.min.x);
  min.y = std::min(min.y, aabb.min.y);
  max.x = std::max(max.x, aabb.max.x);
  max.y = std::max(max.y, aabb.max.y);
}
inline void AABB::expand(float amount)
{
  min.x -= amount;
  min.y -= amount;
  max.x += amount;
  max.y += amount;
}
inline AABB AABB::expanded(float amount) const
{
  AABB ex = *this;
  ex.expand(amount);
  return ex;
}
inline void AABB::move(Vec2 delta)
{
  min += delta;
  max += delta;
}
inline AABB AABB::moved(Vec2 delta) const { return AABB{min + delta, max + delta}; }
inline bool AABB::contains(AABB const& that) const
{
  return min.x <= that.min.x && min.y <= that.min.y && max.x >= that.max.x && max.y >= that.max.y;
}
inline bool AABB::contains(Vec2 const& pt) const
{
  return pt.x <= max.x && pt.y <= max.y && pt.x >= min.x && pt.y >= min.y;
}
inline bool AABB::intersects(AABB const& that) const
{
  return !(max.x < that.min.x || that.max.x < min.x || max.y < that.min.y || that.max.y < min.y);
}

// ref: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
inline bool AABB::intersects(Vec2 segstart, Vec2 segend) const
{
  return intersects(&segstart, &segend);
}
inline bool AABB::intersects(Vec2* segstart, Vec2* segend) const
{
  constexpr uint8_t INSIDE = 0; // 0000
  constexpr uint8_t LEFT   = 1; // 0001
  constexpr uint8_t RIGHT  = 2; // 0010
  constexpr uint8_t BOTTOM = 4; // 0100
  constexpr uint8_t TOP    = 8; // 1000

  // Compute the bit code for a point (x, y) using the clip rectangle
  // bounded diagonally by (xmin, ymin), and (xmax, ymax)
  auto computeOutCode = [=](float x, float y) {
    uint8_t code = INSIDE; // initialised as being inside of clip window
    if (x < min.x)         // to the left of clip window
      code |= LEFT;
    else if (x > max.x) // to the right of clip window
      code |= RIGHT;
    if (y < min.y) // below the clip window
      code |= BOTTOM;
    else if (y > max.y) // above the clip window
      code |= TOP;
    return code;
  };

  // Cohen–Sutherland clipping algorithm clips a line from
  // segstart = (x0, y0) to segend = (x1, y1) against a rectangle with
  // diagonal from (xmin, ymin) to (xmax, ymax).
  auto x0 = segstart->x, y0 = segstart->y;
  auto x1 = segend->x, y1 = segend->y;

  // compute outcodes for P0, P1, and whatever point lies outside the clip rectangle
  auto outcode0 = computeOutCode(x0, y0);
  auto outcode1 = computeOutCode(x1, y1);
  bool accept   = false;

  while (true) {
    if (!(outcode0 | outcode1)) {
      // bitwise OR is 0: both points inside window; trivially accept and exit loop
      accept = true;
      break;
    } else if (outcode0 & outcode1) {
      // bitwise AND is not 0: both points share an outside zone (LEFT, RIGHT, TOP,
      // or BOTTOM), so both must be outside window; exit loop (accept is false)
      break;
    } else {
      // failed both tests, so calculate the line segment to clip
      // from an outside point to an intersection with clip edge
      float x = 0, y = 0;

      // At least one endpoint is outside the clip rectangle; pick it.
      auto outcodeOut = outcode1 > outcode0 ? outcode1 : outcode0;

      // Now find the intersection point;
      // use formulas:
      //   slope = (y1 - y0) / (x1 - x0)
      //   x = x0 + (1 / slope) * (ym - y0), where ym is ymin or ymax
      //   y = y0 + slope * (xm - x0), where xm is xmin or xmax
      // No need to worry about divide-by-zero because, in each case, the
      // outcode bit being tested guarantees the denominator is non-zero
      if (outcodeOut & TOP) { // point is above the clip window
        x = x0 + (x1 - x0) * (max.y - y0) / (y1 - y0);
        y = max.y;
      } else if (outcodeOut & BOTTOM) { // point is below the clip window
        x = x0 + (x1 - x0) * (min.y - y0) / (y1 - y0);
        y = min.y;
      } else if (outcodeOut & RIGHT) { // point is to the right of clip window
        y = y0 + (y1 - y0) * (max.x - x0) / (x1 - x0);
        x = max.x;
      } else if (outcodeOut & LEFT) { // point is to the left of clip window
        y = y0 + (y1 - y0) * (min.x - x0) / (x1 - x0);
        x = min.x;
      }

      // Now we move outside point to intersection point to clip
      // and get ready for next pass.
      if (outcodeOut == outcode0) {
        x0       = x;
        y0       = y;
        outcode0 = computeOutCode(x0, y0);
      } else {
        x1       = x;
        y1       = y;
        outcode1 = computeOutCode(x1, y1);
      }
    }
  }
  if (accept) {
    segstart->x = x0;
    segstart->y = y0;
    segend->x   = x1;
    segend->y   = y1;
  }
  return accept;
}

// Hit Test {{{
inline float pointSegmentDistance(
  Vec2 const& pt,
  Vec2 const& segStart,
  Vec2 const& segEnd,
  Vec2*       outClosestPoint = nullptr)
{
  auto  direction = segEnd - segStart;
  auto  diff      = pt - segEnd;
  float t         = dot(direction, diff);
  Vec2  closept;
  if (t >= 0.f) {
    closept = segEnd;
  } else {
    diff = pt - segStart;
    t    = dot(direction, diff);
    if (t <= 0.f) {
      closept = segStart;
    } else {
      auto sqrLength = dot(direction, direction);
      if (sqrLength > 0.f) {
        t /= sqrLength;
        closept = segStart + direction * t;
      } else {
        closept = segStart;
      }
    }
  }
  diff = pt - closept;
  if (outClosestPoint)
    *outClosestPoint = closept;
  return length(diff);
}

// ray casting algo
// from https://rosettacode.org/wiki/Ray-casting_algorithm#C.2B.2B
inline bool raySeg(Vec2 const& a, Vec2 const& b, Vec2 const& p)
{
  constexpr auto                       epsilon = std::numeric_limits<float>().epsilon();
  constexpr std::numeric_limits<float> limits;
  constexpr float                      MIN = limits.min();
  constexpr float                      MAX = limits.max();

  if (a.y > b.y)
    return raySeg(b, a, p);
  if (p.y == a.y || p.y == b.y)
    return raySeg(a, b, {p.x, p.y + epsilon});
  if (p.y > b.y || p.y < a.y || p.x > std::max(a.x, b.x))
    return false;
  if (p.x < std::min(a.x, b.x))
    return true;
  auto blue = std::abs(a.x - p.x) > MIN ? (p.y - a.y) / (p.x - a.x) : MAX;
  auto red  = std::abs(a.x - b.x) > MIN ? (b.y - a.y) / (b.x - a.x) : MAX;
  return blue >= red;
}

inline bool hitTestPolygon(std::vector<Vec2> const& poly, Vec2 const& pos)
{
  size_t c = 0;
  for (size_t i = 0, n = poly.size(); i < n; ++i)
    if (raySeg(poly[i], poly[(i + 1) % n], pos))
      ++c;
  return c % 2 != 0;
}

inline bool hitTestPolyline(std::vector<Vec2> const& line, Vec2 const& pos, float eps = 1e-4f)
{
  for (size_t i = 0, n = line.size(); i + 1 < n; ++i) {
    if (pointSegmentDistance(pos, line[i], line[i + 1]) <= eps)
      return true;
  }
  return false;
}

inline bool hitTestAABB(AABB const& aabb, Vec2 pos) { return aabb.contains(pos); }

inline bool hitTestCircle(Vec2 const& center, float radius, Vec2 const& pos)
{
  return distance2(pos, center) <= radius * radius;
}

inline bool hitTestPoints(std::vector<Vec2> const& pts, Vec2 const& pos, float eps = 1e-5)
{
  for (auto const& pt : pts) {
    if (distance2(pt, pos) < eps * eps) {
      return true;
    }
  }
  return false;
}
// }}} Hit Test

inline bool strokeIntersects(std::vector<Vec2> const& a, std::vector<Vec2> const& b)
{
  // TODO: implement sweep line algorithm
  for (size_t i = 1; i < a.size(); ++i) {
    for (size_t j = 1; j < b.size(); ++j) {
      if (AABB(a[i - 1], a[i]).intersects(AABB(b[j - 1], b[j]))) {
        if (ccw(a[i - 1], b[j - 1], b[j]) == ccw(a[i], b[j - 1], b[j]))
          continue;
        else if (ccw(a[i - 1], a[i], b[j - 1]) == ccw(a[i - 1], a[i], b[j]))
          continue;
        else
          return true;
      }
    }
  }
  return false;
}

// }}} inline math functions

}
